# Manual Test Report for Morph XKB

Branch: `feature/xkb-full-support` \
As of: 2026-09-05 \
Implementation baseline: `f75d4ba` (phases 1 and 2)

This report is updated during implementation. Test each phase on the development system first. Because the development system runs X11, use a nested Morph session for most development checks. After all phases are complete, run distribution tests in native Morph sessions.

## Test order

1. Build Morph and run the automated tests on the development system.
2. Run the just-completed phase on the development system, preferably nested. Test runtime and dbg when the check concerns the build variant.
3. Repeat the relevant tests on the Debian Sid laptop in a native Morph session after each phase.
4. After the final phase, check relevant differences on Arch/CachyOS and Fedora in native Morph sessions.

Nested and native use the same configuration and physical key positions for the shared input test. Record the actual nested backend from the startup log; Xfce alone does not determine it. Record the commit, distribution, wlroots/libxkbcommon version, mode, backend, build variant, and result for every run.

Legend: `[ ]` open, `[x]` passed, `[!]` failed or blocked. Record missing hardware or unavailable keys in the notes; they do not count as passes.

## Preparation

Change to the Morph project directory before testing. Replace the placeholder
with the actual path to this checkout:

```bash
cd <path_to_morph_project_folder>
```

From the project directory, build both variants and run the automated tests:

```bash
bash scripts/morph-build.sh --both
meson test -C build --print-errorlogs
meson test -C build_dbg config keyboard --print-errorlogs
sh tests/test_shell_runtime.sh --dbg "$PWD"
```

`build/` is the runtime build and `build_dbg/` is the debug build. The dbg shell test must be started with `--dbg`. The automated tests cover configuration, keyboard dispatch, and both launchers; do not repeat these cases manually.

Create a temporary configuration for the manual tests:

```bash
export MORPH_TEST_ROOT=$(mktemp -d /tmp/morph-xkb-report.XXXXXX)
export TERMINAL=alacritty
cat > "$MORPH_TEST_ROOT/morph.conf" <<'CONFIG'
[keyboard]
rules = evdev
model = pc105
layout = de,us
variant = nodeadkeys,
options = grp:alt_shift_toggle

[bind]
mods = Super
key = Return
action = exec
command = alacritty

[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit

[bind]
mods = Ctrl+Super+Alt
key = 1
action = workspace_goto
command = 1

[bind]
mods = Ctrl+Super+Alt
key = 2
action = workspace_goto
command = 2

[bind]
mods = Alt
key = Tab
action = nextWindow

[bind]
mods = Alt+Shift
key = Tab
action = prevWindow
CONFIG
```

Start a nested session from a terminal in the running X11 session:

```bash
export MORPH_TEST_BIN="$PWD/build_dbg/morph"
export MORPH_TEST_VARIANT=dbg
env -u WLR_BACKENDS "$MORPH_TEST_BIN" --no-ipc --log-level info \
    --config "$MORPH_TEST_ROOT/morph.conf" \
    2>&1 | tee "$MORPH_TEST_ROOT/$MORPH_TEST_VARIANT-nested.log"
```

Start a native Morph session for distribution tests from a real console
not from the running X11 session. On Debian Sid, switch to a TTY
log in, and change to the project directory:

```bash
cd <path_to_morph_project_folder>
export MORPH_PROJECT_PATH="$PWD"
export MORPH_TEST_BIN="$MORPH_PROJECT_PATH/build/morph"
export MORPH_TEST_VARIANT=runtime
export MORPH_TEST_LOG="$MORPH_TEST_ROOT/runtime-native.log"

env -u DISPLAY -u WAYLAND_DISPLAY -u WLR_BACKENDS \
    MORPH_LOG_KEYS=1 "$MORPH_TEST_BIN" --log-level info \
    --config "$MORPH_TEST_ROOT/morph.conf" \
    2>&1 | tee "$MORPH_TEST_LOG"
```

