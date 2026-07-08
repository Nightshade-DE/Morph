# morph Test Report for the Feature Branch (as of 2026-06-24)

This report is a practical test checklist for branch `feature/install-and-user-setup`. You can tick items while testing and add notes next to them if useful.

## Legend

- `[ ]` Not tested yet
- `[x]` Tested successfully
- `Expected:` What should be visible in a successful test
- `References:` Relevant files or docs for follow-up

## 0. Automated Baseline Run

- [ ] First build: `(rm -rf build/;) meson setup build`
- [ ] Follow-up builds: `meson setup build --reconfigure`
- [ ] `meson test -C build --print-errorlogs`

Expected:
- Build configures and compiles `morph`
- `config` and `shell-runtime` tests pass

References:
- `meson.build`
- `tests/test_shell_runtime.sh`
- `docs/TESTS.md`

## 1. README and Doc Entry Points

- [ ] Open `README.md` and check whether the entry links make sense
- [ ] Open `docs/OVERVIEW.md` and check whether repo structure + flows read as a coherent path
- [ ] Reach `docs/LAUNCHER.md`, `docs/ENVIRONMENT.md`, and `docs/CLI.md` through the links in `README.md`

Expected:
- `README.md` stays compact and does not overwhelm the reader
- `docs/OVERVIEW.md` explains structure and flow entry points clearly
- Cross-references between launcher, environment, flow, and CLI docs work

References:
- `README.md`
- `docs/OVERVIEW.md`
- `docs/LAUNCHER.md`
- `docs/ENVIRONMENT.md`
- `docs/CLI.md`

## 2. Environment Resolution

- [ ] `MORPH_RESOLVE_ONLY=1 ./testing/morph_run`
- [ ] `MORPH_DBG=2 MORPH_RESOLVE_ONLY=1 ./testing/morph_run`
- [ ] `MORPH_ENV_FILE=/path/to/test-environment MORPH_RESOLVE_ONLY=1 ./testing/morph_run`

Filter:

```bash
rg -n 'Runtime summary|Environment source|MORPH_DBG|MORPH_X11|MORPH_X11_DISPLAY|Managed hook directory' "$LOG_BASE"/morph*.log
```

Expected:
- Caller environment wins
- User/system environment and wrapper defaults are resolved in a traceable way
- `MORPH_ENV_FILE` is clearly visible or clearly effective

References:
- `config/environment`
- `docs/ENVIRONMENT.md`
- `docs/LAUNCHER.md`

## 3. Config Resolution

- [ ] `MORPH_RESOLVE_ONLY=1 ./testing/morph_run`
- [ ] `MORPH_CONFIG=/path/to/other.conf MORPH_RESOLVE_ONLY=1 ./testing/morph_run`
- [ ] `MORPH_ALLOW_BUILTIN_FALLBACK=1 MORPH_RESOLVE_ONLY=1 ./testing/morph_run`

Filter:

```bash
rg -n 'Config file path:|override from MORPH_CONFIG|user config fallback|system config fallback|builtin fallback' "$LOG_BASE"/morph*.log
```

Expected:
- `MORPH_CONFIG` beats user/system fallback
- User config beats system config
- Builtin fallback only appears with explicit opt-in

References:
- `config/morph.conf`
- `testing/morph.conf`
- `docs/CONFIG.md`
- `docs/CLI.md`

## 4. Dev Wrapper, Native

- [ ] Start `./testing/morph_run` in a native session

Filter:

```bash
rg -n 'Selected session mode|Native Wayland mode detected|Loaded managed portals file|Started xdg-desktop-portal|Startup hook completed|Compositor exited' "$LOG_BASE"/morph-startup.log
```

Expected:
- Session mode is `native`
- Portal setup runs before user session components
- Startup hook completes cleanly afterwards

References:
- `testing/morph_run`
- `config/portals`
- `config/startup.sh`
- `docs/LAUNCHER.md`

## 5. Dev Wrapper, Nested

- [ ] Start `./testing/morph_run` inside a running X11 or Wayland session

Filter:

```bash
rg -n 'Selected session mode|Nested mode detected|Nested clients use WAYLAND_DISPLAY|Skipping .*launch_nested|Compositor exited' "$LOG_BASE"/morph-nested-startup.log
```

Expected:
- Session mode is `nested-x11` or `nested-wayland`
- Nested socket is logged
- Native portal startup is skipped

References:
- `testing/morph_run`
- `scripts/system_startup.sh`
- `docs/LAUNCHER.md`

## 6. Startup / Portal Flow

- [ ] Test user startup with `config/startup.sh` only
- [ ] Test the portal base with `config/portals` only
- [ ] Test optional user override for `~/.config/morph/portals`

Expected:
- `system_startup.sh` runs first
- `morph_start_portals()` comes from the managed base or the user override
- `startup.sh` remains the user entry point for session components

References:
- `scripts/system_startup.sh`
- `config/portals`
- `config/startup.sh`
- `docs/LAUNCHER.md`

## 7. Reload Flow

- [ ] Start a running session
- [ ] `./build/morph --reload-config`
- [ ] Test user reload hook with `reload <cmd ...>`
- [ ] Test user reload hook with `reload_once <cmd ...>`

Filter:

```bash
rg -n 'Starting managed reload hook|Running user reload hook|Reloading |reload_once|Managed reload hook completed' "$LOG_BASE"/morph*.log
```

Expected:
- Reload goes through the managed frame
- User reload hook runs afterwards
- `reload` does not create duplicate shutdown tracker entries

References:
- `scripts/system_reload.sh`
- `config/reload.sh`
- `docs/LAUNCHER.md`
- `docs/CLI.md`

## 8. Shutdown Flow and Tracker

- [ ] End a normal session
- [ ] Simulate an abnormal exit if useful
- [ ] Check `shutdown_list.nfo`

Filter:

```bash
rg -n 'Starting session cleanup|Running user shutdown hook|dynamic session cleanup|Sent SIGTERM|Forcing SIGKILL|Session cleanup completed' "$LOG_BASE"/morph*-shutdown.log
cat "$LOG_BASE"/shutdown_list.nfo
```

Expected:
- `config/shutdown.sh` runs before managed cleanup
- `launch` / `launch_nested` processes land in the tracker
- `launch_nokill` does not land in the tracker

References:
- `scripts/system_shutdown.sh`
- `config/shutdown.sh`
- `scripts/shell-helpers.sh`
- `docs/LAUNCHER.md`

## 9. CLI Reference vs Real Behavior

- [ ] `./build/morph --help`
- [ ] `./build/morph --layout tile`
- [ ] `./build/morph --workspace next`
- [ ] `./build/morph --tile-grid left 3`
- [ ] `./build/morph --no-ipc`

Expected:
- `docs/CLI.md` matches the real `--help` output
- IPC-aware options behave as documented
- Startup-only and IPC-only options stay clearly separated

References:
- `docs/CLI.md`
- `src/main.c:4319`
- `src/main.c:4512`

## 10. Session Desktop Files and Install Paths

- [ ] `grep -nE '^(Name|Exec|TryExec)=' sessions/morph.desktop sessions/morph-dev.desktop`
- [ ] `meson introspect --installed build`
- [ ] `./scripts/dev-install.sh install --print-sudo-help`

Expected:
- `sessions/morph.desktop` uses `morph-session`
- `sessions/morph-dev.desktop` points to the dev wrapper
- Meson installs docs and session files to the new target locations

References:
- `sessions/morph.desktop`
- `sessions/morph-dev.desktop`
- `scripts/dev-install.sh`
- `meson.build`

## 11. Runtime / Dev Uninstall

- [ ] `./scripts/system-uninstall.sh --builddir build`
- [ ] optional `sudo ./scripts/system-uninstall.sh --builddir build --remove`
- [ ] `./scripts/dev-install.sh uninstall --link-launcher`

Expected:
- Print mode removes nothing
- Remove mode removes only unchanged install artifacts
- Real user files stay untouched

References:
- `scripts/system-uninstall.sh`
- `scripts/dev-install.sh`
- `testing/testplan-manual-morph.nfo`

## 12. Documentation Structure

- [ ] Were the old root documents moved into `docs/` sensibly?
- [ ] Is `docs/OVERVIEW.md` the right central entry point?
- [ ] Are `docs/roadmaps/` and `docs/BACKLOG.md` placed logically?

Expected:
- `docs/` contains the current documentation centrally
- `sessions/` contains the session desktop files separately from the rest
- no old parallel structure such as `docs_old/` remains

References:
- `docs/OVERVIEW.md`
- `docs/roadmaps/Roadmap_install-and-user-setup.md`
- `docs/BACKLOG.md`
- `README.md`

## Open Follow-Ups for Later Test Rounds

- [ ] Tighten the Mermaid flow notes in `docs/OVERVIEW.md`
- [ ] Review a few visual flow details separately
- [ ] Optionally add more end-to-end tests for display-manager startup and a real runtime session

## Short Conclusion

If the items above turn green, the branch is not only structurally cleaner, but also functionally consistent across wrapper behavior, config/environment resolution, hooks, install paths, and documentation.
