# Roadmap: Full XKB Support

As of: 2026-09-05. Status: phases 1 and 2 implemented; runtime acceptance pending.
English version: [Roadmap.md](Roadmap.md).
Foundations: [audit](problems.de.md) and [requirements](copilot-instructions.md).
Source references refer to the audit baseline `c4523e9`.

## Goal and scope

Morph should reliably load multiple keyboard layouts, process native XKB group
changes, keep clients synchronized, and provide layout-independent shortcuts.
Groups and keymaps should be controllable at runtime and queryable through IPC.
The following phases define the agreed feature scope; they do not yet constitute
a commitment to support arbitrary future wlroots versions or input protocols.

All five RMLVO values are supported: Rules, Model, Layout, Variant, Options.
wlroots remains responsible for normal keyboard state and Wayland delivery.
Compose and text input methods remain the responsibility of clients or their protocols.
Window layouts and workspaces remain separate from keyboard layouts.

## Planned semantics

These decisions serve as the basis for implementation and tests.

- Startup priority: explicit keyboard configuration, caller environment,
  user environment, system environment, defaults. Existing installations
  without keyboard configuration retain their environment settings.
- When layout/model is missing, `us` and `pc105` apply; the supplied
  environment files retain `de,us`. Empty options disable options;
  missing and explicitly empty values are not universally treated as equivalent.
- The `[keyboard]` section in `morph.conf` supplies reloadable RMLVO values.
  Reload reads this file and uses the environment values resolved at
  startup for unspecified values. Environment files are not executed again
  as shell code in the compositor.
- State and group index apply per keyboard. Without device selection,
  runtime commands target the active seat keyboard; without an active keyboard,
  an explicit error is returned. New devices start with the configured initial group.
- Group indices are zero-based; next/previous wrap around. Invalid
  explicit indices are rejected. Focus changes do not change the group.
- Shortcuts are resolved against the primary group by default.
  The active group continues to determine normal client input. An explicit
  compatibility option preserves the previous active-keysym behavior on request.
- IPC exports the group index, display name, and separate RMLVO identifiers.
  `xkb_keymap_layout_get_name()` is not mistaken for a source of `de`/`us`;
  unknown short identifiers are explicitly shown as unavailable.

## Manual acceptance report

[report_current.md](report_current.md) is updated with each phase.
After each implemented phase, the maintainer checks the build and the relevant
tests of the completed phase on a separate Debian Sid laptop.

Only after full implementation do selected checks follow on
Arch/CachyOS Xfce and Fedora 44 Xfce. These cover the relevant input,
XKB, device, and client tests. Add further tests only for relevant
differences or findings. Specifically repeat device checks when using
different hardware.

Before completing each phase, supplement its test IDs with executable steps and
expected results. Run the Debian matrix entries after their respective
phase; run the Arch/Fedora entries in the final native round.
Preserve results and commit IDs; for release, repeat the build and tests affected
by later changes on Debian. Keep both reports synchronized.

## Phase 1: Correct event processing

Priority: high. Dependencies: none. Finding: XKB-01.
Entry points: `src/main.c:4663`, `src/main.c:4713`.

- [x] Remove the recursive `wlr_keyboard_notify_key()` call and the subsequently
  unnecessary recursion guard; do not introduce a second manual state update.
- [x] Respect `update_state`: wlroots updates raw events exactly once;
  modifier masks supplied by the backend are not duplicated by local updates.
- [x] Preserve binding evaluation using the state before the event and
  check the order of key and modifier delivery.
- [x] Handle consumed shortcut presses and their corresponding releases consistently,
  including focus changes, held keys, and repeat.

Acceptance: press/release sequences, CapsLock, AltGr, and repeated
`grp:alt_shift_toggle` produce the expected final state without stuck modifiers.
Native and nested input are tested separately.

