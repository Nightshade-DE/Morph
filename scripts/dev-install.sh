#!/bin/sh
# Development install helper for morph runtime files.
# - Creates opt-in symlinks for user config files under ~/.config/morph.
# - Can expose the dev launcher through ~/.local/bin for quick shell runs.
# - Prints explicit sudo guidance for display-manager-visible dev session setup.
################################################################################

set -eu

REAL_SCRIPT_PATH=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$REAL_SCRIPT_PATH")
COMP_ROOT_DIR=$(dirname "$SCRIPT_DIR")

USER_CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/morph"
USER_BIN_DIR="${HOME}/.local/bin"
SYSTEM_DESKTOP_TARGET="/usr/share/wayland-sessions/morph.desktop"
SYSTEM_DEV_DESKTOP_TARGET="/usr/share/wayland-sessions/morph-dev.desktop"
SYSTEM_DEV_LAUNCHER_TARGET="/usr/local/bin/morph-dev-session"
LOCAL_DESKTOP_DIR="${HOME}/.local/share/wayland-sessions"

usage() {
    cat <<'EOF'
Usage: scripts/dev-install.sh <install|uninstall> [options]

Options:
  --link-launcher      Symlink testing/morph_run into ~/.local/bin/morph-dev-session
  --desktop-local      Copy the session desktop file into ~/.local/share/wayland-sessions
  --print-sudo-help    Print sudo commands for a display-manager-visible dev session

Behavior:
  - ~/.local/bin links are convenient for direct shell launches, but many
    display managers do not search that path for session entries
  - install   creates symlinks only when the target path does not exist yet
  - uninstall removes only symlinks created by this script
  - real user files are never replaced or removed automatically
EOF
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
    link_if_missing "$COMP_ROOT_DIR/testing/morph_run" "$USER_BIN_DIR/morph-dev-session"
}

uninstall_local_launcher_link() {
    unlink_if_matches "$COMP_ROOT_DIR/testing/morph_run" "$USER_BIN_DIR/morph-dev-session"
}

install_local_desktop_copy() {
    mkdir -p "$LOCAL_DESKTOP_DIR"
    install -m 0644 "$COMP_ROOT_DIR/sessions/morph.desktop" "$LOCAL_DESKTOP_DIR/morph.desktop"
    printf 'Installed local desktop file at %s/morph.desktop\n' "$LOCAL_DESKTOP_DIR"
}

print_sudo_help() {
    printf 'Display-manager-visible dev session install:\n'
    printf '  sudo ln -sf %s %s\n' "$COMP_ROOT_DIR/testing/morph_run" "$SYSTEM_DEV_LAUNCHER_TARGET"
    printf '  sudo install -m 0644 %s %s\n' "$COMP_ROOT_DIR/sessions/morph-dev.desktop" "$SYSTEM_DEV_DESKTOP_TARGET"
    printf '\n'
    printf 'Optional production desktop install from the current build layout:\n'
    printf '  sudo install -m 0644 %s %s\n' "$COMP_ROOT_DIR/sessions/morph.desktop" "$SYSTEM_DESKTOP_TARGET"
}

ACTION="${1:-}"
shift || true

LINK_LAUNCHER=0
DESKTOP_LOCAL=0
PRINT_SUDO_HELP=0

while [ $# -gt 0 ]; do
    case "$1" in
        --link-launcher) LINK_LAUNCHER=1 ;;
        --desktop-local) DESKTOP_LOCAL=1 ;;
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
        install_user_links
        if [ "$LINK_LAUNCHER" -eq 1 ]; then
            install_local_launcher_link
        fi
        if [ "$DESKTOP_LOCAL" -eq 1 ]; then
            install_local_desktop_copy
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
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
