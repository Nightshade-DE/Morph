#!/usr/bin/env bash
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
  - /usr/bin/morph_dbg            (from ./build_dbg/morph)
  - /usr/bin/morph-session_dbg    (from ./testing/morph-session_dbg)
  - /usr/share/wayland-sessions/morph_dbg.desktop

Notes:
  - Runtime install stays Meson-managed.
  - Debug install uses plain install(1) for explicit dev targets.
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

install_runtime() {
    if [ ! -f "build/meson-private/coredata.dat" ]; then
        printf 'Runtime build directory not configured: ./build\n' >&2
        printf 'Run: scripts/morph-build.sh --runtime\n' >&2
        exit 1
    fi

    if [ "$DRY" -eq 1 ]; then
        printf '[dry] sudo meson install -C build\n'
    else
        run_root meson install -C build
    fi
}

install_debug() {
    if [ ! -x "build_dbg/morph" ]; then
        printf 'Debug binary missing: ./build_dbg/morph\n' >&2
        printf 'Run: scripts/morph-build.sh --debug\n' >&2
        exit 1
    fi

    if [ ! -f "testing/morph-session_dbg" ]; then
        printf 'Missing dev launcher source: ./testing/morph-session_dbg\n' >&2
        exit 1
    fi

    if [ ! -f "sessions/morph_dbg.desktop" ]; then
        printf 'Missing dev desktop source: ./sessions/morph_dbg.desktop\n' >&2
        exit 1
    fi

    run_root install -d /usr/bin
    run_root install -d /usr/share/wayland-sessions
    run_root install -m 0755 build_dbg/morph /usr/bin/morph_dbg
    run_root install -m 0755 testing/morph-session_dbg /usr/bin/morph-session_dbg
    run_root install -m 0644 sessions/morph_dbg.desktop /usr/share/wayland-sessions/morph_dbg.desktop
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
