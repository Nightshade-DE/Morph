#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>

struct wlr_seat;
struct wlr_surface;
struct comp_server;
struct wlr_input_device;

/** Install the configured keymap before publishing a keyboard to the seat. */
bool comp_keyboard_add(struct comp_server *server, struct wlr_input_device *device);
/** Detach keyboard listeners before seat and backend teardown. */
void comp_keyboard_clear(struct comp_server *server);

/** Dispatch backend key events without updating the wlroots-owned XKB state. */
void comp_keyboard_handle_key(struct wl_listener *listener, void *data);
/** Forward the updated keyboard modifiers and group to the seat. */
void comp_keyboard_handle_modifiers(struct wl_listener *listener, void *data);
/** Transfer focus without exposing held compositor shortcuts to the client. */
void comp_keyboard_notify_enter(struct wlr_seat *seat, struct wlr_surface *surface);
