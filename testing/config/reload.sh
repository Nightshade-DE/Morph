#!/bin/sh
# Development morph reload hook.
# Keep this file focused on user-facing reload actions:
# - restart panels or bars after config reload
# - refresh optional user session components
# The managed runtime invokes this hook after the new config is active.
################################################################################

# Helper commands available in this hook:
# - reload <cmd ...>       Restart one managed session component in-place.
# - reload_once <cmd ...>  Start one managed component only when it is not already running.
# - launch <cmd ...>       Start an additional managed component during reload.
# - launch_nokill <cmd ...> Start a temporary component without shutdown tracking.
# - log_message <lvl> <msg> Write a message into the current managed runtime log.

# Optional user reload actions go here.
# Full example:
# log_message INFO "Reloading user-managed session components."
# reload sfwbar
# reload dunst
# reload_once mutagen
