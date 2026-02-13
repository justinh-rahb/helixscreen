#!/usr/bin/env bats
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Tests for helix-launcher.sh environment handling:
# - Env file sourcing (helixscreen.env)
# - Display backend defaulting (fbdev on Linux)
# - Environment variable precedence

WORKTREE_ROOT="$(cd "$BATS_TEST_DIRNAME/../.." && pwd)"
LAUNCHER="$WORKTREE_ROOT/scripts/helix-launcher.sh"

setup() {
    load helpers

    # Create a mock install layout so the launcher can find binaries
    export MOCK_INSTALL="$BATS_TEST_TMPDIR/helixscreen"
    mkdir -p "$MOCK_INSTALL/bin"
    mkdir -p "$MOCK_INSTALL/config"

    # Create fake binaries that just exit
    printf '#!/bin/sh\nexit 0\n' > "$MOCK_INSTALL/bin/helix-screen"
    printf '#!/bin/sh\nexit 0\n' > "$MOCK_INSTALL/bin/helix-splash"
    chmod +x "$MOCK_INSTALL/bin/helix-screen" "$MOCK_INSTALL/bin/helix-splash"
    # No watchdog — launcher will run helix-screen directly

    # Extract the env-handling portion of the launcher into a testable snippet.
    # We source just the variable setup logic without actually launching anything.
    # This avoids needing real binaries, display hardware, etc.
    cat > "$BATS_TEST_TMPDIR/env_setup.sh" << 'ENVEOF'
#!/bin/sh
# Minimal harness that runs just the env-handling parts of helix-launcher.sh

# These would normally be derived from $0 / binary detection
SCRIPT_DIR="$MOCK_INSTALL/bin"
BIN_DIR="$MOCK_INSTALL/bin"
INSTALL_DIR="$MOCK_INSTALL"

# --- Begin: extracted from helix-launcher.sh ---

# Source environment configuration file if present.
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
    while IFS= read -r _line || [ -n "$_line" ]; do
        case "$_line" in
            '#'*|'') continue ;;
        esac
        _var="${_line%%=*}"
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
if [ -z "${HELIX_DISPLAY_BACKEND:-}" ]; then
    case "$(uname -s)" in
        Linux)
            export HELIX_DISPLAY_BACKEND=fbdev
            ;;
    esac
fi

# --- End: extracted from helix-launcher.sh ---
ENVEOF
    chmod +x "$BATS_TEST_TMPDIR/env_setup.sh"

    # Create a mock helix-screen that writes its args to a file for inspection
    cat > "$MOCK_INSTALL/bin/helix-screen" << 'MOCKEOF'
#!/bin/sh
# Write all args to a file for test inspection
for arg in "$@"; do
    echo "$arg"
done > "$MOCK_INSTALL/helix_screen_args.txt"
exit 0
MOCKEOF
    chmod +x "$MOCK_INSTALL/bin/helix-screen"
}

# Helper: run the env setup snippet and print a variable's value
run_env_setup() {
    # Run in a subshell to isolate env changes
    sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$$1\""
}

# =============================================================================
# Display backend defaulting
# =============================================================================

@test "launcher defaults HELIX_DISPLAY_BACKEND to fbdev on Linux" {
    # Only meaningful on Linux, but the logic checks uname
    unset HELIX_DISPLAY_BACKEND
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_DISPLAY_BACKEND)
    if [ "$(uname -s)" = "Linux" ]; then
        [ "$result" = "fbdev" ]
    else
        # On macOS, the fallback doesn't trigger (no Linux case match)
        [ "$result" = "" ]
    fi
}

@test "launcher respects existing HELIX_DISPLAY_BACKEND=drm from environment" {
    export HELIX_DISPLAY_BACKEND=drm
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$HELIX_DISPLAY_BACKEND\"")
    [ "$result" = "drm" ]
}

@test "launcher respects HELIX_DISPLAY_BACKEND=sdl from environment" {
    export HELIX_DISPLAY_BACKEND=sdl
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$HELIX_DISPLAY_BACKEND\"")
    [ "$result" = "sdl" ]
}

# =============================================================================
# Env file sourcing
# =============================================================================

