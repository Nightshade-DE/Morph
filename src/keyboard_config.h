#pragma once

#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

/** Shared index order for rules, model, layout, variant, and options (RMLVO). */
enum comp_xkb_field {
	COMP_XKB_RULES,
	COMP_XKB_MODEL,
	COMP_XKB_LAYOUT,
	COMP_XKB_VARIANT,
	COMP_XKB_OPTIONS,
	COMP_XKB_FIELD_COUNT,
};

/**
 * Resolved keyboard settings owned by one comp_config instance.
 *
 * Initialize from zero, apply explicit fields, then compile before attaching
 * devices. Editing a value does not update the keymap until compilation succeeds.
 */
struct comp_keyboard_config {
	/** Owned strings; rules may be NULL to select the libxkbcommon system default. */
	char *values[COMP_XKB_FIELD_COUNT];
	/** Owned reference; each attached wlroots keyboard takes its own reference. */
	struct xkb_keymap *keymap;
};

/** Capture environment values for this config load; requires a zeroed object. */
bool comp_keyboard_config_init(struct comp_keyboard_config *config);
/** Set one RMLVO field, retaining explicit empty options and variants. */
bool comp_keyboard_config_set(struct comp_keyboard_config *config, const char *key, const char *value);
/** Validate resolved values and compile a keymap without implicit error recovery. */
bool comp_keyboard_config_compile(struct comp_keyboard_config *config);
/** Compare resolved settings before accepting a config-only reload. */
bool comp_keyboard_config_equal(const struct comp_keyboard_config *a, const struct comp_keyboard_config *b);
/** Release owned settings and the shared keymap reference. */
void comp_keyboard_config_finish(struct comp_keyboard_config *config);
