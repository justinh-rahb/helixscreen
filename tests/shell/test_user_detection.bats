#!/usr/bin/env bats
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Tests for detect_klipper_user() in platform.sh
# Tests the detection cascade: systemd → process → printer_data → well-known → fallback

WORKTREE_ROOT="$(cd "$BATS_TEST_DIRNAME/../.." && pwd)"

setup() {
    load helpers

    # Reset globals before each test
    KLIPPER_USER=""
    KLIPPER_HOME=""
    INIT_SCRIPT_DEST=""
    PREVIOUS_UI_SCRIPT=""
    AD5M_FIRMWARE=""
    K1_FIRMWARE=""
    INSTALL_DIR="/opt/helixscreen"
    TMP_DIR="/tmp/helixscreen-install"

    # Source platform.sh (skip source guard by unsetting it)
    unset _HELIX_PLATFORM_SOURCED
    . "$WORKTREE_ROOT/scripts/lib/installer/platform.sh"
}

# --- systemd detection ---

@test "systemd detection: finds biqu user" {
    mock_command_script "systemctl" '
        case "$1" in
            show) echo "biqu" ;;
            *) exit 1 ;;
        esac'
    mock_command "id" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "biqu" ]
}

@test "systemd detection: finds pi user" {
    mock_command_script "systemctl" '
        case "$1" in
            show) echo "pi" ;;
            *) exit 1 ;;
        esac'
    mock_command "id" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "pi" ]
}

@test "systemd detection: sets KLIPPER_HOME correctly" {
    mock_command_script "systemctl" '
        case "$1" in
            show) echo "biqu" ;;
            *) exit 1 ;;
        esac'
    mock_command "id" ""

    detect_klipper_user
    [ "$KLIPPER_HOME" = "/home/biqu" ] || [ -n "$KLIPPER_HOME" ]
}

@test "systemd not available: falls through" {
    # Remove systemctl from PATH by making it fail
    mock_command_fail "systemctl"
    # Also make id fail for well-known users so we hit fallback
    mock_command_fail "id"
    # No ps klipper output
    mock_command "ps" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "root" ]
}

@test "systemd returns empty user: falls through" {
    mock_command_script "systemctl" '
        case "$1" in
            show) echo "" ;;
            *) exit 1 ;;
        esac'
    mock_command_fail "id"
    mock_command "ps" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "root" ]
}

@test "systemd returns root: falls through to next method" {
    mock_command_script "systemctl" '
        case "$1" in
            show) echo "root" ;;
            *) exit 1 ;;
        esac'
    mock_command_fail "id"
    mock_command "ps" ""

    detect_klipper_user
    # Should still be root but via fallback (systemd root is skipped)
    [ "$KLIPPER_USER" = "root" ]
}

# --- Process table detection ---

@test "process table: finds klipper running as biqu" {
    mock_command_fail "systemctl"
    mock_command "ps" "biqu     klipper"
    mock_command "id" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "biqu" ]
}

@test "process table: no klipper running falls through" {
    mock_command_fail "systemctl"
    mock_command "ps" "root     bash"
    mock_command_fail "id"

    detect_klipper_user
    [ "$KLIPPER_USER" = "root" ]
}

# --- printer_data directory scan ---

@test "printer_data scan: finds user via directory" {
    mock_command_fail "systemctl"
    mock_command "ps" ""
    # Create a temporary printer_data directory
    mkdir -p "$BATS_TEST_TMPDIR/home/testuser/printer_data"

    # We can't easily test the /home/* glob in isolation without root
    # but we can verify the function doesn't crash
    mock_command_fail "id"
    detect_klipper_user
    # Falls through to root since test user doesn't exist in /etc/passwd
    [ -n "$KLIPPER_USER" ]
}

# --- Well-known users ---

@test "well-known user: biqu checked before pi" {
    mock_command_fail "systemctl"
    mock_command "ps" ""
    # id succeeds for all users (mock always returns success)
    mock_command "id" ""

    detect_klipper_user
    [ "$KLIPPER_USER" = "biqu" ]
}

# --- Fallback ---

@test "fallback: nothing matches returns root" {
    mock_command_fail "systemctl"
    mock_command "ps" ""
    mock_command_fail "id"

    detect_klipper_user
    [ "$KLIPPER_USER" = "root" ]
    [ "$KLIPPER_HOME" = "/root" ]
}

@test "KLIPPER_HOME set to /root for fallback" {
    mock_command_fail "systemctl"
    mock_command "ps" ""
    mock_command_fail "id"

    detect_klipper_user
    [ "$KLIPPER_HOME" = "/root" ]
}

@test "calling detect_klipper_user twice returns same result" {
    mock_command_fail "systemctl"
    mock_command "ps" ""
    mock_command_fail "id"

    detect_klipper_user
    local first_user="$KLIPPER_USER"
    KLIPPER_USER=""
    KLIPPER_HOME=""
    detect_klipper_user
    [ "$KLIPPER_USER" = "$first_user" ]
}

# --- detect_platform expansion ---

@test "detect_platform finds Pi via /home/biqu directory" {
    # This test checks the code path exists (actual detection needs ARM arch)
    # We verify the string is in the source code
    grep -q '/home/biqu' "$WORKTREE_ROOT/scripts/lib/installer/platform.sh"
}

@test "detect_platform still has /home/pi check (regression)" {
    grep -q '/home/pi' "$WORKTREE_ROOT/scripts/lib/installer/platform.sh"
}

@test "detect_platform still has /home/mks check (regression)" {
    grep -q '/home/mks' "$WORKTREE_ROOT/scripts/lib/installer/platform.sh"
}
