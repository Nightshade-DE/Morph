#!/bin/sh
# Development install helper for morph runtime files.
# - Creates opt-in symlinks for user config files under ~/.config/morph.
# - Can expose the dev launcher through ~/.local/bin for quick shell runs.
# - Prints explicit sudo guidance for display-manager-visible dev session setup.
################################################################################

set -eu

# Resolve repository root once so all link/copy sources stay stable.
REAL_SCRIPT_PATH=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$REAL_SCRIPT_PATH")
COMP_ROOT_DIR=$(dirname "$SCRIPT_DIR")

# Target layout for user-local and system-visible dev session helpers.
USER_CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/morph"
USER_BIN_DIR="${HOME}/.local/bin"
SYSTEM_DEV_DESKTOP_TARGET="/usr/share/wayland-sessions/morph_dbg.desktop"
SYSTEM_DEV_BIN_TARGET="/usr/bin/morph_dbg"
SYSTEM_DEV_LAUNCHER_TARGET="/usr/bin/morph-session_dbg"
LOCAL_DESKTOP_DIR="${HOME}/.local/share/wayland-sessions"
LOCAL_ICON_DIR="${HOME}/.local/share/icons/hicolor/scalable/apps"
SYSTEM_ICON_DIR="/usr/share/icons/hicolor/scalable/apps"

usage() {
    cat <<'EOF'
Usage: scripts/dev-install.sh <install|uninstall> [options]

Options:
  --link-launcher      Symlink testing/morph-session_dbg into ~/.local/bin/morph-session_dbg
    --desktop-local      Copy debug session desktop file and icon into ~/.local/share
  --system-links       Install/remove system-visible debug symlinks under /usr
  --dry                Print planned commands only (no changes)
  --print-sudo-help    Print sudo commands for a display-manager-visible dev session

Behavior:
  - ~/.local/bin links are convenient for direct shell launches, but many
    display managers do not search that path for session entries
  - install   creates symlinks only when the target path does not exist yet
  - uninstall removes only symlinks created by this script
  - real user files are never replaced or removed automatically
EOF
}

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
        printf 'Need root privileges for command: %s\n' "$*" >&2
        printf 'Run as root or install sudo.\n' >&2
        exit 1
    fi
}

print_target_line() {
    src="$1"
    dst="$2"
    mode="$3"
    printf '  %s <= %s (%s)\n' "$dst" "$src" "$mode"
}

print_install_targets() {
    printf 'dev install targets:\n'
    for rel in morph.conf startup.sh reload.sh shutdown.sh environment portals; do
        print_target_line "$COMP_ROOT_DIR/testing/config/$rel" "$USER_CONFIG_DIR/$rel" symlink
    done

    if [ "$LINK_LAUNCHER" -eq 1 ]; then
        print_target_line "$COMP_ROOT_DIR/testing/morph-session_dbg" "$USER_BIN_DIR/morph-session_dbg" symlink
    fi

    if [ "$DESKTOP_LOCAL" -eq 1 ]; then
        print_target_line "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$LOCAL_DESKTOP_DIR/morph_dbg.desktop" copy
        print_target_line "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$LOCAL_ICON_DIR/morph_dbg.svg" copy
    fi

    if [ "$SYSTEM_LINKS" -eq 1 ]; then
        print_target_line "$COMP_ROOT_DIR/build_dbg/morph" "$SYSTEM_DEV_BIN_TARGET" symlink
        print_target_line "$COMP_ROOT_DIR/testing/morph-session_dbg" "$SYSTEM_DEV_LAUNCHER_TARGET" symlink
        print_target_line "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$SYSTEM_DEV_DESKTOP_TARGET" symlink
        print_target_line "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$SYSTEM_ICON_DIR/morph_dbg.svg" symlink
    fi
}

