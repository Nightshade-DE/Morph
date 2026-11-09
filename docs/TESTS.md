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
  - depressed/latched/locked masks, group-only delivery, and focus snapshots
  - multi-device keymap reload, held-key/modifier/group checks, and installation/rollback failures
- `tests/test_shell_runtime.sh`
  - launcher/runtime contracts
  - managed hook flow
  - config and environment priority checks
  - install/uninstall manifest checks
  - all five XKB variables in both launchers, including explicit empty caller values
- `tests/test_output_shutdown.c`
  - the real compositor startup and teardown with two headless outputs and pixman rendering
  - output removal during an active session, followed by full shutdown
  - no physical input, display server, or external client processes required

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

## Keyboard Phase 3: First Validation

The first two roadmap items cover the existing wlroots modifier path and
transactional keymap replacement. State migration and full client protocol
validation remain pending. Changed RMLVO settings currently require no keys held
down, no active modifiers/locks, and group 0 on every keyboard. Unchanged settings
do not trigger installation or reset state.

The keyboard suite uses real wlroots keyboards and seats to verify:

- Depressed, latched, and locked masks and group-only changes reach the seat grab.
- The existing X11 group correction uses wlroots and forwards one corrected notification.
- Configured XKB group actions remain active during window selection, for both
  raw and backend-supplied modifier updates.
- Window-cycle callbacks receive held modifiers in wlroots binding bits after seat
  delivery, including Alt release while Ctrl remains held; latches and locks are excluded.
- Escape cancels an active window cycle before configured bindings. Its press,
  repeats, release, and focus snapshot stay out of client delivery. Inactive Escape
  still follows normal dispatch. Both raw and backend-supplied state paths are covered.
- A successful reload updates three devices and retains the active keyboard and repeat settings.
- Effective layout changes are logged with old and new values after success,
  including reloads without devices; failed and unchanged reloads report no change.
- Failure on the first, middle, or final device restores every earlier installation.
- Keymaps are installed or restored with the seat detached, preventing intermediate client updates.
- Rollback failure detaches the affected device, restores other devices, and selects a surviving keyboard.
- Keys held down, active modifiers/locks, or a group other than 0 reject changes
  before installation; unchanged settings preserve held state.
- A reload without keyboards permits configuration updates for future hotplug.

Run `meson test -C build keyboard --print-errorlogs` and
`meson test -C build_dbg config keyboard --print-errorlogs`.
Failure injection is confined to the test executable; runtime code has no failure switch.
The seat-grab tests do not replace client protocol or native/nested session testing.
See **Phase 3.0** in [report_current.md](../report_current.md) for the first manual validation.

The config suite checks conflict diagnostics against compiled XKB actions. It
covers Alt+Shift and Alt+Ctrl modifier prefixes, Alt+Space, non-conflicting chords,
and a custom keymap whose Alt+Tab action also conflicts with Alt+Shift+Tab. A linker
wrapper supplies that custom keymap only in the test executable. Diagnostics are
emitted on config load, including reload; they do not reject or rewrite bindings.

Environment reload tests verify old/new diagnostics for replaced, previously unset,
and empty values. Unchanged and caller-protected values produce no change message;
a repeated reload reports zero applied changes.

Implementation check (2026-10-24, wlroots 0.20.2, libxkbcommon 1.8.0):

- Runtime and debug builds pass.
- Runtime config, keyboard, and shell-runtime suites pass; debug config and keyboard suites pass.
- The explicit debug-launcher shell test passes.
- Config and keyboard suites pass with AddressSanitizer/UBSan and leak detection
  outside the sandbox. LeakSanitizer cannot run in the sandbox's ptrace environment.
- Native/nested validation of these phase 3 changes was still pending at that time.

Manual validation update: the maintainer confirmed that the full Phase 3.0
procedure passed natively on Debian Sid, including reload rejection paths,
window-switcher behavior, and binding conflict diagnostics. The optional nested
check for the current version remains pending. Later phase 3 checks still require
their remaining implementation work.

## Output Shutdown Regression

The evidence, correction, and native validation results are recorded in
[shutdown_issue_134.md](../shutdown_issue_134.md).

`output-shutdown` and `output-hotplug` reuse the compositor's object files, including
its real `main()` and output callbacks. The fixture skips socket binding and stops
the real event loop with a timer. The hotplug case also adds and removes a third
headless output before shutdown. Linker wrappers are confined to the test executable.

The original code aborted at `wlr_output_finish()` with a remaining
`head_handle_output_destroy` listener. Backend outputs were destroyed after
display-owned objects, while output callbacks could still publish configuration
through an outdated manager pointer. The regression also failed under Meson's
heap perturbation. After the fix, both cases exit successfully in runtime and debug.

Run the regressions with:

```bash
meson test -C build output-shutdown output-hotplug --print-errorlogs
meson test -C build_dbg output-shutdown output-hotplug --print-errorlogs
```

ASan/UBSan checks pass with leak detection disabled. A full LeakSanitizer run still
reports retained scene, renderer, allocator, and cursor resources; that cleanup
remains separate work. This is not a passing leak check.

Native Debian Sid launcher rechecks on 2026-10-24 confirm compositor exit code 0
for tests 1.8 (debug, 18:34:22) and 1.9 (runtime, 18:38:11). The supplied excerpts
show no repeated output assertion or Morph fatal-signal marker. A separate rerun
of test 1.7 was not reported. See [report_current.md](../report_current.md) for
the recorded results. Remaining `Broken pipe` and `ExitFailure(1)` messages do
not change the compositor's recorded exit status; companion-process cleanup
still requires separate analysis.

## Related Documentation

- [`docs/TESTING.md`](TESTING.md)
- [`testing/testplan-manual-morph.nfo`](../testing/testplan-manual-morph.nfo)
