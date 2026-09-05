#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wlr/util/box.h>

#include "config.h"
#include "server.h"

/* Stubs for server actions referenced by comp_config_try_bindings in config.c. */
void server_set_layout(struct comp_server *server, enum comp_layout layout)
{
    (void)server;
    (void)layout;
}

void server_toggle_layout(struct comp_server *server)
{
    (void)server;
}

void server_arrange_toplevels(struct comp_server *server)
{
    (void)server;
}

void server_workspace_apply_visibility(struct comp_server *server)
{
    (void)server;
}

void server_workspace_go(struct comp_server *server, int idx)
{
    (void)server;
    (void)idx;
}

void server_workspace_relative(struct comp_server *server, int delta)
{
    (void)server;
    (void)delta;
}

void server_workspace_move_focused(struct comp_server *server, int target)
{
    (void)server;
    (void)target;
}

void server_tile_move_focused_n(struct comp_server *server, int steps)
{
    (void)server;
    (void)steps;
}

void server_tile_move_focused_edge(struct comp_server *server, bool to_first)
{
    (void)server;
    (void)to_first;
}

void server_scroll_move(struct comp_server *server, int steps)
{
    (void)server;
    (void)steps;
}

void server_tile_move_focused_grid_vert(struct comp_server *server, int steps)
{
    (void)server;
    (void)steps;
}

void server_tile_move_focused_grid_vert_edge(struct comp_server *server, bool to_top)
{
    (void)server;
    (void)to_top;
}

void server_tile_move_focused_grid_horiz(struct comp_server *server, int steps)
{
    (void)server;
    (void)steps;
}

void server_tile_grid_run_command(struct comp_server *server, const char *cmd)
{
    (void)server;
    (void)cmd;
}

void server_sync_xdg_decorations(struct comp_server *server)
{
    (void)server;
}

bool server_init(struct comp_server *server)
{
    (void)server;
    return true;
}

/**
 * Write `content` to a unique temporary file and return its path.
 *
 * Caller owns cleanup of the resulting file path (unlink when done).
 */
static bool write_temp_file(const char *content, char *out_path, size_t out_len)
{
    char tmpl[] = "/tmp/morph-config-test-XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0)
    {
        return false;
    }
    size_t len = strlen(content);
    ssize_t w = write(fd, content, len);
    close(fd);
    if (w != (ssize_t)len)
    {
        unlink(tmpl);
        return false;
    }
    if (snprintf(out_path, out_len, "%s", tmpl) >= (int)out_len)
    {
        unlink(tmpl);
        return false;
    }
    return true;
}

/**
 * Happy-path config parsing test.
 *
 * Verifies explicit bind/rule parsing and selected default behavior fallbacks.
 */
static int test_valid_config_parse(void)
{
    const char *cfg_text =
        "[bind]\n"
        "mods = Super\n"
        "key = Return\n"
        "action = exec\n"
        "command = foot\n"
        "\n"
        "[tile_rule]\n"
        "app_id = ^mpv$\n"
        "mode = float\n"
        "order = -5\n"
        "\n"
        "[layout_anim]\n"
        "enabled = no\n"
        "lambda = 20\n"
        "epsilon = 0.5\n"
        "\n"
        "[decoration_rule]\n"
        "app_id = ^foot$\n"
        "strip = no\n";

    char path[128];
    if (!write_temp_file(cfg_text, path, sizeof(path)))
    {
        fprintf(stderr, "failed to create temp config\n");
        return 1;
    }

    /* Parse from disk to exercise the same loader path used in production. */
    struct comp_config *cfg = NULL;
    if (!comp_config_load(path, &cfg))
    {
        fprintf(stderr, "valid config parse failed\n");
        unlink(path);
        return 1;
    }

    if (!cfg || cfg->n_binds != 1)
    {
        fprintf(stderr, "expected exactly one bind from test config\n");
        comp_config_free(cfg);
        unlink(path);
        return 1;
    }

    if (cfg->layout_anim_enabled || cfg->layout_anim_lambda != 20.0 || cfg->layout_anim_epsilon != 0.5)
    {
        fprintf(stderr, "layout_anim settings mismatch\n");
        comp_config_free(cfg);
        unlink(path);
        return 1;
    }

    /* Verify that one matching tile_rule mutates float/order as expected. */
    bool float_mode = false;
    int order = 0;
    comp_config_tile_props_for_toplevel(cfg, "mpv", "video", &float_mode, &order);
    if (!float_mode || order != -5)
    {
        fprintf(stderr, "tile_rule resolution mismatch\n");
        comp_config_free(cfg);
        unlink(path);
        return 1;
    }

    /* strip=no means keep client-side decorations for matching windows. */
    if (comp_config_decoration_prefer_server_side_tile_scroll(cfg, "foot", "terminal"))
    {
        fprintf(stderr, "decoration_rule strip=no should keep client-side mode\n");
        comp_config_free(cfg);
        unlink(path);
        return 1;
    }

    if (!comp_config_decoration_prefer_server_side_tile_scroll(cfg, "other", "window"))
    {
        fprintf(stderr, "default decoration behavior should prefer server-side in tile/scroll\n");
        comp_config_free(cfg);
        unlink(path);
        return 1;
    }

    comp_config_free(cfg);
    unlink(path);
    return 0;
}

