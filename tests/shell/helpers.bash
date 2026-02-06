# SPDX-License-Identifier: GPL-3.0-or-later
# Shared test helpers for bats tests
# Source this from setup() in each .bats file

# Stub out logging functions (not available outside installer)
log_info() { :; }
log_warn() { :; }
log_error() { :; }
log_success() { :; }
export -f log_info log_warn log_error log_success

# Create a mock command that outputs specific text
# Usage: mock_command "systemctl" "User=biqu"
mock_command() {
    local cmd="$1" output="$2"
    mkdir -p "$BATS_TEST_TMPDIR/bin"
    cat > "$BATS_TEST_TMPDIR/bin/$cmd" << MOCK_EOF
#!/bin/sh
echo "$output"
MOCK_EOF
    chmod +x "$BATS_TEST_TMPDIR/bin/$cmd"
    export PATH="$BATS_TEST_TMPDIR/bin:$PATH"
}

# Create a mock command that fails (exits non-zero)
mock_command_fail() {
    local cmd="$1"
    mkdir -p "$BATS_TEST_TMPDIR/bin"
    printf '#!/bin/sh\nexit 1\n' > "$BATS_TEST_TMPDIR/bin/$cmd"
    chmod +x "$BATS_TEST_TMPDIR/bin/$cmd"
    export PATH="$BATS_TEST_TMPDIR/bin:$PATH"
}

# Create a mock command with custom script body
# Usage: mock_command_script "systemctl" 'case "$1" in show) echo "User=biqu";; *) exit 1;; esac'
mock_command_script() {
    local cmd="$1" body="$2"
    mkdir -p "$BATS_TEST_TMPDIR/bin"
    cat > "$BATS_TEST_TMPDIR/bin/$cmd" << MOCK_EOF
#!/bin/sh
$body
MOCK_EOF
    chmod +x "$BATS_TEST_TMPDIR/bin/$cmd"
    export PATH="$BATS_TEST_TMPDIR/bin:$PATH"
}

# Create temp directory structure mimicking a Pi system
setup_mock_pi() {
    export MOCK_ROOT="$BATS_TEST_TMPDIR/root"
    mkdir -p "$MOCK_ROOT/home/biqu/printer_data/config"
    mkdir -p "$MOCK_ROOT/opt/helixscreen/config"
}

# SUDO stub (no-op for tests)
SUDO=""
export SUDO
