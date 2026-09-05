# Automated Tests

This document covers the automated tests under `tests/`.

Use [`docs/TESTING.md`](TESTING.md) for the broader local build, smoke-test,
and manual validation workflow.

## Scope

The automated test layer currently includes:

- `tests/test_config.c`
  - config parsing and validation behavior
- `tests/test_keyboard.c`
  - raw key updates and backend-supplied modifier masks through real wlroots dispatch
  - pre-event binding state and key/modifier ordering at the seat grab
  - CapsLock, AltGr, group cycling, consumed shortcuts, repeats, and focus snapshots
- `tests/test_shell_runtime.sh`
  - launcher/runtime contracts
  - managed hook flow
  - config and environment priority checks
  - install/uninstall manifest checks

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
It compares the production keyboard dispatch against an independent XKB state.
Binding actions are stubbed; a seat grab records delivery without a display server.
Both `update_state=true` and `update_state=false` paths are covered. This does not
replace testing actual backend input or observing client protocol traffic.

Manual checks, separately in native and nested sessions:

1. Use `de,us` with `grp:alt_shift_toggle`; cycle six times and verify Y/Z input
   in a terminal after each switch.
2. Press and release Shift, CapsLock, AltGr, and Super; verify no modifier remains
   active after release and CapsLock toggles once per press.
3. Hold a normal key and verify client repeat still works and stops on release.
4. Trigger a compositor shortcut that changes workspace/focus; release its keys
   after the focus change. The destination client must not receive the shortcut key.
5. Switch between a normal window and a keyboard-interactive layer surface while
   holding a consumed shortcut. Verify it does not start repeating in the new client.
6. Record backend, wlroots version, bindings used, and result in the test report.

Phase 1 implementation check (2026-09-05, wlroots 0.19.3):

- `meson compile -C build`: passed.
- `meson test -C build --print-errorlogs`: config, keyboard, shell-runtime passed.
- Keyboard test with `-Db_sanitize=address,undefined`: passed outside the sandbox;
  LeakSanitizer cannot run under the sandbox's ptrace environment.
- Actual native/nested session checks: not yet performed.

## Related Documentation

- [`docs/TESTING.md`](TESTING.md)
- [`testing/testplan-manual-morph.nfo`](../testing/testplan-manual-morph.nfo)
