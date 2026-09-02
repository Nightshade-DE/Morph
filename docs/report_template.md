# Morph Cross-Distribution Test Report Template

Use this template for future feature-branch reports when the same branch must be
checked on more than one Linux distribution. The goal is to run the expensive or
distribution-sensitive checks everywhere, while keeping purely logical checks to
one representative system after code changes.

## Tested Distributions

| Distribution | Version / Variant | Hardware / VM | Notes |
|---|---|---|---|
| Arch |  |  |  |
| Debian |  |  |  |
| Fedora |  |  |  |
| Bedrock |  |  |  |

## Legend

- `[ ]` Not tested yet
- `[x]` Tested successfully
- `[!]` Tested with notes or known limitation
- `[n/a]` Not applicable on this distribution

## Must Run On Every Distro

These checks cover the places that may differ between distributions: compiler and
Meson setup, wlroots runtime behavior, native compositor startup, nested session
probing, portal executable paths, X11 bridge behavior, reload IPC, and shutdown
cleanup.

| Arch | Debian | Fedora | Bedrock | Description |
|---|---|---|---|---|
|[ ]|[ ]|[ ]|[ ]| `0. Automated Baseline Run`: first build, reconfigure build, and `meson test -C build --print-errorlogs` |
|[ ]|[ ]|[ ]|[ ]| `2.1. Default release-wrapper resolution` |
|[ ]|[ ]|[ ]|[ ]| `3.1. Default release-wrapper config resolution` |
|[ ]|[ ]|[ ]|[ ]| `4.1. Start the release wrapper as native session` |
|[ ]|[ ]|[ ]|[ ]| `4.4. Start native release session with X11 bridge disabled` |
|[ ]|[ ]|[ ]|[ ]| `5.1. Start the dev launcher inside an X11 or Wayland session` |
|[ ]|[ ]|[ ]|[ ]| `5.2. Start nested session with X11 bridge explicitly disabled` |
|[ ]|[ ]|[ ]|[ ]| `6.2. Test the managed portal base without a user portal override` |
|[ ]|[ ]|[ ]|[ ]| `7.1. Start a running session first` |
|[ ]|[ ]|[ ]|[ ]| `7.2. Prepare the user reload hook in a second terminal` |
|[ ]|[ ]|[ ]|[ ]| `7.3. Trigger reload from the second terminal` |
|[ ]|[ ]|[ ]|[ ]| `8. Shutdown Flow and Tracker` |

## Run Once After Code Changes

These checks validate Morph's own resolution and hook logic. They should be run
on at least one representative distribution after relevant code changes. Repeat
on another distribution only when the failure surface is related to shell,
filesystem layout, config paths, or runtime environment behavior.

| Status | Description |
|---|---|
|[ ]| `2.3. Caller environment wins over environment files` |
|[ ]| `2.4. Custom environment file is loaded as the system environment layer` |
|[ ]| `2.5. Optional compositor debug variables are forwarded` |
|[ ]| `2.6. Runtime linker path from the caller is preserved` |
|[ ]| `3.3. Explicit config override wins in both wrappers` |
|[ ]| `3.4. Builtin fallback is opt-in only` |
|[ ]| `3.5. User XDG config beats system config` |
|[ ]| `6.1. Test only the user startup hook in a nested session` |
|[ ]| `6.3. Test a user portal override separately` |
|[ ]| `7.4. Test user reload hook with reload <cmd ...>` |
|[ ]| `7.5. Test user reload hook with reload_once <cmd ...>` |
|[ ]| `7.6. Test explicit reload hook config path ${MORPH_USER_CONFIG_DIR}/reload.sh` |

## Mostly Optional

These checks are useful for release polish, documentation review, packaging work,
or when the touched branch changed the corresponding files. They do not normally
need to be repeated on every distribution.

| Status | Description |
|---|---|
|[ ]| `1. README and Doc Entry Points` |
|[ ]| `9. CLI Reference vs Real Behavior` |
|[ ]| `10. Session Desktop Files and Install Paths` |
|[ ]| `11. Runtime / Dev Uninstall` |
|[ ]| `12. Documentation Structure` |

## Distribution Notes

### Arch

- Notes:

### Debian

- Notes:

### Fedora

- Notes:

### Bedrock

- Notes:

## Short Conclusion

- Result:
- Blocking issues:
- Follow-up tests needed:
