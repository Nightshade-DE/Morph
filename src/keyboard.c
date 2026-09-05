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
 * Keyboard key callback.
 *
 * Resolves keysym/modifier state for binding matching and forwards events to
 * focused clients. wlroots updates XKB after this listener returns.
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
	for (size_t i = 0; i < kbd->num_consumed; i++) {
		if (kbd->consumed[i] == event->keycode) {
			if (!pressed) {
				kbd->consumed[i] = kbd->consumed[--kbd->num_consumed];
			}
			return;
		}
	}
	if (pressed) {
		const char *kdbg = getenv("MORPH_LOG_KEYS");
		if (kdbg && kdbg[0] && strcmp(kdbg, "0") != 0 && sym != XKB_KEY_NoSymbol) {
			char name[128];
			if (xkb_keysym_get_name(sym, name, sizeof(name)) < 0) {
				snprintf(name, sizeof(name), "(bad)");
			}
			wlr_log(WLR_INFO, "keyboard: keysym=%s mods=0x%x", name, mods_filtered);
		}
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

/** Keyboard modifiers callback: forward effective modifiers to seat. */
void comp_keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	(void)data;
	struct comp_keyboard *kbd = wl_container_of(listener, kbd, modifiers);
	if (!kbd->server || !kbd->server->seat || kbd->server->display_terminate_requested) {
		return;
	}
	struct wlr_keyboard *wlr_kbd = wlr_keyboard_from_input_device(kbd->dev);
	wlr_seat_set_keyboard(kbd->server->seat, wlr_kbd);
	wlr_seat_keyboard_notify_modifiers(kbd->server->seat, &wlr_kbd->modifiers);
	if ((wlr_keyboard_get_modifiers(wlr_kbd) & WLR_MODIFIER_LOGO) == 0) {
		kbd->server->suppress_logo_pointer_drag = false;
	}
}

/** Keep compositor shortcuts out of the held-key snapshot sent on focus changes. */
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
