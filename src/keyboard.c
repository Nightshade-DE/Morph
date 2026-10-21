#include "keyboard.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "config.h"
#include "server.h"

/**
 * Unlink a registered wrapper and clear the device's compositor back-reference.
 *
 * The backend owns the input device and its keymap. The caller handles seat
 * selection separately; this helper is shared by hot-unplug and shutdown.
 */
static void keyboard_remove(struct comp_keyboard *kbd) {
	wlr_keyboard_from_input_device(kbd->dev)->data = NULL;
	wl_list_remove(&kbd->destroy.link);
	wl_list_remove(&kbd->key.link);
	wl_list_remove(&kbd->modifiers.link);
	wl_list_remove(&kbd->link);
	free(kbd);
}

static bool keyboard_debug_enabled(void) {
	const char *value = getenv("MORPH_LOG_KEYS");
	return value && value[0] && strcmp(value, "0") != 0;
}

static void keyboard_note_group_action(struct comp_keyboard *kbd, xkb_keysym_t sym) {
	if (kbd->layout_count <= 1) {
		return;
	}
	xkb_layout_index_t group = kbd->group_lock_active ? kbd->locked_group :
		wlr_keyboard_from_input_device(kbd->dev)->modifiers.group;
	switch (sym) {
	case XKB_KEY_ISO_Next_Group:
	case XKB_KEY_ISO_Next_Group_Lock:
		group = (group + 1) % kbd->layout_count;
		break;
	case XKB_KEY_ISO_Prev_Group:
	case XKB_KEY_ISO_Prev_Group_Lock:
		group = group == 0 ? kbd->layout_count - 1 : group - 1;
		break;
	case XKB_KEY_ISO_First_Group:
	case XKB_KEY_ISO_First_Group_Lock:
		group = 0;
		break;
	case XKB_KEY_ISO_Last_Group:
	case XKB_KEY_ISO_Last_Group_Lock:
		group = kbd->layout_count - 1;
		break;
	default:
		return;
	}
	kbd->locked_group = group;
	kbd->group_lock_active = true;
}

static void keyboard_stabilize_group(struct comp_keyboard *kbd, struct wlr_keyboard *wlr_kbd) {
	if (!kbd->group_lock_active || !wlr_kbd->xkb_state || kbd->locked_group >= kbd->layout_count ||
		wlr_kbd->modifiers.group == kbd->locked_group) {
		return;
	}
	xkb_state_update_mask(wlr_kbd->xkb_state, wlr_kbd->modifiers.depressed,
		wlr_kbd->modifiers.latched, wlr_kbd->modifiers.locked, 0, 0, kbd->locked_group);
	wlr_kbd->modifiers.group = kbd->locked_group;
}

/**
 * Handle backend device destruction while the seat is still available.
 *
 * If this was the active keyboard, select the oldest remaining device or clear
 * the seat keyboard. Removing an inactive device preserves the current selection.
 */
static void keyboard_destroy(struct wl_listener *listener, void *data) {
	(void)data;
	struct comp_keyboard *kbd = wl_container_of(listener, kbd, destroy);
	struct comp_server *server = kbd->server;
	bool active = wlr_seat_get_keyboard(server->seat) == wlr_keyboard_from_input_device(kbd->dev);
	keyboard_remove(kbd);
	if (active) {
		struct wlr_keyboard *next = NULL;
		if (!wl_list_empty(&server->keyboards)) {
			struct comp_keyboard *first = wl_container_of(server->keyboards.next, first, link);
			next = wlr_keyboard_from_input_device(first->dev);
		}
		wlr_seat_set_keyboard(server->seat, next);
	}
	server_update_seat_capabilities(server);
}

/**
 * Install the validated config keymap and register a backend-owned keyboard.
 *
 * Requires an initialized seat and keyboard list. Failure logs the cause and
 * leaves the seat and listener lists unchanged. Success assigns a session ID,
 * stores the wrapper in keyboard->data, and makes the device active on the seat.
 */
bool comp_keyboard_add(struct comp_server *server, struct wlr_input_device *device) {
	struct wlr_keyboard *keyboard = wlr_keyboard_from_input_device(device);
	struct comp_keyboard *kbd = calloc(1, sizeof(*kbd));
	if (!kbd) {
		wlr_log(WLR_ERROR, "keyboard: cannot allocate device state for '%s'", device->name);
		return false;
	}
	/* Do not publish a device until keymap installation has succeeded. */
	if (!server->config->keyboard.keymap || server->next_keyboard_id == UINT64_MAX ||
		!wlr_keyboard_set_keymap(keyboard, server->config->keyboard.keymap)) {
		wlr_log(WLR_ERROR, "keyboard: cannot install keymap for '%s'", device->name);
		free(kbd);
		return false;
	}
	kbd->server = server;
	kbd->dev = device;
	kbd->id = ++server->next_keyboard_id;
	kbd->layout_count = xkb_keymap_num_layouts(keyboard->keymap);
	kbd->destroy.notify = keyboard_destroy;
	kbd->key.notify = comp_keyboard_handle_key;
	kbd->modifiers.notify = comp_keyboard_handle_modifiers;
	wl_signal_add(&device->events.destroy, &kbd->destroy);
	wl_signal_add(&keyboard->events.key, &kbd->key);
	wl_signal_add(&keyboard->events.modifiers, &kbd->modifiers);
	wl_list_insert(server->keyboards.prev, &kbd->link);
	keyboard->data = kbd;
	wlr_keyboard_set_repeat_info(keyboard, 25, 600);
	wlr_seat_set_keyboard(server->seat, keyboard);
	server_update_seat_capabilities(server);
	return true;
}

/**
 * Detach all keyboard wrappers before seat and backend teardown.
 *
 * Clear the seat selection first, then remove our listeners so later backend
 * release/destroy events cannot access compositor state during shutdown.
 * Device destruction remains the backend's responsibility.
 */
void comp_keyboard_clear(struct comp_server *server) {
	wlr_seat_set_keyboard(server->seat, NULL);
	struct comp_keyboard *kbd, *tmp;
	wl_list_for_each_safe(kbd, tmp, &server->keyboards, link) {
		keyboard_remove(kbd);
	}
}

/**
 * Keyboard key callback.
 *
 * Resolves keysym/modifier state for binding matching and forwards events to
 * focused clients. For raw events, wlroots updates XKB after this listener
 * returns; nested backends supply modifier masks separately. Do not re-enter
 * wlr_keyboard_notify_key() or update XKB here, even for consumed shortcuts.
 */
void comp_keyboard_handle_key(struct wl_listener *listener, void *data) {
	struct comp_keyboard *kbd = wl_container_of(listener, kbd, key);
	/* During shutdown wlroots may emit late key events; avoid seat access then. */
	if (!kbd->server || !kbd->server->seat || kbd->server->display_terminate_requested) {
		return;
	}
	struct wlr_keyboard_key_event *event = data;
	struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(kbd->dev);

	uint32_t mods_filtered = 0;
	xkb_keysym_t sym = XKB_KEY_NoSymbol;
	if (wlr_kbd->xkb_state && wlr_kbd->keymap) {
		mods_filtered = wlr_keyboard_get_modifiers(wlr_kbd) & COMP_BIND_MOD_FILTER;
		/* wlroots uses Linux evdev codes; XKB uses evdev + 8. Never call xkb with a
		 * keycode outside this keymap's range (libxkbcommon can fault otherwise). */
		uint64_t xkbc = (uint64_t)event->keycode + 8u;
		xkb_keycode_t lo = xkb_keymap_min_keycode(wlr_kbd->keymap);
		xkb_keycode_t hi = xkb_keymap_max_keycode(wlr_kbd->keymap);
		if (xkbc >= lo && xkbc <= hi) {
			sym = xkb_state_key_get_one_sym(wlr_kbd->xkb_state, xkbc);
		}
	}

	const bool pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
	/* Consumption lasts until release, regardless of later modifiers or focus. */
	for (size_t i = 0; i < kbd->num_consumed; i++) {
		if (kbd->consumed[i] == event->keycode) {
			if (!pressed) {
				kbd->consumed[i] = kbd->consumed[--kbd->num_consumed];
			}
			return;
		}
	}
	if (pressed) {
		if (keyboard_debug_enabled() && sym != XKB_KEY_NoSymbol) {
			char name[128];
			if (xkb_keysym_get_name(sym, name, sizeof(name)) < 0) {
				snprintf(name, sizeof(name), "(bad)");
			}
			wlr_log(WLR_INFO, "keyboard: keycode=%u keysym=%s mods=0x%x update_state=%d",
				event->keycode, name, mods_filtered, event->update_state);
		}
		keyboard_note_group_action(kbd, sym);
	}

	wlr_seat_set_keyboard(kbd->server->seat, wlr_kbd);
	/* Reserve before dispatch: a binding can change focus synchronously. */
	bool reserved = pressed && kbd->num_consumed < WLR_KEYBOARD_KEYS_CAP;
	if (reserved) {
		kbd->consumed[kbd->num_consumed++] = event->keycode;
	}
	if (reserved && comp_config_try_bindings(kbd->server->config, kbd->server, pressed, mods_filtered, sym)) {
		if (mods_filtered & WLR_MODIFIER_LOGO) {
			kbd->server->suppress_logo_pointer_drag = true;
		}
		return;
	}
	if (reserved) {
		kbd->num_consumed--;
	}
	wlr_seat_keyboard_notify_key(kbd->server->seat, event->time_msec, event->keycode, event->state);
}

