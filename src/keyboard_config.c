#include "keyboard_config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>

/* All tables follow comp_xkb_field order so parsing and environment lookup agree. */
static const char *field_names[] = {"rules", "model", "layout", "variant", "options"};
static const char *environment_names[] = {
	"XKB_DEFAULT_RULES", "XKB_DEFAULT_MODEL", "XKB_DEFAULT_LAYOUT",
	"XKB_DEFAULT_VARIANT", "XKB_DEFAULT_OPTIONS",
};
/* NULL keeps system rules; empty strings explicitly disable variants and options. */
static const char *defaults[] = {NULL, "pc105", "us", "", ""};

/**
 * Release a complete or partially initialized keyboard configuration.
 *
 * Attached wlroots keyboards retain their own keymap references. Clearing this
 * object releases only its ownership and makes repeated cleanup safe.
 */
void comp_keyboard_config_finish(struct comp_keyboard_config *config) {
	for (size_t i = 0; i < COMP_XKB_FIELD_COUNT; i++) {
		free(config->values[i]);
	}
	xkb_keymap_unref(config->keymap);
	memset(config, 0, sizeof(*config));
}

/**
 * Replace one named RMLVO value with an owned copy of the parser input.
 *
 * Unknown fields and empty rules/model/layout entries fail without changing the
 * previous value. Empty variants/options clear inherited settings. The caller
 * must compile after editing values; this function does not replace the keymap.
 */
bool comp_keyboard_config_set(struct comp_keyboard_config *config, const char *key, const char *value) {
	for (size_t i = 0; i < COMP_XKB_FIELD_COUNT; i++) {
		if (strcasecmp(key, field_names[i]) != 0) {
			continue;
		}
		if (!value[0] && i <= COMP_XKB_LAYOUT) {
			wlr_log(WLR_ERROR, "keyboard: %s must not be empty", key);
			return false;
		}
		char *copy = strdup(value);
		if (!copy) {
			wlr_log(WLR_ERROR, "keyboard: cannot allocate %s", key);
			return false;
		}
		free(config->values[i]);
		config->values[i] = copy;
		return true;
	}
	wlr_log(WLR_ERROR, "keyboard: unknown key '%s'", key);
	return false;
}

/**
 * Initialize a zeroed configuration from the current process environment.
 *
 * The launcher has already resolved caller/user/system precedence. Copy these
 * values before parsing explicit [keyboard] overrides. Empty rules/model/layout
 * values use defaults, while empty variants/options remain intentional overrides.
 * Allocation failure releases all values captured by this call.
 */
bool comp_keyboard_config_init(struct comp_keyboard_config *config) {
	for (size_t i = 0; i < COMP_XKB_FIELD_COUNT; i++) {
		const char *value = getenv(environment_names[i]);
		if (!value || (!value[0] && i <= COMP_XKB_LAYOUT)) {
			value = defaults[i];
		}
		if (value && !comp_keyboard_config_set(config, field_names[i], value)) {
			comp_keyboard_config_finish(config);
			return false;
		}
	}
	return true;
}

/** XKB may return a recovered keymap after errors; reject that configuration. */
static void keymap_log(struct xkb_context *context, enum xkb_log_level level,
		const char *format, va_list args) {
	bool *failed = xkb_context_get_user_data(context);
	if (level <= XKB_LOG_LEVEL_ERROR) {
		*failed = true;
	}
	char message[1024];
	vsnprintf(message, sizeof(message), format, args);
	wlr_log(level <= XKB_LOG_LEVEL_ERROR ? WLR_ERROR : WLR_INFO, "XKB: %s", message);
}

/** Count comma-separated entries, including empty variant placeholders. */
static size_t entry_count(const char *value) {
	size_t count = 1;
	for (; *value; value++) {
		count += *value == ',';
	}
	return count;
}

/**
 * Validate layout/variant pairing and compile the resolved RMLVO settings.
 *
 * XKB errors or a reduced layout count reject the candidate even when the
 * library returns a usable fallback. Success replaces this object's keymap
 * reference; failure preserves its previous keymap. No device state is changed.
 */
bool comp_keyboard_config_compile(struct comp_keyboard_config *config) {
	const char *layout = config->values[COMP_XKB_LAYOUT];
	const char *variant = config->values[COMP_XKB_VARIANT];
	if (!layout || !layout[0] || layout[0] == ',' || layout[strlen(layout) - 1] == ',' ||
		strstr(layout, ",,") || (variant && entry_count(variant) > entry_count(layout))) {
		wlr_log(WLR_ERROR, "keyboard: invalid layout list or too many variants");
		return false;
	}
	struct xkb_rule_names names = {
		.rules = config->values[COMP_XKB_RULES],
		.model = config->values[COMP_XKB_MODEL],
		.layout = layout,
		.variant = variant,
		.options = config->values[COMP_XKB_OPTIONS],
	};
	/* Values are already resolved; XKB must not reapply environment defaults. */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
	if (!context) {
		wlr_log(WLR_ERROR, "keyboard: cannot create XKB context");
		return false;
	}
	bool failed = false;
	xkb_context_set_user_data(context, &failed);
	xkb_context_set_log_fn(context, keymap_log);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	/* Keymaps retain their context; do not retain a pointer to this stack frame. */
	xkb_context_set_user_data(context, NULL);
	xkb_context_set_log_fn(context, NULL);
	xkb_context_unref(context);
	if (!keymap || failed || xkb_keymap_num_layouts(keymap) != entry_count(layout)) {
		xkb_keymap_unref(keymap);
		wlr_log(WLR_ERROR, "keyboard: cannot compile requested keymap (layout='%s', variant='%s')",
			layout, variant ? variant : "");
		return false;
	}
	xkb_keymap_unref(config->keymap);
	config->keymap = keymap;
	return true;
}

/**
 * Compare resolved settings rather than independently compiled keymap pointers.
 *
 * The reload guard uses exact field equality. NULL (system rules) and an
 * explicit rules name stay distinct even if they currently produce the same map.
 */
bool comp_keyboard_config_equal(const struct comp_keyboard_config *a, const struct comp_keyboard_config *b) {
	for (size_t i = 0; i < COMP_XKB_FIELD_COUNT; i++) {
		if (!a->values[i] || !b->values[i]) {
			if (a->values[i] != b->values[i]) {
				return false;
			}
		} else if (strcmp(a->values[i], b->values[i]) != 0) {
			return false;
		}
	}
	return true;
}
