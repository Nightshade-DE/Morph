# Morph Installation

This document collects the current build dependencies and the supported install
and uninstall flows for both runtime and development setups.

## Dependencies

Morph currently builds against the following primary dependencies:

- `wlroots-0.19` `>= 0.19`
- `pixman-1` `>= 0.44`
- `wayland-server` `>= 1.23`
- `xkbcommon` `>= 1.7`
- `wayland-protocols` `>= 1.32`
- `wayland-scanner` `>= 1.23`
- POSIX shell `sh`

Version note:

- `meson.build` currently enforces `wayland-protocols >= 1.32` directly
- the other `>=` values are practical orientation points from the currently
  tested toolchain, not additional Meson-enforced version gates

For a native DRM session you also need the usual wlroots runtime stack,
especially DRM/libinput and their system libraries.

The authoritative build dependency declarations live in `meson.build`. This
document mirrors that information so installation steps stay in one place.

## Release Install

The standard runtime install uses Meson and installs into the configured
prefix.

Build and install:

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

Installed artifacts:

- `morph` under the configured `bindir` (typically `/usr/local/bin/morph`)
- `morph-session` under the same `bindir` (typically `/usr/local/bin/morph-session`)
- runtime files under `sysconfdir/morph` (typically `/usr/local/etc/morph/`)
- `morph.desktop` under `share/wayland-sessions` (typically `/usr/local/share/wayland-sessions/morph.desktop`)
- selected documentation under `share/doc/morph` (typically `/usr/local/share/doc/morph/`)

This is the right path for:

- system-wide testing through a display manager
- packaging
- any setup that should match the runtime file layout

## Release Uninstall

System installs are removed conservatively through
`scripts/system-uninstall.sh`.

Preview the current install manifest:

```bash
./scripts/system-uninstall.sh --builddir build
```

Remove only unchanged installed files:

```bash
sudo ./scripts/system-uninstall.sh --builddir build --remove
```

Important details:

- the script reads `meson introspect --installed`
- removal is opt-in; default mode is print-only
- files that were modified after install are kept with a warning
- missing or unverifiable files are kept with a warning
- empty parent directories are pruned afterwards when safe

This keeps local admin changes under `/etc`, `/usr/share`, or custom prefixes
from being deleted silently.

## Development Install

Development install is handled by `scripts/dev-install.sh`. It does not install
the compositor system-wide by default. Instead it creates safe symlinks for a
repository-backed developer workflow.

Base development install:

```bash
./scripts/dev-install.sh install
```

This links the development copies from `testing/config/` into
`$XDG_CONFIG_HOME/morph` or `~/.config/morph`:

- `morph.conf`
- `startup.sh`
- `reload.sh`
- `shutdown.sh`
- `environment`
- `portals`

Optional helper installs:

```bash
./scripts/dev-install.sh install --link-launcher
./scripts/dev-install.sh install --desktop-local
./scripts/dev-install.sh install --print-sudo-help
```

Options:

- `--link-launcher`
  - creates `~/.local/bin/morph-dev-session` as a symlink to
    `testing/morph_run`
- `--desktop-local`
  - copies `sessions/morph.desktop` into
    `~/.local/share/wayland-sessions`
- `--print-sudo-help`
  - prints the commands needed to expose the development session through
    system paths such as `/usr/local/bin` and `/usr/share/wayland-sessions`

Safety behavior:

- existing real user files are never replaced
- unrelated symlinks are never replaced
- such cases produce explicit warnings and the existing files stay in use

## Development Uninstall

Remove the development symlinks that `dev-install.sh` created:

```bash
./scripts/dev-install.sh uninstall
```

If you also linked the local launcher, remove that symlink too:

```bash
./scripts/dev-install.sh uninstall --link-launcher
```

Important details:

- only symlinks that point back to this repository are removed
- unrelated symlinks are left untouched
- real user files are never removed
- local desktop copies created with `--desktop-local` are not removed by the
  current helper and must be deleted manually if no longer needed

## Recommended Flows

Use the runtime install when you want a real system session:

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

Use the development install when you want repository-backed config and quick
iteration:

```bash
./scripts/dev-install.sh install --link-launcher
./testing/morph_run
```

For display-manager-visible development sessions, use the printed sudo guidance
from:

```bash
./scripts/dev-install.sh install --print-sudo-help
```
