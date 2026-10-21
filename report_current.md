# Morph XKB Test Report

Branch: `feature/xkb-full-support`. Prepared: 2026-09-05.
Implementation baseline: `f75d4ba` (phases 1 and 2).

This is the cumulative manual acceptance report for all seven XKB phases.
The previous install/setup checklist has been replaced; its checked results do
not apply to this branch. No distribution result below has been claimed yet.

## Test policy

- Test on the development system first, preferably nested with the dbg build.
  Ordinary functional checks can run nested or native; record mode and variant.
- After each phase, run relevant checks on the separate Debian Sid laptop in a
  native Morph session. After full implementation, run C0-C4 natively on
  Arch/CachyOS and Fedora; add C5/C6 only for relevant findings or differences.
- C1 is the single required nested/native comparison test: the same procedure
  and config for runtime and dbg, each in both modes. Run once on the same
  commit, preferably on the same machine. Native Debian results may supply
  evidence; record hardware differences. Do not double this comparison on every distro.
- C0 automatically checks both builds and launchers. E9 additionally exercises
  the real dbg launcher on the development system. Do not repeat the whole
  functional matrix per build variant. Distro tests normally use the runtime build.
- Run extended functional checks on the development system first, preferably
  nested. Run E2 and device-related parts of E4/E6 natively with real hardware.
  Repeat elsewhere only for relevant differences or failures.
- At completion, rerun C0 and checks affected by later changes on Debian.
  Retain unchanged results with their commit IDs.
- Preserve IDs and results; update steps for changed behavior. Build success is
  not manual acceptance. Future phases remain PLANNED until executable steps exist.

Result status: `OPEN`, `PASS`, `FAIL`, `BLOCKED`; record reasons for failures or
blocks. Planning status: `PLANNED` = not implemented, `FINAL` = deferred until
Arch/Fedora final checks, `IF_NEEDED` = only for relevant findings or differences.
Planning values and unavailable hardware are not passes.

## Environments

| Environment | Commit/date | wlroots / libxkbcommon | Mode / backend | Keyboard(s), editor |
| --- | --- | --- | --- | --- |
| Development system (record distribution) | pending | pending | nested and native, runtime and dbg | pending |
| Debian unstable: Siduction Xfce | pending | pending | native Morph session, runtime | pending |
| Arch: CachyOS Xfce | pending | pending | native Morph session, runtime | pending |
| Fedora 44 Xfce | pending | pending | native Morph session, runtime | pending |

Record commit (`git rev-parse --short HEAD`), package versions, build variant,
mode, and backend. On the development system, run C1 twice with the same config:
once nested and once native. The nested backend must be identified from the
startup log (Wayland or X11). Distro checks are native Morph sessions; their
ordinary Xfce desktop does not determine a nested backend. Use the same physical
key positions when comparing modes or distributions.

## Core matrix: development first, distro tests native

