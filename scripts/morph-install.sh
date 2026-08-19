#!/usr/bin/env bash
# Install helper that combines Meson-managed runtime install with explicit
# debug-session artifacts.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/morph-install.sh [--runtime|--debug|--both] [--dry]

Install modes:
  --runtime   Install runtime artifacts from ./build via meson install
  --debug     Install debug session artifacts from ./build_dbg
  --both      Install runtime + debug artifacts

Options:
  --dry       Print commands only (no changes)

Debug install artifacts:
  - ~/.config/morph/* symlinks to testing/config/* when no real user files exist
  - ~/.local/bin/morph-session_dbg symlink to ./testing/morph-session_dbg
  - /usr/bin/morph_dbg symlink to ./build_dbg/morph
  - /usr/bin/morph-session_dbg wrapper for ./testing/morph-session_dbg
  - /usr/share/wayland-sessions/morph_dbg.desktop symlink to ./sessions/morph_dbg.desktop
  - /usr/share/icons/hicolor/scalable/apps/morph_dbg.svg symlink to ./assets/icons/morph_dbg.svg

Notes:
  - Runtime install stays Meson-managed.
  - Debug install delegates to scripts/dev-install.sh so user-local and
    display-manager-visible dev entry points stay in sync.
EOF
}

MODE="both"
DRY=0

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
        printf 'Need root privileges for install command: %s\n' "$*" >&2
        printf 'Run as root or install sudo.\n' >&2
        exit 1
    fi
}

print_runtime_dry_plan() {
    # Read Meson's current install manifest so dry-run output reflects real
    # runtime targets without requiring root operations.
    manifest="$(meson introspect --installed build 2>/dev/null || true)"

    if [ -z "$manifest" ] || [ "$manifest" = "{}" ]; then
        printf 'runtime install targets: unavailable (meson introspect returned no entries)\n'
        return 0
    fi

    printf 'runtime install targets:\n'
    printf '%s\n' "$manifest" \
        | tr ',' '\n' \
        | sed -nE 's/^[[:space:]]*\{[[:space:]]*//; s/[[:space:]]*\}[[:space:]]*$//; s/^[[:space:]]*"([^"]+)"[[:space:]]*:[[:space:]]*"([^"]+)"[[:space:]]*$/  \2 <= \1/p'
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

    # User-local dev links must belong to the invoking user even when the
    # combined install helper itself was started with sudo for the /usr phase.
    sudo -u "$SUDO_USER" env HOME="$user_home" XDG_CONFIG_HOME="$user_home/.config" "$@"
}

run_dev_install_user_links() {
    require_dev_install_helper

    if [ "$DRY" -eq 1 ]; then
        run_as_invoking_user ./scripts/dev-install.sh install --link-launcher --dry
    else
        run_as_invoking_user ./scripts/dev-install.sh install --link-launcher
    fi
}

run_dev_install_system_links() {
    require_dev_install_helper

    if [ "$DRY" -eq 1 ]; then
        ./scripts/dev-install.sh install --system-links --skip-user-links --dry
    else
        ./scripts/dev-install.sh install --system-links --skip-user-links
    fi
}

install_runtime() {
    printf '[morph-install] runtime phase\n'

    if [ ! -f "build/meson-private/coredata.dat" ]; then
        printf 'Runtime build directory not configured: ./build\n' >&2
        printf 'Run: scripts/morph-build.sh --runtime\n' >&2
        exit 1
    fi

    if [ "$DRY" -eq 1 ]; then
        printf '[dry] sudo meson install -C build\n'
        print_runtime_dry_plan
    else
        run_root meson install -C build
    fi
}

install_debug() {
    printf '[morph-install] debug phase\n'

    if [ ! -x "build_dbg/morph" ]; then
        printf 'Debug binary missing: ./build_dbg/morph\n' >&2
        printf 'Run: scripts/morph-build.sh --debug\n' >&2
        exit 1
    fi

    run_dev_install_user_links
    run_dev_install_system_links
}

case "$MODE" in
    runtime)
        install_runtime
        ;;
    debug)
        install_debug
        ;;
    both)
        install_runtime
        install_debug
        ;;
    *)
        printf 'Internal mode error: %s\n' "$MODE" >&2
        exit 1
        ;;
esac

printf '[morph-install] ok (%s)%s\n' "$MODE" "$( [ "$DRY" -eq 1 ] && printf ' [dry]' )"