Implementation status: code and automated regression tests for phase 1 are
implemented on `feature/xkb-full-support`. The build and all three Meson tests pass
with wlroots 0.19.3. Actual native/nested session checks remain pending; see
`docs/TESTS.md`.

Manual acceptance: [report_current.md](report_current.md), **Phase 1**.

## Phase 2: Configuration and keymap lifecycle

Priority: high. Dependency: phase 1. Findings: XKB-03, XKB-05, GEN-02.
Entry points: `src/main.c:5392`, `src/main.c:5446`, `src/config.c:31`,
`scripts/morph-session:47`, `testing/morph-session_dbg:61`.

- [x] Capture, restore, and export `XKB_DEFAULT_RULES` in both launchers.
- [x] Introduce shared RMLVO resolution with documented priority, defaults, and
  empty-value handling; associate multiple layouts with their variants.
- [x] Parse and validate `[keyboard]`; fix buffer issue GEN-02 in the process
  and handle excessively long lines explicitly.
- [x] Encapsulate keymap creation and installation; check every return value.
  Report a clear error when starting without a valid keymap; hotplug failures
  must not leave a partially initialized keyboard or terminate the session.
- [x] Track keyboards for reload and hotplug; manage listeners, references, and
  device identities with an explicit lifecycle.

Acceptance: all five variables reach the release and debug launchers correctly.
Invalid rules, variants, and layouts, as well as installation failures, produce
understandable errors without leaks or silent changes to another configuration.

Implementation status: code and automated tests for phase 2 are implemented.
All three Meson suites pass; configuration and keyboard tests also pass with
AddressSanitizer/UBSan. Both launchers have been tested with system/user/caller
values. Actual hotplug and native/nested sessions still require manual acceptance.
Changed keyboard settings still require a restart; replacement
while running follows in phase 3.

Manual acceptance: [report_current.md](report_current.md), **Phase 2**.

## Phase 3: Client synchronization and keymap reload

Priority: high. Dependencies: phases 1 and 2. Finding: XKB-04, in part.
Entry points: `src/main.c:4738`, `src/main.c:2810`, `src/main.c:4497`.

- [ ] Use the existing wlroots path for depressed/latched/locked and the effective
  group; do not build a parallel manual `wl_keyboard` implementation.
- [ ] Prepare and validate keymaps for reload before replacing existing
  ones. On installation failures, also restore keyboards already changed;
  a rollback failure must be explicitly visible.
- [ ] Handle held keys, modifiers, locks, and the group during replacement according to
  a documented policy. Match the group by layout/variant and occurrence;
  if it is missing, fall back to the configured initial group.
- [ ] Deliver the keymap and associated modifiers in the correct order;
  cover focus changes, new clients, and changes to the active keyboard.
- [ ] Preserve repeat rate and delay during reload.

Acceptance: clients see the same effective group as Morph. A failed
reload preserves the previous working configuration; successful reloads
require neither a client restart nor reconnecting the keyboard.

Manual acceptance: [report_current.md](report_current.md), **Phase 3**.

## Phase 4: Layout-independent shortcuts

Priority: medium. Dependencies: phases 1 and 2. Finding: XKB-02.
Entry points: `src/main.c:4693`, `src/config.c:605`, `src/config.c:630`.

- [ ] Implement binding resolution through group 0 without switching the live
  XKB state for evaluation.
- [ ] Explicitly handle Shift, CapsLock, AltGr, consumed modifiers, and multiple keysyms
  for a key; document the semantics with examples.
- [ ] Provide a compatibility mode for active keysyms and explain the intentional
  change in default behavior in the migration notes.
- [ ] Do not replace native `grp:...` actions with hardcoded toggle shortcuts.

Acceptance: shortcuts on Y/Z, punctuation, and Latin letters work
on the same primary keys with `de,us` and `us,ru`. Shift shortcuts remain
distinguishable; normal text input continues to use the active group.

Manual acceptance: [report_current.md](report_current.md), **Phase 4**.

## Phase 5: Runtime control and status model

