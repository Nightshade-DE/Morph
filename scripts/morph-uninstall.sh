#!/usr/bin/env bash
# Uninstall helper for runtime and debug install flows.
# Runtime removal delegates to the conservative Meson manifest-based helper.
# Debug removal deletes explicit artifacts installed by morph-install.sh.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/morph-uninstall.sh [--runtime|--debug|--both] [--dry]

Uninstall modes:
  --runtime   Remove runtime artifacts via scripts/system-uninstall.sh
  --debug     Remove debug session artifacts installed by scripts/morph-install.sh
  --both      Run runtime uninstall first, then debug uninstall

Options:
  --dry       Print commands only (no changes)

Debug uninstall targets:
  - /usr/bin/morph_dbg
  - /usr/bin/morph-session_dbg
  - /usr/share/wayland-sessions/morph_dbg.desktop

Notes:
  - Runtime uninstall keeps the same conservative file verification behavior as
    scripts/system-uninstall.sh.
EOF
}

MODE="both"
DRY=0

# Last mode option wins, matching the install/build helper behavior.
while [ $# -gt 0 ]; do
    case "$1" in
        --runtime) MODE="runtime" ;;
        --debug) MODE="debug" ;;
        --both) MODE="both" ;;
        --dry) DRY=1 ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

run_root() {
    if [ "$DRY" -eq 1 ]; then
        printf '[dry] %s\n' "$*"
        return 0
    fi

    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        printf 'Need root privileges for uninstall command: %s\n' "$*" >&2
        printf 'Run as root or install sudo.\n' >&2
        exit 1
    fi
}

remove_if_exists() {
    target="$1"

    # Treat symlinks explicitly so broken links can still be removed.
    if [ -e "$target" ] || [ -L "$target" ]; then
        run_root rm -f "$target"
    else
        printf '[morph-uninstall] skip missing: %s\n' "$target"
    fi
}

uninstall_runtime() {
    printf '[morph-uninstall] runtime phase\n'

    if [ ! -f "scripts/system-uninstall.sh" ]; then
        printf 'Missing runtime uninstall helper: ./scripts/system-uninstall.sh\n' >&2
        exit 1
    fi

    if [ "$DRY" -eq 1 ]; then
        printf '[dry] sudo ./scripts/system-uninstall.sh --builddir build --remove\n'
    else
        run_root ./scripts/system-uninstall.sh --builddir build --remove
    fi
}

uninstall_debug() {
    printf '[morph-uninstall] debug phase\n'

    remove_if_exists /usr/bin/morph_dbg
    remove_if_exists /usr/bin/morph-session_dbg
    remove_if_exists /usr/share/wayland-sessions/morph_dbg.desktop
}

case "$MODE" in
    runtime)
        uninstall_runtime
        ;;
    debug)
        uninstall_debug
        ;;
    both)
        uninstall_runtime
        uninstall_debug
        ;;
    *)
        printf 'Internal mode error: %s\n' "$MODE" >&2
        exit 1
        ;;
esac

printf '[morph-uninstall] ok (%s)%s\n' "$MODE" "$( [ "$DRY" -eq 1 ] && printf ' [dry]' )"
