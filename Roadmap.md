# Roadmap: Complete XKB Support

Date: 2026-09-05. Status: phases 1 and 2 implemented; runtime acceptance pending.
German version: [Roadmap.de.md](Roadmap.de.md).
Sources: [audit](problems.de.md) and [requirements](copilot-instructions.md).
Source references refer to audit baseline `c4523e9`.

## Goal and scope

Morph should reliably load multiple keyboard layouts, process native XKB group
changes, keep clients synchronized, and provide layout-independent shortcuts.
Groups and keymaps should be controllable at runtime and queryable through IPC.
The phases below define the intended feature scope; they do not promise support
for arbitrary future wlroots versions or input protocols.

Support all five RMLVO values: Rules, Model, Layout, Variant, Options.
wlroots remains responsible for normal keyboard state and Wayland delivery.
Compose and input methods remain client or dedicated protocol responsibilities.
Window layouts and workspaces remain separate from keyboard layouts.

## Planned semantics

These decisions provide the basis for implementation and tests.

- Startup precedence: explicit keyboard configuration, caller environment,
  user environment, system environment, defaults. Existing installations without
  keyboard configuration retain their environment settings.
- Missing layout/model use `us` and `pc105`; shipped environment files retain
  `de,us`. Empty options disable options; missing and explicitly empty values
  are not indiscriminately treated as equivalent.
- The `[keyboard]` section in `morph.conf` supplies reloadable RMLVO values.
  Reload reads this file and uses the environment values resolved at startup
  for unspecified fields. The compositor does not re-execute environment files
  as shell code.
- State and group index belong to each keyboard. Runtime commands without a
  device selector target the active seat keyboard; no active keyboard produces
  an explicit error. New devices start with the configured initial group.
- Group indices are zero-based; next/previous wrap around. Invalid explicit
  indices are rejected. Focus changes do not change groups.
- Shortcuts resolve against the primary group by default. The active group
  continues to determine normal client input. An explicit compatibility option
  preserves the previous active-keysym behavior when requested.
- IPC exports group index, display name, and separate RMLVO identifiers.
  `xkb_keymap_layout_get_name()` is not treated as a source of `de`/`us` codes;
  unknown short identifiers are explicitly represented as unavailable.

## Manual acceptance report

Maintain [report_current.md](report_current.md) during every phase. After each
implemented phase, the maintainer runs C0 and the relevant core and extended
checks on a separate Debian Sid laptop.

Only after implementation is complete, run selected checks on Arch/CachyOS Xfce
and Fedora 44 Xfce. The current final selection C0-C4 targets dependencies,
XKB data, and client/backend behavior. Add other checks only for relevant
differences or findings. Repeat E1/E2 for different backends or hardware.

Before completing each phase, add executable steps and expected results for its
report IDs. Execute the Debian matrix entries after their phase; execute the
Arch/Fedora entries in the final native round. Preserve results and commit IDs;
at release, rerun C0 and checks affected by later changes on Debian. Keep both
report languages aligned.

## Phase 1: Correct event processing

Priority: high. Dependencies: none. Finding: XKB-01.
Entry points: `src/main.c:4663`, `src/main.c:4713`.

- [x] Remove the recursive `wlr_keyboard_notify_key()` call and the resulting
  unnecessary recursion guard; do not introduce a second manual state update.
- [x] Respect `update_state`: wlroots updates raw events exactly once;
  backend-supplied modifier masks are not duplicated through local updates.
- [x] Preserve binding evaluation against pre-event state and verify key and
  modifier delivery order.
- [x] Handle consumed shortcut presses and their releases consistently,
  including focus changes, held keys, and repeat.

Acceptance: press/release sequences, CapsLock, AltGr, and repeated
`grp:alt_shift_toggle` produce the expected final state without stuck modifiers.
Verify native and nested input separately.

