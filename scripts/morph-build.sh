#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/morph-build.sh [--runtime|--debug|--both]

Build modes:
  --runtime   Configure+compile release/runtime build in ./build
  --debug     Configure+compile debug build in ./build_dbg
  --both      Build both runtime and debug variants

Notes:
  - Existing build directories are reconfigured in-place.
  - Runtime build uses: buildtype=release, strip=true
  - Debug build uses:   buildtype=debug, strip=false
EOF
}

MODE="both"

while [ $# -gt 0 ]; do
    case "$1" in
        --runtime) MODE="runtime" ;;
        --debug) MODE="debug" ;;
        --both) MODE="both" ;;
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

setup_and_build() {
    builddir="$1"
    buildtype="$2"
    strip="$3"

    printf '[morph-build] setup: %s (buildtype=%s, strip=%s)\n' "$builddir" "$buildtype" "$strip"

    if [ -f "$builddir/meson-private/coredata.dat" ]; then
        meson setup "$builddir" --reconfigure --buildtype "$buildtype" -Dstrip="$strip"
    else
        meson setup "$builddir" --buildtype "$buildtype" -Dstrip="$strip"
    fi

    printf '[morph-build] compile: %s\n' "$builddir"
    meson compile -C "$builddir"
}

case "$MODE" in
    runtime)
        setup_and_build build release true
        ;;
    debug)
        setup_and_build build_dbg debug false
        ;;
    both)
        setup_and_build build release true
        setup_and_build build_dbg debug false
        ;;
    *)
        printf 'Internal mode error: %s\n' "$MODE" >&2
        exit 1
        ;;
esac

printf '[morph-build] ok (%s)\n' "$MODE"
