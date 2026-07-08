# Roadmap: Install and User Setup Rework

This roadmap applies to branch `feature/install-and-user-setup`, which starts from `33e99c9` and therefore predates Nathan's later `/etc` commit to have a clean starting point but with /etc in mind.

## Guidelines

- [ ] No commits without explicit approval.
- [ ] Finish each larger change first, then test it appropriately.
- [ ] If a manual test is required, the item is only considered done after your confirmation.
- [ ] After the implementation works: document new or changed functions, structs, and central data flows with docstrings and meaningful inline comments.
- [ ] Then update affected `.md` files.
- [ ] Only at the very end, prepare a fitting commit message with 2-5 meaningful bullet points.

## Target State

- [x] Create a runtime-ready session startup that is clearer for users than the current developer-heavy flow.
- [x] Provide system-wide base configuration under `/etc/morph`.
- [x] Layer user configuration under `~/.config/morph` as an override on top of the system base.
- [x] Model `startup`, `reload`, and `shutdown` as a clear, robust lifecycle.
- [x] Keep the current test and development flow, but separate it cleanly from runtime session initialization.

## Target Paths

### System-wide

- [x] `/usr/bin/morph` for the binary.
- [x] `/usr/bin/<session-wrapper>` for the runtime session wrapper.
- [x] `/etc/morph/morph.conf` as base configuration.
- [x] `/etc/morph/startup.sh` as the base hook for session components and autostarts.
- [x] `/etc/morph/reload.sh` for reload-specific coupling.
- [x] `/etc/morph/shutdown.sh` for optional system-wide extra shutdown behavior.
- [x] `/etc/morph/environment` for system-wide runtime defaults.
- [x] `/etc/morph/portals` as a single file for portal handling.
- [x] `/usr/share/wayland-sessions/morph.desktop` for display managers.
- [x] `/usr/share/doc/morph/` for docs and reference files.

### User-specific

- [x] `~/.config/morph/morph.conf` as the user override for the base configuration.
- [x] `~/.config/morph/startup.sh` for additional user autostarts and session components.
- [x] `~/.config/morph/reload.sh` for user reload behavior.
- [x] Optional `~/.config/morph/shutdown.sh` for extra user shutdown logic.
- [x] Optional `~/.config/morph/environment` for user-specific environment overrides.

## Architecture Decisions

- [x] `/etc/morph` contains the base layer. `~/.config/morph` overrides it selectively.
- [x] Portal handling lives in a single file, not in a directory.
- [x] `startup.sh` is reduced to a user-facing file that only contains session components and additional autostarts.
- [x] Nested/native detection is pulled out of the user hooks.
- [x] There are three startup helpers:
  - [x] `launch` for regular managed processes.
  - [x] `launch_nested` for processes that should only start in nested sessions.
  - [x] `launch_nokill` for native system services that are intentionally excluded from shutdown tracking.
- [x] `launch_nokill` is not meaningful in nested sessions and must be documented clearly there.
- [x] `reload.sh` is treated as a real lifecycle hook instead of a later special case.
- [x] Foreign or incompatible hook scripts must not silently break the core flow; instead they need clear error messages and a defined fallback.

## Work Phases

### 1. Baseline and Current-State Analysis

- [x] Create branch `feature/install-and-user-setup` from `33e99c9`.
- [x] Document the current relevant files and code paths:
  - [x] `testing/morph_run`
  - [x] `config/startup.sh`
  - [x] `config/shutdown.sh`
  - [x] `config/environment`
  - [x] `sessions/morph.desktop`
  - [x] `meson.build`
  - [x] Config search paths in `src/config.c`
- [x] Record hard compatibility boundaries:
  - [x] What stays dev-only?
  - [x] What becomes runtime?
  - [x] Which user files must remain stable?

#### Current State

- [x] `testing/morph_run` is currently the central session launcher and mixes runtime and dev responsibilities.
- [x] `config/startup.sh` was not a pure user hook and used to contain session decisions and portal logic directly.
- [x] `config/shutdown.sh` was tied to the managed `launch` flow and did not yet expose controlled user-hook integration.
- [x] `config/environment` is still explicitly aligned with the test/launcher flow.
- [x] `sessions/morph.desktop` still starts the binary directly instead of a wrapper.
- [x] A local `/usr/bin/morph -> testing/morph_run` workaround may already exist and must be treated as transitional.
- [x] `meson.build` still installs only the binary, the config test, and the session desktop file.
- [x] The binary config search logic still does not know `/etc/morph/morph.conf`.

#### Current Branch State after the First Runtime Refactors

