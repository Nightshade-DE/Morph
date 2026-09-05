#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>

#include "config.h"
#include "keyboard.h"
#include "server.h"

/** Real wlroots keyboard with a seat grab recording compositor delivery. */
struct keyboard_test {
	struct comp_server server;
	struct comp_keyboard keyboard;
	struct wlr_keyboard wlr;
	struct wlr_seat_keyboard_grab grab;
	struct xkb_state *reference;
	struct wlr_surface surface;
	unsigned keys;
	unsigned modifiers;
	unsigned bindings;
	uint32_t entered[WLR_KEYBOARD_KEYS_CAP];
	size_t num_entered;
	bool consume;
	char last_event;
	xkb_keysym_t expected_sym;
	uint32_t expected_mods;
};

/** Replace only binding actions; dispatch and state updates use production code. */
bool comp_config_try_bindings(struct comp_config *cfg, struct comp_server *server,
		bool pressed, uint32_t mods, xkb_keysym_t sym) {
	(void)cfg;
	struct keyboard_test *test = wl_container_of(server, test, server);
	assert(pressed);
	assert(sym == test->expected_sym);
	assert(mods == test->expected_mods);
	if (!test->consume || sym != XKB_KEY_Return) {
		return false;
	}
	test->bindings++;
	/* Exercise a focus change inside the action, before dispatch returns. */
	comp_keyboard_notify_enter(server->seat, &test->surface);
	return true;
}

static void record_enter(struct wlr_seat_keyboard_grab *grab,
		struct wlr_surface *surface, const uint32_t keys[], size_t count,
		const struct wlr_keyboard_modifiers *modifiers) {
	(void)surface;
	(void)modifiers;
	struct keyboard_test *test = wl_container_of(grab, test, grab);
	test->num_entered = count;
	if (count) {
		memcpy(test->entered, keys, count * sizeof(*keys));
	}
}

static void record_key(struct wlr_seat_keyboard_grab *grab, uint32_t time,
		uint32_t key, uint32_t state) {
	(void)time;
	(void)key;
	(void)state;
	struct keyboard_test *test = wl_container_of(grab, test, grab);
	/* A state-changing raw key must reach the seat before its modifiers. */
	assert(test->last_event != 'm');
	test->keys++;
	test->last_event = 'k';
}

static void record_modifiers(struct wlr_seat_keyboard_grab *grab,
		const struct wlr_keyboard_modifiers *modifiers) {
	struct keyboard_test *test = wl_container_of(grab, test, grab);
	assert(modifiers->group == test->wlr.modifiers.group);
	test->modifiers++;
	test->last_event = 'm';
}

static const struct wlr_keyboard_grab_interface grab_interface = {
	.enter = record_enter,
	.key = record_key,
	.modifiers = record_modifiers,
};

