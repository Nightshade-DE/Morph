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
    - /usr/bin/morph_dbg            (symlink to ./build_dbg/morph)
    - /usr/bin/morph-session_dbg    (symlink to ./testing/morph-session_dbg)
    - /usr/share/wayland-sessions/morph_dbg.desktop (symlink to ./sessions/morph_dbg.desktop)
    - /usr/share/icons/hicolor/scalable/apps/morph_dbg.svg (symlink to ./assets/icons/morph_dbg.svg)

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

print_target_line() {
    src="$1"
    dst="$2"
    mode="$3"
    printf '  %s <= %s (%s)\n' "$dst" "$src" "$mode"
}

print_debug_plan() {
    printf 'debug install targets:\n'

    print_target_line "$PWD/build_dbg/morph" /usr/bin/morph_dbg symlink
    print_target_line "$PWD/testing/morph-session_dbg" /usr/bin/morph-session_dbg symlink
    print_target_line "$PWD/sessions/morph_dbg.desktop" /usr/share/wayland-sessions/morph_dbg.desktop symlink
    print_target_line "$PWD/assets/icons/morph_dbg.svg" /usr/share/icons/hicolor/scalable/apps/morph_dbg.svg symlink
}

install_debug_file() {
    mode="$1"
    src="$2"
    dst="$3"

    if [ "$DRY" -eq 1 ]; then
        run_root install -C -m "$mode" "$src" "$dst"
        return 0
    fi

    # Report whether the destination was newly created, updated, or unchanged.
    status="installed"
    if [ -e "$dst" ]; then
        if [ -f "$dst" ] && cmp -s "$src" "$dst"; then
            status="unchanged"
        else
            status="updated"
        fi
    fi

    run_root install -C -m "$mode" "$src" "$dst"
    printf '[morph-install] %s: %s\n' "$status" "$dst"
}

install_debug_symlink() {
    src="$1"
    dst="$2"

    src_abs=$(readlink -f "$src")
    if [ -z "$src_abs" ] || [ ! -e "$src_abs" ]; then
        printf 'Missing symlink source: %s\n' "$src" >&2
        exit 1
    fi

    if [ "$DRY" -eq 1 ]; then
        run_root ln -sfn "$src_abs" "$dst"
        return 0
    fi

    status="linked"
    if [ -L "$dst" ]; then
        current_target=$(readlink -f "$dst")
        if [ "$current_target" = "$src_abs" ]; then
            status="unchanged"
        else
            status="updated"
        fi
    elif [ -e "$dst" ]; then
        status="updated"
    fi

    run_root ln -sfn "$src_abs" "$dst"
    printf '[morph-install] %s symlink: %s -> %s\n' "$status" "$dst" "$src_abs"
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

    if [ ! -f "testing/morph-session_dbg" ]; then
        printf 'Missing dev launcher source: ./testing/morph-session_dbg\n' >&2
        exit 1
    fi

    if [ ! -f "sessions/morph_dbg.desktop" ]; then
        printf 'Missing dev desktop source: ./sessions/morph_dbg.desktop\n' >&2
        exit 1
    fi

    if [ ! -f "assets/icons/morph_dbg.svg" ]; then
        printf 'Missing icon source: ./assets/icons/morph_dbg.svg\n' >&2
        exit 1
    fi

    print_debug_plan

    # Ensure destination directories exist before installing explicit debug artifacts.
    run_root install -d /usr/bin
    run_root install -d /usr/share/wayland-sessions
    run_root install -d /usr/share/icons/hicolor/scalable/apps
    install_debug_symlink build_dbg/morph /usr/bin/morph_dbg
    install_debug_symlink testing/morph-session_dbg /usr/bin/morph-session_dbg
    install_debug_symlink sessions/morph_dbg.desktop /usr/share/wayland-sessions/morph_dbg.desktop
    install_debug_symlink assets/icons/morph_dbg.svg /usr/share/icons/hicolor/scalable/apps/morph_dbg.svg
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
