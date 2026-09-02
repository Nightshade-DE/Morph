# Automated Tests

This document covers the automated tests under `tests/`.

Use [`docs/TESTING.md`](TESTING.md) for the broader local build, smoke-test,
and manual validation workflow.

## Scope

The automated test layer currently includes:

- `tests/test_config.c`
  - config parsing and validation behavior
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

## Related Docs

- [`docs/TESTING.md`](TESTING.md)
- [`testing/testplan-manual-morph.nfo`](../testing/testplan-manual-morph.nfo)
