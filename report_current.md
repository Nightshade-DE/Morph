# morph Test Report for the Feature Branch (as of 2026-08-03)

This report is a practical test checklist for branch `feature/install-and-user-setup`. You can tick items while testing and add notes next to them if useful.

Last checked against branch tip `45e700e` plus the local test fixes for release/debug launcher separation.

The following Linux distributions will be tested in this report:

- Arch (CachyOS Xfce)
- Debian unstable (Siduction Xfce)
- Fedora 44 Xfce

**Note:** The tests don't need any installed version of Morph. All will run **inside** the repository.

## Legend

- `[ ]` Not tested yet
- `[x]` Tested successfully
- `Expected:` What should be visible in a successful test
- `References:` Relevant files or docs for follow-up

## Table of Contents

- [0. Automated Baseline Run](#0-automated-baseline-run)
- [1. README and Doc Entry Points](#1-readme-and-doc-entry-points)
- [2. Environment Resolution](#2-environment-resolution)
- [3. Config Resolution](#3-config-resolution)
- [4. Release and Dev Wrapper, Native](#4-release-and-dev-wrapper-native)
- [5. Dev Wrapper, Nested](#5-dev-wrapper-nested)
- [6. Startup / Portal Flow](#6-startup--portal-flow)
- [7. Reload Flow](#7-reload-flow)
- [8. Shutdown Flow and Tracker](#8-shutdown-flow-and-tracker)
- [9. CLI Reference vs Real Behavior](#9-cli-reference-vs-real-behavior)
- [10. Session Desktop Files](#10-session-desktop-files)
- [11. Runtime / Dev Install](#11-runtime--dev-install)
- [12. Runtime / Dev Uninstall](#12-runtime--dev-uninstall)
- [13. Morph Scripts](#13-morph-scripts)
- [14. Documentation Structure](#14-documentation-structure)
- [Open Follow-Ups for Later Test Rounds](#open-follow-ups-for-later-test-rounds)
- [Short Conclusion](#short-conclusion)

## 0. Automated Baseline Run

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **First build: `(rm -rf build/;) meson setup build`**|
|[x]|[x]|[x]| **Follow-up builds: `meson setup build --reconfigure`** |
|[x]|[x]|[x]| **`meson test -C build --print-errorlogs`** |

Expected:

- Build configures and compiles `morph`
- `config` and `shell-runtime` tests pass

References:

- `meson.build`
- `tests/test_shell_runtime.sh`
- `docs/TESTS.md`

## 1. README and Doc Entry Points

[x] **Open [`README.md`](README.md) and check whether the entry links make sense**

[x] **Open [`docs/OVERVIEW.md`](docs/OVERVIEW.md) and check whether repo structure + flows read as a coherent path**

[x] **Reach `docs/LAUNCHER.md`, [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md), and [`docs/CLI.md`](docs/CLI.md) through the links in [`README.md`](README.md)**

Expected:

- [`README.md`](README.md) stays compact and does not overwhelm the reader
- [`docs/OVERVIEW.md`](docs/OVERVIEW.md) explains structure and flow entry points clearly
- Cross-references between launcher, environment, flow, and CLI docs work

References:

- [`README.md`](README.md)
- [`docs/OVERVIEW.md`](docs/OVERVIEW.md)
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)
- [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md)
- [`docs/CLI.md`](docs/CLI.md)

## 2. Environment Resolution

These checks validate the environment priority chain without starting a real compositor session.
`MORPH_RESOLVE_ONLY=1` stops after wrapper resolution and writes the startup summary.

Set the log base once before running the checks:

```bash
export LOG_BASE="${XDG_STATE_HOME:-$HOME/.local/state}/morph"
```

Use this filter after each check to inspect the relevant startup-summary lines. If the wrapper starts nested, Meson or the launcher may write to `morph-nested-startup.log`; the wildcard keeps both native and nested logs covered.

```bash
rg -n 'Runtime summary|Debug summary|Verbose environment details|Environment source|MORPH_[A-Z0-9_]+|Crash handler enabled|LD_LIBRARY_PATH|Managed system hook directory|Managed system config directory|User config directory|Compositor binary|Resolve-only mode requested' "$LOG_BASE"/morph*.log
```

Delete all log files before starting a new test with

```bash
rm ~/.local/state/morph/*
```

### 2.1. Default release-wrapper resolution

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Default release-wrapper resolution** |

```bash
MORPH_RESOLVE_ONLY=1 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `Runtime summary:` is present.
- `Environment source` shows either a loaded environment file or the defaults-only note.
- `Compositor binary` ends in `/build/morph`.
- `Managed system hook directory` points at the repository `scripts/` directory.
- `Managed system config directory` points at the repository `config/` directory.
- `User config directory` still points at `${XDG_CONFIG_HOME:-$HOME/.config}/morph`.
- The run ends with `Resolve-only mode requested; skipping compositor start after priority checks.`

### 2.2. Debug-wrapper resolution

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Debug-wrapper resolution** |

```bash
MORPH_RESOLVE_ONLY=1 ./testing/morph-session_dbg
```

Expected:

- `Debug summary:` is present.
- `Compositor binary` ends in `/build_dbg/morph`.
- `Managed system hook directory` points at the repository `scripts/` directory.
- `Managed system config directory` points at the repository `testing/config/` directory.
- Repository-local testing config paths are used.
- The run ends with `Resolve-only mode requested; skipping compositor start after priority checks.`

### 2.3. Caller environment wins over environment files

[x] **Caller environment wins over environment files**

```bash
MORPH_DBG=2 MORPH_RESOLVE_ONLY=1 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `MORPH_DBG` resolves to `2`.
- `MORPH_LOG_LEVEL` resolves to `debug`.
- `Crash handler enabled` resolves to `1`.
- Any lower-priority `MORPH_DBG` value from [`config/environment`](config/environment) or a user environment file is ignored.
- `Compositor binary` still points at `/build/morph`.

### 2.4. Custom environment file is loaded as the system environment layer

[x] **Custom environment file is loaded as the system environment layer**

```bash
MORPH_ENV_FILE=/path/to/test-environment MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `Environment source` names `/path/to/test-environment`.
- Values from that file appear unless overridden by caller variables.
- If a user environment file also exists, `Environment source` mentions both files in priority order.
- `Compositor binary` still points at `/build/morph`.

### 2.5. Optional compositor debug variables are forwarded

[x] **Optional compositor debug variables are forwarded**

```bash
MORPH_DBG=1 MORPH_DEBUG_XDG=1 MORPH_DEBUG_XDG_COMMITS=1 \
   MORPH_LOG_KEYS=1 MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `Verbose environment details:` is present because `MORPH_DBG=1` enables the diagnostic block.
- `MORPH_DEBUG_XDG` resolves to `1`.
- `MORPH_DEBUG_XDG_COMMITS` resolves to `1`.
- `MORPH_LOG_KEYS` resolves to `1`.
- Optional `MORPH_*` diagnostic variables that are unset are omitted from the summary.
- Values may come from the caller environment or from the loaded environment files.
- The wrapper reaches resolve-only mode without warnings about unknown variables.
- `Compositor binary` still points at `/build/morph`.

### 2.6. Runtime linker path from the caller is preserved

[x] **Runtime linker path from the caller is preserved**

```bash
LD_LIBRARY_PATH=/tmp/morph-ld-test MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- The wrapper reaches resolve-only mode cleanly.
- `LD_LIBRARY_PATH` in `Runtime summary:` is exactly `/tmp/morph-ld-test`.
- This confirms that the caller-provided value survived environment-file loading and wrapper resolution.
- Release wrapper fallback logic must only auto-fill `LD_LIBRARY_PATH` when it was empty.
- `Compositor binary` still points at `/build/morph`.

References:

- [`config/environment`](config/environment)
- [`testing/config/environment`](testing/config/environment)
- [`docs/ENVIRONMENT.md`](docs/ENVIRONMENT.md)
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)

## 3. Config Resolution

These checks validate the config lookup order without starting the compositor.
This section intentionally covers both wrapper families:
- release/runtime wrapper: `scripts/morph-session`
- development/debug wrapper: `testing/morph-session_dbg`

Use the same `LOG_BASE` value from section 2:

```bash
export LOG_BASE="${XDG_STATE_HOME:-$HOME/.local/state}/morph"
```

Use this filter after each check to inspect the selected config path and fallback reason:

```bash
rg -n 'Config file path|override from MORPH_CONFIG|user config fallback|system config fallback|builtin fallback|User config directory|Resolve-only mode requested' "$LOG_BASE"/morph*.log
```

Delete all log files before starting a new test with

```bash
rm ~/.local/state/morph/*
```

### 3.1. Default release-wrapper config resolution

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Default release-wrapper config resolution** |

Prepare the user config state before this check. The system fallback is only
visible when no readable user config exists at
`${XDG_CONFIG_HOME:-$HOME/.config}/morph/morph.conf`.

[x] **No user config exists - system-fallback**

If `~/.config/morph/morph.conf` exists, temporarily move the directory away:

```bash
mv ~/.config/morph ~/.config/morph.test-backup
```

```bash
MORPH_RESOLVE_ONLY=1 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- Without a readable user config, `Config file path` points at `config/morph.conf`.
- Without a readable user config, the reason is `system config fallback`.
- The run ends with `Resolve-only mode requested; skipping compositor start after priority checks.`

[x] **User config exists - user-fallback**

Create a user config and run the same command again to verify the user fallback:

```bash
mkdir -p ~/.config/morph
cp config/morph.conf ~/.config/morph/morph.conf
```

If you moved an existing directory away, restore it before the second run:

```bash
mv ~/.config/morph.test-backup ~/.config/morph
```

```bash
MORPH_RESOLVE_ONLY=1 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- With a readable user config restored or created, `Config file path` points at the user config.
- With a readable user config restored or created, the reason is `user config fallback`.
- The run ends with `Resolve-only mode requested; skipping compositor start after priority checks.`

[x] **Builtin fallback is opt-in only**

Without any found config and no builtin fallback enabled Morph start will fail hard:

```bash
XDG_CONFIG_HOME=/tmp/morph-empty-config \
   MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/missing-morph.conf" \
   sh ./scripts/morph-session
```

With activated builtin fallback Morph starts:

```bash
XDG_CONFIG_HOME=/tmp/morph-empty-config \
   MORPH_ALLOW_BUILTIN_FALLBACK=1 MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/missing-morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- Missing config is a hard error without `MORPH_ALLOW_BUILTIN_FALLBACK=1`.
- `User config directory` points at `/tmp/morph-empty-config/morph`.
- `Builtin fallback enabled` resolves to `1`.
- With explicit opt-in, the log shows `builtin fallback (no readable config file)`.
- The compositor receives no stale `MORPH_CONFIG` value when builtin fallback is selected.
- Repeat with `testing/morph-session_dbg` when checking dev-wrapper parity.

**Note:** Ensure that in config/environment `MORPH_ALLOW_BUILTIN_FALLBACK=1` commented out or set to 0! 

### 3.2. Default debug-wrapper config resolution

[x] **Default debug-wrapper config resolution**

```bash
MORPH_RESOLVE_ONLY=1 ./testing/morph-session_dbg
```

Expected:

- `Config file path` points at `testing/config/morph.conf`.
- The reason is `system config fallback` unless a user config exists.
- The run ends with `Resolve-only mode requested; skipping compositor start after priority checks.`

### 3.3. Explicit config override wins in both wrappers

[x] **Explicit config override wins in both wrappers**

```bash
MORPH_CONFIG=/path/to/other.conf MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `MORPH_CONFIG` beats user and system fallback.
- `Config file path` points at `/path/to/other.conf`.
- The reason is `override from MORPH_CONFIG`.
- Repeat with `testing/morph-session_dbg` when checking dev-wrapper parity.
- If the override file is not readable, the wrapper exits with `Configured morph config is not readable: ...`.

### 3.5. User XDG config beats system config

[x] **User XDG config beats system config**

```bash
XDG_CONFIG_HOME=/tmp/morph-xdg-test MORPH_RESOLVE_ONLY=1 \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Expected:

- `User config directory` points at `/tmp/morph-xdg-test/morph`.
- User config is selected when `/tmp/morph-xdg-test/morph/morph.conf` exists.
- User fallback follows `${XDG_CONFIG_HOME:-$HOME/.config}/morph/morph.conf`.
- System config remains the managed wrapper fallback.
- System config paths are not reused as the user hook/config directory.
- Repeat with `testing/morph-session_dbg` when checking dev-wrapper parity.

References:

- `config/morph.conf`
- `testing/config/morph.conf`
- [`docs/CONFIG.md`](docs/CONFIG.md)
- [`docs/CLI.md`](docs/CLI.md)

## 4. Release and Dev Wrapper, Native

These checks validate native session startup with the release wrapper first.
The dev wrapper is checked afterwards for parity, but the release path is the
primary user-facing path when a normal build is being tested.
Run them from a TTY or display-manager session where Morph can own the Wayland session.

Prepare both binaries before running this section. The release checks use
`build/morph`; the dev parity check uses `build_dbg/morph` through
`testing/morph-session_dbg`.

```bash
meson setup build --buildtype=release
meson compile -C build
meson setup build_dbg --buildtype=debug
meson compile -C build_dbg
```

If the build directories already exist, reconfigure them instead:

```bash
meson setup build --reconfigure --buildtype=release
meson compile -C build
meson setup build_dbg --reconfigure --buildtype=debug
meson compile -C build_dbg
```

Set the log base once if it is not already set:

```bash
export LOG_BASE="${XDG_STATE_HOME:-$HOME/.local/state}/morph"
```

Use this filter after each check:

```bash
rg -n 'Runtime summary|Debug summary|Selected session mode|Native Wayland mode detected|Loaded managed portals file|Started xdg-desktop-portal|Set portal variables|Startup hook completed|Compositor exited|Compositor binary|MORPH_DBG|MORPH_LOG_LEVEL|Crash handler enabled|Crash log file path|MORPH_X11' "$LOG_BASE"/morph-startup.log
```

Delete all log files before starting a new test with

```bash
rm ~/.local/state/morph/*
```

### 4.1. Start the release wrapper as native session

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Start the release wrapper as native session** |

```bash
MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

For repeated TTY testing, add this alias to `~/.bashrc`:

```bash
alias start41='MORPH_BIN="$PWD/build/morph" MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" MORPH_SYSTEM_CONFIG_DIR="$PWD/config" MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" sh ./scripts/morph-session'
```

Reload `.bashrc` and start the native release session with:

```bash
source ~/.bashrc
start41
```

Expected:

- `Runtime summary:` is present.
- `Selected session mode` is `native` or `native-tty`.
- `Compositor binary` points at `build/morph`.
- Managed portal setup runs before the user startup hook.
- The startup hook completes without aborting the session.

### 4.2. Start the dev wrapper as native parity check

[x] **Start the dev wrapper as native parity check**

```bash
./testing/morph-session_dbg
```

Expected:

- `Debug summary:` is present.
- `Selected session mode` is `native` or `native-tty`.
- `Compositor binary` points at `build_dbg/morph`.
- Managed portal setup and startup hook ordering match the release-wrapper run.

### 4.3. Start native release session with debug logging

[x] **Start native release session with debug logging**

```bash
MORPH_DBG=2 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

For repeated TTY testing, add this alias to `~/.bashrc`:

```bash
alias start43='MORPH_DBG=2 MORPH_BIN="$PWD/build/morph" MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" MORPH_SYSTEM_CONFIG_DIR="$PWD/config" MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" sh ./scripts/morph-session'
```

Reload `.bashrc` and start the native debug-log session with:

```bash
source ~/.bashrc
start43
```

Expected:

- `MORPH_DBG` resolves to `2`.
- `MORPH_LOG_LEVEL` resolves to `debug`.
- `Crash handler enabled` resolves to `1`.
- `Crash log file path` points under `$LOG_BASE`.

### 4.4. Start native release session with X11 bridge disabled

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Start native release session with X11 bridge disabled** |

```bash
MORPH_X11=0 MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

For repeated TTY testing, add this alias to `~/.bashrc`:

```bash
alias start44='MORPH_X11=0 MORPH_BIN="$PWD/build/morph" MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" MORPH_SYSTEM_CONFIG_DIR="$PWD/config" MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" sh ./scripts/morph-session'
```

Reload `.bashrc` and start the native release session with X11 disabled:

```bash
source ~/.bashrc
start44
```

Expected:

- `MORPH_X11` resolves to `0`.
- Native Wayland startup still completes.
- No xwayland-satellite process is started by the wrapper/compositor path. Test this by running `xterm` within `alacritty`:
  Error message: No X11 display found.
- Repeat with `testing/morph-session_dbg` only when checking dev-wrapper parity.

References:

- `scripts/morph-session`
- `testing/morph-session_dbg`
- `config/portals`
- `config/startup.sh`
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)

## 5. Dev Wrapper, Nested

These checks validate the dev launcher when it runs inside an existing desktop session.
`morph-session_dbg` is intentional here as well, because nested smoke checks belong to the development wrapper path.
Nested runs should avoid native-only session services and use a separate startup log.

Before running this section, make sure the shell belongs to an existing X11 or
Wayland desktop session. If neither `WAYLAND_DISPLAY` nor a reachable `DISPLAY`
is available, the wrapper correctly falls back to native DRM/libinput mode and
the nested test is invalid.

```bash
env | rg '^(WAYLAND_DISPLAY|DISPLAY|XDG_RUNTIME_DIR|XAUTHORITY)='
```

For X11 sessions, the wrapper probes the display with `xdpyinfo` first and
`xset` as fallback. At least one of them must be installed and able to query the
current display:

```bash
command -v xdpyinfo || command -v xset
DISPLAY="$DISPLAY" xdpyinfo >/dev/null 2>&1 || DISPLAY="$DISPLAY" xset q >/dev/null 2>&1
```

If this prints `DISPLAY=... is unreachable and no WAYLAND_DISPLAY found; falling
back to native backend.`, stop this section and fix the caller session first.

Set the log base once if it is not already set:

```bash
export LOG_BASE="${XDG_STATE_HOME:-$HOME/.local/state}/morph"
```

Use this filter after each check:

```bash
rg -n 'Debug summary|Selected session mode|Nested mode detected|Nested clients use WAYLAND_DISPLAY|MORPH_X11|xwayland-satellite|Skipping .*launch_nested|Configured user .* hook file is not readable|Loaded managed portals file|Started xdg-desktop-portal|Compositor exited|Compositor binary' "$LOG_BASE"/morph-nested-startup.log "$LOG_BASE"/morph-nested-shutdown.log
```

### 5.1. Start the dev launcher inside an X11 or Wayland session

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Start the dev launcher inside an X11 or Wayland session** |

```bash
./testing/morph-session_dbg
```

Expected:

- `Selected session mode` is `nested-x11` or `nested-wayland`.
- `MORPH_X11` resolves to its default value `1`.
- `xwayland-satellite` starts, and an X11-only client such as `xterm` opens inside the nested Morph window.
- `Nested clients use WAYLAND_DISPLAY=...` is logged.
- Native portal startup is skipped in nested mode.
- The nested startup log is `morph-nested-startup.log`.
- Without a user install, missing optional user hook files are logged as skipped instead of executed as shell commands.

### 5.2. Start nested session with X11 bridge explicitly disabled

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Start nested session with X11 bridge explicitly disabled** |

```bash
MORPH_X11=0 ./testing/morph-session_dbg
```

Expected:

- `MORPH_X11` resolves to `0`.
- The nested Wayland socket is still exported.
- No xwayland-satellite startup is expected.
- The log shows `xwayland-satellite disabled; DISPLAY unset for child clients`.
- X11-only test clients such as `xterm` must not open on the parent desktop.

### 5.3. Start nested session with a fixed X11 bridge display

[x] **Start nested session with a fixed X11 bridge display**

```bash
MORPH_X11=1 MORPH_X11_DISPLAY=:12 ./testing/morph-session_dbg
```

Expected:

- `MORPH_X11` resolves to `1`.
- `MORPH_X11_DISPLAY` resolves to `:12`.
- The wrapper does not replace the caller-provided display with auto `:2..:99`.

References:

- `testing/morph-session_dbg`
- `scripts/system_startup.sh`
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)

## 6. Startup / Portal Flow

These checks validate the managed startup order and the separation between
user startup content and portal setup. They use a temporary XDG configuration
directory, so the real `~/.config/morph` is not modified.

The startup-hook check is nested and can be run from the current graphical
session. The portal checks are native-only and must be run from a TTY or a
display-manager session where Morph owns the Wayland session.

For the release-wrapper checks, build the release binary first if necessary:

```bash
meson compile -C build
```

Each test sets `LOG_BASE` to its temporary log directory. Use this filter after
each check:

```bash
rg -n 'Selected session mode|Config file path|Sourcing default user startup hook|Sourcing user startup hook file from config|Sourcing default user reload hook|Sourcing user reload hook file from config|Sourcing default user shutdown hook|Sourcing user shutdown hook file from config|TEST startup hook reached|TEST portal override reached|Loaded managed portals file|Started xdg-desktop-portal|Set portal variables|Startup hook completed|Managed reload hook completed|Session cleanup completed|Compositor exited' "$LOG_BASE"/morph*.log
```

### 6.1. Test only the user startup hook in a nested session

[x] **Test only the user startup hook in a nested session**

Create a minimal config without a `[hooks]` startup entry. This intentionally
tests the XDG fallback path, where the managed hook layer sources
`$MORPH_USER_CONFIG_DIR/startup.sh`. The hook writes a marker and a log entry; it
does not start dunst, swayidle, a bar, or any other session component.

```bash
HOOK_ROOT=$(mktemp -d /tmp/morph-hook-test.XXXXXX)
mkdir -p "$HOOK_ROOT/xdg/morph" "$HOOK_ROOT/state" "$HOOK_ROOT/log"
export LOG_BASE="$HOOK_ROOT/log"

cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No [hooks] entries here: this test exercises the managed XDG fallback hooks.
EOF

cat > "$HOOK_ROOT/xdg/morph/startup.sh" <<'EOF'
touch "$MORPH_HOOK_TEST_ROOT/startup.marker"
log_startup INFO "TEST startup hook reached."
EOF

: > "$HOOK_ROOT/xdg/morph/shutdown.sh"
: > "$HOOK_ROOT/xdg/morph/reload.sh"
chmod +x "$HOOK_ROOT/xdg/morph/"*.sh
```

Start the release wrapper from the terminal that owns the current graphical
session:

```bash
MORPH_HOOK_TEST_ROOT="$HOOK_ROOT" \
   XDG_CONFIG_HOME="$HOOK_ROOT/xdg" \
   XDG_STATE_HOME="$HOOK_ROOT/state" \
   MORPH_LOG_DIR="$HOOK_ROOT/log" \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Stop the nested session with `Ctrl+C` after the startup log has been
written. Then verify the hook itself and the managed order:

```bash
test -f "$HOOK_ROOT/startup.marker" && echo "startup marker: OK"
rg -n 'Nested mode detected|Sourcing default user startup hook|TEST startup hook reached|Startup hook completed' "$LOG_BASE/morph-nested-startup.log"
rm -rf "$HOOK_ROOT"
```

Expected for:
- `Config file path`: the temporary user config is selected.
- `Nested mode detected`: `system_startup.sh` prepared the nested client environment.
- `Sourcing default user startup hook`: no `[hooks] startup` entry was configured, so the managed XDG fallback was used.
- `TEST startup hook reached.`: only the temporary user startup hook ran.
- `startup marker: OK`: the hook was sourced successfully.
- No portal startup messages: portals are skipped in nested mode.
- No dunst, swayidle, or other session component is launched by this test.

### 6.2. Test the managed portal base without a user portal override

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Test the managed portal base without a user portal override** |

This check must run in a **native session**. Ensure the managed portal file
exists and that the resolver can find the default portal executable set before
starting:

```bash
test -r config/portals
for dir in /usr/libexec /usr/lib /usr/local/libexec /usr/local/lib; do
    if [ -x "$dir/xdg-desktop-portal" ] && \
       [ -x "$dir/xdg-desktop-portal-wlr" ] && \
       [ -x "$dir/xdg-desktop-portal-gtk" ]; then
        printf 'portal executable directory: %s\n' "$dir"
        break
    fi
done
```

If the executables live in a custom directory, set `MORPH_PORTAL_LIBEXEC_DIR`
for the wrapper run.

Use a temporary user config with no `portals` override and no `[hooks] startup`
entry. The fallback startup hook is empty so this check focuses on portal
initialization:

```bash
HOOK_ROOT=$(mktemp -d /tmp/morph-portal-test.XXXXXX)
mkdir -p "$HOOK_ROOT/xdg/morph" "$HOOK_ROOT/state" "$HOOK_ROOT/log"
export LOG_BASE="$HOOK_ROOT/log"

cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No [hooks] startup entry here: this test exercises the managed XDG fallback hook.
[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit
EOF
: > "$HOOK_ROOT/xdg/morph/startup.sh"
chmod +x "$HOOK_ROOT/xdg/morph/startup.sh"
```

It can be added as a function to `~/.bashrc`:

```bash
thook62() {
    export HOOK_ROOT
    HOOK_ROOT=$(mktemp -d /tmp/morph-portal-test.XXXXXX)

    mkdir -p \
        "$HOOK_ROOT/xdg/morph" \
        "$HOOK_ROOT/state" \
        "$HOOK_ROOT/log"

    export LOG_BASE="$HOOK_ROOT/log"

    cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No [hooks] startup entry here: this test exercises the managed XDG fallback hook.
[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit
EOF

    : > "$HOOK_ROOT/xdg/morph/startup.sh"
    chmod +x "$HOOK_ROOT/xdg/morph/startup.sh"

    printf 'HOOK_ROOT=%s\nLOG_BASE=%s\n' "$HOOK_ROOT" "$LOG_BASE"
}
```

Reload `.bashrc` and run function:

```bash
source ~/.bashrc
thook62
```

Start the release wrapper directly from the native TTY/display-manager
session:

```bash
XDG_CONFIG_HOME="$HOOK_ROOT/xdg" \
   XDG_STATE_HOME="$HOOK_ROOT/state" \
   MORPH_LOG_DIR="$HOOK_ROOT/log" \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Here the alias for `~/.bashrc`:

```bash
alias start62='XDG_CONFIG_HOME="$HOOK_ROOT/xdg" XDG_STATE_HOME="$HOOK_ROOT/state" MORPH_LOG_DIR="$HOOK_ROOT/log" MORPH_BIN="$PWD/build/morph" MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" MORPH_SYSTEM_CONFIG_DIR="$PWD/config" MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" sh ./scripts/morph-session'
```

Reload `.bashrc` and start the test session with:

```bash
source ~/.bashrc
start62
```

After the session starts, inspect the log and stop the session normally:

```bash
rg -n 'Native Wayland mode detected|Portal executable directory|Loaded managed portals file|Killed stale xdg-desktop-portal|Started xdg-desktop-portal instances|Set portal variables' "$LOG_BASE/morph-startup.log"
rm -rf "$HOOK_ROOT"
```

For repeated log checks, add this alias to `~/.bashrc`:

```bash
alias check62='rg -n "Native Wayland mode detected|Portal executable directory|Loaded managed portals file|Killed stale xdg-desktop-portal|Started xdg-desktop-portal instances|Set portal variables" "$LOG_BASE/morph-startup.log"'
```

Reload `.bashrc` and inspect the log with:

```bash
source ~/.bashrc
check62
```

Expected for:
- `Native Wayland mode detected`: the native startup path was selected.
- `Portal executable directory`: the managed resolver selected the distro-specific portal directory.
- `Loaded managed portals file`: the managed `config/portals` base was sourced.
- `Started xdg-desktop-portal instances.`: the three configured portal processes were requested.
- `Set portal variables`: `QT_QPA_PLATFORM`, `GDK_BACKEND`, and `GTK_USE_PORTAL` are present.
- No user portal override message: the temporary user directory contains no `portals` file.

### 6.3. Test a user portal override separately

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Test a user portal override separately** |

Repeat the preparation commands from test 2, then add only a user `portals`
file that replaces `morph_start_portals()`:

```bash
cat > "$HOOK_ROOT/xdg/morph/portals" <<'EOF'
morph_start_portals() {
      log_startup INFO "TEST portal override reached."
}
EOF
```

Or add it as a function to `~/.bashrc`:

```bash
thook63() {
    local root

    root=$(mktemp -d /tmp/morph-portal-test.XXXXXX) || return 1

    HOOK_ROOT="$root"
    LOG_BASE="$HOOK_ROOT/log"
    export HOOK_ROOT LOG_BASE

    mkdir -p \
        "$HOOK_ROOT/xdg/morph" \
        "$HOOK_ROOT/state" \
        "$HOOK_ROOT/log" || return 1

    cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No [hooks] startup entry here: this test exercises the managed XDG fallback hook.
[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit
EOF

    : > "$HOOK_ROOT/xdg/morph/startup.sh"
    chmod +x "$HOOK_ROOT/xdg/morph/startup.sh"

    cat > "$HOOK_ROOT/xdg/morph/portals" <<'EOF'
morph_start_portals() {
    log_startup INFO "TEST portal override reached."
}
EOF

    printf 'HOOK_ROOT=%s\nLOG_BASE=%s\n' "$HOOK_ROOT" "$LOG_BASE"
}
```

Reload `.bashrc` and run function:

```bash
source ~/.bashrc
thook63
```

Run the same native release-wrapper command from test 2. Then inspect the
startup log:

```bash
rg -n 'Loaded managed portals file|TEST portal override reached|Started xdg-desktop-portal instances|Set portal variables' "$LOG_BASE/morph-startup.log"
rm -rf "$HOOK_ROOT"
```

For repeated log checks, add this alias to `~/.bashrc`:

```bash
alias check63='rg -n "Loaded managed portals file|TEST portal override reached|Started xdg-desktop-portal instances|Set portal variables" "$LOG_BASE/morph-startup.log"'
```

Reload `.bashrc` and inspect the log with:

```bash
source ~/.bashrc
check63
```

Expected for:
- `Loaded managed portals file`: the managed base remains the first layer.
- `TEST portal override reached.`: the user override replaced the base function.
- No `Started xdg-desktop-portal instances.` or `Set portal variables`: the test override intentionally does not call the base implementation.
- The user override affects portal setup only; it does not replace the fallback `startup.sh`.

### 6.4. Repeat the startup-hook check with the debug wrapper

[x] **Repeat the startup-hook check with the debug wrapper. do the check in *Nested mode* only.**

Create a minimal config without a `[hooks]` startup entry. This intentionally
tests the XDG fallback path, where the managed hook layer sources
`$MORPH_USER_CONFIG_DIR/startup.sh`. The hook writes a marker and a log entry; it
does not start dunst, swayidle, a bar, or any other session component.

```bash
HOOK_ROOT=$(mktemp -d /tmp/morph-hook-test.XXXXXX)
mkdir -p "$HOOK_ROOT/xdg/morph" "$HOOK_ROOT/state" "$HOOK_ROOT/log"
export LOG_BASE="$HOOK_ROOT/log"

cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No [hooks] entries here: this test exercises the managed XDG fallback hooks.
EOF

cat > "$HOOK_ROOT/xdg/morph/startup.sh" <<'EOF'
touch "$MORPH_HOOK_TEST_ROOT/startup.marker"
log_startup INFO "TEST startup hook reached."
EOF

: > "$HOOK_ROOT/xdg/morph/shutdown.sh"
: > "$HOOK_ROOT/xdg/morph/reload.sh"
chmod +x "$HOOK_ROOT/xdg/morph/"*.sh
```

Start the debug wrapper from the terminal that owns the current graphical
session:

```bash
MORPH_HOOK_TEST_ROOT="$HOOK_ROOT" \
   XDG_CONFIG_HOME="$HOOK_ROOT/xdg" \
   XDG_STATE_HOME="$HOOK_ROOT/state" \
   MORPH_LOG_DIR="$HOOK_ROOT/log" \
   ./testing/morph-session_dbg
```

Stop the nested session with `Ctrl+C` after the startup log has been
written. Then verify the hook itself and the managed order:

```bash
test -f "$HOOK_ROOT/startup.marker" && echo "startup marker: OK"
rg -n 'Nested mode detected|Sourcing default user startup hook|TEST startup hook reached|Startup hook completed' "$LOG_BASE/morph-nested-startup.log"
rm -rf "$HOOK_ROOT"
```

Expected for:
- `Debug summary`: the debug wrapper reports its runtime summary.
- `Compositor binary`: the path ends in `build_dbg/morph`.
- The same startup marker and `TEST startup hook reached.` message appear.
- Hook ordering and nested portal behavior match the release-wrapper check.

References:

- `scripts/system_startup.sh`
- `scripts/system_reload.sh`
- `scripts/system_shutdown.sh`
- `config/portals`
- `config/startup.sh`
- `config/reload.sh`
- `config/shutdown.sh`
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)
- [`docs/CONFIG.md`](docs/CONFIG.md)

## 7. Reload Flow

The reload hook template lives at `config/reload.sh`. This section is a
**nested reload test by default**: reload does not need native DRM/session
ownership, and the nested path keeps the second terminal reachable in the parent
desktop.

The test uses a modified setup from section 6.1 but with a quit keybinding `Ctrl+Super+Alt + Esc`.
The temporary config intentionally has no `[hooks] reload` entry at first, so the
first reload uses the default XDG fallback hook at `$HOOK_ROOT/xdg/morph/reload.sh`.
A later subtest adds `reload = ${MORPH_USER_CONFIG_DIR}/reload.sh` to verify the
explicit config-path branch separately. The reload command talks to the already
running compositor through `$XDG_RUNTIME_DIR/morph-ipc.sock`.

Terminal layout:
- **Terminal A:** host terminal that prepares `HOOK_ROOT` and starts Morph nested.
- **Terminal B:** another host terminal in the same graphical login/session, same Unix user, and same `XDG_RUNTIME_DIR`.
- Do not open Terminal B inside Morph for this test; if the reload breaks, the test terminal should remain outside the compositor under test.
- For an optional native reload check, Terminal B can be another TTY or SSH session as the same user, but it must see the same `$XDG_RUNTIME_DIR/morph-ipc.sock`.

### 7.1. Start a running session first

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Start a running session first** |

In Terminal A, repeat the temporary preparation from section 6.1.:

```bash
HOOK_ROOT=$(mktemp -d /tmp/morph-hook-test.XXXXXX)
mkdir -p "$HOOK_ROOT/xdg/morph" "$HOOK_ROOT/state" "$HOOK_ROOT/log"
export LOG_BASE="$HOOK_ROOT/log"

cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
# No lifecycle [hooks] entries here: this test exercises the managed XDG fallback hooks.
[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit
EOF

cat > "$HOOK_ROOT/xdg/morph/startup.sh" <<'EOF'
touch "$MORPH_HOOK_TEST_ROOT/startup.marker"
log_startup INFO "TEST startup hook reached."
EOF

: > "$HOOK_ROOT/xdg/morph/shutdown.sh"
: > "$HOOK_ROOT/xdg/morph/reload.sh"
chmod +x "$HOOK_ROOT/xdg/morph/"*.sh
```

Before starting Morph, print the values that Terminal B must import:

```bash
printf 'export HOOK_ROOT=%q\nexport LOG_BASE=%q\n' "$HOOK_ROOT" "$LOG_BASE"
```

Then start the nested Morph session with the section 6.1 release-wrapper command
and leave it running:

```bash
MORPH_HOOK_TEST_ROOT="$HOOK_ROOT" \
   XDG_CONFIG_HOME="$HOOK_ROOT/xdg" \
   XDG_STATE_HOME="$HOOK_ROOT/state" \
   MORPH_LOG_DIR="$HOOK_ROOT/log" \
   MORPH_BIN="$PWD/build/morph" \
   MORPH_SYSTEM_HOOK_DIR="$PWD/scripts" \
   MORPH_SYSTEM_CONFIG_DIR="$PWD/config" \
   MORPH_SYSTEM_CONFIG_FILE="$PWD/config/morph.conf" \
   sh ./scripts/morph-session
```

Do not stop it with `Ctrl+C` or `Ctrl+Super+Alt + Esc` or until after the reload check.
Do not start a second wrapper for the reload check.

Expected:

- Reload is tested against the already running nested compositor, not by starting a new wrapper.
- Terminal B will run outside Morph, in the parent graphical session.
- Terminal B can import the printed `HOOK_ROOT` and `LOG_BASE` values.
- `reload = reload.sh` is intentionally not used: the current helper treats that as a relative configured path, not as the managed XDG fallback.

### 7.2. Prepare the user reload hook in a second terminal

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Prepare the user reload hook in a second terminal** |

In Terminal B, stay in the same repository checkout and import the `HOOK_ROOT`
and `LOG_BASE` values printed by Terminal A. Example:

```bash
cd /home/tf/workset/morph-install-and-user-setup
export HOOK_ROOT=/tmp/morph-hook-test.XXXXXX
export LOG_BASE="$HOOK_ROOT/log"
test -d "$HOOK_ROOT/xdg/morph"
```

Then replace the reload hook while the nested Morph session is still running:

```bash
cat > "$HOOK_ROOT/xdg/morph/reload.sh" <<'EOF'
log_message INFO "TEST reload hook reached."
reload true
reload_once true
log_message INFO "TEST reload hook completed."
EOF
chmod +x "$HOOK_ROOT/xdg/morph/reload.sh"
```

Expected:

- Terminal B imported the same `HOOK_ROOT` that Terminal A used to start Morph.
- `$HOOK_ROOT/xdg/morph/reload.sh` exists and is executable.
- No edit to `config/reload.sh` is required for this manual test.

### 7.3. Trigger reload from the second terminal

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Trigger reload from the second terminal** |

Run this in Terminal B. The socket check confirms that the command can reach the
running compositor before triggering reload:

```bash
test -S "$XDG_RUNTIME_DIR/morph-ipc.sock"
./build/morph --reload-config
```

Filter:

```bash
rg -n 'Starting managed reload hook|Sourcing default user reload hook|TEST reload hook reached|TEST reload hook completed|Managed reload hook completed' "$LOG_BASE"/morph*.log
```

Expected:

- Terminal B can see `$XDG_RUNTIME_DIR/morph-ipc.sock`.
- The log contains `Starting managed reload hook`.
- The log contains `Sourcing default user reload hook`.
- The log contains `TEST reload hook reached`.
- The log contains `TEST reload hook completed`.
- The log contains `Managed reload hook completed`.
- The session keeps running after `./build/morph --reload-config`.

[x] **Test user reload hook with `reload <cmd ...>`**

Inspect the same reload log after 7.3:

```bash
rg -n 'Reloading true|No running instance found for reload: true|Stopped running instance for reload: true' "$LOG_BASE"/morph*.log
```

Expected:

- The log contains `Reloading true`.
- The log contains either `No running instance found for reload: true` or `Stopped running instance for reload: true`.
- The helper is available inside `reload.sh`, proving that the hook was sourced through the managed helper layer.

[x] **Test user reload hook with `reload_once <cmd ...>`**

Inspect the same reload log after 7.3:

```bash
rg -n 'Starting component through reload_once: true|Skipping reload_once for already running component: true' "$LOG_BASE"/morph*.log
```

Expected:

- The log contains either `Starting component through reload_once: true` or `Skipping reload_once for already running component: true`.
- Repeating `./build/morph --reload-config` must not create duplicate shutdown tracker entries in `shutdown_list.nfo` for the same component. See 8.2 for more infos.

[x] **Test reload hook config path `${MORPH_USER_CONFIG_DIR}/reload.sh`**

This is a second reload-path check. It changes only the temporary config so the
reload hook is explicit instead of using the default fallback path.

In Terminal B, update the temporary config:

```bash
cat > "$HOOK_ROOT/xdg/morph/morph.conf" <<'EOF'
[hooks]
reload = ${MORPH_USER_CONFIG_DIR}/reload.sh
[bind]
mods = Ctrl+Super+Alt
key = Escape
action = quit
EOF
```

Trigger reload again from Terminal B:

```bash
./build/morph --reload-config
```

Filter:

```bash
rg -n 'Sourcing default user reload hook|Sourcing user reload hook file from config|TEST reload hook reached|TEST reload hook completed|Managed reload hook completed' "$LOG_BASE"/morph*.log
```

Expected:

- The new reload uses `Sourcing user reload hook file from config`.
- Earlier fallback runs may still show `Sourcing default user reload hook` in the same log file; compare the newest matching lines.
- Helper functions such as `reload`, `reload_once`, `launch`, `launch_nokill`, and `log_message` remain available.

References:

- `scripts/system_reload.sh`
- `config/reload.sh`
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)
- [`docs/CLI.md`](docs/CLI.md)

## 8. Shutdown Flow and Tracker

Continue with the still running nested session from chapter 7. Terminal A keeps the
wrapper session alive, and Terminal B keeps the exported `HOOK_ROOT` and
`LOG_BASE` values from 7.2.

Do not start a second wrapper for the normal shutdown test. The purpose of this
chapter is to verify that the session which already handled reload also shuts
down through the managed cleanup path.

### 8.1. End the running nested session normally

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **End the running chapter 7 session normally** |

In the nested Morph session from chapter 7, press the configured quit binding:

```text
Ctrl+Super+Alt + Esc
```

Expected:

- The running chapter 7 session exits without using `Ctrl+C`.
- Terminal A returns to the shell after the wrapper has completed shutdown.
- The shutdown hook and managed cleanup run as part of the same session.
- The test uses the release wrapper `./scripts/morph-session`, not the debug wrapper.

### 8.2. Inspect the shutdown log

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Inspect the managed shutdown log** |

Run this in Terminal B after 8.1 has returned Terminal A to the shell:

```bash
rg -n 'Starting session cleanup|Sourcing default user shutdown hook|Nested mode detected|Performing dynamic session cleanup|No programs registered to kill|Sent SIGTERM|Forcing SIGKILL|Session cleanup completed' "$LOG_BASE"/morph*-shutdown.log
```

Expected:

- The log contains `Starting session cleanup`.
- The log contains `Sourcing default user shutdown hook` because chapter 7 does not configure an explicit shutdown hook.
- The log contains `Nested mode detected` for the nested wrapper path.
- The log contains either `Performing dynamic session cleanup` or `No programs registered to kill`.
- The log contains `Session cleanup completed`.
- `Sent SIGTERM` or `Forcing SIGKILL` may be absent when the registered helper process already exited.

### 8.3. Check `shutdown_list.nfo`

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Check the shutdown tracker file** |

Run this in Terminal B:

```bash
cat "$LOG_BASE"/shutdown_list.nfo
```

Expected:

- The tracker file exists under the chapter 7 `LOG_BASE` directory.
- Components registered through managed helpers such as `reload` or `reload_once` can appear in the tracker.
- Repeated reloads must not create duplicate tracker entries for the same `reload_once` component.
- Commands started with `launch_nokill` must not be added to the tracker.
- For current test no `shutdown_list.nfo` was created.

### 8.4. Optional abnormal stop test

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Simulate an abnormal stop if needed** |

Use this only after the normal shutdown test has been completed. Start a fresh
chapter 7 session first if you want to compare abnormal-stop behavior against the
normal managed shutdown path.

Example abnormal stop from Terminal A:

```text
Ctrl+C
```

Expected:

- The result is documented separately from the normal shutdown result.
- Missing normal shutdown-hook lines are acceptable for this optional abnormal-stop case.
- Any remaining helper cleanup behavior is compared against the normal 8.1 to 8.3 result.

### 8.5. Clean up temporary files

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Remove the temporary hook test directory** |

Run this in Terminal B after all chapter 8 checks are done:

```bash
rm -rf "$HOOK_ROOT"
unset HOOK_ROOT LOG_BASE
```

Expected:

- The temporary hook test directory is removed.
- `HOOK_ROOT` and `LOG_BASE` are no longer set in Terminal B.

References:

- `scripts/system_shutdown.sh`
- `config/shutdown.sh`
- `scripts/shell-helpers.sh`
- [`docs/LAUNCHER.md`](docs/LAUNCHER.md)

## 9. CLI Reference vs Real Behavior

This chapter checks the bare `morph` binary CLI against [`docs/CLI.md`](docs/CLI.md).
It does not test wrapper environment setup; wrapper behavior is covered in earlier
chapters.

Run the IPC success checks against a running nested Morph session. If chapter 8
already stopped the chapter 7 session, start a fresh chapter 7 session first and
keep Terminal B in the same repository checkout.

### 9.1. Compare `--help` with `docs/CLI.md`

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Compare CLI help output with the documented CLI reference** |

Run this in Terminal B:

```bash
./build/morph --help | tee /tmp/morph-cli-help.txt
rg -n -- '--help|--config|--allow-builtin-fallback|--layout|--scroll|--tile-move|--tile-grid|--scroll-move|--workspace|--workspace-move|--reload-config|--ipc|--no-ipc|--log-level|--quiet|--verbose|--log-file|--crash-log|--no-crash-handler|--crash-test' /tmp/morph-cli-help.txt docs/CLI.md
```

Expected:

- `./build/morph --help` exits with status `0`.
- Every option printed by `--help` is documented in [`docs/CLI.md`](docs/CLI.md).
- [`docs/CLI.md`](docs/CLI.md) does not document removed or renamed options.
- Startup-only options and IPC-aware options are visibly separated in the documentation.

### 9.2. Test IPC-only commands without a running compositor

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Confirm IPC-only commands fail cleanly without a running session** |

Run this only when no Morph session is currently listening on the IPC socket:

```bash
test ! -S "$XDG_RUNTIME_DIR/morph-ipc.sock"
set +e
./build/morph --workspace next; echo "workspace_exit=$?"
./build/morph --tile-grid left 3; echo "tile_grid_exit=$?"
./build/morph --reload-config; echo "reload_exit=$?"
set -e
```

Expected:

- The socket check confirms that no Morph IPC socket is available.
- `--workspace next` exits with status `1` and reports that no running Morph IPC target was found.
- `--tile-grid left 3` exits with status `1` and reports that no running Morph IPC target was found.
- `--reload-config` exits with status `1` and reports that no running Morph IPC target was found.
- These commands do not start a new compositor instance by themselves.

### 9.3. Test IPC-only commands with a running compositor

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Confirm IPC-only commands reach a running nested session** |

Use a running nested Morph session from chapter 7, or start a fresh chapter 7
session if chapter 8 already stopped it. Then run this in Terminal B:

```bash
test -S "$XDG_RUNTIME_DIR/morph-ipc.sock"
./build/morph --workspace next
./build/morph --tile-grid left 3
./build/morph --reload-config
```

Optional log filter when `LOG_BASE` is still set from the hook tests:

```bash
rg -n 'ipc:|reload: loaded config|Starting managed reload hook|Managed reload hook completed' "$LOG_BASE"/morph*.log
```

Expected:

- The socket check confirms that the running nested Morph session exposes IPC.
- `--workspace next` exits with status `0`.
- `--tile-grid left 3` exits with status `0` even when there is no focused tiled window to move.
- `--reload-config` exits with status `0` and the log shows the managed reload path.
- No second compositor instance is started for these IPC-only commands.

### 9.4. Test layout command behavior

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Confirm `--layout` uses IPC when a session is already running** |

Run this only while the nested Morph session from 9.3 is still running:

```bash
test -S "$XDG_RUNTIME_DIR/morph-ipc.sock"
./build/morph --layout tile
./build/morph --layout scroll
./build/morph --layout stack
```

Expected:

- Each `--layout` command exits with status `0`.
- The running session receives the layout changes through IPC.
- No additional compositor window appears.
- This confirms the documented special behavior: `--layout` is IPC-aware, but unlike `--workspace` or `--tile-grid`, it may start a compositor when no IPC target exists.

### 9.5. Check `--no-ipc` as startup-only option

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Confirm `--no-ipc` is documented as startup-only behavior** |

Do not run `./build/morph --no-ipc` inside an already running manual test session
unless you intentionally want to start a separate isolated compositor instance.
For this report, check the documentation and help output instead:

```bash
rg -n -- '--no-ipc|Disable IPC socket|startup-only|Startup-only' /tmp/morph-cli-help.txt docs/CLI.md
```

Expected:

- `--no-ipc` is present in `--help`.
- [`docs/CLI.md`](docs/CLI.md) describes `--no-ipc` as disabling socket creation for the instance being started.
- The report keeps `--no-ipc` separate from runtime IPC commands.

References:

- [`docs/CLI.md`](docs/CLI.md)
- `src/main.c:6564`
- `src/main.c:6747`
- `src/main.c:7021`
- `src/main.c:7079`

## 10. Session Desktop Files

This chapter only checks the session desktop entry wiring. It does not install
anything.

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Check runtime and debug session desktop entries** |

```bash
grep -nE '^(Name|Exec|TryExec)=' sessions/morph.desktop sessions/morph_dbg.desktop
```

Expected:

- `sessions/morph.desktop` has `Name=Morph`.
- `sessions/morph.desktop` uses `Exec=morph-session` and `TryExec=morph-session`.
- `sessions/morph_dbg.desktop` has `Name=Morph (Dev)`.
- `sessions/morph_dbg.desktop` uses `/usr/bin/morph-session_dbg` for `Exec` and `TryExec`.
- The runtime desktop file points to the release wrapper; the debug desktop file points to the dev wrapper.

References:

- `sessions/morph.desktop`
- `sessions/morph_dbg.desktop`

## 11. Runtime / Dev Install

This chapter contains real install flows and dry-run previews. Run the real
install commands only on a test system where installing under `/usr`, `/etc`,
and user config paths is intended.

Clarification:
- `meson introspect --installed build` installs nothing. It only prints Meson's install manifest.
- `./scripts/dev-install.sh install --print-sudo-help` is **not** a dry run. It creates the normal dev user symlinks, then prints additional sudo guidance.
- Use `./scripts/dev-install.sh install --dry --print-sudo-help` when you only want to inspect the dev install plan.

### 11.1. Inspect runtime install targets without installing

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Inspect Meson runtime install manifest** |

With `jq` installed:

```bash
meson introspect --installed build | jq -r 'to_entries | sort_by(.value)[] | "\(.value) <= \(.key)"'
```

If `jq` is not installed, use the Python fallback:

```bash
meson introspect --installed build | python3 -c '
import json, sys
data = json.load(sys.stdin)
for src, dst in sorted(data.items(), key=lambda item: item[1]):
    print(f"{dst} <= {src}")
'
```

Expected:

- The command exits with status `0`.
- The formatted output maps build/source files to install targets.
- Runtime targets include `/usr/bin/morph`, `/usr/bin/morph-session`, `/etc/morph/*`, `/usr/share/wayland-sessions/morph.desktop`, icons, and docs.
- `README.md` installs to `/usr/share/doc/morph/README.md`.
- Project docs install under `/usr/share/doc/morph/docs/`.
- The example config installs as `/usr/share/doc/morph/docs/morph.conf.example` from `docs/morph.conf.example`, not as `testing/config/morph.conf`.
- No files are installed or changed by this command.

### 11.2. Install runtime files with Meson

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Install runtime files with Meson** |

The following command installs morph in the system:

```bash
sudo meson install -C build
```

Expected:

- Runtime binary is installed as `/usr/bin/morph`.
- Runtime wrapper is installed as `/usr/bin/morph-session`.
- Managed runtime files are installed under `/etc/morph/`.
- Runtime session file is installed as `/usr/share/wayland-sessions/morph.desktop`.
- Runtime docs and icons are installed to their Meson target paths.
- Documentation keeps the repository-style `docs/` subdirectory under `/usr/share/doc/morph/docs/`.
- No `testing/config/morph.conf` file is installed as `/usr/share/doc/morph/docs/morph.conf`.

### 11.3. Test installed runtime session

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Test runtime session through login manager and shell** |

Runtime through login manager:

```bash
# Log out, select the "Morph" Wayland session in the login manager, then log in.
```

Runtime from a real TTY:

```bash
morph-session
```

Runtime from a terminal inside an existing graphical Wayland session, forcing the
native path for this process:

```bash
env -u WAYLAND_DISPLAY morph-session
```

Expected:

- The runtime login-manager entry starts the installed release wrapper from `/usr/bin/morph-session`.
- The runtime TTY start also uses the installed release wrapper and starts a native Morph session.
- The runtime `env -u WAYLAND_DISPLAY` start uses the same wrapper but removes the parent Wayland hint for this command.
- Session can be exited through the configured quit binding; no nested parent compositor is required for this check.
- Open ~/.local/state/morph/morph-startup.log and search for `Config File Path:`. If no ~/.config/morph/morph.conf available - system fallback to /etc/morph/morph.conf.

References:

- `meson.build`
- `scripts/system-uninstall.sh`
- `sessions/morph.desktop`

### 11.4. Inspect dev install targets without installing

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Preview dev install plan** |

```bash
./scripts/dev-install.sh install --dry --print-sudo-help
```

Expected:

- The command prints dev install targets and sudo guidance.
- No symlinks, desktop files, icons, or user config files are created.
- The output makes clear which files would be linked under `${XDG_CONFIG_HOME:-$HOME/.config}/morph`.

### 11.5. Install dev user links with `dev-install.sh`

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Install dev user links** |

```bash
./scripts/dev-install.sh install --link-launcher --desktop-local
```

Expected:

- Dev config symlinks are created under `${XDG_CONFIG_HOME:-$HOME/.config}/morph` when no real files already exist.
- Existing real user files are left untouched and reported as warnings.
- `--link-launcher` creates `~/.local/bin/morph-session_dbg` when possible.
- `--desktop-local` installs the debug session desktop file and icon under `~/.local/share`.
- No system-visible `/usr` debug links are created unless `--system-links` is explicitly used.

### 11.6. Install dev session into system paths

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Install display-manager-visible dev session links under `/usr`** |

Build the debug binary first if it does not exist yet:

```bash
if [ -f build_dbg/meson-private/coredata.dat ]; then
    meson setup build_dbg --reconfigure --buildtype debug -Dstrip=false
else
    meson setup build_dbg --buildtype debug -Dstrip=false
fi
meson compile -C build_dbg
```

Then install the system-visible dev links:

```bash
./scripts/dev-install.sh install --system-links
```

Expected:

- `/usr/bin/morph_dbg` is a symlink to `build_dbg/morph` in this checkout.
- `/usr/bin/morph-session_dbg` is a symlink to `testing/morph-session_dbg` in this checkout.
- `/usr/share/wayland-sessions/morph_dbg.desktop` is a symlink to `sessions/morph_dbg.desktop` in this checkout.
- `/usr/share/icons/hicolor/scalable/apps/morph_dbg.svg` is a symlink to `assets/icons/morph_dbg.svg` in this checkout.
- The dev session becomes visible to display managers that read `/usr/share/wayland-sessions`.
- This does not install runtime `/etc/morph` files; it keeps using repository-local debug runtime paths.

**General Note:**
A login manager needs access to the session files. Therefore, you need to grant execute permissions to it (this allows traversing, but not listing files):
```bash
chmod o+x /home/<your_home>
```

### 11.6. Test installed dev session

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Test dev sessions through login manager and shell** |

Dev through login manager:

```bash
# Log out, select the "Morph (Dev)" Wayland session in the login manager, then log in.
```

Dev from a real TTY:

```bash
morph-session_dbg
```

Dev from a terminal inside an existing graphical Wayland session, forcing the
native path for this process:

```bash
env -u WAYLAND_DISPLAY morph-session_dbg
```

Expected:

- The dev login-manager entry starts the installed debug wrapper from `/usr/bin/morph-session_dbg`.
- The dev TTY start also uses the installed debug wrapper and reports the debug runtime summary.
- The dev `env -u WAYLAND_DISPLAY` start uses the same debug wrapper but removes the parent Wayland hint for this command.
- Session can be exited through the configured quit binding; no nested parent compositor is required for this check.

References:

- `meson.build`
- `scripts/dev-install.sh`
- `scripts/system-uninstall.sh`
- `sessions/morph.desktop`
- `sessions/morph_dbg.desktop`

## 12. Runtime / Dev Uninstall

This chapter separates runtime uninstall from dev uninstall. Prefer the
conservative helper when validating installed runtime artifacts, because it
checks Meson's manifest and leaves changed files untouched.

### 12.1. Inspect runtime uninstall plan without removing files

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Inspect conservative runtime uninstall plan** |

```bash
./scripts/system-uninstall.sh --builddir build
```

Expected:

- The command prints installed paths from `meson introspect --installed build`.
- No files are removed.
- Missing files and changed files are reported but left untouched.

### 12.2. Remove runtime files with the conservative helper

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Remove unchanged runtime install artifacts** |

```bash
sudo ./scripts/system-uninstall.sh --builddir build --remove
```

Expected:

- Only files that still match the current Meson install sources are removed.
- Changed files are left untouched with a warning.
- Real user files under `~/.config/morph` are not touched.

### 12.3. Remove runtime files with the Meson/Ninja uninstall target

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Check Meson-generated uninstall target** |

The Meson command set commonly does not provide a `meson uninstall` command,
but the generated Ninja build has an `uninstall` target. Check the local Meson
help if a distro carries a newer uninstall command.

```bash
meson --help | rg 'uninstall' || true
ninja -C build -t targets | rg '^uninstall:'
sudo ninja -C build uninstall
```

Expected:

- The Meson help check documents whether the local Meson package exposes a direct uninstall command.
- The Ninja target check confirms that the build has an `uninstall` target.
- The final command removes Meson-installed runtime files according to Meson's generated uninstall logic.
- This path is less conservative than `scripts/system-uninstall.sh`; use it only when testing the raw Meson/Ninja behavior.

### 12.4. Remove dev user links with `dev-install.sh`

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Remove dev user links** |

```bash
./scripts/dev-install.sh uninstall --link-launcher
```

Expected:

- Dev symlinks created by `dev-install.sh` are removed when they still point at this checkout.
- Unrelated symlinks are left untouched.
- Real user files are never removed.

### 12.5. Remove system-visible dev session links

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Remove `/usr` dev session links** |

```bash
./scripts/dev-install.sh uninstall --system-links
```

Expected:

- `/usr/bin/morph_dbg` is removed.
- `/usr/bin/morph-session_dbg` is removed.
- `/usr/share/wayland-sessions/morph_dbg.desktop` is removed.
- `/usr/share/icons/hicolor/scalable/apps/morph_dbg.svg` is removed.
- Runtime `/usr/bin/morph`, `/usr/bin/morph-session`, and `/etc/morph/*` are not touched by this dev uninstall path.

References:

- `scripts/system-uninstall.sh`
- `scripts/dev-install.sh`
- `scripts/morph-uninstall.sh`
- `testing/testplan-manual-morph.nfo`

## 13. Morph Scripts

These checks cover the newer convenience scripts. They should match the direct
Meson/dev-install flows above without hiding what they do.

### 13.1. Build

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Build runtime and debug variants through the helper** |

```bash
./scripts/morph-build.sh --runtime
./scripts/morph-build.sh --debug
```

Expected:

- `--runtime` configures or reconfigures `build/` as release with stripping enabled and compiles `build/morph`.
- `--debug` configures or reconfigures `build_dbg/` as debug with stripping disabled and compiles `build_dbg/morph`.
- Existing build directories are reconfigured in-place.

### 13.2. Install

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Preview unified runtime/debug install helper** |

```bash
./scripts/morph-install.sh --runtime --dry
./scripts/morph-install.sh --debug --dry
./scripts/morph-install.sh --both --dry
```

Expected:

- `--runtime --dry` prints the Meson runtime install command and manifest-derived targets without installing files.
- `--debug --dry` prints the delegated dev install plan, including `~/.config/morph/*`, `~/.local/bin/morph-session_dbg`, and the system-visible debug session links, without creating them.
- `--both --dry` prints runtime first and debug second.
- The dry-run output stays consistent with chapters 11.1 and 11.3.

### 13.3. Uninstall

| Arch | Debian | Fedora | Description |
|---|---|---|---|
|[x]|[x]|[x]| **Preview unified runtime/debug uninstall helper** |

```bash
./scripts/morph-uninstall.sh --runtime --dry
./scripts/morph-uninstall.sh --debug --dry
./scripts/morph-uninstall.sh --both --dry
```

Expected:

- `--runtime --dry` prints the conservative runtime uninstall command without removing files.
- `--debug --dry` prints the delegated dev uninstall plan, including matching `~/.config/morph/*` links, `~/.local/bin/morph-session_dbg`, and the system-visible debug session links, without removing them.
- `--both --dry` prints runtime first and debug second.
- The debug uninstall path removes only symlinks that still point back to this checkout, removes an empty `~/.config/morph` directory after those links are gone, and leaves unrelated user files untouched.

References:

- `scripts/morph-build.sh`
- `scripts/morph-install.sh`
- `scripts/morph-uninstall.sh`

## 14. Documentation Structure

[x] **Were the old root documents moved into `docs/` sensibly?**
[x] **Is [`docs/OVERVIEW.md`](docs/OVERVIEW.md) the right central entry point?**
[x] **Are `docs/roadmaps/` and [`docs/BACKLOG.md`](docs/BACKLOG.md) placed logically?**

Expected:

- `docs/` contains the current documentation centrally
- `sessions/` contains the session desktop files separately from the rest
- no old parallel structure such as `docs_old/` remains

References:

- [`docs/OVERVIEW.md`](docs/OVERVIEW.md)
- [`docs/roadmaps/Roadmap_install-and-user-setup.md`](docs/roadmaps/Roadmap_install-and-user-setup.md)
- [`docs/BACKLOG.md`](docs/BACKLOG.md)
- [`README.md`](README.md)

## Open Follow-Ups for Later Test Rounds

- [ ] **Tighten the Mermaid flow notes in [`docs/OVERVIEW.md`](docs/OVERVIEW.md)**
- [ ] **Review a few visual flow details separately**
- [ ] **Optionally add more end-to-end tests for display-manager startup and a real runtime session**

## Short Conclusion

If the items above turn green, the branch is not only structurally cleaner, but also functionally consistent across wrapper behavior, [`config/environment`](config/environment) resolution, hooks, install paths, and documentation.
