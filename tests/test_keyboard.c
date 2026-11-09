#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/log.h>

#include "config.h"
#include "keyboard.h"
#include "server.h"

/* Linker wrapping limits failure injection to calls made by this test target. */
static bool fail_keymap_install;
/* Bits select individual calls so an install and its rollback can fail separately. */
static uint64_t failed_installations;
static unsigned keymap_install_calls;
static unsigned layout_change_logs;
static char last_layout_change[256];

/** Observe effective layout changes without confusing them with environment defaults. */
static void record_layout_change(enum wlr_log_importance importance, const char *format, va_list args) {
	char message[512];
	vsnprintf(message, sizeof(message), format, args);
	if (strstr(message, "reload: keyboard: layout=")) {
		layout_change_logs++;
		snprintf(last_layout_change, sizeof(last_layout_change), "%.*s", (int)sizeof(last_layout_change) - 1, message);
	} else if (importance == WLR_ERROR) {
		fprintf(stderr, "%s\n", message);
	}
}

/** Capture only one transaction so failure and unchanged reloads cannot look successful. */
static bool reload_with_log(struct comp_server *server, const struct comp_keyboard_config *config) {
	enum wlr_log_importance verbosity = wlr_log_get_verbosity();
	layout_change_logs = 0;
	last_layout_change[0] = '\0';
	wlr_log_init(WLR_INFO, record_layout_change);
	bool ok = comp_keyboard_reload(server, config);
	wlr_log_init(verbosity, NULL);
	return ok;
}

bool __real_wlr_keyboard_set_keymap(struct wlr_keyboard *keyboard, struct xkb_keymap *keymap);

/** Inject installation failure without changing the runtime keyboard API. */
bool __wrap_wlr_keyboard_set_keymap(struct wlr_keyboard *keyboard, struct xkb_keymap *keymap) {
	unsigned call = keymap_install_calls++;
	bool fail = fail_keymap_install ||
		(call < 64 && (failed_installations & (UINT64_C(1) << call)));
	return !fail && __real_wlr_keyboard_set_keymap(keyboard, keymap);
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
	unsigned cycle_modifiers;
	unsigned cycle_cancels;
	uint32_t cycle_depressed;
	uint32_t entered[WLR_KEYBOARD_KEYS_CAP];
	size_t num_entered;
	struct wlr_keyboard_modifiers delivered_modifiers;
	struct wlr_keyboard_modifiers entered_modifiers;
	bool consume;
	char last_event;
	xkb_keysym_t expected_sym;
	uint32_t expected_mods;
};

/** Replace only binding actions; dispatch and state updates use runtime code. */
bool comp_config_try_bindings(struct comp_config *cfg, struct comp_server *server,
		bool pressed, uint32_t mods, xkb_keysym_t sym) {
	/* Reload fixtures use real device registration without dispatching bindings. */
	if (cfg) {
		return false;
	}
	struct keyboard_test *test = wl_container_of(server, test, server);
	assert(pressed);
	assert(sym == test->expected_sym);
	assert(mods == test->expected_mods);
	if (!test->consume || (sym != XKB_KEY_Return && sym != XKB_KEY_Escape)) {
		return false;
	}
	test->bindings++;
	/* Exercise a focus change inside the action, before dispatch returns. */
	comp_keyboard_notify_enter(server->seat, &test->surface);
	return true;
}

bool server_window_cycle_active(const struct comp_server *server) {
	return server->cycle.active;
}

/** Record the compositor integration boundary after the seat receives its masks. */
void server_window_cycle_notify_mods(struct comp_server *server, uint32_t depressed) {
	if (server->config) {
		return;
	}
	struct keyboard_test *test = wl_container_of(server, test, server);
	assert(test->last_event == 'm');
	test->cycle_modifiers++;
	test->cycle_depressed = depressed;
}

/** Simulate cancellation's focus change so consumed Esc cannot leak into its snapshot. */
void server_window_cycle_cancel(struct comp_server *server) {
	struct keyboard_test *test = wl_container_of(server, test, server);
	test->cycle_cancels++;
	server->cycle.active = false;
	comp_keyboard_notify_enter(server->seat, &test->surface);
}