Implementation status: phase 1 code and automated regression tests are complete
on `feature/xkb-full-support`. The current development build and all three Meson
tests pass with wlroots 0.20.2. Actual native/nested session checks remain open;
see `docs/TESTS.md`.

Manual acceptance: [report_current.md](report_current.md), **C1 / E1**.

## Phase 2: Configuration and keymap lifecycle

Priority: high. Dependency: phase 1. Findings: XKB-03, XKB-05, GEN-02.
Entry points: `src/main.c:5392`, `src/main.c:5446`, `src/config.c:31`,
`scripts/morph-session:47`, `testing/morph-session_dbg:61`.

- [x] Capture, restore, and export `XKB_DEFAULT_RULES` in both launchers.
- [x] Introduce shared RMLVO resolution with documented precedence, defaults,
  and empty-value handling; associate multiple layouts with their variants.
- [x] Parse and validate `[keyboard]`; fix buffer issue GEN-02 and handle
  oversized lines explicitly.
- [x] Encapsulate keymap creation and installation; check every return value.
  Report a clear error when startup has no valid keymap; hotplug failures must
  not leave a partially initialized keyboard or terminate the session.
- [x] Track keyboards for reload and hotplug; manage listeners, references,
  and device identities with an explicit lifecycle.

Acceptance: all five variables reach release and debug launchers correctly.
Invalid rules, variants, layouts, and installation failures produce actionable
errors without leaks or silent substitution of another configuration.

Implementation status: phase 2 code and automated tests are complete. All three
Meson suites pass; config and keyboard tests also pass with AddressSanitizer/UBSan.
Both launchers are tested with system/user/caller values. Actual device hotplug
and native/nested session checks remain manual acceptance work. Changed keyboard
settings currently require a restart; live replacement is phase 3.

Manual acceptance: [report_current.md](report_current.md), **C2 / E2-E3**.

## Phase 3: Client synchronization and keymap reload

Priority: high. Dependencies: phases 1 and 2. Finding: part of XKB-04.
Entry points: `src/main.c:4738`, `src/main.c:2810`, `src/main.c:4497`.

- [ ] Use the existing wlroots path for depressed/latched/locked modifiers and
  effective group; do not build a parallel manual `wl_keyboard` implementation.
- [ ] Prepare and validate reload keymaps before replacing existing ones.
  Installation failure must also restore keyboards already changed;
  a rollback failure must be explicitly visible.
- [ ] Handle held keys, modifiers, locks, and groups during replacement using
  a documented policy. Match the group by layout/variant and occurrence;
  if absent, fall back to the configured initial group.
- [ ] Deliver keymap and associated modifiers in the correct order; cover
  focus changes, new clients, and active keyboard changes.
- [ ] Preserve repeat rate and delay across reload.

Acceptance: clients see the same effective group as Morph. A failed reload
preserves the previous working configuration; successful reloads require
neither client restarts nor reconnecting keyboards.

Manual acceptance: [report_current.md](report_current.md), **C3 / E4**.

## Phase 4: Layout-independent shortcuts

Priority: medium. Dependencies: phases 1 and 2. Finding: XKB-02.
Entry points: `src/main.c:4693`, `src/config.c:605`, `src/config.c:630`.

- [ ] Implement binding resolution through group 0 without switching the
  production XKB state for evaluation.
- [ ] Explicitly handle Shift, CapsLock, AltGr, consumed modifiers, and multiple
  keysyms per key; document the semantics with examples.
- [ ] Provide an active-keysym compatibility mode and explain the intentional
  default behavior change in migration notes.
- [ ] Do not replace native `grp:...` actions with hardcoded toggle shortcuts.

Acceptance: Y/Z, punctuation, and Latin-letter shortcuts work on the same primary
keys with `de,us` and `us,ru`. Shift shortcuts remain distinguishable;
normal text input continues to use the active group.

Manual acceptance: [report_current.md](report_current.md), **C4 / E5**.

## Phase 5: Runtime control and state model