/**
 * Negative parsing test for tile_grid_move validation.
 *
 * `left 0` is invalid because directional counts must be >= 1.
 */
static int test_invalid_tile_grid_command(void)
{
    fprintf(stderr, "NOTE: the following config parser ERROR messages are expected; this test intentionally feeds invalid config.\n");

    const char *cfg_text =
        "[bind]\n"
        "mods = Super\n"
        "key = T\n"
        "action = tile_grid_move\n"
        "command = left 0\n";

    char path[128];
    if (!write_temp_file(cfg_text, path, sizeof(path)))
    {
        fprintf(stderr, "failed to create temp invalid config\n");
        return 1;
    }

    struct comp_config *cfg = NULL;
    bool ok = comp_config_load(path, &cfg);
    unlink(path);
    if (ok)
    {
        fprintf(stderr, "invalid config unexpectedly parsed\n");
        comp_config_free(cfg);
        return 1;
    }
    return 0;
}

/**
 * Missing config file should hard-fail when an explicit path was requested.
 */
static int test_missing_config_fails(void)
{
    fprintf(stderr, "NOTE: the following missing-config ERROR message is expected; this test verifies hard-fail behavior.\n");

    struct comp_config *cfg = NULL;
    unsetenv("MORPH_ALLOW_BUILTIN_FALLBACK");
    bool ok = comp_config_load("/tmp/morph-config-this-file-does-not-exist", &cfg);
    if (ok)
    {
        fprintf(stderr, "missing config should fail instead of falling back silently\n");
        comp_config_free(cfg);
        return 1;
    }
    return 0;
}

/**
 * Missing config file may fall back to builtin defaults when explicitly enabled.
 */
static int test_missing_config_can_use_builtin_fallback(void)
{
    fprintf(stderr, "NOTE: the following missing-config ERROR message is expected before builtin fallback is accepted.\n");

    struct comp_config *cfg = NULL;
    setenv("MORPH_ALLOW_BUILTIN_FALLBACK", "1", 1);
    bool ok = comp_config_load("/tmp/morph-config-this-file-does-not-exist", &cfg);
    unsetenv("MORPH_ALLOW_BUILTIN_FALLBACK");
    if (!ok || !cfg)
    {
        fprintf(stderr, "missing config should use builtin fallback when enabled\n");
        return 1;
    }
    if (cfg->n_binds == 0)
    {
        fprintf(stderr, "builtin fallback should provide default binds\n");
        comp_config_free(cfg);
        return 1;
    }
    comp_config_free(cfg);
    return 0;
}

/** Missing default config path must use the same explicit fallback as a missing file path. */
static int test_unresolved_config_path_can_use_builtin_fallback(void)
{
    struct comp_config *cfg = NULL;
    setenv("MORPH_ALLOW_BUILTIN_FALLBACK", "1", 1);
    bool ok = comp_config_load(NULL, &cfg);
    unsetenv("MORPH_ALLOW_BUILTIN_FALLBACK");
    if (!ok || !cfg || cfg->n_binds != 4 ||
        cfg->binds[0].mods != WLR_MODIFIER_LOGO || cfg->binds[0].keysym != XKB_KEY_Return ||
        cfg->binds[0].action != COMP_KEYBIND_EXEC || strcmp(cfg->binds[0].command, "${TERMINAL:-foot}") != 0 ||
        cfg->binds[1].mods != (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT) || cfg->binds[1].keysym != XKB_KEY_Q ||
        cfg->binds[1].action != COMP_KEYBIND_CLOSE || cfg->binds[2].mods != WLR_MODIFIER_LOGO ||
        cfg->binds[2].keysym != XKB_KEY_Escape || cfg->binds[2].action != COMP_KEYBIND_QUIT ||
        cfg->binds[3].mods != WLR_MODIFIER_LOGO || cfg->binds[3].keysym != XKB_KEY_t ||
        cfg->binds[3].action != COMP_KEYBIND_LAYOUT_TOGGLE)
    {
        fprintf(stderr, "unresolved config path should use all builtin default binds when enabled\n");
        comp_config_free(cfg);
        return 1;
    }
    comp_config_free(cfg);
    return 0;
}