/** Capture the held-key snapshot supplied by the compositor during a focus change. */
static void record_enter(struct wlr_seat_keyboard_grab *grab,
		struct wlr_surface *surface, const uint32_t keys[], size_t count,
		const struct wlr_keyboard_modifiers *modifiers) {
	(void)surface;
	struct keyboard_test *test = wl_container_of(grab, test, grab);
	test->entered_modifiers = *modifiers;
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
	test->delivered_modifiers = *modifiers;
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
	test->keyboard.layout_count = xkb_keymap_num_layouts(test->wlr.keymap);
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

/** Reproduce backend modifier updates that lose the group selected by ISO_Next_Group. */
static void test_group_reset_stabilized(void) {
	struct keyboard_test test;
	test_init(&test);
	send_key(&test, KEY_LEFTSHIFT, true, true);
	send_key(&test, KEY_LEFTALT, true, true);
	assert(test.wlr.modifiers.group == 1);
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(test.reference, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(test.reference, XKB_STATE_MODS_LATCHED);
	xkb_mod_mask_t locked = xkb_state_serialize_mods(test.reference, XKB_STATE_MODS_LOCKED);
	unsigned delivered = test.modifiers;
	wlr_keyboard_notify_modifiers(&test.wlr, depressed, latched, locked, 0);
	assert(test.modifiers == delivered + 1);
	assert(test.delivered_modifiers.group == 1);
	xkb_state_update_mask(test.reference, depressed, latched, locked, 0, 0, 1);
	assert(test.wlr.modifiers.group == 1);
	assert(xkb_state_serialize_layout(test.wlr.xkb_state, XKB_STATE_LAYOUT_EFFECTIVE) == 1);
	send_key(&test, KEY_LEFTALT, false, true);
	send_key(&test, KEY_LEFTSHIFT, false, true);
	assert(test.wlr.modifiers.group == 1);
	assert(xkb_state_key_get_one_sym(test.wlr.xkb_state, KEY_Z + 8) == XKB_KEY_z);
	test_finish(&test);
}

/** Group-only changes and every modifier component must reach the same seat path. */
static void test_modifier_delivery(void) {
	struct keyboard_test test;
	test_init(&test);
	xkb_mod_mask_t shift = UINT32_C(1) << xkb_keymap_mod_get_index(test.wlr.keymap, XKB_MOD_NAME_SHIFT);
	xkb_mod_mask_t ctrl = UINT32_C(1) << xkb_keymap_mod_get_index(test.wlr.keymap, XKB_MOD_NAME_CTRL);
	xkb_mod_mask_t caps = UINT32_C(1) << xkb_keymap_mod_get_index(test.wlr.keymap, XKB_MOD_NAME_CAPS);
	wlr_keyboard_notify_modifiers(&test.wlr, shift, ctrl, caps, 0);
	assert(test.modifiers == 1);
	assert(test.delivered_modifiers.depressed == shift);
	assert(test.delivered_modifiers.latched == ctrl);
	assert(test.delivered_modifiers.locked == caps);
	wlr_keyboard_notify_modifiers(&test.wlr, shift, ctrl, caps, 1);
	assert(test.modifiers == 2 && test.delivered_modifiers.group == 1);
	assert(test.delivered_modifiers.depressed == shift);
	assert(test.delivered_modifiers.latched == ctrl);
	assert(test.delivered_modifiers.locked == caps);
	comp_keyboard_notify_enter(test.server.seat, &test.surface);
	assert(test.entered_modifiers.depressed == shift);
	assert(test.entered_modifiers.latched == ctrl);
	assert(test.entered_modifiers.locked == caps);
	assert(test.entered_modifiers.group == 1);
	wlr_keyboard_notify_modifiers(&test.wlr, shift, ctrl, caps, 1);
	assert(test.modifiers == 2);
	assert(test.cycle_modifiers == test.modifiers);
	assert(test.cycle_depressed == WLR_MODIFIER_SHIFT);
	test_finish(&test);
}

/** Window cycling must see Alt release even with another modifier still held. */
static void test_cycle_modifier_release(bool raw) {
	struct keyboard_test test;
	test_init(&test);
	send_key(&test, KEY_LEFTALT, true, raw);
	test.server.cycle.active = true;
	test.server.cycle.hold_mods = WLR_MODIFIER_ALT;
	send_key(&test, KEY_TAB, true, raw);
	send_key(&test, KEY_TAB, false, raw);
	assert(test.cycle_depressed == WLR_MODIFIER_ALT);
	send_key(&test, KEY_LEFTCTRL, true, raw);
	assert(test.cycle_depressed == (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL));
	send_key(&test, KEY_LEFTALT, false, raw);
	assert(test.cycle_depressed == WLR_MODIFIER_CTRL);
	send_key(&test, KEY_LEFTCTRL, false, raw);
	assert(test.cycle_depressed == 0);
	assert(test.cycle_modifiers == test.modifiers);
	test_finish(&test);
}

/** Esc cancels before bindings and stays consumed through repeats, release, and focus changes. */
static void test_cycle_escape(bool raw) {
	struct keyboard_test test;
	test_init(&test);
	test.consume = true;
	send_key(&test, KEY_LEFTALT, true, raw);
	test.server.cycle.active = true;
	test.server.cycle.hold_mods = WLR_MODIFIER_ALT;
	send_key(&test, KEY_ESC, true, raw);
	assert(test.cycle_cancels == 1 && !test.server.cycle.active);
	assert(test.bindings == 0 && test.keys == 1);
	assert(test.num_entered == 1 && test.entered[0] == KEY_LEFTALT);
	send_key(&test, KEY_ESC, true, raw);
	send_key(&test, KEY_ESC, false, raw);
	assert(test.cycle_cancels == 1 && test.bindings == 0 && test.keys == 1);
	send_key(&test, KEY_LEFTALT, false, raw);
	assert(test.keys == 2 && test.keyboard.num_consumed == 0);
	test.consume = false;
	send_key(&test, KEY_ESC, true, raw);
	send_key(&test, KEY_ESC, false, raw);
	assert(test.keys == 4 && test.cycle_cancels == 1);
	test_finish(&test);
}

/** Window selection must leave configured XKB group actions active. */
static void test_cycle_keeps_xkb(bool raw) {
	struct keyboard_test test;
	test_init(&test);
	send_key(&test, KEY_LEFTALT, true, raw);
	test.server.cycle.active = true;
	send_key(&test, KEY_LEFTSHIFT, true, raw);
	assert(test.wlr.modifiers.group == 1);
	send_key(&test, KEY_LEFTSHIFT, false, raw);
	send_key(&test, KEY_LEFTALT, false, raw);
	assert(test.wlr.modifiers.group == 1 && test.wlr.modifiers.depressed == 0);
	test_finish(&test);
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
	for (unsigned i = 0; i < 6; i++) {
		send_key(&test, KEY_LEFTSHIFT, true, raw);
		send_key(&test, KEY_LEFTALT, true, raw);
		send_key(&test, KEY_LEFTALT, false, raw);
		send_key(&test, KEY_LEFTSHIFT, false, raw);
		assert(test.wlr.modifiers.group == (i + 1) % 2);
	}
	assert(test.keys == 58);
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

/** Observe keymap installation while no candidate map can reach seat clients. */
struct reload_observer {
	struct wl_listener keymap;
	struct comp_server *server;
	unsigned changes;
};

/** Three devices exercise rollback past more than one successful installation. */
struct reload_test {
	struct comp_server server;
	struct comp_config config;
	struct comp_keyboard_config candidate;
	struct wlr_keyboard keyboards[3];
	struct reload_observer observers[3];
};

/** Avoid caller XKB defaults so transaction tests have deterministic candidates. */
static void reload_config_init(struct comp_keyboard_config *config, const char *layout) {
	assert(comp_keyboard_config_init(config));
	assert(comp_keyboard_config_set(config, "rules", "evdev"));
	assert(comp_keyboard_config_set(config, "model", "pc105"));
	assert(comp_keyboard_config_set(config, "layout", layout));
	assert(comp_keyboard_config_set(config, "variant", ""));
	assert(comp_keyboard_config_set(config, "options", "grp:alt_shift_toggle"));
	assert(comp_keyboard_config_compile(config));
}

/** A partially applied candidate must stay invisible through the seat's keymap listener. */
static void record_reload_keymap(struct wl_listener *listener, void *data) {
	(void)data;
	struct reload_observer *observer = wl_container_of(listener, observer, keymap);
	assert(wlr_seat_get_keyboard(observer->server->seat) == NULL);
	observer->changes++;
}

/** Register real keyboards; failure injection starts only after their initial maps exist. */
static void reload_test_init(struct reload_test *test) {
	memset(test, 0, sizeof(*test));
	test->server.config = &test->config;
	test->server.wl_display = wl_display_create();
	assert(test->server.wl_display);
	test->server.seat = wlr_seat_create(test->server.wl_display, "reload");
	assert(test->server.seat);
	wl_list_init(&test->server.keyboards);
	reload_config_init(&test->config.keyboard, "de,us");
	reload_config_init(&test->candidate, "us,de,it");
	for (size_t i = 0; i < 3; i++) {
		wlr_keyboard_init(&test->keyboards[i], NULL, "reload-test");
		assert(comp_keyboard_add(&test->server, &test->keyboards[i].base));
		test->observers[i].server = &test->server;
		test->observers[i].keymap.notify = record_reload_keymap;
		wl_signal_add(&test->keyboards[i].events.keymap, &test->observers[i].keymap);
	}
	keymap_install_calls = 0;
}

/** Remove observers before backend destruction, including quarantined devices. */
static void reload_test_finish(struct reload_test *test) {
	failed_installations = 0;
	comp_keyboard_clear(&test->server);
	for (size_t i = 0; i < 3; i++) {
		wl_list_remove(&test->observers[i].keymap.link);
		wlr_keyboard_finish(&test->keyboards[i]);
	}
	comp_keyboard_config_finish(&test->config.keyboard);
	comp_keyboard_config_finish(&test->candidate);
	wl_display_destroy(test->server.wl_display);
}

/** Successful replacement reaches every device while retaining seat selection and repeat. */
static void test_reload_success(void) {
	struct reload_test test;
	reload_test_init(&test);
	wlr_keyboard_set_repeat_info(&test.keyboards[0], 31, 420);
	struct wlr_keyboard *active = wlr_seat_get_keyboard(test.server.seat);
	assert(reload_with_log(&test.server, &test.candidate));
	assert(layout_change_logs == 1);
	assert(strstr(last_layout_change, "reload: keyboard: layout=de,us => layout=us,de,it"));
	assert(keymap_install_calls == 3);
	assert(wlr_seat_get_keyboard(test.server.seat) == active);
	assert(test.keyboards[0].repeat_info.rate == 31 && test.keyboards[0].repeat_info.delay == 420);
	for (size_t i = 0; i < 3; i++) {
		assert(test.keyboards[i].keymap == test.candidate.keymap);
		assert(test.observers[i].changes == 1);
		struct comp_keyboard *kbd = test.keyboards[i].data;
		assert(kbd->layout_count == 3 && !kbd->group_lock_active);
		assert(xkb_state_key_get_one_sym(test.keyboards[i].xkb_state, KEY_Z + 8) == XKB_KEY_z);
	}
	/* The caller owns the config commit; this helper never frees or replaces it. */
	assert(test.server.config == &test.config);
	reload_test_finish(&test);
}

/** A failed first, middle, or final install restores all already-changed devices. */
static void test_reload_failure(unsigned failing_device) {
	struct reload_test test;
	reload_test_init(&test);
	/* A backend may have supplied a device-specific map. Rollback must restore
	 * that map rather than blindly reinstalling the config's shared keymap. */
	struct comp_keyboard_config device_config = {0};
	reload_config_init(&device_config, "fr");
	wl_list_remove(&test.observers[0].keymap.link);
	assert(wlr_keyboard_set_keymap(&test.keyboards[0], device_config.keymap));
	wl_signal_add(&test.keyboards[0].events.keymap, &test.observers[0].keymap);
	((struct comp_keyboard *)test.keyboards[0].data)->layout_count = 1;
	keymap_install_calls = 0;
	failed_installations = UINT64_C(1) << failing_device;
	assert(!reload_with_log(&test.server, &test.candidate));
	assert(layout_change_logs == 0);
	assert(keymap_install_calls == 2 * failing_device + 1);
	assert(wlr_seat_get_keyboard(test.server.seat) == &test.keyboards[2]);
	for (size_t i = 0; i < 3; i++) {
		assert(test.keyboards[i].keymap == (i == 0 ? device_config.keymap : test.config.keyboard.keymap));
		assert(test.observers[i].changes == (i < failing_device ? 2u : 0u));
		assert(((struct comp_keyboard *)test.keyboards[i].data)->layout_count == (i == 0 ? 1u : 2u));
	}
	assert(wl_list_length(&test.server.keyboards) == 3);
	reload_test_finish(&test);
	comp_keyboard_config_finish(&device_config);
}

/** Quarantine only the device whose rollback failed; restore the rest and select a survivor. */
static void test_reload_rollback_failure(int active_index) {
	struct reload_test test;
	reload_test_init(&test);
	struct wlr_keyboard *active = active_index < 0 ? NULL : &test.keyboards[active_index];
	wlr_seat_set_keyboard(test.server.seat, active);
	/* Third install fails, then the rollback of the second keyboard fails. */
	failed_installations = (UINT64_C(1) << 2) | (UINT64_C(1) << 3);
	assert(!comp_keyboard_reload(&test.server, &test.candidate));
	assert(keymap_install_calls == 5);
	assert(test.keyboards[1].data == NULL);
	assert(test.keyboards[0].keymap == test.config.keyboard.keymap);
	assert(test.keyboards[2].keymap == test.config.keyboard.keymap);
	assert(wl_list_length(&test.server.keyboards) == 2);
	assert(wlr_seat_get_keyboard(test.server.seat) == (active_index == 1 ? &test.keyboards[0] : active));
	assert(wl_list_empty(&test.keyboards[1].events.key.listener_list));
	reload_test_finish(&test);
}

/** Until migration is implemented, held keys, locks, and groups reject before installation. */
static void test_reload_preflight(void) {
	struct reload_test test;
	reload_test_init(&test);
	const struct wlr_keyboard_modifiers states[] = {
		{.depressed = 1}, {.latched = 1}, {.locked = 2}, {.group = 1},
	};
	for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		const struct wlr_keyboard_modifiers *mods = &states[i];
		wlr_keyboard_notify_modifiers(&test.keyboards[0], mods->depressed, mods->latched, mods->locked, mods->group);
		assert(!comp_keyboard_reload(&test.server, &test.candidate));
		assert(keymap_install_calls == 0);
		assert(test.keyboards[0].keymap == test.config.keyboard.keymap);
	}
	wlr_keyboard_notify_modifiers(&test.keyboards[0], 0, 0, 0, 0);
	struct wlr_keyboard_key_event event = {
		.keycode = KEY_A, .update_state = true, .state = WL_KEYBOARD_KEY_STATE_PRESSED,
	};
	wlr_keyboard_notify_key(&test.keyboards[0], &event);
	assert(!comp_keyboard_reload(&test.server, &test.candidate));
	assert(keymap_install_calls == 0);
	/* Unchanged RMLVO must keep the existing state even while keys are held. */
	assert(reload_with_log(&test.server, &test.config.keyboard));
	assert(layout_change_logs == 0);
	assert(keymap_install_calls == 0);
	event.state = WL_KEYBOARD_KEY_STATE_RELEASED;
	wlr_keyboard_notify_key(&test.keyboards[0], &event);
	assert(comp_keyboard_reload(&test.server, &test.candidate));
	reload_test_finish(&test);
}

/** Reload without devices must allow the caller to commit settings for future hotplug. */
static void test_reload_without_keyboards(void) {
	struct reload_test test;
	reload_test_init(&test);
	comp_keyboard_clear(&test.server);
	assert(reload_with_log(&test.server, &test.candidate));
	assert(layout_change_logs == 1);
	assert(keymap_install_calls == 0);
	assert(wlr_seat_get_keyboard(test.server.seat) == NULL);
	reload_test_finish(&test);
}

/** Run dispatch and device-lifecycle regressions without starting a compositor backend. */
int main(void) {
	test_state(true);
	test_state(false);
	test_group_reset_stabilized();
	test_modifier_delivery();
	test_cycle_modifier_release(true);
	test_cycle_modifier_release(false);
	test_cycle_escape(true);
	test_cycle_escape(false);
	test_cycle_keeps_xkb(true);
	test_cycle_keeps_xkb(false);
	test_consumed();
	test_lifecycle();
	test_reload_success();
	for (unsigned i = 0; i < 3; i++) {
		test_reload_failure(i);
	}
	test_reload_rollback_failure(-1);
	test_reload_rollback_failure(1);
	test_reload_rollback_failure(2);
	test_reload_preflight();
	test_reload_without_keyboards();
	puts("keyboard tests passed");
	return 0;
}
