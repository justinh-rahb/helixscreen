#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixScreen Installer
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash
#
# Or download and run:
#   wget https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh
#   chmod +x install.sh
#   ./install.sh
#
# Options:
#   --update    Update existing installation (preserves config)
#   --uninstall Remove HelixScreen
#   --version   Specify version (default: latest)
#

# Fail fast on any error
set -euo pipefail

# Configuration
GITHUB_REPO="prestonbrown/helixscreen"
INSTALL_DIR="/opt/helixscreen"
SERVICE_NAME="helixscreen"
TMP_DIR="/tmp/helixscreen-install"
INIT_SYSTEM=""  # Will be set to "systemd" or "sysv"

# Known competing screen UIs to stop
COMPETING_UIS="guppyscreen GuppyScreen KlipperScreen klipperscreen featherscreen FeatherScreen"

# Track what we've done for cleanup
CLEANUP_TMP=false
CLEANUP_SERVICE=false
BACKUP_CONFIG=""
ORIGINAL_INSTALL_EXISTS=false

# Colors (if terminal supports it)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    CYAN=''
    BOLD=''
    NC=''
fi

# Logging functions
log_info() { echo -e "${CYAN}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1" >&2; }

# Error handler - cleanup and report what went wrong
error_handler() {
    local exit_code=$?
    local line_no=$1

    echo ""
    log_error "=========================================="
    log_error "Installation FAILED at line $line_no"
    log_error "Exit code: $exit_code"
    log_error "=========================================="
    echo ""

    # Cleanup temporary files
    if [ "$CLEANUP_TMP" = true ] && [ -d "$TMP_DIR" ]; then
        log_info "Cleaning up temporary files..."
        rm -rf "$TMP_DIR"
    fi

    # If we backed up config and install failed, try to restore state
    if [ -n "$BACKUP_CONFIG" ] && [ -f "$BACKUP_CONFIG" ]; then
        log_info "Restoring backed up configuration..."
        if [ -d "$INSTALL_DIR" ]; then
            $SUDO cp "$BACKUP_CONFIG" "${INSTALL_DIR}/helixconfig.json" 2>/dev/null || true
        fi
    fi

    echo ""
    log_error "Installation was NOT completed."
    log_error "Your system should be in its original state."
    echo ""
    log_info "For help, please:"
    log_info "  1. Check the error message above"
    log_info "  2. Verify network connectivity"
    log_info "  3. Report issues at: https://github.com/${GITHUB_REPO}/issues"
    echo ""

    exit $exit_code
}

# Set up error trap
trap 'error_handler $LINENO' ERR