static void test_init(struct keyboard_test *test) {
	memset(test, 0, sizeof(*test));
	test->server.wl_display = wl_display_create();
	assert(test->server.wl_display);
	test->server.seat = wlr_seat_create(test->server.wl_display, "test");
	assert(test->server.seat);
	wlr_keyboard_init(&test->wlr, NULL, "test");
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
	assert(ctx);
	struct xkb_rule_names rules = {
		.rules = "evdev", .model = "pc105", .layout = "de,us",
		.variant = "", .options = "grp:alt_shift_toggle",
	};
	struct xkb_keymap *map = xkb_keymap_new_from_names(ctx, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	assert(map);
	assert(wlr_keyboard_set_keymap(&test->wlr, map));
	test->reference = xkb_state_new(map);
	assert(test->reference);
	xkb_keymap_unref(map);
	xkb_context_unref(ctx);
	test->keyboard.server = &test->server;
	test->keyboard.dev = &test->wlr.base;
	test->wlr.data = &test->keyboard;
	test->keyboard.key.notify = comp_keyboard_handle_key;
	test->keyboard.modifiers.notify = comp_keyboard_handle_modifiers;
	wl_signal_add(&test->wlr.events.key, &test->keyboard.key);
	wl_signal_add(&test->wlr.events.modifiers, &test->keyboard.modifiers);
	wlr_seat_set_keyboard(test->server.seat, &test->wlr);
	test->grab.interface = &grab_interface;
	wlr_seat_keyboard_start_grab(test->server.seat, &test->grab);
}

static void test_finish(struct keyboard_test *test) {
	wlr_seat_keyboard_end_grab(test->server.seat);
	wlr_seat_set_keyboard(test->server.seat, NULL);
	wl_list_remove(&test->keyboard.key.link);
	wl_list_remove(&test->keyboard.modifiers.link);
	wlr_keyboard_finish(&test->wlr);
	xkb_state_unref(test->reference);
	wl_display_destroy(test->server.wl_display);
}

/** Compare against one independent XKB update for each backend event. */
static void send_key(struct keyboard_test *test, uint32_t key, bool pressed, bool raw) {
	test->last_event = 0;
	test->expected_sym = xkb_state_key_get_one_sym(test->wlr.xkb_state, key + 8);
	test->expected_mods = wlr_keyboard_get_modifiers(&test->wlr) & COMP_BIND_MOD_FILTER;
	struct wlr_keyboard_key_event event = {
		.keycode = key, .update_state = raw,
		.state = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
	};
	xkb_state_update_key(test->reference, key + 8, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
	wlr_keyboard_notify_key(&test->wlr, &event);
	if (!raw) {
		/* Nested backends supply masks separately instead of updating from keys. */
		wlr_keyboard_notify_modifiers(&test->wlr,
			xkb_state_serialize_mods(test->reference, XKB_STATE_MODS_DEPRESSED),
			xkb_state_serialize_mods(test->reference, XKB_STATE_MODS_LATCHED),
			xkb_state_serialize_mods(test->reference, XKB_STATE_MODS_LOCKED),
			xkb_state_serialize_layout(test->reference, XKB_STATE_LAYOUT_EFFECTIVE));
	}
	const enum xkb_state_component components[] = {
		XKB_STATE_MODS_DEPRESSED, XKB_STATE_MODS_LATCHED, XKB_STATE_MODS_LOCKED,
	};
	for (size_t i = 0; i < sizeof(components) / sizeof(components[0]); i++) {
		assert(xkb_state_serialize_mods(test->wlr.xkb_state, components[i]) ==
			xkb_state_serialize_mods(test->reference, components[i]));
	}
	assert(test->wlr.modifiers.group ==
		xkb_state_serialize_layout(test->reference, XKB_STATE_LAYOUT_EFFECTIVE));
}

static void test_state(bool raw) {
	struct keyboard_test test;
	test_init(&test);
	const uint32_t keys[] = {KEY_A, KEY_CAPSLOCK, KEY_CAPSLOCK, KEY_RIGHTALT, KEY_LEFTMETA};
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		send_key(&test, keys[i], true, raw);
		send_key(&test, keys[i], false, raw);
	}
	for (unsigned i = 0; i < 6; i++) {
		send_key(&test, KEY_LEFTALT, true, raw);
		send_key(&test, KEY_LEFTSHIFT, true, raw);
		send_key(&test, KEY_LEFTSHIFT, false, raw);
		send_key(&test, KEY_LEFTALT, false, raw);
		assert(test.wlr.modifiers.group == (i + 1) % 2);
	}
	assert(test.keys == 34);
	assert(test.modifiers > 0);
	assert(test.wlr.modifiers.depressed == 0);
	assert(test.wlr.num_keycodes == 0);
	test_finish(&test);
}

static void test_consumed(void) {
	struct keyboard_test test;
	test_init(&test);
	test.consume = true;
	send_key(&test, KEY_LEFTMETA, true, true);
	send_key(&test, KEY_ENTER, true, true);
	assert(test.bindings == 1 && test.keys == 1);
	assert(test.num_entered == 1 && test.entered[0] == KEY_LEFTMETA);
	assert(test.server.suppress_logo_pointer_drag);
	send_key(&test, KEY_ENTER, true, true);
	assert(test.bindings == 1 && test.keys == 1);
	comp_keyboard_notify_enter(test.server.seat, &test.surface);
	assert(test.num_entered == 1 && test.entered[0] == KEY_LEFTMETA);
	send_key(&test, KEY_LEFTMETA, false, true);
	assert(!test.server.suppress_logo_pointer_drag);
	send_key(&test, KEY_ENTER, false, true);
	assert(test.keys == 2 && test.keyboard.num_consumed == 0);
	test.consume = false;
	send_key(&test, KEY_ENTER, true, true);
	comp_keyboard_notify_enter(test.server.seat, &test.surface);
	assert(test.num_entered == 1 && test.entered[0] == KEY_ENTER);
	send_key(&test, KEY_ENTER, false, true);
	assert(test.keys == 4);
	test_finish(&test);
}

int main(void) {
	test_state(true);
	test_state(false);
	test_consumed();
	puts("keyboard tests passed");
	return 0;
}