| ID | Phase | Essential check | Development: C1 nested + native (runtime/dbg) | Debian: native per phase | Arch: native final | Fedora: native final |
| --- | --- | --- | --- | --- | --- | --- |
| [C0](#c0) | Every phase / 7 | [Runtime/dbg builds and automated suites](#c0) | OPEN | OPEN | FINAL | FINAL |
| [C1](#c1) | 1 | [Shared input test and nested/native comparison](#c1) | OPEN | OPEN | FINAL | FINAL |
| [C2](#c2) | 2 | [Config precedence and explicitly disabled group options](#c2) | OPEN | OPEN | FINAL | FINAL |
| [C3](#c3) | 3 | [Live keymap reload in existing clients; failed reload preserves input](#c3) | PLANNED | PLANNED | FINAL | FINAL |
| [C4](#c4) | 4 | [Shortcuts remain on primary-layout keys across groups](#c4) | PLANNED | PLANNED | FINAL | FINAL |
| [C5](#c5) | 5 | [Next/previous/index control matches actual text input](#c5) | PLANNED | PLANNED | IF_NEEDED | IF_NEEDED |
| [C6](#c6) | 6 | [IPC snapshot/events match state; idle client does not freeze input](#c6) | PLANNED | PLANNED | IF_NEEDED | IF_NEEDED |

Phase 7 consolidates Debian results and the selected Arch/Fedora checks; it does
not introduce another full set of manual tests.

<a id="c0"></a>

## C0: Build and automated baseline

From the repository root; no system installation is required:

```bash
bash scripts/morph-build.sh --both
meson test -C build --print-errorlogs
meson test -C build_dbg config keyboard --print-errorlogs
sh tests/test_shell_runtime.sh --dbg "$PWD"
```

Expected: both runtime and debug builds succeed and all registered suites pass. Currently these are
`config`, `keyboard`, and `shell-runtime`. The shell suite already tests all five
XKB exports and system/user/caller precedence in both launchers. These automated
subcases need no separate manual replay. C1-C6 are independent manual checks;
execute them according to the distribution schedule in the core matrix.

Run C0 on the development system and in the scheduled distro rounds.
`build/` is the release/runtime build; `build_dbg/` is the debug build. Invoke the
dbg shell suite explicitly with `--dbg`; a normal Meson run in the debug build
directory does not automatically select the debug launcher.

## Session fixture for C1 and C2

Create an isolated config from the repository root. Keep the printed directory
for logs and later test phases. This does not change installed or active test configs.
Set `TERMINAL` to an available Wayland terminal if `foot` is not installed.

```bash
export MORPH_TEST_ROOT=$(mktemp -d /tmp/morph-xkb-report.XXXXXX)
export TERMINAL=foot
cat > "$MORPH_TEST_ROOT/morph.conf" <<'EOF'
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
command = ${TERMINAL:-foot}

[bind]
mods = Super
key = Escape
action = quit

[bind]
mods = Super
key = 2
action = workspace_goto
command = 2

[bind]
mods = Super
key = 1
action = workspace_goto
command = 1
EOF
printf 'Test files: %s\n' "$MORPH_TEST_ROOT"
```

### Launch mode and build variant

Select the dbg build for the initial development test:

```bash
export MORPH_TEST_BIN="$PWD/build_dbg/morph"
export MORPH_TEST_VARIANT=dbg
```

Start nested from a terminal in the running desktop:

```bash
env -u WLR_BACKENDS "$MORPH_TEST_BIN" --no-ipc --log-level info \
    --config "$MORPH_TEST_ROOT/morph.conf" \
    > "$MORPH_TEST_ROOT/$MORPH_TEST_VARIANT-nested.log" 2>&1
```

For the comparison and distro tests, start natively from a real TTY.
The test directory and variables must be set in this shell:

```bash
env -u DISPLAY -u WAYLAND_DISPLAY -u WLR_BACKENDS \
    "$MORPH_TEST_BIN" --no-ipc --log-level info \
    --config "$MORPH_TEST_ROOT/morph.conf" \
    > "$MORPH_TEST_ROOT/$MORPH_TEST_VARIANT-native.log" 2>&1
```

For the runtime run, change the variant and use the same launch command:

```bash
export MORPH_TEST_BIN="$PWD/build/morph"
export MORPH_TEST_VARIANT=runtime
```

Open the terminal with Super+Return and launch an editor; exit with Super+Escape.
Direct startup does not use launcher hooks. `--no-ipc` avoids conflicts with
another Morph socket. E9 checks the dbg launcher separately. Reload/IPC tests
require their own IPC-enabled setup.

<a id="c1"></a>

## C1: Shared input test and mode comparison (phase 1)

Use this one procedure and unchanged config for both modes and build variants.
On the development system, run all four combinations once on the same commit:
runtime nested, runtime native, dbg nested, and dbg native. This is the required
equivalence check between nested and native input handling. On Debian, run the
same C1 procedure natively after each phase; on Arch and Fedora, run it in the
final native round. Other functional tests do not need four combinations.

| Build | Nested | Native | Machine/commit and differences |
| --- | --- | --- | --- |
| Runtime (`build/morph`) | OPEN | OPEN | pending |
| dbg (`build_dbg/morph`) | OPEN | OPEN | pending |

1. In the editor, type on the Y/Z key positions. Press Alt+Shift six times;
   verify that German and US output alternate after every press.
2. Check Shift, CapsLock twice, AltGr, and NumLock if available. Release the keys;
   no modifier should remain active unintentionally. CapsLock toggles once per press.
3. Hold a letter for repeat and release it; repeat must stop immediately.
4. Switch between two editor windows and continue typing; input follows focus.
5. Exit Morph using Super+Escape and verify that the parent desktop or TTY is usable again.

Expected: correct text, exactly one group change per toggle, no stuck keys,
working repeat/focus, and a clean exit. Record unavailable modifier keys in notes.

Comparison passes when text, switching, modifiers, repeat, and focus behave
the same across modes and builds. Different debug logging is expected.
If the parent intercepts Alt+Shift or Super, adjust its bindings for the test or
record BLOCKED; this is not a passed comparison. Coverage applies to the recorded
nested backend, not automatically every other backend.

<a id="c2"></a>

## C2: Configuration overrides (phase 2)

1. Use the same fixture, but add `XKB_DEFAULT_LAYOUT=us` to the selected launch
   command. The explicit `layout = de,us` must still win; verify Alt+Shift switching.
2. Exit Morph. In the temporary config only, change `options = grp:alt_shift_toggle`
   to `options =`. Restart with `XKB_DEFAULT_OPTIONS=grp:alt_shift_toggle` added
   to the command. Alt+Shift must no longer switch the layout.
3. Restore the fixture's original options after the check.

Expected: explicit config overrides the environment, including an empty value.
Rules/model/variant validation and parser boundaries are covered by C0 rather
than a separate per-distribution manual matrix.

## Extended checks: development first, hardware native

| ID | Phase | Check | Status | Distribution/backend, commit, evidence |
| --- | --- | --- | --- | --- |
| E1 | 1 | Consumed shortcuts across focus changes | OPEN | pending |
| E2 | 2 | Two keyboards, active-device removal, last-device reconnect | OPEN | pending |
| E3 | 2 | Invalid startup config and current reload restriction | OPEN | pending |
| E4 | 3 | Reload with held keys, reordered groups, multiple keyboards | PLANNED | pending |
| E5 | 4 | Punctuation/AltGr/Shift bindings and compatibility mode | PLANNED | pending |
| E6 | 5 | Device targeting, removed IDs, one-layout wraparound | PLANNED | pending |
| E7 | 6 | Fragmented requests, slow subscribers, reconnect ordering | PLANNED | pending |
| E8 | 7 | Final regression review, documentation, additional backend | PLANNED | pending |
| [E9](#e9) | 1-2 / 7 | Real dbg launcher: startup, XKB, logs, exit | OPEN | pending |

### E1: Consumed shortcuts

Use the appropriate launch command, preferably nested on the development system.
The general mode comparison belongs exclusively to C1.

Open an editor on workspace 1 and another on workspace 2. Hold Super+1/Super+2
while switching, then release after the destination has focus. The destination
must not receive or repeat the consumed number key. Repeat with a keyboard-
interactive layer surface if one is available; record its name or missing coverage.

Expected: normal input survives backend forwarding, and handled shortcut keys
are absent from client input after release or a focus change.

### E2: Device lifecycle

In a native test session, attach two keyboards and type with each. Remove the
last-used keyboard; the remaining device must still type correctly. Remove the
last keyboard and reconnect it; input must return without restarting Morph.
Exit normally. Record device models and any crash, stuck modifier, or lost focus.
Use physical devices here: unplugging a host keyboard may not create a device
removal event inside a nested compositor.

### E3: Rejection paths

With the test session stopped, copy the temporary config to `invalid.conf` in
`MORPH_TEST_ROOT` and set `layout = morph_nonexistent_layout` in that copy.
Run `./build/morph --no-ipc --config "$MORPH_TEST_ROOT/invalid.conf"`.
Expected: a nonzero exit and a clear XKB error before the backend starts.
Do not change the working session config for this startup check.

For the phase-2 reload guard, use a standalone native or nested test session with no other
Morph instance in the same runtime directory. Start the valid fixture without
`--no-ipc`. From a terminal inside that Morph session, edit only its temporary
keyboard layout and run the absolute path to this checkout's `build/morph` with
`--reload-config`. Inspect the compositor log: reload must be rejected and the
old layout must remain active. The command's exit status alone is not proof of
application success in the current IPC implementation. Restore the config and exit.

This reload rejection is temporary. When phase 3 lands, retain E3's prior result
as historical evidence and replace the reload portion with C3's live-update test.

<a id="e9"></a>

### E9: Real dbg launcher

Run once per relevant launcher/XKB change on the development system, normally
nested. First stop other Morph test sessions in the same runtime directory.
The launcher enables IPC; do not run this test nested inside another Morph instance.

```bash
mkdir -p "$MORPH_TEST_ROOT/empty-config" "$MORPH_TEST_ROOT/dbg-wrapper"
env -u MORPH_BIN -u WLR_BACKENDS \
    XDG_CONFIG_HOME="$MORPH_TEST_ROOT/empty-config" \
    MORPH_CONFIG="$MORPH_TEST_ROOT/morph.conf" MORPH_RESOLVE_ONLY=0 \
    MORPH_LOG_DIR="$MORPH_TEST_ROOT/dbg-wrapper" MORPH_DBG=1 \
    sh ./testing/morph-session_dbg
```
Expected: the startup summary identifies `build_dbg/morph`, the temporary config,
and debug logging. Open a terminal, type Y/Z, switch once with Alt+Shift, and type
again. Exit with Super+Escape; the launcher must return normally to the shell.
Check logs under `dbg-wrapper` for startup, exit status, and absence of crash
markers. Leave `MORPH_BIN` unset to test the debug launcher's own default binary.
For native execution from a TTY, also add `-u DISPLAY -u WAYLAND_DISPLAY` to `env`.
This test is not automatically repeated on every distribution.

## Tests to activate with later phases

These are acceptance scenarios, not runnable commands for the current build.
During each phase, add exact syntax and fixture changes here before marking its
core and extended rows OPEN. Reuse existing editors and the temporary config.

| Phase | Core scenario (follow the distribution schedule above) | Extended scenario (development first; hardware native) |
| --- | --- | --- |
| <a id="c3"></a>3 | Reload a changed keymap while two clients stay open; verify new input in both and a newly opened client. Reject an invalid reload and verify old input still works. | Hold modifiers during reload, reorder/remove the active group, and test two keyboards; compare results with the implemented state policy. |
| <a id="c4"></a>4 | Bind a visible action to a primary Y/Z key and a Latin letter; verify the same physical keys under `de,us` and `us,ru` while text follows the active group. | Check punctuation, Shift, AltGr, consumed modifiers, and the active-keysym compatibility setting. |
| <a id="c5"></a>5 | Exercise next, previous, and a valid index; query state and confirm editor input. An invalid index leaves state unchanged. | Target each of two keyboards, remove a target, reject its stale ID, and check one-layout wraparound. |
| <a id="c6"></a>6 | Subscribe, verify initial snapshot and one ordered change per actual switch. Leave another IPC client connected without sending data; editor input must remain responsive. | Fragment requests, stop reading from a subscriber, reconnect, and verify generation/sequence behavior through reload and hotplug. |
| 7 | Consolidate Debian phase results, rerun affected checks, and run C0-C4 on Arch/Fedora after implementation is complete. | Review extended coverage, additional available backends, sanitizer results, and the RMLVO/user instructions. |

## Findings and evidence

Add one row per result or defect; preserve earlier rows when retesting.

| Test ID | Commit/date | Distribution/backend | Result | Observation / log path / follow-up |
| --- | --- | --- | --- | --- |
| pending | pending | pending | OPEN | No manual results recorded yet |

## Final acceptance

- [ ] C1 confirms matching nested/native input for runtime and dbg on the same commit.
- [ ] C0 checks both builds; E9 verifies the real dbg launcher.
- [ ] Debian phase checks pass; C0 and checks affected by later changes are rerun.
- [ ] Final C0-C4 checks pass on Arch/CachyOS and Fedora; any additional selection
  is documented with its reason and tested commit.
- [ ] Required extended cases pass on recorded environments; missing hardware or
  backend coverage is explicit and is not presented as verified support.
- [ ] Phase-2 reload rejection steps have been updated for phase-3 behavior.
- [ ] No unresolved failure remains without a documented decision.
- [ ] This report retains the essential checks and evidence from every phase.

References: [Roadmap](Roadmap.md), [automated tests](docs/TESTS.md),
[configuration](docs/CONFIG.md), [environment](docs/ENVIRONMENT.md).