Priority: medium. Dependencies: phases 3 and 4. Finding: XKB-04.
Entry points: `src/config.h:27`, `src/server.h:170`.

- [ ] Implement shared operations for next, previous, set-index, and status
  queries; CLI, bindings, and IPC use the same logic.
- [ ] Implement device selection and behavior when the active keyboard is removed;
  short-lived IDs must not silently target a different device.
- [ ] Synchronize group changes through appropriate wlroots state functions;
  preserve other modifier components and validate bounds.
- [ ] Status includes device ID, active seat keyboard, keymap generation,
  available groups, current group, names, layout identifiers, and variant identifiers.
- [ ] Native group changes, programmatic changes, reload, and hotplug
  use the same status update; unchanged state does not generate an event.

Acceptance: two keyboards with different groups retain defined
states. Single-layout configurations, cyclic changes, and invalid indices
work without side effects on modifiers or window layouts.

Manual acceptance: [report_current.md](report_current.md), **Phase 5**.

## Phase 6: IPC and CLI

Priority: medium. Dependency: phase 5; fix GEN-01 before release.
Entry points: `src/main.c:4302`, `src/main.c:4539`.

- [ ] Process IPC clients using nonblocking, event-driven handling; handle bounded
  buffers, partial messages, disconnects, timeouts, and slow readers.
- [ ] Define a versioned, line-delimited request/response/event format;
  keyboard commands receive their own namespace alongside window `layout`.
- [ ] Implement status queries, next/previous/set-index, and subscriptions.
  Subscribers first receive a snapshot, then ordered changes with
  generation/sequence; failures are returned as error responses.
- [ ] Keep existing IPC commands compatible; document the CLI and IPC protocol.
  External panel adapters remain optional; the subscription interface is required.

Acceptance: connections without data, fragmented messages, and slow subscribers
do not block the session. A panel can track initial state, group changes,
reload, and hotplug without polling.

Manual acceptance: [report_current.md](report_current.md), **Phase 6**.

## Phase 7: Regression tests and release

Dependencies: all previous phases. Tests are already added with each phase.

- [ ] Add automated tests for RMLVO priority, empty values, parser boundaries,
  group sequences, shortcuts, failure paths, reload, and IPC to Meson.
- [ ] Pass the test build directory to the shell suite instead of assuming
  `build/`; specifically test both launchers.
- [ ] A Wayland integration test records keymap, enter, key, and modifier events
  and checks the group and order, including when switching between clients.
- [ ] Test native libinput, nested Wayland, and available X11 backend paths;
  record who supplies state updates for each backend.
- [ ] Cover the matrix: `us`, `de,us`, `us,de`, variants, disabled options,
  `grp:alt_shift_toggle`, another group option, CapsLock, NumLock, AltGr,
  two keyboards, hotplug, held keys, and failed reload.
- [ ] Ensure the build, existing tests, and relevant sanitizer runs pass;
  compare supported wlroots versions with versions actually tested.
- [ ] Update `docs/CONFIG.md`, `docs/CLI.md`, `docs/ENVIRONMENT.md`, `docs/TESTS.md`
  and the manual test plan; keep both roadmaps synchronized.
- [ ] Explain RMLVO (Rules, Model, Layout, Variant, Options) in the user documentation
  with a complete example, variant placeholders, and group switching.

Manual acceptance: [report_current.md](report_current.md), **Phase 7**.

## Completion criteria and order

Order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7. GEN-01 can be
prepared after phase 2 but must be completed before the IPC release.
GEN-03 and GEN-04 remain separate workspace tasks from `problems.de.md`.

The roadmap is complete when all acceptance criteria are demonstrably met,
XKB-01 through XKB-05 and the required GEN-01/GEN-02 are closed, and
no untested backend support is presented as verified.
For each phase, the commit, test command, result, and remaining limitations
are recorded in the subsequent implementation report. The previously successful build and
configuration test do not replace XKB runtime acceptance testing.
