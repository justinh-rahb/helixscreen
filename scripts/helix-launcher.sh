#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# helix-launcher.sh - Launch HelixScreen with watchdog supervision
#
# When watchdog is available (embedded targets), it manages the splash screen
# lifecycle and provides crash recovery. Otherwise, launches helix-screen directly.
#
# NOTE: Written for POSIX sh compatibility (no bash arrays) to work on AD5M BusyBox.
#
# Usage:
#   ./helix-launcher.sh [options]
#
# Launcher-specific options:
#   --debug              Enable debug-level logging (-vv)
#   --log-dest=<dest>    Log destination: auto, journal, syslog, file, console
#   --log-file=<path>    Log file path (when --log-dest=file)
#
# Environment variables:
#   HELIX_DATA_DIR=<d>   Override asset directory (ui_xml/, assets/, config/)
#   HELIX_DEBUG=1        Same as --debug
#   HELIX_LOG_DEST=<d>   Same as --log-dest (auto|journal|syslog|file|console)
#   HELIX_LOG_FILE=<f>   Same as --log-file
#
# All other options are passed through to helix-screen.
#
# Logging behavior:
#   - When run via systemd: auto-detects journal (recommended)
#   - When run interactively: auto-detects console
#   - Use --log-dest=file --log-file=/path for explicit file logging
#
# Installation:
#   Copy to /opt/helixscreen/bin/ or similar
#   Make executable: chmod +x helix-launcher.sh
#   Use with systemd service: config/helixscreen.service

set -e

# Hide the Linux console text cursor (visible as a blinking block on fbdev)
setterm --cursor off 2>/dev/null || printf '\033[?25l' > /dev/tty1 2>/dev/null || true

# Parse launcher-specific arguments (POSIX-compatible, no arrays)
# Passthrough args stored as space-separated string
# CLI flags take priority over env vars; env vars are applied after env file sourcing below
PASSTHROUGH_ARGS=""
CLI_DEBUG=""
CLI_LOG_DEST=""
CLI_LOG_FILE=""
for arg in "$@"; do
    case "$arg" in
        --debug)
            CLI_DEBUG=1
            ;;
        --log-dest=*)
            CLI_LOG_DEST="${arg#--log-dest=}"
            ;;
        --log-file=*)
            CLI_LOG_FILE="${arg#--log-file=}"
            ;;
        *)
            PASSTHROUGH_ARGS="${PASSTHROUGH_ARGS} ${arg}"
            ;;
    esac
done

# Determine script and binary locations
# Use $0 instead of BASH_SOURCE for POSIX compatibility
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Support installed and development layouts
# Installed: launcher is in bin/ alongside binaries
# Development: launcher is in scripts/, binaries in build/bin/
if [ -x "${SCRIPT_DIR}/helix-screen" ]; then
    # Installed: binaries in same directory as launcher (bin/)
    BIN_DIR="${SCRIPT_DIR}"
elif [ -x "${SCRIPT_DIR}/../build/bin/helix-screen" ]; then
    # Development: launcher in scripts/, binaries in build/bin/
    BIN_DIR="${SCRIPT_DIR}/../build/bin"
else
    echo "Error: Cannot find helix-screen binary" >&2
    echo "Looked in: ${SCRIPT_DIR} and ${SCRIPT_DIR}/../build/bin" >&2
    exit 1
fi

SPLASH_BIN="${BIN_DIR}/helix-splash"
MAIN_BIN="${BIN_DIR}/helix-screen"
WATCHDOG_BIN="${BIN_DIR}/helix-watchdog"

# Derive the install root (parent of bin/)
INSTALL_DIR="$(cd "${BIN_DIR}/.." && pwd)"

# Ensure SSL certificate verification works for HTTPS requests (e.g., update checker).
# Static glibc builds embed OpenSSL with compiled-in cert paths from the Docker build
# container, which don't exist on the target device. Set SSL_CERT_FILE to a valid path.
if [ -z "${SSL_CERT_FILE:-}" ]; then
    for _cert_path in \
        /etc/ssl/certs/ca-certificates.crt \
        /etc/pki/tls/certs/ca-bundle.crt \
        /etc/ssl/cert.pem \
        "${INSTALL_DIR}/certs/ca-certificates.crt"; do
        if [ -f "$_cert_path" ]; then
            export SSL_CERT_FILE="$_cert_path"
            break
        fi
    done
    unset _cert_path
fi

# Source environment configuration file if present.
# Supports both installed (/etc/helixscreen/) and deployed (config/) locations.
# Variables already set in the environment take precedence — the env file only
# provides defaults for unset variables.
_helix_env_file=""
for _env_path in \
    "${INSTALL_DIR}/config/helixscreen.env" \
    /etc/helixscreen/helixscreen.env; do
    if [ -f "$_env_path" ]; then
        _helix_env_file="$_env_path"
        break
    fi
done
unset _env_path