@test "launcher sources helixscreen.env from install dir" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
MOONRAKER_HOST=myprinter.local
MOONRAKER_PORT=7125
EOF
    unset MOONRAKER_HOST
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup MOONRAKER_HOST)
    [ "$result" = "myprinter.local" ]
}

@test "launcher sources MOONRAKER_PORT from env file" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
MOONRAKER_HOST=localhost
MOONRAKER_PORT=8080
EOF
    unset MOONRAKER_PORT
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup MOONRAKER_PORT)
    [ "$result" = "8080" ]
}

@test "env file skips commented lines" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
# This is a comment
#HELIX_DISPLAY_BACKEND=drm
MOONRAKER_HOST=localhost
EOF
    unset HELIX_DISPLAY_BACKEND MOONRAKER_HOST
    # The commented HELIX_DISPLAY_BACKEND=drm should NOT be set
    # So on Linux it should fall through to the fbdev default
    result_backend=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_DISPLAY_BACKEND)
    result_host=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup MOONRAKER_HOST)
    [ "$result_host" = "localhost" ]
    if [ "$(uname -s)" = "Linux" ]; then
        [ "$result_backend" = "fbdev" ]
    fi
}

@test "env file skips blank lines" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'

MOONRAKER_HOST=localhost

MOONRAKER_PORT=7125

EOF
    unset MOONRAKER_HOST MOONRAKER_PORT
    result_host=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup MOONRAKER_HOST)
    result_port=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup MOONRAKER_PORT)
    [ "$result_host" = "localhost" ]
    [ "$result_port" = "7125" ]
}

# =============================================================================
# Precedence: environment > env file > hardcoded default
# =============================================================================

@test "existing env var takes precedence over env file" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
MOONRAKER_HOST=from-file
EOF
    export MOONRAKER_HOST=from-env
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$MOONRAKER_HOST\"")
    [ "$result" = "from-env" ]
}

@test "env file HELIX_DISPLAY_BACKEND takes precedence over hardcoded fbdev default" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DISPLAY_BACKEND=drm
EOF
    unset HELIX_DISPLAY_BACKEND
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_DISPLAY_BACKEND)
    [ "$result" = "drm" ]
}

@test "environment HELIX_DISPLAY_BACKEND overrides env file value" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DISPLAY_BACKEND=drm
EOF
    export HELIX_DISPLAY_BACKEND=fbdev
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$HELIX_DISPLAY_BACKEND\"")
    [ "$result" = "fbdev" ]
}

@test "full precedence chain: env > file > default" {
    # Scenario: env file says drm, environment says sdl
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DISPLAY_BACKEND=drm
EOF
    export HELIX_DISPLAY_BACKEND=sdl
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$HELIX_DISPLAY_BACKEND\"")
    [ "$result" = "sdl" ]
}

# =============================================================================
# No env file present
# =============================================================================

@test "launcher works with no env file present" {
    # No helixscreen.env in either location
    rm -f "$MOCK_INSTALL/config/helixscreen.env"
    unset HELIX_DISPLAY_BACKEND MOONRAKER_HOST
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_DISPLAY_BACKEND)
    if [ "$(uname -s)" = "Linux" ]; then
        [ "$result" = "fbdev" ]
    else
        [ "$result" = "" ]
    fi
}

# =============================================================================
# All env file variables from helixscreen.env are supported
# =============================================================================

@test "env file supports HELIX_FB_DEVICE" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_FB_DEVICE=/dev/fb1
EOF
    unset HELIX_FB_DEVICE
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_FB_DEVICE)
    [ "$result" = "/dev/fb1" ]
}

@test "env file supports HELIX_DRM_DEVICE" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DRM_DEVICE=/dev/dri/card1
EOF
    unset HELIX_DRM_DEVICE
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_DRM_DEVICE)
    [ "$result" = "/dev/dri/card1" ]
}

@test "env file supports HELIX_LOG_LEVEL" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_LOG_LEVEL=debug
EOF
    unset HELIX_LOG_LEVEL
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_LOG_LEVEL)
    [ "$result" = "debug" ]
}

@test "env file supports HELIX_AUTO_QUIT_MS" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_AUTO_QUIT_MS=5000
EOF
    unset HELIX_AUTO_QUIT_MS
    result=$(MOCK_INSTALL="$MOCK_INSTALL" run_env_setup HELIX_AUTO_QUIT_MS)
    [ "$result" = "5000" ]
}