link_if_missing() {
    src="$1"
    dst="$2"

    if [ -L "$dst" ]; then
        current_target=$(readlink -f "$dst")
        if [ "$current_target" = "$src" ]; then
            printf 'Keeping existing symlink: %s -> %s\n' "$dst" "$src"
            return 0
        fi
        # A user-managed symlink may already point at a different config tree.
        # Keep it untouched and continue so the install helper remains safe on
        # established setups instead of partially replacing user decisions.
        printf 'WARN: unrelated symlink already exists, leaving it untouched and will use it: %s\n' "$dst" >&2
        return 0
    fi

    if [ -e "$dst" ]; then
        # Real user files must win over repository sample links. Report the
        # situation clearly and continue with the existing user-owned file.
        printf 'WARN: real file already exists, leaving it untouched and will use it: %s\n' "$dst" >&2
        return 0
    fi

    if [ "$DRY" -eq 1 ]; then
        printf '[dry] ln -s %s %s\n' "$src" "$dst"
        return 0
    fi

    ln -s "$src" "$dst"
    printf 'Linked %s -> %s\n' "$dst" "$src"
}

unlink_if_matches() {
    src="$1"
    dst="$2"

    if [ ! -L "$dst" ]; then
        return 0
    fi

    current_target=$(readlink -f "$dst")
    if [ "$current_target" != "$src" ]; then
        printf 'Keeping unrelated symlink: %s\n' "$dst"
        return 0
    fi

    if [ "$DRY" -eq 1 ]; then
        printf '[dry] rm -f %s\n' "$dst"
        return 0
    fi

    rm -f "$dst"
    printf 'Removed symlink %s\n' "$dst"
}

install_user_links() {
    mkdir -p "$USER_CONFIG_DIR"

    for rel in morph.conf startup.sh reload.sh shutdown.sh environment portals; do
        link_if_missing "$COMP_ROOT_DIR/testing/config/$rel" "$USER_CONFIG_DIR/$rel"
    done
}

uninstall_user_links() {
    for rel in morph.conf startup.sh reload.sh shutdown.sh environment portals; do
        unlink_if_matches "$COMP_ROOT_DIR/testing/config/$rel" "$USER_CONFIG_DIR/$rel"
    done
}

install_local_launcher_link() {
    mkdir -p "$USER_BIN_DIR"
    link_if_missing "$COMP_ROOT_DIR/testing/morph-session_dbg" "$USER_BIN_DIR/morph-session_dbg"
}

uninstall_local_launcher_link() {
    unlink_if_matches "$COMP_ROOT_DIR/testing/morph-session_dbg" "$USER_BIN_DIR/morph-session_dbg"
}

install_local_desktop_copy() {
    if [ "$DRY" -eq 1 ]; then
        printf '[dry] install -d %s\n' "$LOCAL_DESKTOP_DIR"
        printf '[dry] install -m 0644 %s %s/morph_dbg.desktop\n' "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$LOCAL_DESKTOP_DIR"
        printf '[dry] install -d %s\n' "$LOCAL_ICON_DIR"
        printf '[dry] install -m 0644 %s %s/morph_dbg.svg\n' "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$LOCAL_ICON_DIR"
        return 0
    fi

    mkdir -p "$LOCAL_DESKTOP_DIR"
    install -m 0644 "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$LOCAL_DESKTOP_DIR/morph_dbg.desktop"
    printf 'Installed local desktop file at %s/morph_dbg.desktop\n' "$LOCAL_DESKTOP_DIR"

    mkdir -p "$LOCAL_ICON_DIR"
    install -m 0644 "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$LOCAL_ICON_DIR/morph_dbg.svg"
    printf 'Installed local icon at %s/morph_dbg.svg\n' "$LOCAL_ICON_DIR"
}