/**
 * Forward wlroots' serialized modifier masks and effective group to the seat.
 *
 * Group-only changes follow the same path. Resolve Logo through wlroots rather
 * than comparing XKB mask positions with WLR modifier constants.
 */
void comp_keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	(void)data;
	struct comp_keyboard *kbd = wl_container_of(listener, kbd, modifiers);
	if (!kbd->server || !kbd->server->seat || kbd->server->display_terminate_requested) {
		return;
	}
	struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(kbd->dev);
	wlr_seat_set_keyboard(kbd->server->seat, wlr_kbd);
	keyboard_stabilize_group(kbd, wlr_kbd);
	if (keyboard_debug_enabled()) {
		wlr_log(WLR_INFO, "keyboard: modifiers depressed=0x%x latched=0x%x locked=0x%x group=%u effective=0x%x",
			(unsigned)wlr_kbd->modifiers.depressed, (unsigned)wlr_kbd->modifiers.latched,
			(unsigned)wlr_kbd->modifiers.locked, (unsigned)wlr_kbd->modifiers.group,
			wlr_keyboard_get_modifiers(wlr_kbd));
	}
	wlr_seat_keyboard_notify_modifiers(kbd->server->seat, &wlr_kbd->modifiers);
	if ((wlr_keyboard_get_modifiers(wlr_kbd) & WLR_MODIFIER_LOGO) == 0) {
		kbd->server->suppress_logo_pointer_drag = false;
	}
}

/**
 * Transfer keyboard focus with consumed shortcut keys removed from the snapshot.
 *
 * Requires a valid target surface. Filter a local copy: wlroots must retain the
 * complete pressed-key list for its own XKB updates and device cleanup.
 */
void comp_keyboard_notify_enter(struct wlr_seat *seat, struct wlr_surface *surface) {
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (!keyboard) {
		wlr_seat_keyboard_notify_enter(seat, surface, NULL, 0, NULL);
		return;
	}
	struct comp_keyboard *kbd = keyboard->data;
	uint32_t keys[WLR_KEYBOARD_KEYS_CAP];
	size_t count = 0;
	for (size_t i = 0; i < keyboard->num_keycodes; i++) {
		bool consumed = false;
		for (size_t j = 0; kbd && j < kbd->num_consumed; j++) {
			if (keyboard->keycodes[i] == kbd->consumed[j]) {
				consumed = true;
				break;
			}
		}
		if (!consumed) {
			keys[count++] = keyboard->keycodes[i];
		}
	}
	wlr_seat_keyboard_notify_enter(seat, surface, keys, count, &keyboard->modifiers);
}
