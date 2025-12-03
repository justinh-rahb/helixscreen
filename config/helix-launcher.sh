#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# helix-launcher.sh - Launch HelixScreen with early splash screen
#
# This script starts the splash screen immediately for instant visual feedback,
# then launches the main application in parallel. The splash automatically exits
# when the main app takes over the display.
#
# Usage:
#   ./helix-launcher.sh [options]
#
# All options are passed through to helix-ui-proto.
#
# Installation:
#   Copy to /opt/helixscreen/bin/ or similar
#   Make executable: chmod +x helix-launcher.sh
#   Use with systemd service: config/helixscreen.service

set -e

# Determine script and binary locations
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Support both installed and development layouts
if [ -x "${SCRIPT_DIR}/helix-splash" ]; then
    # Installed: binaries in same directory as script
    BIN_DIR="${SCRIPT_DIR}"
elif [ -x "${SCRIPT_DIR}/../build/bin/helix-splash" ]; then
    # Development: binaries in build/bin relative to config/
    BIN_DIR="${SCRIPT_DIR}/../build/bin"
else
    echo "Error: Cannot find helix-splash binary" >&2
    echo "Looked in: ${SCRIPT_DIR} and ${SCRIPT_DIR}/../build/bin" >&2
    exit 1
fi

SPLASH_BIN="${BIN_DIR}/helix-splash"
MAIN_BIN="${BIN_DIR}/helix-ui-proto"

# Verify main binary exists
if [ ! -x "${MAIN_BIN}" ]; then
    echo "Error: Cannot find helix-ui-proto binary at ${MAIN_BIN}" >&2
    exit 1
fi

# Default screen dimensions (can be overridden by environment)
: "${HELIX_SCREEN_WIDTH:=800}"
: "${HELIX_SCREEN_HEIGHT:=480}"

# Log function
log() {
    echo "[helix-launcher] $*"
}

# Cleanup function for signal handling
cleanup() {
    log "Shutting down..."
    if [ -n "${SPLASH_PID}" ] && kill -0 "${SPLASH_PID}" 2>/dev/null; then
        kill "${SPLASH_PID}" 2>/dev/null || true
        wait "${SPLASH_PID}" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

# Start splash screen in background (if binary exists)
if [ -x "${SPLASH_BIN}" ]; then
    log "Starting splash screen (${HELIX_SCREEN_WIDTH}x${HELIX_SCREEN_HEIGHT})"
    "${SPLASH_BIN}" -w "${HELIX_SCREEN_WIDTH}" -h "${HELIX_SCREEN_HEIGHT}" &
    SPLASH_PID=$!
    log "Splash PID: ${SPLASH_PID}"

    # Pass splash PID to main app so it can signal when ready
    SPLASH_ARGS="--splash-pid=${SPLASH_PID}"
else
    log "Splash binary not found, starting main app directly"
    SPLASH_PID=""
    SPLASH_ARGS=""
fi

# Start main application
# Main app will send SIGUSR1 to splash when it takes over the display
log "Starting main application"
"${MAIN_BIN}" ${SPLASH_ARGS} "$@"
EXIT_CODE=$?

# Ensure splash is terminated (should have exited when main app took display)
if [ -n "${SPLASH_PID}" ] && kill -0 "${SPLASH_PID}" 2>/dev/null; then
    log "Cleaning up splash process"
    kill "${SPLASH_PID}" 2>/dev/null || true
    wait "${SPLASH_PID}" 2>/dev/null || true
fi

log "Exiting with code ${EXIT_CODE}"
exit ${EXIT_CODE}