/** Execute all config parser regression tests; return non-zero on first failure. */
/** Load a config fixture and check that failure never publishes partial state. */
static struct comp_config *load_fixture(const char *text, bool expected) {
	char path[128];
	assert(write_temp_file(text, path, sizeof(path)));
	struct comp_config *config = NULL;
	assert(comp_config_load(path, &config) == expected);
	assert(unlink(path) == 0);
	assert(expected || config == NULL);
	return config;
}

/** Cover RMLVO precedence, explicit empty values, and invalid keymap rejection. */
static void test_keyboard_config(void) {
	const char *vars[] = {"XKB_DEFAULT_RULES", "XKB_DEFAULT_MODEL", "XKB_DEFAULT_LAYOUT",
		"XKB_DEFAULT_VARIANT", "XKB_DEFAULT_OPTIONS"};
	for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
		assert(unsetenv(vars[i]) == 0);
	}
	struct comp_config *config = load_fixture("[keyboard]\n", true);
	assert(strcmp(config->keyboard.values[COMP_XKB_LAYOUT], "us") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_MODEL], "pc105") == 0);
	assert(xkb_keymap_num_layouts(config->keyboard.keymap) == 1);
	comp_config_free(config);
	assert(setenv("XKB_DEFAULT_RULES", "evdev", 1) == 0);
	assert(setenv("XKB_DEFAULT_MODEL", "pc104", 1) == 0);
	assert(setenv("XKB_DEFAULT_LAYOUT", "de,us", 1) == 0);
	assert(setenv("XKB_DEFAULT_VARIANT", "nodeadkeys,", 1) == 0);
	assert(setenv("XKB_DEFAULT_OPTIONS", "grp:alt_shift_toggle", 1) == 0);
	config = load_fixture("[keyboard]\n", true);
	assert(strcmp(config->keyboard.values[COMP_XKB_MODEL], "pc104") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_RULES], "evdev") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_VARIANT], "nodeadkeys,") == 0);
	assert(xkb_keymap_num_layouts(config->keyboard.keymap) == 2);
	comp_config_free(config);
	config = load_fixture("  [keyboard]  \n model = pc105\n layout = us,de\n variant = ,nodeadkeys\n options = \n", true);
	assert(strcmp(config->keyboard.values[COMP_XKB_MODEL], "pc105") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_LAYOUT], "us,de") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_VARIANT], ",nodeadkeys") == 0);
	assert(strcmp(config->keyboard.values[COMP_XKB_OPTIONS], "") == 0);
	comp_config_free(config);
	for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
		assert(unsetenv(vars[i]) == 0);
	}
	const char *invalid[] = {
		"[keyboard]\nunknown = us\n", "[keyboard]\nlayout = \n",
		"[keyboard]\nmodel = \n", "[keyboard]\nrules = \n",
		"[keyboard]\nrules = morph_nonexistent_rules\n",
		"[keyboard]\nlayout = morph_nonexistent_layout\n",
		"[keyboard]\nlayout = de,morph_nonexistent_layout\n",
		"[keyboard]\nvariant = morph_nonexistent_variant\n",
		"[keyboard]\nlayout = de,,us\n",
		"[keyboard]\nlayout = us\nvariant = ,nodeadkeys\n",
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		load_fixture(invalid[i], false);
	}
}

/** Exercise the trim boundary and reject oversized physical lines before parsing. */
static void test_line_boundaries(void) {
	char line[4097];
	memset(line, ' ', sizeof(line));
	/* A valid section at the end of a maximum-length line exercises leading trim. */
	memcpy(line + 4084, "[keyboard]", 10);
	line[4094] = '\n';
	line[4095] = '\0';
	comp_config_free(load_fixture(line, true));
	line[4094] = '\0';
	comp_config_free(load_fixture(line, true));
	memset(line, ' ', sizeof(line));
	line[4095] = '\n';
	line[4096] = '\0';
	load_fixture(line, false);
	line[4095] = '\0';
	load_fixture(line, false);
}

int main(void) {
	test_keyboard_config();
	test_line_boundaries();
    if (test_valid_config_parse() != 0)
    {
        return 1;
    }
    if (test_invalid_tile_grid_command() != 0)
    {
        return 1;
    }
    if (test_missing_config_fails() != 0)
    {
        return 1;
    }
    if (test_missing_config_can_use_builtin_fallback() != 0)
    {
        return 1;
    }
    if (test_unresolved_config_path_can_use_builtin_fallback() != 0)
    {
        return 1;
    }
    return 0;
}