- [x] `config/startup.sh` is now reduced to a slim user hook:
  - [x] managed startup preparation lives in `scripts/system_startup.sh`
  - [x] `config/startup.sh` only contains user-facing session entries and relies on the managed hook frame
  - [x] nested/native detection and portal startup are no longer inside the actual user hook body
- [x] shutdown responsibilities are now split explicitly:
  - [x] `config/shutdown.sh` is the optional user shutdown hook
  - [x] `scripts/system_shutdown.sh` is the managed cleanup runtime that must still run after the user hook
- [x] The current dev flow now uses managed hook dispatch instead of direct user-hook execution:
  - [x] `testing/morph_run` exports [`MORPH_MANAGED_HOOKS=1`](docs/ENVIRONMENT.md#morph-managed-hooks)
  - [x] the binary then dispatches `scripts/system_startup.sh` and `scripts/system_shutdown.sh`
  - [x] configured user hooks remain in the config and are only run from the managed runtime in the intended order
- [x] Portal handling is now encapsulated as its own managed runtime building block:
  - [x] `config/portals` contains the base flow for native sessions
  - [x] an optional override under `~/.config/morph/portals` is loaded after the base file
  - [x] the effective portal and toolkit variables are logged dynamically into the startup log
- [x] The test layer now covers more than config parsing:
  - [x] `tests/test_shell_runtime.sh` verifies central shell runtime contracts
  - [x] `meson.build` runs those shell tests in the same `meson test` pass as the existing C test
  - [x] `testing/test-howto_start-variants.nfo` now matches the new manual validation flow

#### Compatibility Boundaries

- [x] Dev-only remains everything explicitly described as a test or developer path.
- [x] Runtime must include the pieces that initialize a real session and must stay stably reachable for users.
- [x] The main stable user-facing files remain `~/.config/morph/morph.conf`, `startup.sh`, `reload.sh`, and optional `shutdown.sh` and `environment`.
- [x] `shutdown_list.nfo` remains runtime state, not a user-editable file.

### 2. Target Architecture for Runtime Files and Ownership

- [x] Separate system paths and user paths.
- [x] Define ownership for `config`, hooks, `environment`, `portals`, docs, and the session file.
- [x] Define override rules between `/etc/morph` and `~/.config/morph`.
- [x] Separate runtime files from templates and pure documentation.
- [x] Account for the current `/usr/bin/morph -> testing/morph_run` workaround as a migration case.

#### Target Architecture and Ownership

- [x] `/usr/bin/morph` is the installed binary in the target state, not the session wrapper.
- [x] `/usr/bin/<session-wrapper>` is the runtime entry point for real sessions.
- [x] The installed session file under `/usr/share/wayland-sessions/` will point to the runtime wrapper instead of the binary.
- [x] `/etc/morph` is the system-wide base layer.
- [x] `~/.config/morph` is the user override layer.
- [x] `/usr/share/doc/morph/` is documentation/reference only.
- [x] `testing/*` remains the development and test layer.

#### Override and Resolution Rules

- [x] Config resolution target order is defined.
- [x] Environment resolution target order in the wrapper is defined.
- [x] User hooks have priority over system hooks, but the managed lifecycle frame must remain active.
- [x] Portal handling is not assumed as a loose user snippet; it remains part of managed session startup.

#### Runtime Files vs. Templates vs. Docs

- [x] Live runtime files: `/etc/morph/*`, `~/.config/morph/*`, installed session file.
- [x] Development/test templates: `testing/config/*`, `testing/morph_run`.
- [x] Reference-only files: Markdown docs and example files that are not installed as runtime files.

#### Migration Notes for the Current Workaround

- [x] A local `/usr/bin/morph -> testing/morph_run` link is transitional, not the target state.
- [x] The new runtime wrapper must consciously take over this role.
- [x] Dev install may still offer a quick symlink flow, but it must stay clearly separate from runtime system install.

### 3. Simplify the Startup Flow

- [x] Reduce `startup.sh` to user content:
  - [x] Additional autostart services
  - [x] Session components
- [x] Provide `launch`, `launch_nested`, and `launch_nokill` as a clear documented hook API.
- [x] Keep `launch` and `launch_nested` integrated with managed shutdown.
- [x] Ensure `launch_nokill` never lands in `shutdown_list.nfo`.
- [x] Define helper behavior clearly for nested and native sessions.

#### Target Shape of `startup.sh`

- [x] `startup.sh` is now a simple user hook with two content areas:
  - [x] Additional autostart services
  - [x] Session components
- [x] Nested/native detection, backend selection, base logging/runtime setup, portal handling, and tracker management no longer belong directly in the user hook.
- [x] Users should only describe what should start, not how the runtime frame works internally.

#### Hook Helper API Contract

- [x] `launch <cmd ...>` starts a regular session process, logs it, and registers it for shutdown.
- [x] `launch_nested <cmd ...>` starts only in nested sessions, logs skips in native sessions, and registers running processes for shutdown.
- [x] `launch_nokill <cmd ...>` starts a native session process without shutdown registration and is not meant for nested sessions.
- [x] All three helpers remain part of a managed runtime frame and are not reimplemented by users.

#### Runtime Layer Responsibilities

- [x] The runtime layer decides whether the session is nested or native before hook execution.
- [x] The runtime layer exports the necessary state to the helpers:
  - [x] session mode
  - [x] log targets
  - [x] runtime state paths
  - [x] shutdown tracker file
- [x] The runtime layer makes helper functions available before `startup.sh` is loaded.
- [x] `startup.sh` should use the helpers without evaluating session mode itself.

#### Behavior in Nested and Native

- [x] In native sessions, `launch` starts managed components, `launch_nested` logs a skip, and `launch_nokill` starts explicitly unmanaged processes.
- [x] In nested sessions, `launch` remains usable for nested-safe processes, `launch_nested` is the preferred nested entry point, and `launch_nokill` should not be used.
- [x] Users no longer need their own `if [ "$WLR_BACKENDS" = ... ]` blocks for this split.

#### Example Hook Shape in the Target State

- [x] Desired user-facing simplicity:
  - [x] `launch sfwbar`
  - [x] `launch_nested waybar`
  - [x] `launch_nokill lxqt-policykit-agent`
- [x] Such hooks should work without knowledge of internal runtime files, session detection, or shutdown wiring.

#### What Carries Over from the Old State

- [x] Keep shared logging helpers, the central shutdown tracker, and simple launch helpers instead of raw background processes.
- [x] Remove direct nested/native branching, embedded portal handling, and the implicit mix of runtime core and user content from `config/startup.sh`.
- [x] Runtime preparation now lives before user content in a separate file: `scripts/system_startup.sh`, which then starts the user hook.
- [x] The current dev flow now has three separated layers:
  - [x] `testing/morph_run` as launcher and runtime frame
  - [x] `scripts/system_startup.sh` as managed startup preparation
  - [x] `config/startup.sh` as the slim user hook for session components

### 4. Extract Portal Handling

- [x] Move portal startup logic into a single `portals` file.
- [x] Define whether it is wired system-wide, user-wide, or on both levels.
- [x] Ensure portal handling does not depend on user autostarts.
- [x] Document how users can override the behavior without being forced to.
- [x] Log effective portal exports and optional desktop overrides clearly at runtime.

### 5. Make the Shutdown Flow Robust

- [x] Separate the managed shutdown core from the optional user shutdown hook.
- [x] Define how `shutdown_list.nfo` remains authoritative.
- [x] Wire an optional user shutdown hook so the core flow cannot be lost.
- [x] Provide clear error paths for incompatible hook usage.
- [x] Define a fallback if only `morph` itself can be shut down or restarted safely.

### 6. Introduce the Reload Flow

- [x] Integrate `reload.sh` into the lifecycle.
- [x] Define when `reload` is needed instead of a normal restart.
- [x] Separate a managed core reload from optional user reload behavior.
- [ ] Record examples of reload-relevant components (the remaining non-panel example is deferred until a concrete case exists):
  - [x] panels such as `sfwbar` or `waybar`
  - [ ] other session components with their own reinitialization needs

- [x] Reload stays bound to the existing IPC/CLI entry point:
  - [x] `reload config` / `reload` inside the running compositor
  - [x] `morph --reload-config` as the CLI frontend to that path
- [x] The managed reload flow now uses `scripts/system_reload.sh`.
- [x] User reload hooks remain config-driven and fall back to `~/.config/morph/reload.sh`.
- [x] The helper API now includes `reload <cmd ...>` for targeted restarts of managed session components without duplicating shutdown tracker entries.
- [x] The helper API now includes `reload_once <cmd ...>` for optional extra components that should only be started once during reload.

### 7. Lock Down Config and Environment Priorities

- [x] Define the config priority chain:
  - [x] CLI `-c/--config`
  - [x] explicit environment variables
  - [x] user configuration
  - [x] system configuration
  - [x] then fail instead of silently starting without a config
- [x] Define the environment priority chain:
  - [x] caller environment
  - [x] user environment
  - [x] system environment
  - [x] wrapper defaults
- [x] Decide whether the binary itself should know `/etc/morph/morph.conf` or whether the wrapper sets [`MORPH_CONFIG`](docs/ENVIRONMENT.md#morph-config).
- [x] Ensure system defaults act as a base and user overrides layer on top.

- [x] The binary now knows `/etc/morph/morph.conf` as the final default search path.
- [x] The dev launcher now treats [`MORPH_CONFIG`](docs/ENVIRONMENT.md#morph-config) as the canonical config override variable.
- [x] Missing or unreadable config files now raise an explicit error instead of triggering a silent start without a config.
- [x] An explicit compatibility switch (`--allow-builtin-fallback` or [`MORPH_ALLOW_BUILTIN_FALLBACK=1`](docs/ENVIRONMENT.md#morph-allow-builtin-fallback)) can re-enable the old builtin fallback when needed.

### 8. Installation Strategy

- [x] Create a dedicated dev install script.
- [x] Plan dev install symlinks into `~/.config/morph`.
- [x] Provide clear `sudo` output/help for dev install of the session desktop file.
- [x] Provide the binary and session wrapper under stable paths for user scripts.
- [x] Define system install cleanly via Meson and/or a separate install script.
- [x] Plan uninstall strategy from the start:
  - [x] dev uninstall for created symlinks and locally installed helpers
  - [x] system uninstall for installed files under `/usr/bin`, `/etc/morph`, `/usr/share/wayland-sessions`, and `/usr/share/doc/morph`
  - [x] never remove real user files automatically; only revert install-created symlinks or generated files
- [x] Ensure `testing/*` is never installed as runtime.

### 9. Meson and Packaging Adjustments

- [x] Add install targets for `/etc/morph/*`.
- [x] Add an install target for the session wrapper.
- [x] Wire the session desktop file cleanly to the runtime entry point.
- [x] Install docs and reference files into `/usr/share/doc/morph/`.
- [x] Separate which files are templates and which are real runtime files.
- [x] Document and, where useful, automate a clean uninstall path for system and dev installs.

### 10. Tests

#### Automated

- [x] Add tests for config priorities.
- [x] Add tests for environment priorities.
- [x] Add tests for `launch`, `launch_nested`, and `launch_nokill`.
- [x] Add tests for the shutdown tracker and `shutdown_list.nfo`.
- [x] Integrate tests for startup preparation and portal initialization into the shared Meson test run.
- [x] Add tests for reload hook wiring.
- [x] Add tests for install artifacts or at least build/packaging checks.
- [x] Add tests for conservative system uninstall manifest handling.

#### Manual

- [ ] Test native sessions through the runtime wrapper.
- [ ] Test nested sessions through the runtime wrapper.
- [ ] Test display-manager startup through the `.desktop` file.
- [ ] Test user overrides against `/etc/morph/morph.conf`.
- [ ] Test `launch_nested` behavior in a real nested session.
- [ ] Test `launch_nokill` behavior in a native session.
- [ ] Test reload scenarios with a panel or similar session component.
- [ ] Test error paths with incompatible hook scripts.

## Open Design Questions

- [x] Only prepare the final runtime wrapper name in this branch; handle the actual rename to `morph` later.
  - [x] The runtime binary, session wrapper, desktop files, config paths, and user-facing [`MORPH_*`](docs/ENVIRONMENT.md#launcher-variables) variables now use the `morph` name.
- [x] Decide whether `config` and `environment` resolution stays purely in shell/wrapper logic or moves partially into the binary.
  - [x] `morph.conf` and its default search paths stay in the binary because the config is the compositor-facing interface.
  - [x] `environment` files and their priority chain stay in the wrapper/shell layer because they belong to session orchestration and runtime variable setup.
  - [x] The binary may continue to consume prepared environment variables such as `XKB_DEFAULT_*` and [`MORPH_DEBUG_XDG`](docs/ENVIRONMENT.md#morph-debug-xdg), but it should not resolve `environment` files itself.
- [x] Decide how much user hooks need to be constrained so the managed lifecycle remains robust.
  - [x] User hooks stay flexible and may be used both through the session wrapper and directly with the binary.
  - [x] The managed startup, reload, and shutdown frame remains mandatory and cannot be disabled by hook selection alone.
  - [x] Invalid or incompatible hook usage should be absorbed through logging and fallback behavior instead of silently losing the core lifecycle flow.

## Definition of Done per Work Package

- [ ] Implementation is functionally complete.
- [x] Automated tests run where relevant.
- [ ] Manual tests are documented and, where needed, confirmed by you.
- [ ] New or changed functions, structs, and important data flows are documented.
- [ ] Important places have short, meaningful inline comments.
- [x] Affected Markdown documentation is updated.
- [ ] A commit message with 2-5 useful bullet points is prepared, but not executed yet.
