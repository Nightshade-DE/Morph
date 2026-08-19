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
  - ~/.config/morph/* symlinks when they still point at testing/config/*
  - ~/.local/bin/morph-session_dbg when it still points at ./testing/morph-session_dbg
  - /usr/bin/morph_dbg
  - /usr/bin/morph-session_dbg
  - /usr/share/wayland-sessions/morph_dbg.desktop
  - /usr/share/icons/hicolor/scalable/apps/morph_dbg.svg

Notes:
  - Runtime uninstall keeps the same conservative file verification behavior as
    scripts/system-uninstall.sh.
  - Debug uninstall delegates to scripts/dev-install.sh so it mirrors the debug
    install flow and leaves unrelated user files untouched.
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

require_dev_install_helper() {
    if [ ! -x "scripts/dev-install.sh" ]; then
        printf 'Missing dev install helper: ./scripts/dev-install.sh\n' >&2
        exit 1
    fi
}

run_as_invoking_user() {
    if [ "$(id -u)" -ne 0 ] || [ -z "${SUDO_USER:-}" ] || [ "${SUDO_USER:-}" = root ]; then
        "$@"
        return $?
    fi

    user_home=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    if [ -z "$user_home" ]; then
        printf 'Unable to resolve home directory for sudo user: %s\n' "$SUDO_USER" >&2
        exit 1
    fi

    # User-local dev links must be removed from the invoking user's home even
    # when the combined uninstall helper itself was started with sudo.
    sudo -u "$SUDO_USER" env HOME="$user_home" XDG_CONFIG_HOME="$user_home/.config" "$@"
}

run_dev_uninstall_user_links() {
    require_dev_install_helper

    if [ "$DRY" -eq 1 ]; then
        run_as_invoking_user ./scripts/dev-install.sh uninstall --link-launcher --dry
    else
        run_as_invoking_user ./scripts/dev-install.sh uninstall --link-launcher
    fi
}

run_dev_uninstall_system_links() {
    require_dev_install_helper

    if [ "$DRY" -eq 1 ]; then
        ./scripts/dev-install.sh uninstall --system-links --skip-user-links --dry
    else
        ./scripts/dev-install.sh uninstall --system-links --skip-user-links
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
    run_dev_uninstall_user_links
    run_dev_uninstall_system_links
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
