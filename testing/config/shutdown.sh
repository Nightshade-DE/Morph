#!/bin/sh
# Development morph shutdown hook.
# Keep this file focused on user-facing shutdown content:
# - optional user cleanup
# - optional user-managed service shutdown
# The managed runtime invokes this hook before compositor-owned cleanup begins.
################################################################################

# Helper commands available in this hook:
# - log_shutdown <lvl> <msg> Write a message into the standard shutdown log.

# Activate helper functions for shutdown logging so user cleanup can emit
# messages into the standard shutdown log when needed.
CURRENT_LOG_FILE="${MORPH_SHUTDOWN_LOG_FILE:?MORPH_SHUTDOWN_LOG_FILE is not set}"
. "${MORPH_HELPER_LIB:?MORPH_HELPER_LIB is not set}"

# Optional user cleanup goes here.
# Example:
# log_shutdown INFO "Running user shutdown cleanup."
# pkill -x my-user-service
#
# Full example:
# log_shutdown INFO "Stopping user-managed session components."
# pkill -x my-user-service
# pkill -x my-other-helper