`MORPH_TEST_ROOT` must point to the test directory created during
preparation. The long native command is wrapped by the
[`.bashrc` function `mxkb-native`](#distribution-test-helpers-for-bashrc);
on a test machine, after setting up the helpers:

```bash
cd <path_to_morph_project_folder>
mxkb-prepare
mxkb-native
```

Starting through the login manager is not required for this test. The
console shows which variables and native backend path are
actually used.

### Distribution test helpers for `.bashrc`

These functions are intended for native distribution tests. Copy them into the test machine `.bashrc`, run `source ~/.bashrc`, and start from the repository.

```bash
morph_xkb_prepare() {
    if [ -f "$PWD/meson.build" ]; then
        export MORPH_PROJECT_PATH="$PWD"
    else
        export MORPH_PROJECT_PATH="${MORPH_PROJECT_PATH:-$PWD}"
    fi
    if [ ! -f "$MORPH_PROJECT_PATH/meson.build" ]; then
        printf 'MORPH_PROJECT_PATH is not a Morph project: %s\n' "$MORPH_PROJECT_PATH" >&2
        return 1
    fi
    cd "$MORPH_PROJECT_PATH" || return

    export MORPH_TEST_ROOT="${MORPH_TEST_ROOT:-$(mktemp -d /tmp/morph-xkb-report.XXXXXX)}"
    export MORPH_TEST_BIN="$MORPH_PROJECT_PATH/build/morph"
    export MORPH_TEST_LOG="${MORPH_TEST_LOG:-$MORPH_TEST_ROOT/runtime-native.log}"
    export TERMINAL="${TERMINAL:-alacritty}"

    cat > "$MORPH_TEST_ROOT/morph.conf" <<'CONFIG'
[keyboard]
rules = evdev
model = pc105
layout = de,us
variant = nodeadkeys,
options = grp:alt_shift_toggle

[bind]
mods = Super
key = Return
action = exec
command = alacritty

[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit

[bind]
mods = Ctrl+Super+Alt
key = 1
action = workspace_goto
command = 1

[bind]
mods = Ctrl+Super+Alt
key = 2
action = workspace_goto
command = 2

[bind]
mods = Alt
key = Tab
action = nextWindow

[bind]
mods = Alt+Shift
key = Tab
action = prevWindow
CONFIG

    printf 'MORPH_PROJECT_PATH=%s\n' "$MORPH_PROJECT_PATH"
    printf 'MORPH_TEST_ROOT=%s\n' "$MORPH_TEST_ROOT"
    printf 'MORPH_TEST_BIN=%s\n' "$MORPH_TEST_BIN"
    printf 'MORPH_TEST_LOG=%s\n' "$MORPH_TEST_LOG"
}

morph_xkb_native() {
    morph_xkb_prepare || return

    env -u DISPLAY -u WAYLAND_DISPLAY -u WLR_BACKENDS MORPH_LOG_KEYS=1 \
        "$MORPH_TEST_BIN" --log-level info \
        --config "$MORPH_TEST_ROOT/morph.conf" \
        2>&1 | tee "$MORPH_TEST_LOG"

    return "${PIPESTATUS[0]}"
}

morph_xkb_debug_launcher() {
    morph_xkb_prepare || return

    export MORPH_LAUNCHER_LOG_DIR="$MORPH_TEST_ROOT/dbg-launcher-logs"
    mkdir -p "$MORPH_LAUNCHER_LOG_DIR" || return

    env -u MORPH_BIN \
        MORPH_CONFIG="$MORPH_TEST_ROOT/morph.conf" \
        MORPH_LOG_DIR="$MORPH_LAUNCHER_LOG_DIR" \
        MORPH_PROJECT_PATH="$MORPH_PROJECT_PATH" \
        "$MORPH_PROJECT_PATH/testing/morph-session_dbg"
    launcher_status=$?
    printf 'Debug launcher exit status: %s\n' "$launcher_status"
    printf 'Debug startup log: %s\n' "$MORPH_LAUNCHER_LOG_DIR/morph-nested-startup.log"
    return "$launcher_status"
}

morph_xkb_runtime_launcher() {
    morph_xkb_prepare || return

    export MORPH_LAUNCHER_LOG_DIR="$MORPH_TEST_ROOT/runtime-launcher-logs"
    mkdir -p "$MORPH_LAUNCHER_LOG_DIR" || return

    MORPH_BIN="$MORPH_PROJECT_PATH/build/morph" \
        MORPH_CONFIG="$MORPH_TEST_ROOT/morph.conf" \
        MORPH_LOG_DIR="$MORPH_LAUNCHER_LOG_DIR" \
        MORPH_PROJECT_PATH="$MORPH_PROJECT_PATH" \
        "$MORPH_PROJECT_PATH/scripts/morph-session"
    launcher_status=$?
    printf 'Runtime launcher exit status: %s\n' "$launcher_status"
    printf 'Runtime startup log: %s\n' "$MORPH_LAUNCHER_LOG_DIR/morph-nested-startup.log"
    return "$launcher_status"
}

morph_xkb_phase2_config_over_env() {
    morph_xkb_prepare || return

    XKB_DEFAULT_LAYOUT=us MORPH_LOG_KEYS=1 \
        "$MORPH_PROJECT_PATH/build_dbg/morph" --log-level info \
        --config "$MORPH_TEST_ROOT/morph.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/dbg-nested-config-over-env.log"
    return "${PIPESTATUS[0]}"
}

morph_xkb_phase2_empty_options() {
    morph_xkb_prepare || return

    cp "$MORPH_TEST_ROOT/morph.conf" "$MORPH_TEST_ROOT/morph-empty-options.conf" || return
    sed -i 's/^options =.*/options =/' "$MORPH_TEST_ROOT/morph-empty-options.conf" || return

    XKB_DEFAULT_OPTIONS=grp:alt_shift_toggle MORPH_LOG_KEYS=1 \
        "$MORPH_PROJECT_PATH/build_dbg/morph" --log-level info \
        --config "$MORPH_TEST_ROOT/morph-empty-options.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/dbg-nested-empty-options.log"
    return "${PIPESTATUS[0]}"
}

morph_xkb_phase2_two_keyboards_nested() {
    morph_xkb_prepare || return

    env -u WLR_BACKENDS MORPH_LOG_KEYS=1 \
        "$MORPH_PROJECT_PATH/build_dbg/morph" --no-ipc --log-level info \
        --config "$MORPH_TEST_ROOT/morph.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/dbg-nested-two-keyboards.log"
    return "${PIPESTATUS[0]}"
}

morph_xkb_phase2_two_keyboards_native() {
    morph_xkb_prepare || return

    env -u DISPLAY -u WAYLAND_DISPLAY -u WLR_BACKENDS MORPH_LOG_KEYS=1 \
        "$MORPH_PROJECT_PATH/build/morph" --log-level info \
        --config "$MORPH_TEST_ROOT/morph.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/runtime-native-two-keyboards.log"
    return "${PIPESTATUS[0]}"
}

morph_xkb_phase2_reload_session() {
    morph_xkb_prepare || return

    env -u WLR_BACKENDS MORPH_LOG_KEYS=1 \
        "$MORPH_PROJECT_PATH/build_dbg/morph" --log-level info \
        --config "$MORPH_TEST_ROOT/morph.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/dbg-nested-reload.log"
    return "${PIPESTATUS[0]}"
}

morph_xkb_debug_launcher_log() {
    if [ -z "${MORPH_TEST_ROOT:-}" ]; then
        printf 'Set MORPH_TEST_ROOT first with mxkb-prepare.\n' >&2
        return 1
    fi

    rg -n "Config file path:|Selected session mode:|Compositor binary:|Compositor exited normally|keyboard:|ERROR|CRASH|crash" \
        "$MORPH_TEST_ROOT/dbg-launcher-logs"
}

morph_xkb_runtime_launcher_log() {
    if [ -z "${MORPH_TEST_ROOT:-}" ]; then
        printf 'Set MORPH_TEST_ROOT first with mxkb-prepare.\n' >&2
        return 1
    fi

    rg -n "Config file path:|Selected session mode:|Compositor binary:|Compositor exited normally|keyboard:|ERROR|CRASH|crash" \
        "$MORPH_TEST_ROOT/runtime-launcher-logs"
}

morph_xkb_phase2_config_log() {
    if [ -z "${MORPH_TEST_ROOT:-}" ]; then
        printf 'Set MORPH_TEST_ROOT first with mxkb-prepare.\n' >&2
        return 1
    fi

    rg -n "keyboard:|XKB:|ERROR|Config file path|Compositor exited" \
        "$MORPH_TEST_ROOT/dbg-nested-config-over-env.log"
}

morph_xkb_phase2_invalid_log() {
    if [ -z "${MORPH_TEST_ROOT:-}" ]; then
        printf 'Set MORPH_TEST_ROOT first with mxkb-prepare.\n' >&2
        return 1
    fi

    rg -n "XKB:|invalid layout|invalid_morph_layout|Failed|ERROR" \
        "$MORPH_TEST_ROOT/runtime-invalid-layout.log"
}

morph_xkb_phase2_reload_log() {
    if [ -z "${MORPH_TEST_ROOT:-}" ]; then
        printf 'Set MORPH_TEST_ROOT first with mxkb-prepare.\n' >&2
        return 1
    fi

    rg -n "keyboard: invalid layout list or too many variants|reload: keyboard settings changed; restart Morph to apply them|ipc: config reload failed" \
        "$MORPH_TEST_ROOT/dbg-nested-reload.log"
}

morph_xkb_reload() {
    if [ -z "${MORPH_PROJECT_PATH:-}" ]; then
        export MORPH_PROJECT_PATH="$PWD"
    fi
    if [ -z "${MORPH_TEST_BIN:-}" ]; then
        export MORPH_TEST_BIN="$MORPH_PROJECT_PATH/build/morph"
    fi

    "$MORPH_TEST_BIN" --reload-config
}

morph_xkb_invalid_layout() {
    morph_xkb_prepare || return

    awk '
        /^layout[[:space:]]*=/ { print "layout = invalid_morph_layout"; next }
        { print }
    ' "$MORPH_TEST_ROOT/morph.conf" > "$MORPH_TEST_ROOT/runtime-invalid-layout.conf"

    env -u DISPLAY -u WAYLAND_DISPLAY -u WLR_BACKENDS \
        "$MORPH_TEST_BIN" --log-level info \
        --config "$MORPH_TEST_ROOT/runtime-invalid-layout.conf" \
        2>&1 | tee "$MORPH_TEST_ROOT/runtime-invalid-layout.log"

    return "${PIPESTATUS[0]}"
}

morph_xkb_logs() {
    morph_xkb_prepare || return

    rg -n "keyboard:|reload:|ipc:|XKB:|Failed|ERROR" "$MORPH_TEST_ROOT"
}

alias mxkb-prepare='morph_xkb_prepare'
alias mxkb-native='morph_xkb_native'
alias mxkb-dbg-launcher='morph_xkb_debug_launcher'
alias mxkb-runtime-launcher='morph_xkb_runtime_launcher'
alias mxkb-p2-config='morph_xkb_phase2_config_over_env'
alias mxkb-p2-empty-options='morph_xkb_phase2_empty_options'
alias mxkb-p2-two-nested='morph_xkb_phase2_two_keyboards_nested'
alias mxkb-p2-two-native='morph_xkb_phase2_two_keyboards_native'
alias mxkb-p2-reload-session='morph_xkb_phase2_reload_session'
alias mxkb-dbg-launcher-log='morph_xkb_debug_launcher_log'
alias mxkb-runtime-launcher-log='morph_xkb_runtime_launcher_log'
alias mxkb-p2-config-log='morph_xkb_phase2_config_log'
alias mxkb-p2-invalid-log='morph_xkb_phase2_invalid_log'
alias mxkb-p2-reload-log='morph_xkb_phase2_reload_log'
alias mxkb-reload='morph_xkb_reload'
alias mxkb-invalid='morph_xkb_invalid_layout'
alias mxkb-logs='morph_xkb_logs'
```

Typical distribution procedure:

```bash
cd <path_to_morph_project_folder>
mxkb-prepare
mxkb-native
mxkb-logs
```

For the reload test, run `mxkb-native` in terminal A and `mxkb-reload` in terminal B. For the invalid-layout test, run `mxkb-invalid`; errors appear in the terminal and are also saved.

## Phase 1: Input and modifiers

Test `build_dbg/morph` nested on the development system first. Then repeat the
same procedure with `build/morph`. If both variants pass, test the dbg launcher
separately.

### Phase 1.1: Basic startup

Instructions:

- [x] Start Morph with the temporary `de,us` configuration.
- [x] Open an editor inside the Morph session.
- [-] Record the commit, mode, backend, build variant, and log path in `## Results` below.

Expected:

- Morph starts without XKB errors.
- The editor accepts keyboard input.
- The log contains no crash markers or keymap installation errors.

### Phase 1.2: Layout switching with Alt+Shift

Instructions:

- [x] Type using the physical Y/Z key positions in the editor.
- [x] Press `Alt+Shift`.
- [x] Type using the same physical Y/Z positions again.
- [x] Press `Alt+Shift` six times in total and briefly check Y/Z after each switch.

Expected:

- Output alternates between the German and US layouts after every `Alt+Shift` press.
- The same physical keys produce the corresponding Y/Z output for the active layout.
- Switching remains stable; no key press is lost and no key repeats unexpectedly.

### Phase 1.3: Modifiers and locks

Instructions:

- [x] Hold `Shift`, type a letter, and release `Shift`.
- [x] Press `CapsLock` twice and type a letter after each press.
- [x] Test `AltGr` with a character available through the German layout.
- [-] Toggle `NumLock` and test the numpad if available.

Expected:

- `Shift` is active only while the key is held.
- `CapsLock` toggles exactly once per key press.
- `AltGr` produces the expected characters and is no longer active after release.
- `NumLock` behaves normally; record a missing numpad only in the notes.

### Phase 1.4: Key repeat

Instructions:

- [x] Hold a normal letter until repetition is visible.
- [x] Release the key.
- [x] Repeat the same check after a layout switch.

Expected:

- Repeat starts normally.
- Repeat stops immediately when the key is released.
- The same behavior applies to the active layout after a layout switch.

### Phase 1.5: Focus switching

Instructions:

- [x] Open two editor windows.
- [x] Type in window A.
- [x] Switch focus to window B and continue typing there.
- [x] Switch back to window A and continue typing.

Expected:

- Input always goes to the focused window.
- A window receives no key presses after it loses focus.
- Layout switching and modifiers remain unchanged after focus changes.

### Phase 1.6: Compositor shortcuts

Instructions:

- [x] Switch workspaces with `Ctrl+Super+Alt+1` and `Ctrl+Super+Alt+2`.
- [x] Type in an editor in the target workspace.
- [x] Check whether the shortcut digit appears in the editor.

Expected:

- Workspace switching works.
- The digit consumed by the compositor is not forwarded to the application.
- The digit does not remain stuck or repeat after the shortcut.

### Phase 1.7: Clean exit

Instructions:

- [x] Exit Morph with `Ctrl+Super+Alt+Escape`.
- [x] Use the parent desktop or TTY after Morph exits.
- [x] Record the exit status and notable log lines.

Expected:

- Morph exits cleanly.
- The desktop or TTY is immediately usable again.
- The log contains no crash markers.

### Phase 1.8: Debug launcher

Prerequisites:

- [x] The debug build exists at `$MORPH_PROJECT_PATH/build_dbg/morph`.
- [x] `mxkb-prepare` was run in a terminal in the Morph project directory.
- [x] No other Morph test instance is running with the same test configuration.

Instructions:

1. Load the functions from the preparation section in the test machine's `.bashrc` and run `source ~/.bashrc`.
2. Run `mxkb-prepare` in the Morph project directory. Check the printed values for `MORPH_PROJECT_PATH`, `MORPH_TEST_ROOT`, and `MORPH_TEST_BIN`.
3. Run `mxkb-dbg-launcher`. The command blocks until the nested Morph session exits.
4. In the nested session, open a terminal with `Super+Enter`, type `y` and `z`, switch groups with `Alt+Shift`, and type `y` and `z` again.
5. Exit Morph with `Ctrl+Super+Alt+Escape`.
6. Run `mxkb-dbg-launcher-log`. The alias checks the launcher log for the configuration, binary, session mode, keyboard messages, and errors.

Expected:

- [x] The launcher automatically uses `$MORPH_PROJECT_PATH/build_dbg/morph`; `MORPH_BIN` is not set for this test.
- [x] The temporary configuration and session mode are shown in the startup log.
- [x] Keyboard input, layout switching, and clean exit work.
- [x] The launcher exits successfully and the log contains no XKB, error, or crash markers.

### Phase 1.9: Runtime launcher

Prerequisites:

- [x] The runtime build exists at `$MORPH_PROJECT_PATH/build/morph`.
- [x] `mxkb-prepare` was run and the temporary configuration is at `$MORPH_TEST_ROOT/morph.conf`.
- [x] No other Morph test instance is running with the same test configuration.

Instructions:

1. If the `.bashrc` helpers are not loaded, run `source ~/.bashrc`.
2. Run `mxkb-prepare` in the Morph project directory and check the printed paths.
3. Run `mxkb-runtime-launcher`. The command blocks until the nested Morph session exits.
4. In the nested session, open a terminal with `Super+Enter`, type `y` and `z`, switch groups with `Alt+Shift`, and type `y` and `z` again.
5. Exit Morph with `Ctrl+Super+Alt+Escape`.
6. Run `mxkb-runtime-launcher-log`. The alias checks the launcher log for the configuration, binary, session mode, keyboard messages, and errors.

Expected:

- [x] The launcher uses the current runtime build at `$MORPH_PROJECT_PATH/build/morph`.
- [x] The temporary configuration and session mode are shown in the startup log.
- [x] Keyboard input, layout switching, and clean exit work.
- [x] There is no unknown `[keyboard]` section, XKB error, or crash marker.

The procedure must work the same in the runtime and dbg builds. Differences in
debug log output are expected. If the X11 desktop intercepts `Alt+Shift` or
`Super`, adjust the desktop binding for the test and record the change.

## Phase 2: Configuration and devices

Phase 2 tests the priority of configuration sources, multiple keyboards, and
the behavior of invalid changes or changes that cannot yet be applied live.
Run tests 2.1 and 2.2 nested on the development system first. Tests 2.3 through
2.5 require real keyboards and are repeated in a native session.

Important for the precedence check: `XKB_DEFAULT_LAYOUT=us` is an environment
variable, not an XKB CLI parameter. Morph currently has no option such as
`--xkb-layout`; `--config` only selects the configuration file. Phase 2.1
therefore tests `Config > Environment`. Where a real CLI parameter exists, the
priority remains `CLI > Config > Environment > Defaults`.

Preparation for all tests:

1. In the Morph project directory, run `source ~/.bashrc` and then `mxkb-prepare`.
2. Use the printed values for `MORPH_PROJECT_PATH`, `MORPH_TEST_ROOT`, and `MORPH_TEST_BIN`.
3. Use a separate log path under `$MORPH_TEST_ROOT` for each test.
4. Commands using `build_dbg/morph` are nested tests. For native tests, use `mxkb-native` or the runtime command specified in the section.

### Phase 2.1: Config overrides environment

Instructions:

1. In terminal A, run `mxkb-p2-config`. The alias sets `XKB_DEFAULT_LAYOUT=us`, starts the debug build, and saves the log to `$MORPH_TEST_ROOT/dbg-nested-config-over-env.log`.
2. In the nested session, open a terminal with `Super+Enter` and type `y` and `z`.
3. Press `Alt+Shift` and type `y` and `z` again.
4. In terminal A, run `mxkb-p2-config-log`.
5. Exit Morph with `Ctrl+Super+Alt+Escape`.

Expected:

- [x] The `[keyboard]` config with `layout = de,us` overrides the environment variable `XKB_DEFAULT_LAYOUT=us`.
- [x] The startup layout is `de`; `Alt+Shift` switches to `us`.
- [x] No XKB compile errors occur.

### Phase 2.2: Empty options clear environment options

Instructions:

1.  In terminal A, run `mxkb-p2-empty-options`. The alias creates a test copy:
   ```bash
   cp "$MORPH_TEST_ROOT/morph.conf" "$MORPH_TEST_ROOT/morph-empty-options.conf"
   ```
   Set `options =`
   ```bash
   sed -i 's/^options =.*/options =/' "$MORPH_TEST_ROOT/morph-empty-options.conf"
   ```
   Sets `XKB_DEFAULT_OPTIONS` and saves the log to `$MORPH_TEST_ROOT/dbg-nested-empty-options.log`.
2. In the terminal, type `y` and `z`, press `Alt+Shift`, and type `y` and `z` again.
3. Check the result with:
   ```bash
   rg -n "ISO_Next_Group|keyboard:|XKB:|ERROR" \
       "$MORPH_TEST_ROOT/dbg-nested-empty-options.log"
   ```
4. Exit Morph with `Ctrl+Super+Alt+Escape`.

Expected:

- [x] The empty config option clears `XKB_DEFAULT_OPTIONS`.
- [x] `Alt+Shift` does not switch groups.
- [x] No `ISO_Next_Group` appears for this key press.

### Phase 2.3: Two keyboards

Instructions:

1. Connect two keyboards. In nested mode, run this only if both devices are visible to the test system; otherwise test directly in native mode.
2. For the nested test, run `mxkb-p2-two-nested` in terminal A.
3. For the native test, run `mxkb-p2-two-native` in a TTY.
4. Type in one client with keyboard A and then with keyboard B.
5. Press `Alt+Shift` on both keyboards and check `y` and `z` on each.
6. Compare the logs after exiting:
   ```bash
   rg -n "keyboard: keycode=|keyboard: modifiers|XKB:|ERROR" \
       "$MORPH_TEST_ROOT/dbg-nested-two-keyboards.log" \
       "$MORPH_TEST_ROOT/runtime-native-two-keyboards.log"
   ```

Expected:

- [x] Both keyboards receive the same configured keymap.
- [x] Input from both devices reaches the focused client.
- [x] Group switching works on both keyboards.
- [x] Group state is per keyboard: when keyboard B switches to `us`, keyboard A remains on `de`. This is expected behavior.

### Phase 2.4: Remove one keyboard

Instructions:

1. Start a native session with two keyboards connected.
2. Type in the same client with both keyboards and check the input in the log.
3. Remove the last-used keyboard.
4. Continue typing with the remaining keyboard, press `Alt+Shift`, and type `y` and `z` again.
5. Exit Morph and check the session log for `ERROR`, `CRASH`, or abort messages.

Expected:

- [x] Morph remains stable after the removal.
- [x] The remaining keyboard continues to work.
- [x] Focus and layout state remain usable.

### Phase 2.5: Remove and reconnect the last keyboard

Instructions:

1. Start a native session with exactly one keyboard.
2. Type `y` and `z` in a client.
3. Remove the last keyboard, wait briefly, and reconnect it.
4. Type `y` and `z` again and check `Alt+Shift`.
5. Check the session log for keymap installation, `ERROR`, and `CRASH`.

Expected:

- [x] Morph continues without a crash.
- [x] After reconnection, the configured startup keymap and its startup group (`de`) are installed; a device-specific group state previously set to `us` is not restored.
- [x] Input works without restarting Morph.

### Phase 2.6: Invalid layout ID

Instructions:

1. Run `mxkb-invalid`. It creates a copy with `layout = invalid_morph_layout`, prints the errors directly, and saves them to `$MORPH_TEST_ROOT/runtime-invalid-layout.log`.
2. If the exit status is needed separately, run `set -o pipefail` first.
3. Run `mxkb-p2-invalid-log` afterwards.

Expected:

- [x] Morph does not start a usable session.
- [x] The error occurs before or during backend startup.
- [x] The log identifies the XKB/keymap error and invalid layout ID.

### Phase 2.7: Keyboard reload rejected during Phase 2

Instructions:

1. In terminal A, run `mxkb-p2-reload-session`. The alias starts Morph with IPC and writes the log to `$MORPH_TEST_ROOT/dbg-nested-reload.log`.
2. In terminal B, set the same variables without running `mxkb-prepare` again:
   ```bash
   export MORPH_PROJECT_PATH="<path-to-morph-project-directory>"
   export MORPH_TEST_ROOT="<same-path-as-terminal-A>"
   export MORPH_TEST_BIN="$MORPH_PROJECT_PATH/build_dbg/morph"
   ```
3. Case A: set `layout = invalid_morph_layout` in `$MORPH_TEST_ROOT/morph.conf` and run `mxkb-reload` in terminal B.
4. Check case A with `mxkb-p2-reload-log`.
5. Restore a valid configuration: `layout = de` and `variant = nodeadkeys`.
6. Case B: run `mxkb-reload` again in terminal B and then check `mxkb-p2-reload-log`.
7. Continue typing `y` and `z` in the running session and exit Morph with `Ctrl+Super+Alt+Escape`.

Expected:

- [x] Case A is rejected because the configuration is invalid.
- [x] Case B is rejected because Phase 2 cannot apply valid keyboard changes live yet.
- [x] In both cases, the previous keymap remains active and input continues to work.

The reload restriction applies only to Phase 2. Once Phase 3 implements live
reload, replace this final check with the new reload procedure.

## Later phases - not implemented yet

Prepare each of the following checks on the development system first and run it after the corresponding phase is implemented. Then run the relevant native distribution tests.

### Phase 3: Keymap reload and clients

Phase 3 can only be tested after live reload is implemented. With the current Phase 2 implementation, the following result is expected and is not a test failure:

```text
reload: keyboard settings changed; restart Morph to apply them
ipc: config reload failed
```

These messages indicate that the Phase 2 reload restriction is still active. Phase 3.1 is testable only when valid keyboard changes are accepted by reload.

### Phase 3 preparation: start and reload calls

This preparation applies to Phases 3.1 through 3.4.

- In terminals A and B, first change to the repository and set the shared test variables. `MORPH_TEST_ROOT` must be the same path created during preparation:

```bash
    cd <path_to_morph_project_folder>
    export MORPH_PROJECT_PATH="$PWD"
export MORPH_TEST_ROOT=/tmp/morph-xkb-report.5vOvpK
    export MORPH_TEST_BIN="$MORPH_PROJECT_PATH/build_dbg/morph"
export MORPH_PHASE3_LOG="$MORPH_TEST_ROOT/dbg-nested-phase3-reload.log"
```

`MORPH_TEST_ROOT` must point to the real test directory. Replace the example path with the path printed by `echo "$MORPH_TEST_ROOT"` after preparation.

- Terminal A then starts the Morph session with IPC and writes the log to both the console and a file:

```bash
env -u WLR_BACKENDS MORPH_LOG_KEYS=1 "$MORPH_TEST_BIN" --log-level info \
    --config "$MORPH_TEST_ROOT/morph.conf" \
    2>&1 | tee "$MORPH_PHASE3_LOG"
```

- Terminal B triggers the reload. The nested session `WAYLAND_DISPLAY` is not needed; the IPC client uses the socket in the current `XDG_RUNTIME_DIR`.

```bash
"$MORPH_TEST_BIN" --reload-config
```

- For a valid reload with reordered groups, change the active config as follows:

```ini
[keyboard]
layout = us,de
variant = ,nodeadkeys
options = grp:alt_shift_toggle
```

- For an invalid reload test, change only the layout line to an invalid value:

```ini
layout = invalid_morph_layout
```

- The important reload messages appear directly in terminal A. Optionally check the saved log afterwards:

```bash
rg -n "reload:|keyboard:|ipc: config reload" "$MORPH_PHASE3_LOG"
```

### Phase 3.1: Valid keymap reload with open clients

Instructions:

- [ ] Start Morph in terminal A with the [Phase 3 start call](#phase-3-preparation-start-and-reload-calls).
- [ ] Open two clients, for example Geany and foot.
- [ ] Type Y/Z in both clients and switch between `de` and `us` with `Alt+Shift`.
- [ ] Change the active config using the [valid Phase 3 reload change](#phase-3-preparation-start-and-reload-calls).
- [ ] Trigger the reload in terminal B with the [Phase 3 reload call](#phase-3-preparation-start-and-reload-calls).
- [ ] Check Y/Z and `Alt+Shift` in both existing clients and a newly started client.

Expected:

- Reload is accepted.
- Existing clients use the new keymap.
- Newly started clients use the same new keymap.
- `Alt+Shift` still switches between the configured groups.

### Phase 3.2: Invalid reload has no effect

Instructions:

- [ ] Use a running Phase 3 session with working input.
- [ ] Change the active config using the [invalid Phase 3 reload change](#phase-3-preparation-start-and-reload-calls).
- [ ] Trigger the reload in terminal B with the [Phase 3 reload call](#phase-3-preparation-start-and-reload-calls).
- [ ] Check the error messages in terminal A, then continue typing in the open clients.

Expected:

- The reload is rejected.
- The previous keymap remains active.
- Open clients remain usable.

### Phase 3.3: Reload with held modifiers

Instructions:

- [ ] Start Morph in terminal A with the [Phase 3 start call](#phase-3-preparation-start-and-reload-calls) and open an editor.
- [ ] Start a delayed reload in terminal B: `sleep 5; "$MORPH_TEST_BIN" --reload-config`.
- [ ] Immediately switch back to the Morph session and focus the editor.
- [ ] Hold `Shift` in the Morph session until the reload visibly appears in the terminal A log.
- [ ] Release `Shift` and continue typing in the editor.
- [ ] Repeat the same procedure with `Alt` and then with `Ctrl+Super+Alt`. Restart the delayed reload in terminal B each time.

Expected:

- The reload does not hang while a modifier is held.
- No modifier remains active after it is released.
- Input and compositor shortcuts continue to work.

### Phase 3.4: Reload with reordered groups and two keyboards

`Reordered groups` means that the same layouts remain available, but their order changes, for example from `de,us` to `us,de`. The variants must move with their corresponding positions: `variant = nodeadkeys,` becomes `variant = ,nodeadkeys`.

Instructions:

- [ ] Start Morph in terminal A with the [Phase 3 start call](#phase-3-preparation-start-and-reload-calls).
- [ ] Connect two keyboards.
- [ ] Test Y/Z with keyboards A and B.
- [ ] Change the active config using the [valid Phase 3 reload change](#phase-3-preparation-start-and-reload-calls).
- [ ] Trigger the reload in terminal B with the [Phase 3 reload call](#phase-3-preparation-start-and-reload-calls).
- [ ] Test Y/Z and `Alt+Shift` with both keyboards.

Expected:

- Both keyboards receive the new keymap.
- The active group remains valid or is deliberately set to a valid group.
- Y/Z follow the new group order on both keyboards.

### Phase 4: Layout-independent shortcuts

- [ ] Bind actions to the primary Y/Z keys and Latin letters.
- [ ] Test the same physical keys under `de,us` and `us,ru`.
- [ ] Test punctuation, Shift, AltGr, consumed modifiers, and compatibility mode for active keysyms.

### Phase 5: Runtime control and state

- [ ] Run Next, Previous, and a valid index, then check text input.
- [ ] Run an invalid index. The state must not change.
- [ ] Address two keyboards individually, remove one target, and verify that its old ID is rejected.
- [ ] Test switching with only one layout.

### Phase 6: IPC and CLI

- [ ] Check the initial state and exactly one ordered message for each actual change.
- [ ] Leave another IPC client connected. An editor must not block.
- [ ] Test fragmented requests, slow subscribers, reconnection, and ordering during reload and hotplug.

### Phase 7: Completion

- [ ] Retest later changes on Debian.
- [ ] Run the relevant tests natively on Arch/CachyOS and Fedora.
- [ ] Check other available backends, sanitizer results, and the RMLVO/user documentation.

## Distribution tests

After the successful development run, execute the relevant tests in this order:

1. Debian Sid: after each completed phase, run the corresponding tests in a native Morph session.
2. Arch/CachyOS: after the final phase, run the relevant input, XKB, device, and client tests natively.
3. Fedora: run the same selection natively as on Arch/CachyOS.

For each distribution, record the commit, build variant, mode, backend, hardware, wlroots/libxkbcommon version, and observations. The shared input test uses the same physical key positions on every system.

Native distribution tests use the helpers from the `Preparation` section.

## Results

| Date | System / native or nested | Build | Test section | Result and notes |
| --- | --- | --- | --- | --- |
| open | Development system / nested | dbg | [Phase 1.1 Basic startup](#phase-11-basic-startup) | open |
| 2026-09-10 | Development system / nested | dbg | [Phase 1.2 Layout switching](#phase-12-layout-switching-with-altshift) | [x] After the fix, `group=1` remains stable; Y/Z keysyms visibly switch between the German and US layouts. Log: `/tmp/morph-xkb-report.Y4qWcr/dbg-nested-group-fix.log` |
| open | Development system / nested | dbg | [Phase 1.3 Modifiers and locks](#phase-13-modifiers-and-locks) | open |
| open | Development system / nested | dbg | [Phase 1.4 Key repeat](#phase-14-key-repeat) | open |
| open | Development system / nested | dbg | [Phase 1.5 Focus switching](#phase-15-focus-switching) | open |
| open | Development system / nested | dbg | [Phase 1.6 Compositor shortcuts](#phase-16-compositor-shortcuts) | open |
| open | Development system / nested | dbg | [Phase 1.7 Clean exit](#phase-17-clean-exit) | open |
| open | Development system / nested | dbg | [Phase 1.8 Debug launcher](#phase-18-debug-launcher) | open |
| open | Development system / nested | runtime | [Phase 1.9 Runtime launcher](#phase-19-runtime-launcher) | open |
| open | Development system / nested | runtime | [Phase 1 Input and modifiers](#phase-1-input-and-modifiers) | open |
| open | Debian Sid / native | runtime | [Phase 1 Input and modifiers](#phase-1-input-and-modifiers) | open |
| open | Development system / nested | dbg | [Phase 2.1 Config overrides environment](#phase-21-config-overrides-environment) | open |
| open | Development system / nested | dbg | [Phase 2.2 Empty options](#phase-22-empty-options-clear-environment-options) | open |
| open | Development system / native | runtime | [Phase 2.3 Two keyboards](#phase-23-two-keyboards) | open |
| open | Development system / native | runtime | [Phase 2.4 Remove one keyboard](#phase-24-remove-one-keyboard) | open |
| open | Development system / native | runtime | [Phase 2.5 Reconnect the last keyboard](#phase-25-remove-and-reconnect-the-last-keyboard) | open |
| open | Development system / nested | dbg | [Phase 2.6 Invalid layout ID](#phase-26-invalid-layout-id) | open |
| open | Development system / nested | dbg | [Phase 2.7 Reject keyboard reload](#phase-27-keyboard-reload-rejected-during-phase-2) | open |
| open | Debian Sid / native | runtime | [Phase 2 Configuration and devices](#phase-2-configuration-and-devices) | open |
| open | Development system / nested | dbg | [Phase 3.1 Valid keymap reload](#phase-31-valid-keymap-reload-with-open-clients) | open |
| open | Development system / nested | dbg | [Phase 3.2 Invalid reload](#phase-32-invalid-reload-has-no-effect) | open |
| open | Development system / nested | dbg | [Phase 3.3 Reload with held modifiers](#phase-33-reload-with-held-modifiers) | open |
| open | Development system / native | runtime | [Phase 3.4 Reordered groups and two keyboards](#phase-34-reload-with-reordered-groups-and-two-keyboards) | open |
| open | Debian Sid / native | runtime | [Phase 3 through the current phase](#phase-3-keymap-reload-and-clients) | open |
| open | Arch/CachyOS / native | runtime | [Relevant final tests](#distribution-tests) | open |
| open | Fedora / native | runtime | [Relevant final tests](#distribution-tests) | open |

## Completion criteria

- [ ] All tests up to and including the current phase pass on the development system.
- [ ] Debian tests were run after each completed phase.
- [ ] The relevant Arch/CachyOS and Fedora tests were run natively after the final phase.
- [ ] Runtime and dbg were tested for checks involving the launcher or build variant.
- [ ] Every failure, unavailable hardware component, and untested backend is documented with the commit and log path.

References: [Roadmap](Roadmap.de.md), [automated tests](docs/TESTS.md), [configuration](docs/CONFIG.md), [environment](docs/ENVIRONMENT.md).
