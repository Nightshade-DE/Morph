# Automated Tests

This document covers the automated tests under `tests/`.

Use [`docs/TESTING.md`](TESTING.md) for the broader local build, smoke-test,
and manual validation workflow.

## Scope

The automated test layer currently includes:

- `tests/test_config.c`
  - config parsing and validation behavior
  - keyboard environment/config precedence, variants, invalid keymaps, and line limits
- `tests/test_keyboard.c`
  - raw key updates and backend-supplied modifier masks through real wlroots dispatch
  - pre-event binding state and key/modifier ordering at the seat grab
  - CapsLock, AltGr, group cycling, consumed shortcuts, repeats, and focus snapshots
  - keyboard add/remove, active-device selection, cleanup, and failed keymap installation
- `tests/test_shell_runtime.sh`
  - launcher/runtime contracts
  - managed hook flow
  - config and environment priority checks
  - install/uninstall manifest checks
  - all five XKB variables in both launchers, including explicit empty caller values

## Run

Standard run:

```bash
meson test -C build --print-errorlogs
```

This runs the shell-runtime launcher checks in release mode through
`scripts/morph-session` and points the wrapper at `build/morph`.

Verbose run while debugging:

```bash
meson test -C build --print-errorlogs -v
```

Single test example:

```bash
meson test -C build shell-runtime -v
```

Explicit debug-wrapper run:

```bash
meson setup build_dbg --buildtype=debug
meson compile -C build_dbg
sh tests/test_shell_runtime.sh --dbg "$PWD"
```

Use `--dbg` only when validating `testing/morph-session_dbg` and
`build_dbg/morph`. The standard Meson test should stay release-compatible.

## Output

If a test fails, inspect:

- `build/meson-logs/testlog.txt`
- the failing test command printed by Meson

For shell-runtime failures, also inspect runtime logs under:

- `${XDG_STATE_HOME:-$HOME/.local/state}/morph`

## Keyboard Phase 1 Validation

Run the automated regression test with `meson test -C build keyboard -v`.
It compares the runtime keyboard dispatch against an independent XKB state.
Binding actions are stubbed; a seat grab records delivery without a display server.
Both `update_state=true` and `update_state=false` paths are covered. This does not
replace testing actual backend input or observing client protocol traffic.

Manual acceptance is tracked in [report_current.md](../report_current.md):
test this phase first on the development system, preferably nested, with runtime
and dbg where relevant. Repeat the phase 1 checks on Debian Sid in a native Morph
session, then run the relevant native checks on Arch/Fedora after implementation.
Keep results in `report_current.md`.

Phase 1 implementation check (2026-09-05, wlroots 0.19.3):

- `meson compile -C build`: passed.
- `meson test -C build --print-errorlogs`: config, keyboard, shell-runtime passed.
- Keyboard test with `-Db_sanitize=address,undefined`: passed outside the sandbox;
  LeakSanitizer cannot run under the sandbox's ptrace environment.
- Actual native/nested session checks: not yet performed.

## Keyboard Phase 2 Validation

Automated checks (2026-09-05, wlroots 0.19.3):

- `meson compile -C build` and all three Meson suites pass.
- Config and keyboard tests pass with `-Db_sanitize=address,undefined`.
- The keyboard test uses a linker wrapper to inject a keymap installation failure;
  it verifies that the active keyboard survives and no rejected-device listeners remain.
- Launcher tests run a stub executable to inspect the actual exported environment
  for both wrappers. They do not start a graphical session or managed services.

Manual acceptance is tracked in [report_current.md](../report_current.md):
test configuration, hotplug, and rejection paths first on the development system,
preferably nested. Repeat the relevant phase 2 checks on Debian in a native Morph
session and on Arch/Fedora in the final native round. Update the reload check when
phase 3 replaces rejection with live keymap updates.

## Related Documentation

- [`docs/TESTING.md`](TESTING.md)
- [`testing/testplan-manual-morph.nfo`](../testing/testplan-manual-morph.nfo)