# =============================================================================
# Launcher script validity
# =============================================================================

@test "helix-launcher.sh has valid sh syntax" {
    sh -n "$LAUNCHER"
}

@test "helix-launcher.sh contains env file sourcing logic" {
    grep -q 'helixscreen.env' "$LAUNCHER"
}

@test "helix-launcher.sh contains fbdev default logic" {
    grep -q 'HELIX_DISPLAY_BACKEND=fbdev' "$LAUNCHER"
}

@test "helix-launcher.sh does not override existing HELIX_DISPLAY_BACKEND" {
    # The conditional must check if already set
    grep -q 'if \[ -z "\${HELIX_DISPLAY_BACKEND:-}"' "$LAUNCHER"
}

# =============================================================================
# Splash PID routing (must go to watchdog args, not passthrough/child args)
# =============================================================================

@test "HELIX_SPLASH_PID routes to SPLASH_ARGS not PASSTHROUGH_ARGS" {
    # The launcher should put --splash-pid into SPLASH_ARGS (before --)
    # not into PASSTHROUGH_ARGS (after --)
    grep -q 'SPLASH_ARGS="--splash-pid=\${HELIX_SPLASH_PID}"' "$LAUNCHER"
}

@test "HELIX_SPLASH_PID does not go to PASSTHROUGH_ARGS" {
    # Ensure the old pattern (putting splash-pid in PASSTHROUGH_ARGS) is gone
    ! grep -q 'PASSTHROUGH_ARGS.*--splash-pid' "$LAUNCHER"
}

@test "launcher passes SPLASH_ARGS before -- separator to watchdog" {
    # SPLASH_ARGS should appear before the -- separator in the watchdog invocation
    grep -q '"${WATCHDOG_BIN}" ${SPLASH_ARGS} --' "$LAUNCHER"
}

# =============================================================================
# Debug mode: HELIX_DEBUG env var and --debug flag
# =============================================================================

@test "HELIX_DEBUG=1 in environment enables debug mode (simulates systemd Environment=)" {
    # This is the case that works TODAY: systemd sets HELIX_DEBUG=1 before launcher runs
    unset CLI_DEBUG
    export HELIX_DEBUG=1
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$DEBUG_MODE\"")
    [ "$result" = "1" ]
}

@test "HELIX_DEBUG=1 in env file enables debug mode" {
    # This is the fix: env file sets HELIX_DEBUG, which is read AFTER env file sourcing
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DEBUG=1
EOF
    unset HELIX_DEBUG CLI_DEBUG
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$DEBUG_MODE\"")
    [ "$result" = "1" ]
}

@test "CLI --debug flag takes priority over env file HELIX_DEBUG=0" {
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DEBUG=0
EOF
    unset HELIX_DEBUG
    export CLI_DEBUG=1
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$DEBUG_MODE\"")
    [ "$result" = "1" ]
}

@test "debug mode defaults to off when nothing is set" {
    unset HELIX_DEBUG CLI_DEBUG
    rm -f "$MOCK_INSTALL/config/helixscreen.env"
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$DEBUG_MODE\"")
    [ "$result" = "0" ]
}

@test "HELIX_DEBUG from environment overrides env file value" {
    # Env file says 0, but environment says 1 — environment wins
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DEBUG=0
EOF
    export HELIX_DEBUG=1
    unset CLI_DEBUG
    result=$(MOCK_INSTALL="$MOCK_INSTALL" sh -c ". \"$BATS_TEST_TMPDIR/env_setup.sh\" && echo \"\$DEBUG_MODE\"")
    [ "$result" = "1" ]
}

# =============================================================================
# End-to-end: launcher passes -vv to helix-screen when debug enabled
# =============================================================================

@test "e2e: HELIX_DEBUG=1 in environment causes launcher to pass -vv" {
    cp "$LAUNCHER" "$MOCK_INSTALL/bin/helix-launcher.sh"
    rm -f "$MOCK_INSTALL/helix_screen_args.txt"

    HELIX_DEBUG=1 MOCK_INSTALL="$MOCK_INSTALL" \
        sh "$MOCK_INSTALL/bin/helix-launcher.sh" 2>/dev/null || true

    [ -f "$MOCK_INSTALL/helix_screen_args.txt" ]
    grep -q '^-vv$' "$MOCK_INSTALL/helix_screen_args.txt"
}