if [ -n "$_helix_env_file" ]; then
    # Read each VAR=value line; only export if not already set
    while IFS= read -r _line || [ -n "$_line" ]; do
        # Skip comments and blank lines
        case "$_line" in
            '#'*|'') continue ;;
        esac
        _var="${_line%%=*}"
        # Only set if not already in environment
        eval "_existing=\"\${${_var}:-}\"" 2>/dev/null || continue
        if [ -z "$_existing" ]; then
            eval "export $_line" 2>/dev/null || true
        fi
    done < "$_helix_env_file"
    unset _line _var _existing
fi
unset _helix_env_file

# Resolve debug/logging settings: CLI flags > env vars (incl. env file) > defaults
DEBUG_MODE="${CLI_DEBUG:-${HELIX_DEBUG:-0}}"
LOG_DEST="${CLI_LOG_DEST:-${HELIX_LOG_DEST:-auto}}"
LOG_FILE="${CLI_LOG_FILE:-${HELIX_LOG_FILE:-}}"

# Default display backend to fbdev on embedded Linux targets.
# DRM atomic modesetting has compatibility issues with some SoCs and breaks
# framebuffer-based VNC setups. fbdev works reliably across all Pi hardware.
# Can be overridden by setting HELIX_DISPLAY_BACKEND in the environment or
# systemd service file.
if [ -z "${HELIX_DISPLAY_BACKEND:-}" ]; then
    case "$(uname -s)" in
        Linux)
            export HELIX_DISPLAY_BACKEND=fbdev
            ;;
    esac
fi

# Log function (must be defined before first use)
# Uses stderr to avoid polluting stdout which could be captured unexpectedly
log() {
    echo "[helix-launcher] $*" >&2
}

# Verify main binary exists
if [ ! -x "${MAIN_BIN}" ]; then
    echo "Error: Cannot find helix-screen binary at ${MAIN_BIN}" >&2
    exit 1
fi

# Check if watchdog is available (embedded targets only, provides crash recovery)
USE_WATCHDOG=0
if [ -x "${WATCHDOG_BIN}" ]; then
    USE_WATCHDOG=1
    log "Watchdog available: crash recovery enabled"
fi

# Check if splash is already running (started by init script for earlier visibility)
# If so, pass the PID to helix-screen for cleanup, and don't start another
# HELIX_NO_SPLASH=1 disables splash entirely (for debugging)
SPLASH_ARGS=""
if [ "${HELIX_NO_SPLASH:-0}" = "1" ]; then
    log "Splash disabled (HELIX_NO_SPLASH=1)"
elif [ -n "${HELIX_SPLASH_PID}" ]; then
    # Splash was pre-started by init script, pass PID to watchdog (before --)
    # so watchdog can forward it to helix-screen on first launch
    SPLASH_ARGS="--splash-pid=${HELIX_SPLASH_PID}"
    log "Using pre-started splash (PID ${HELIX_SPLASH_PID})"
elif [ -x "${SPLASH_BIN}" ]; then
    # No pre-started splash, let watchdog manage it
    SPLASH_ARGS="--splash-bin=${SPLASH_BIN}"
    log "Splash binary: ${SPLASH_BIN}"
fi

# Cleanup function for signal handling
cleanup() {
    log "Shutting down..."
    # Kill watchdog/helix-screen if we started them
    killall helix-watchdog helix-screen helix-splash 2>/dev/null || true
}

trap cleanup EXIT INT TERM

log "Starting main application"

# Build command flags
EXTRA_FLAGS=""

# Debug mode: debug-level logging
if [ "${DEBUG_MODE}" = "1" ]; then
    EXTRA_FLAGS="-vv"
    log "Debug mode enabled (debug-level logging)"
fi

# Logging destination
if [ "${LOG_DEST}" != "auto" ]; then
    EXTRA_FLAGS="${EXTRA_FLAGS} --log-dest=${LOG_DEST}"
    log "Log destination: ${LOG_DEST}"
fi

# Explicit log file path (only meaningful with --log-dest=file)
if [ -n "${LOG_FILE}" ]; then
    EXTRA_FLAGS="${EXTRA_FLAGS} --log-file=${LOG_FILE}"
    log "Log file: ${LOG_FILE}"
fi

# Run main application (via watchdog if available for crash recovery)
# Note: PASSTHROUGH_ARGS is unquoted to allow word splitting (POSIX compatible)
if [ "${USE_WATCHDOG}" = "1" ]; then
    # Watchdog supervises helix-screen and manages splash lifecycle
    # Watchdog and splash auto-detect resolution from display hardware
    log "Starting via watchdog supervisor"
    # shellcheck disable=SC2086
    "${WATCHDOG_BIN}" ${SPLASH_ARGS} -- \
        "${MAIN_BIN}" ${EXTRA_FLAGS} ${PASSTHROUGH_ARGS}
    EXIT_CODE=$?
else
    # Direct launch (development, or watchdog not built)
    # shellcheck disable=SC2086
    "${MAIN_BIN}" ${EXTRA_FLAGS} ${PASSTHROUGH_ARGS}
    EXIT_CODE=$?
fi

log "Exiting with code ${EXIT_CODE}"
exit ${EXIT_CODE}