install_system_links() {
    if [ ! -x "$COMP_ROOT_DIR/build_dbg/morph" ]; then
        printf 'Missing debug binary: %s\n' "$COMP_ROOT_DIR/build_dbg/morph" >&2
        printf 'Run: ./scripts/morph-build.sh --debug\n' >&2
        exit 1
    fi

    run_root install -d /usr/bin
    run_root install -d /usr/share/wayland-sessions
    run_root install -d "$SYSTEM_ICON_DIR"
    run_root ln -sfn "$COMP_ROOT_DIR/build_dbg/morph" "$SYSTEM_DEV_BIN_TARGET"
    run_root ln -sfn "$COMP_ROOT_DIR/testing/morph-session_dbg" "$SYSTEM_DEV_LAUNCHER_TARGET"
    run_root ln -sfn "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$SYSTEM_DEV_DESKTOP_TARGET"
    run_root ln -sfn "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$SYSTEM_ICON_DIR/morph_dbg.svg"
}

uninstall_system_links() {
    run_root rm -f "$SYSTEM_DEV_BIN_TARGET"
    run_root rm -f "$SYSTEM_DEV_LAUNCHER_TARGET"
    run_root rm -f "$SYSTEM_DEV_DESKTOP_TARGET"
    run_root rm -f "$SYSTEM_ICON_DIR/morph_dbg.svg"
}

print_sudo_help() {
    printf 'Display-manager-visible dev session install:\n'
    printf '  sudo ln -sf %s %s\n' "$COMP_ROOT_DIR/testing/morph-session_dbg" "$SYSTEM_DEV_LAUNCHER_TARGET"
    printf '  sudo ln -sf %s %s\n' "$COMP_ROOT_DIR/sessions/morph_dbg.desktop" "$SYSTEM_DEV_DESKTOP_TARGET"
    printf '  sudo install -d %s\n' "$SYSTEM_ICON_DIR"
    printf '  sudo ln -sf %s %s/morph_dbg.svg\n' "$COMP_ROOT_DIR/assets/icons/morph_dbg.svg" "$SYSTEM_ICON_DIR"
    printf '\n'
    printf 'Note:\n'
    printf '  morph-session_dbg runs directly from the repository and uses\n'
    printf '  %s/scripts and %s/testing/config as its managed runtime layer.\n' "$COMP_ROOT_DIR" "$COMP_ROOT_DIR"
    printf '  Therefore, this dev helper does not install shell-helpers.sh or\n'
    printf '  system_startup.sh/system_reload.sh/system_shutdown.sh into /etc/morph.\n'
    printf '\n'
    printf 'Optional runtime-style install (installs /etc/morph files and /usr/bin/morph-session):\n'
    printf '  ./scripts/morph-install.sh --runtime\n'
    printf '  # or: sudo meson install -C build\n'
    printf '\n'
    printf 'Runtime session desktop remains part of runtime install only:\n'
    printf '  ./scripts/morph-install.sh --runtime\n'
}

ACTION="${1:-}"
shift || true

# Flags are opt-in extras for install/uninstall behavior.
LINK_LAUNCHER=0
DESKTOP_LOCAL=0
SYSTEM_LINKS=0
DRY=0
PRINT_SUDO_HELP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --link-launcher) LINK_LAUNCHER=1 ;;
        --desktop-local) DESKTOP_LOCAL=1 ;;
        --system-links) SYSTEM_LINKS=1 ;;
        --dry) DRY=1 ;;
        --print-sudo-help) PRINT_SUDO_HELP=1 ;;
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

case "$ACTION" in
    install)
        print_install_targets

        # Core dev flow: link testing config into ~/.config/morph.
        install_user_links
        if [ "$LINK_LAUNCHER" -eq 1 ]; then
            install_local_launcher_link
        fi
        if [ "$DESKTOP_LOCAL" -eq 1 ]; then
            install_local_desktop_copy
        fi
        if [ "$SYSTEM_LINKS" -eq 1 ]; then
            install_system_links
        fi
        if [ "$PRINT_SUDO_HELP" -eq 1 ]; then
            print_sudo_help
        fi
        ;;
    uninstall)
        uninstall_user_links
        if [ "$LINK_LAUNCHER" -eq 1 ]; then
            uninstall_local_launcher_link
        fi
        if [ "$SYSTEM_LINKS" -eq 1 ]; then
            uninstall_system_links
        fi
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