# Cleanup function for normal exit
cleanup_on_success() {
    if [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}

# Detect platform
detect_platform() {
    local arch=$(uname -m)
    local kernel=$(uname -r)

    # Check for AD5M (armv7l with specific kernel)
    if [ "$arch" = "armv7l" ]; then
        # AD5M has a specific kernel identifier
        if echo "$kernel" | grep -q "ad5m\|5.4.61"; then
            echo "ad5m"
            return
        fi
    fi

    # Check for Raspberry Pi (aarch64 or armv7l)
    if [ "$arch" = "aarch64" ] || [ "$arch" = "armv7l" ]; then
        if [ -f /etc/os-release ] && grep -q "Raspbian\|Debian" /etc/os-release; then
            echo "pi"
            return
        fi
        # Also check for MainsailOS
        if [ -d /home/pi ] || [ -d /home/mks ]; then
            echo "pi"
            return
        fi
    fi

    # Default to pi for unknown ARM platforms
    if [ "$arch" = "aarch64" ] || [ "$arch" = "armv7l" ]; then
        echo "pi"
        return
    fi

    echo "unsupported"
}

# Check if running as root (required for AD5M, optional for Pi)
check_permissions() {
    local platform=$1

    if [ "$platform" = "ad5m" ]; then
        if [ "$(id -u)" != "0" ]; then
            log_error "AD5M installation requires root privileges."
            log_error "Please run: sudo $0 $*"
            exit 1
        fi
        SUDO=""
    else
        # Pi: warn if not root but allow sudo
        if [ "$(id -u)" != "0" ]; then
            if ! command -v sudo &> /dev/null; then
                log_error "Not running as root and sudo is not available."
                log_error "Please run as root or install sudo."
                exit 1
            fi
            log_info "Not running as root. Will use sudo for privileged operations."
            SUDO="sudo"
        else
            SUDO=""
        fi
    fi
}

# Check required commands exist
check_requirements() {
    local missing=""

    # Need either curl or wget
    if ! command -v curl &> /dev/null && ! command -v wget &> /dev/null; then
        missing="curl or wget"
    fi

    # Need tar
    if ! command -v tar &> /dev/null; then
        missing="${missing:+$missing, }tar"
    fi

    # Need gunzip (for AD5M)
    if ! command -v gunzip &> /dev/null; then
        missing="${missing:+$missing, }gunzip"
    fi

    # Note: systemctl is optional - we support SysV init too

    if [ -n "$missing" ]; then
        log_error "Missing required commands: $missing"
        log_error "Please install them and try again."
        exit 1
    fi
}

# Install runtime dependencies for Pi platform
# Required for DRM display and evdev input handling
install_runtime_deps() {
    local platform=$1

    # Only needed for Pi - AD5M uses framebuffer with static linking
    if [ "$platform" != "pi" ]; then
        return 0
    fi

    log_info "Checking runtime dependencies for display/input..."

    # Required libraries for DRM display and libinput
    # Note: GPU libs (libgles2, libegl1, libgbm1) not needed - using software rendering
    local deps="libdrm2 libinput10"
    local missing=""

    for dep in $deps; do
        # Check if package is installed (dpkg-query returns 0 if installed)
        if ! dpkg-query -W -f='${Status}' "$dep" 2>/dev/null | grep -q "install ok installed"; then
            missing="${missing:+$missing }$dep"
        fi
    done

    if [ -n "$missing" ]; then
        log_info "Installing missing libraries: $missing"
        $SUDO apt-get update -qq
        # shellcheck disable=SC2086
        $SUDO apt-get install -y --no-install-recommends $missing
        log_success "Runtime libraries installed"
    else
        log_success "All runtime libraries already installed"
    fi
}

# Check available disk space
check_disk_space() {
    local platform=$1
    local required_mb=50  # Need at least 50MB

    # Get available space in MB
    local available_mb
    if [ "$platform" = "ad5m" ]; then
        # BusyBox df output format is different
        available_mb=$(df /opt 2>/dev/null | tail -1 | awk '{print int($4/1024)}')
    else
        available_mb=$(df -m /opt 2>/dev/null | tail -1 | awk '{print $4}')
    fi

    if [ -n "$available_mb" ] && [ "$available_mb" -lt "$required_mb" ]; then
        log_error "Insufficient disk space on /opt"
        log_error "Required: ${required_mb}MB, Available: ${available_mb}MB"
        exit 1
    fi

    log_info "Disk space check: ${available_mb}MB available"
}

# Detect init system (systemd vs SysV)
detect_init_system() {
    # Check for systemd
    if command -v systemctl &> /dev/null && [ -d /run/systemd/system ]; then
        INIT_SYSTEM="systemd"
        log_info "Init system: systemd"
        return
    fi

    # Check for SysV init (BusyBox or traditional)
    if [ -d /etc/init.d ]; then
        INIT_SYSTEM="sysv"
        log_info "Init system: SysV (BusyBox/traditional)"
        return
    fi

    log_error "Could not detect init system."
    log_error "Neither systemd nor /etc/init.d found."
    exit 1
}

# Stop competing screen UIs (GuppyScreen, KlipperScreen, etc.)
stop_competing_uis() {
    log_info "Checking for competing screen UIs..."

    local found_any=false

    for ui in $COMPETING_UIS; do
        # Check systemd services
        if [ "$INIT_SYSTEM" = "systemd" ]; then
            if $SUDO systemctl is-active --quiet "$ui" 2>/dev/null; then
                log_info "Stopping $ui (systemd service)..."
                $SUDO systemctl stop "$ui" 2>/dev/null || true
                $SUDO systemctl disable "$ui" 2>/dev/null || true
                found_any=true
            fi
        fi

        # Check SysV init scripts
        for initscript in /etc/init.d/S*${ui}* /etc/init.d/${ui}* /opt/config/mod/.root/S*${ui}*; do
            if [ -x "$initscript" ] 2>/dev/null; then
                log_info "Stopping $ui ($initscript)..."
                $SUDO "$initscript" stop 2>/dev/null || true
                # Disable by removing execute permission (non-destructive)
                $SUDO chmod -x "$initscript" 2>/dev/null || true
                found_any=true
            fi
        done

        # Kill any remaining processes by name
        if command -v killall &> /dev/null; then
            if killall -0 "$ui" 2>/dev/null; then
                log_info "Killing remaining $ui processes..."
                $SUDO killall "$ui" 2>/dev/null || true
                found_any=true
            fi
        elif command -v pidof &> /dev/null; then
            local pids=$(pidof "$ui" 2>/dev/null)
            if [ -n "$pids" ]; then
                log_info "Killing remaining $ui processes..."
                for pid in $pids; do
                    $SUDO kill "$pid" 2>/dev/null || true
                done
                found_any=true
            fi
        fi
    done

    if [ "$found_any" = true ]; then
        log_info "Waiting for competing UIs to stop..."
        sleep 2
    else
        log_info "No competing UIs found"
    fi
}

# Get latest release version from GitHub
get_latest_version() {
    local url="https://api.github.com/repos/${GITHUB_REPO}/releases/latest"
    local version=""

    log_info "Fetching latest version from GitHub..."

    if command -v curl &> /dev/null; then
        version=$(curl -sSL --connect-timeout 10 "$url" 2>/dev/null | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    elif command -v wget &> /dev/null; then
        version=$(wget -qO- --timeout=10 "$url" 2>/dev/null | grep '"tag_name"' | sed -E 's/.*"([^"]+)".*/\1/')
    fi

    if [ -z "$version" ]; then
        log_error "Failed to fetch latest version from GitHub."
        log_error "Check your network connection and try again."
        log_error "URL: $url"
        exit 1
    fi

    echo "$version"
}

# Download release tarball
download_release() {
    local version=$1
    local platform=$2
    local filename="helixscreen-${platform}-${version}.tar.gz"
    local url="https://github.com/${GITHUB_REPO}/releases/download/${version}/${filename}"
    local dest="${TMP_DIR}/helixscreen.tar.gz"

    log_info "Downloading HelixScreen ${version} for ${platform}..."
    log_info "URL: $url"

    mkdir -p "$TMP_DIR"
    CLEANUP_TMP=true

    local http_code=""
    if command -v curl &> /dev/null; then
        http_code=$(curl -sSL --connect-timeout 30 -w "%{http_code}" -o "$dest" "$url")
    elif command -v wget &> /dev/null; then
        if wget -q --timeout=30 -O "$dest" "$url"; then
            http_code="200"
        else
            http_code="failed"
        fi
    fi

    if [ ! -f "$dest" ] || [ ! -s "$dest" ]; then
        log_error "Failed to download release."
        log_error "URL: $url"
        if [ -n "$http_code" ] && [ "$http_code" != "200" ]; then
            log_error "HTTP status: $http_code"
        fi
        log_error ""
        log_error "Possible causes:"
        log_error "  - Version ${version} may not exist for platform ${platform}"
        log_error "  - Network connectivity issues"
        log_error "  - GitHub may be unavailable"
        exit 1
    fi

    # Verify it's a valid gzip file
    if ! gunzip -t "$dest" 2>/dev/null; then
        log_error "Downloaded file is not a valid gzip archive."
        log_error "The download may have been corrupted or incomplete."
        exit 1
    fi

    local size=$(ls -lh "$dest" | awk '{print $5}')
    log_success "Downloaded ${filename} (${size})"
}

# Extract tarball (handles BusyBox on AD5M)
extract_release() {
    local platform=$1
    local tarball="${TMP_DIR}/helixscreen.tar.gz"

    log_info "Extracting release to ${INSTALL_DIR}..."

    # Check if install dir already exists
    if [ -d "${INSTALL_DIR}" ]; then
        ORIGINAL_INSTALL_EXISTS=true

        # Backup existing config
        if [ -f "${INSTALL_DIR}/helixconfig.json" ]; then
            BACKUP_CONFIG="${TMP_DIR}/helixconfig.json.backup"
            cp "${INSTALL_DIR}/helixconfig.json" "$BACKUP_CONFIG"
            log_info "Backed up existing configuration"
        fi
    fi

    # Remove old installation
    $SUDO rm -rf "${INSTALL_DIR}"

    # Create parent directory
    $SUDO mkdir -p "$(dirname ${INSTALL_DIR})"

    # Extract - AD5M uses BusyBox tar which doesn't support -z
    cd "$(dirname ${INSTALL_DIR})"
    if [ "$platform" = "ad5m" ]; then
        if ! gunzip -c "$tarball" | $SUDO tar xf -; then
            log_error "Failed to extract tarball."
            log_error "The archive may be corrupted."
            exit 1
        fi
    else
        if ! $SUDO tar -xzf "$tarball"; then
            log_error "Failed to extract tarball."
            log_error "The archive may be corrupted."
            exit 1
        fi
    fi

    # Verify extraction succeeded
    if [ ! -f "${INSTALL_DIR}/helix-screen" ]; then
        log_error "Extraction failed - helix-screen binary not found."
        log_error "Expected: ${INSTALL_DIR}/helix-screen"
        exit 1
    fi

    # Restore config if it existed
    if [ -n "$BACKUP_CONFIG" ] && [ -f "$BACKUP_CONFIG" ]; then
        $SUDO cp "$BACKUP_CONFIG" "${INSTALL_DIR}/helixconfig.json"
        log_info "Restored existing configuration"
    fi

    log_success "Extracted to ${INSTALL_DIR}"
}

# Install service (systemd or SysV depending on init system)
install_service() {
    local platform=$1

    if [ "$INIT_SYSTEM" = "systemd" ]; then
        install_service_systemd
    else
        install_service_sysv
    fi
}

# Install systemd service
install_service_systemd() {
    log_info "Installing systemd service..."

    local service_src="${INSTALL_DIR}/config/helixscreen.service"
    local service_dest="/etc/systemd/system/${SERVICE_NAME}.service"

    if [ ! -f "$service_src" ]; then
        log_error "Service file not found: $service_src"
        log_error "The release package may be incomplete."
        exit 1
    fi

    $SUDO cp "$service_src" "$service_dest"

    if ! $SUDO systemctl daemon-reload; then
        log_error "Failed to reload systemd daemon."
        exit 1
    fi

    CLEANUP_SERVICE=true
    log_success "Installed systemd service"
}

# Install SysV init script
install_service_sysv() {
    log_info "Installing SysV init script..."

    local init_src="${INSTALL_DIR}/config/helixscreen.init"
    local init_dest="/etc/init.d/S90helixscreen"

    if [ ! -f "$init_src" ]; then
        log_error "Init script not found: $init_src"
        log_error "The release package may be incomplete."
        exit 1
    fi

    $SUDO cp "$init_src" "$init_dest"
    $SUDO chmod +x "$init_dest"

    CLEANUP_SERVICE=true
    log_success "Installed SysV init script at $init_dest"
}

# Enable and start service
start_service() {
    if [ "$INIT_SYSTEM" = "systemd" ]; then
        start_service_systemd
    else
        start_service_sysv
    fi
}

# Start service (systemd)
start_service_systemd() {
    log_info "Enabling and starting HelixScreen (systemd)..."

    if ! $SUDO systemctl enable "$SERVICE_NAME"; then
        log_error "Failed to enable ${SERVICE_NAME} service."
        exit 1
    fi

    if ! $SUDO systemctl start "$SERVICE_NAME"; then
        log_error "Failed to start ${SERVICE_NAME} service."
        log_error "Check logs with: journalctl -u ${SERVICE_NAME} -n 50"
        exit 1
    fi

    # Wait a moment and check if it's running
    sleep 2
    if $SUDO systemctl is-active --quiet "$SERVICE_NAME"; then
        log_success "HelixScreen is running!"
    else
        log_warn "Service may not have started correctly."
        log_warn "Check status with: systemctl status $SERVICE_NAME"
    fi
}

# Start service (SysV init)
start_service_sysv() {
    log_info "Starting HelixScreen (SysV init)..."

    local init_script="/etc/init.d/S90helixscreen"

    if [ ! -x "$init_script" ]; then
        log_error "Init script not executable: $init_script"
        exit 1
    fi

    if ! $SUDO "$init_script" start; then
        log_error "Failed to start HelixScreen."
        log_error "Check logs in: /tmp/helixscreen.log"
        exit 1
    fi

    # Wait and verify
    sleep 2
    if $SUDO "$init_script" status >/dev/null 2>&1; then
        log_success "HelixScreen is running!"
    else
        log_warn "Service may not have started correctly."
        log_warn "Check: $init_script status"
    fi
}

# Stop service for update
stop_service() {
    if [ "$INIT_SYSTEM" = "systemd" ]; then
        if $SUDO systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
            log_info "Stopping existing HelixScreen service (systemd)..."
            $SUDO systemctl stop "$SERVICE_NAME" || true
        fi
    else
        local init_script="/etc/init.d/S90helixscreen"
        if [ -x "$init_script" ]; then
            log_info "Stopping existing HelixScreen service (SysV)..."
            $SUDO "$init_script" stop 2>/dev/null || true
        fi
        # Also try to kill by name
        if command -v killall &> /dev/null; then
            $SUDO killall helix-screen 2>/dev/null || true
            $SUDO killall helix-splash 2>/dev/null || true
        fi
    fi
}

# Uninstall HelixScreen
uninstall() {
    log_info "Uninstalling HelixScreen..."

    # Detect init system first
    detect_init_system

    if [ "$INIT_SYSTEM" = "systemd" ]; then
        # Stop and disable systemd service
        $SUDO systemctl stop "$SERVICE_NAME" 2>/dev/null || true
        $SUDO systemctl disable "$SERVICE_NAME" 2>/dev/null || true
        $SUDO rm -f "/etc/systemd/system/${SERVICE_NAME}.service"
        $SUDO systemctl daemon-reload
    else
        # Stop and remove SysV init script
        local init_script="/etc/init.d/S90helixscreen"
        if [ -x "$init_script" ]; then
            $SUDO "$init_script" stop 2>/dev/null || true
            $SUDO rm -f "$init_script"
        fi
    fi

    # Kill any remaining processes
    if command -v killall &> /dev/null; then
        $SUDO killall helix-screen 2>/dev/null || true
        $SUDO killall helix-splash 2>/dev/null || true
    fi

    # Remove installation
    if [ -d "$INSTALL_DIR" ]; then
        $SUDO rm -rf "$INSTALL_DIR"
        log_success "Removed ${INSTALL_DIR}"
    fi

    log_success "HelixScreen uninstalled"
    log_info "Note: Reboot to restore previous UI (if applicable)"
}

# Print usage
usage() {
    echo "HelixScreen Installer"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --update       Update existing installation (preserves config)"
    echo "  --uninstall    Remove HelixScreen"
    echo "  --version VER  Install specific version (default: latest)"
    echo "  --help         Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Fresh install, latest version"
    echo "  $0 --update           # Update existing installation"
    echo "  $0 --version v1.1.0   # Install specific version"
}

# Main installation flow
main() {
    local update_mode=false
    local uninstall_mode=false
    local version=""

    # Initialize SUDO (will be set properly in check_permissions)
    SUDO=""

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --update)
                update_mode=true
                shift
                ;;
            --uninstall)
                uninstall_mode=true
                shift
                ;;
            --version)
                if [ -z "${2:-}" ]; then
                    log_error "--version requires a version argument"
                    exit 1
                fi
                version="$2"
                shift 2
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    echo ""
    echo -e "${BOLD}========================================${NC}"
    echo -e "${BOLD}       HelixScreen Installer${NC}"
    echo -e "${BOLD}========================================${NC}"
    echo ""

    # Detect platform
    local platform=$(detect_platform)
    log_info "Detected platform: ${BOLD}${platform}${NC}"

    if [ "$platform" = "unsupported" ]; then
        log_error "Unsupported platform: $(uname -m)"
        log_error "HelixScreen supports:"
        log_error "  - Raspberry Pi (aarch64/armv7l)"
        log_error "  - FlashForge Adventurer 5M (armv7l)"
        exit 1
    fi

    # Check permissions
    check_permissions "$platform"

    # Handle uninstall (doesn't need all checks)
    if [ "$uninstall_mode" = true ]; then
        uninstall
        exit 0
    fi

    # Pre-flight checks
    log_info "Running pre-flight checks..."
    check_requirements
    install_runtime_deps "$platform"
    check_disk_space "$platform"
    detect_init_system

    # Get version
    if [ -z "$version" ]; then
        version=$(get_latest_version)
    fi
    log_info "Target version: ${BOLD}${version}${NC}"

    # Stop competing UIs (GuppyScreen, KlipperScreen, FeatherScreen, etc.)
    stop_competing_uis

    # Stop existing service if updating
    if [ "$update_mode" = true ]; then
        if [ ! -d "$INSTALL_DIR" ]; then
            log_warn "No existing installation found. Performing fresh install."
        fi
        stop_service
    fi

    # Download and install
    download_release "$version" "$platform"
    extract_release "$platform"
    install_service "$platform"

    # Start service
    start_service

    # Cleanup on success
    cleanup_on_success

    echo ""
    echo -e "${GREEN}${BOLD}========================================${NC}"
    echo -e "${GREEN}${BOLD}    Installation Complete!${NC}"
    echo -e "${GREEN}${BOLD}========================================${NC}"
    echo ""
    echo "HelixScreen ${version} installed to ${INSTALL_DIR}"
    echo ""
    echo "Useful commands:"
    if [ "$INIT_SYSTEM" = "systemd" ]; then
        echo "  systemctl status ${SERVICE_NAME}    # Check status"
        echo "  journalctl -u ${SERVICE_NAME} -f    # View logs"
        echo "  systemctl restart ${SERVICE_NAME}   # Restart"
    else
        echo "  /etc/init.d/S90helixscreen status   # Check status"
        echo "  cat /tmp/helixscreen.log            # View logs"
        echo "  /etc/init.d/S90helixscreen restart  # Restart"
    fi
    echo ""

    if [ "$platform" = "ad5m" ]; then
        echo "Note: You may need to reboot for the display to update."
    fi
}

# Run main
main "$@"