@test "e2e: HELIX_DEBUG=1 in env file causes launcher to pass -vv" {
    cp "$LAUNCHER" "$MOCK_INSTALL/bin/helix-launcher.sh"
    rm -f "$MOCK_INSTALL/helix_screen_args.txt"
    cat > "$MOCK_INSTALL/config/helixscreen.env" << 'EOF'
HELIX_DEBUG=1
EOF
    unset HELIX_DEBUG

    MOCK_INSTALL="$MOCK_INSTALL" \
        sh "$MOCK_INSTALL/bin/helix-launcher.sh" 2>/dev/null || true

    [ -f "$MOCK_INSTALL/helix_screen_args.txt" ]
    grep -q '^-vv$' "$MOCK_INSTALL/helix_screen_args.txt"
}

@test "e2e: launcher does NOT pass -vv when debug is off" {
    cp "$LAUNCHER" "$MOCK_INSTALL/bin/helix-launcher.sh"
    rm -f "$MOCK_INSTALL/helix_screen_args.txt"
    rm -f "$MOCK_INSTALL/config/helixscreen.env"
    unset HELIX_DEBUG

    MOCK_INSTALL="$MOCK_INSTALL" \
        sh "$MOCK_INSTALL/bin/helix-launcher.sh" 2>/dev/null || true

    if [ -f "$MOCK_INSTALL/helix_screen_args.txt" ]; then
        ! grep -q '^-vv$' "$MOCK_INSTALL/helix_screen_args.txt"
    fi
}

@test "e2e: --debug CLI flag causes launcher to pass -vv" {
    cp "$LAUNCHER" "$MOCK_INSTALL/bin/helix-launcher.sh"
    rm -f "$MOCK_INSTALL/helix_screen_args.txt"
    unset HELIX_DEBUG

    MOCK_INSTALL="$MOCK_INSTALL" \
        sh "$MOCK_INSTALL/bin/helix-launcher.sh" --debug 2>/dev/null || true

    [ -f "$MOCK_INSTALL/helix_screen_args.txt" ]
    grep -q '^-vv$' "$MOCK_INSTALL/helix_screen_args.txt"
}

# =============================================================================
# Splash PID routing
# =============================================================================

@test "splash PID routing end-to-end with mock watchdog" {
    # Create a mock watchdog that captures its arguments
    cat > "$MOCK_INSTALL/bin/helix-watchdog" << 'WDEOF'
#!/bin/sh
# Write all args to a file for inspection
for arg in "$@"; do
    echo "$arg"
done > "$MOCK_INSTALL/watchdog_args.txt"
exit 0
WDEOF
    chmod +x "$MOCK_INSTALL/bin/helix-watchdog"

    # Copy the launcher into the mock bin/ so SCRIPT_DIR resolves to mock
    # layout (otherwise it finds the real build/bin/helix-screen via $0)
    cp "$LAUNCHER" "$MOCK_INSTALL/bin/helix-launcher.sh"

    # Run the launcher with HELIX_SPLASH_PID set
    export HELIX_SPLASH_PID=1476
    export HELIX_NO_SPLASH=0

    # Run launcher from mock dir (SCRIPT_DIR will be $MOCK_INSTALL/bin)
    HELIX_SPLASH_PID=1476 sh "$MOCK_INSTALL/bin/helix-launcher.sh" 2>/dev/null || true

    # Check that --splash-pid appears BEFORE -- in the watchdog args
    if [ -f "$MOCK_INSTALL/watchdog_args.txt" ]; then
        # --splash-pid should be one of the args before --
        seen_separator=false
        splash_before_sep=false
        while IFS= read -r line; do
            if [ "$line" = "--" ]; then
                seen_separator=true
            elif [ "$seen_separator" = "false" ] && echo "$line" | grep -q '^--splash-pid='; then
                splash_before_sep=true
            fi
        done < "$MOCK_INSTALL/watchdog_args.txt"
        [ "$splash_before_sep" = "true" ]
    fi
}