Priority: medium. Dependencies: phases 3 and 4. Finding: XKB-04.
Entry points: `src/config.h:27`, `src/server.h:170`.

- [ ] Implement shared next, previous, set-index, and status operations;
  CLI, bindings, and IPC use the same logic.
- [ ] Implement device selection and active keyboard removal behavior;
  transient IDs must not silently target a different device.
- [ ] Synchronize group changes through appropriate wlroots state functions;
  preserve other modifier components and validate bounds.
- [ ] State includes device ID, active seat keyboard, keymap generation,
  available groups, current group, names, layout identifiers, and variants.
- [ ] Native changes, programmatic changes, reload, and hotplug use the same
  state update path; unchanged state does not generate an event.

Acceptance: two keyboards with different groups retain defined states.
Single-layout configurations, wraparound, and invalid indices work without
side effects on modifiers or window layouts.

Manual acceptance: [report_current.md](report_current.md), **C5 / E6**.

## Phase 6: IPC and CLI

Priority: medium. Dependency: phase 5; fix GEN-01 before release.
Entry points: `src/main.c:4302`, `src/main.c:4539`.

- [ ] Process IPC clients through nonblocking event handling; handle bounded
  buffers, partial messages, disconnects, timeouts, and slow readers.
- [ ] Define a versioned, line-delimited request/response/event format;
  keyboard commands use a separate namespace from window `layout` commands.
- [ ] Implement status queries, next/previous/set-index, and subscriptions.
  Subscribers receive a snapshot followed by ordered changes with generation
  and sequence information; failures produce error responses.
- [ ] Preserve existing IPC commands; document CLI and IPC protocol.
  External panel adapters remain optional; the subscription interface is required.

Acceptance: idle connections, fragmented messages, and slow subscribers do not
block the session. A panel can track initial state, group changes, reload,
and hotplug without polling.

Manual acceptance: [report_current.md](report_current.md), **C6 / E7**.

## Phase 7: Regression tests and release

Dependencies: all preceding phases. Add tests alongside each implementation phase.

- [ ] Add automated tests for RMLVO precedence, empty values, parser boundaries,
  group sequences, shortcuts, failures, reload, and IPC to Meson.
- [ ] Pass the test build directory to the shell suite instead of assuming
  `build/`; explicitly test both launchers.
- [ ] Add a Wayland integration test recording keymap, enter, key, and modifier
  events and checking groups and ordering, including switches between clients.
- [ ] Test native libinput, nested Wayland, and available X11 backend paths;
  record which component supplies state updates for each backend.
- [ ] Cover this matrix: `us`, `de,us`, `us,ru`, variants, disabled options,
  `grp:alt_shift_toggle`, another group option, CapsLock, NumLock, AltGr,
  two keyboards, hotplug, held keys, and failed reload.
- [ ] Pass builds, existing tests, and relevant sanitizer runs; align supported
  wlroots versions with versions actually tested.
- [ ] Update `docs/CONFIG.md`, `docs/CLI.md`, `docs/ENVIRONMENT.md`,
  `docs/TESTS.md`, and the manual test plan; keep both roadmaps synchronized.
- [ ] Explain RMLVO (Rules, Model, Layout, Variant, Options) in user documentation
  with a complete example, including variant placeholders and group switching.

Manual acceptance: [report_current.md](report_current.md), **C0-C6 / E8**.

## Completion criteria and order

Order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7. GEN-01 can be prepared after phase 2
but must be complete before IPC release. GEN-03 and GEN-04 remain separate
workspace tasks recorded in `problems.de.md`.

The roadmap is complete when every acceptance criterion has evidence,
XKB-01 through XKB-05 and required GEN-01/GEN-02 are closed, and untested
backend support is not presented as verified. Record the commit, test command,
result, and remaining limitations for each phase in the implementation report.
The previously successful build and config test do not replace XKB runtime
acceptance testing.
