#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>

void __real_wl_display_run(struct wl_display *display);
struct wlr_backend *__real_wlr_backend_autocreate(struct wl_event_loop *loop,
	struct wlr_session **session_ptr);

static struct wlr_backend *test_backend;

/** Retain the real backend only for the optional runtime hotplug regression. */
struct wlr_backend *__wrap_wlr_backend_autocreate(struct wl_event_loop *loop,
		struct wlr_session **session_ptr) {
	test_backend = __real_wlr_backend_autocreate(loop, session_ptr);
	return test_backend;
}

/**
 * Skip only socket binding: this fixture has no clients and must also run in
 * sandboxes that forbid Unix sockets. The compositor still creates real wlroots
 * globals, a pixman renderer, a headless backend, and two outputs.
 */
const char *__wrap_wl_display_add_socket_auto(struct wl_display *display) {
	(void)display;
	return "morph-output-shutdown-test";
}

/** Add and remove an output while protocol globals and the compositor are active. */
static void hotplug_output(struct wlr_backend *backend, void *data) {
	bool *tested = data;
	if (!wlr_backend_is_headless(backend)) {
		return;
	}
	struct wlr_output *output = wlr_headless_add_output(backend, 800, 600);
	assert(output);
	wlr_output_destroy(output);
	*tested = true;
}

/** Leave the event loop normally so main() performs the full compositor teardown. */
static int stop_display(void *data) {
	if (getenv("MORPH_TEST_OUTPUT_HOTPLUG")) {
		bool tested = false;
		wlr_multi_for_each_backend(test_backend, hotplug_output, &tested);
		assert(tested);
	}
	wl_display_terminate(data);
	return 0;
}

/** End a short session without signals, physical devices, or test-only runtime flags. */
void __wrap_wl_display_run(struct wl_display *display) {
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	struct wl_event_source *timer = wl_event_loop_add_timer(loop, stop_display, display);
	assert(timer);
	assert(wl_event_source_timer_update(timer, 1) == 0);
	__real_wl_display_run(display);
	wl_event_source_remove(timer);
}
