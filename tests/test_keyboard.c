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

/* Linker wrapping limits failure injection to calls made by this test target. */
static bool fail_keymap_install;

bool __real_wlr_keyboard_set_keymap(struct wlr_keyboard *keyboard, struct xkb_keymap *keymap);

/** Inject installation failure without changing the runtime keyboard API. */
bool __wrap_wlr_keyboard_set_keymap(struct wlr_keyboard *keyboard, struct xkb_keymap *keymap) {
	return !fail_keymap_install && __real_wlr_keyboard_set_keymap(keyboard, keymap);
}

/** Keyboard-only capability stub; pointer/touch devices are outside this fixture. */
void server_update_seat_capabilities(struct comp_server *server) {
	wlr_seat_set_capabilities(server->seat,
		wl_list_empty(&server->keyboards) ? 0 : WL_SEAT_CAPABILITY_KEYBOARD);
}

/**
 * Real wlroots keyboard with a seat grab recording compositor delivery.
 *
 * The independent XKB state is the update oracle. The surface is only a target
 * for the recording grab; this fixture does not create a Wayland client.
 */
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

/** Replace only binding actions; dispatch and state updates use runtime code. */
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

/** Capture the held-key snapshot supplied by the compositor during a focus change. */
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

/** Count forwarded keys and reject modifier delivery that precedes its raw key. */
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

/** Record modifier delivery and verify it carries the keyboard's current group. */
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

/** Build an isolated de,us fixture with runtime listeners and a recording seat grab. */
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

/** Remove fixture listeners before releasing its keyboard, XKB state, and display. */
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

/** Verify lock/modifier and group sequences for raw keys or backend-supplied masks. */
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

/** Keep shortcut presses, repeats, and releases out of client delivery across focus changes. */
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

/** Failed hotplug must preserve the active keyboard and leave no listeners behind. */
static void test_lifecycle(void) {
	struct comp_server server = {0};
	struct comp_config config = {0};
	server.config = &config;
	server.wl_display = wl_display_create();
	assert(server.wl_display);
	server.seat = wlr_seat_create(server.wl_display, "lifecycle");
	assert(server.seat);
	wl_list_init(&server.keyboards);
	assert(comp_keyboard_config_init(&config.keyboard));
	assert(comp_keyboard_config_set(&config.keyboard, "rules", "evdev"));
	assert(comp_keyboard_config_set(&config.keyboard, "layout", "de,us"));
	assert(comp_keyboard_config_set(&config.keyboard, "variant", ""));
	assert(comp_keyboard_config_compile(&config.keyboard));
	struct wlr_keyboard first, second, rejected;
	wlr_keyboard_init(&first, NULL, "first");
	wlr_keyboard_init(&second, NULL, "second");
	wlr_keyboard_init(&rejected, NULL, "rejected");
	assert(comp_keyboard_add(&server, &first.base));
	uint64_t first_id = ((struct comp_keyboard *)first.data)->id;
	assert(comp_keyboard_add(&server, &second.base));
	assert(((struct comp_keyboard *)second.data)->id > first_id);
	assert(wlr_seat_get_keyboard(server.seat) == &second);
	fail_keymap_install = true;
	assert(!comp_keyboard_add(&server, &rejected.base));
	fail_keymap_install = false;
	assert(rejected.data == NULL && rejected.keymap == NULL);
	assert(wl_list_empty(&rejected.events.key.listener_list));
	assert(wl_list_empty(&rejected.base.events.destroy.listener_list));
	assert(wl_list_length(&server.keyboards) == 2);
	assert(wlr_seat_get_keyboard(server.seat) == &second);
	wlr_keyboard_finish(&second);
	assert(wlr_seat_get_keyboard(server.seat) == &first);
	assert(wl_list_length(&server.keyboards) == 1);
	assert(comp_keyboard_add(&server, &rejected.base));
	assert(((struct comp_keyboard *)rejected.data)->id > first_id + 1);
	comp_keyboard_clear(&server);
	assert(wl_list_empty(&server.keyboards));
	assert(first.data == NULL && rejected.data == NULL);
	assert(wlr_seat_get_keyboard(server.seat) == NULL);
	wlr_keyboard_finish(&first);
	wlr_keyboard_finish(&rejected);
	comp_keyboard_config_finish(&config.keyboard);
	wl_display_destroy(server.wl_display);
}

/** Run dispatch and device-lifecycle regressions without starting a compositor backend. */
int main(void) {
	test_state(true);
	test_state(false);
	test_consumed();
	test_lifecycle();
	puts("keyboard tests passed");
	return 0;
}
