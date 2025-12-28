#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixPrint Moonraker Plugin Installer
#
# This script installs the helix_print Moonraker plugin.
#
# Usage:
#   ./install.sh              # Interactive install (manual config steps)
#   ./install.sh --auto       # Full auto-install (updates config, restarts Moonraker)
#   ./install.sh --uninstall  # Remove the plugin
#
# Remote install (from GitHub):
#   curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/moonraker-plugin/remote-install.sh | bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_FILE="$SCRIPT_DIR/helix_print.py"
PHASE_TRACKING_CFG="$SCRIPT_DIR/../config/helix_phase_tracking.cfg"
AUTO_MODE=false
ENABLE_PHASE_TRACKING=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Find Moonraker installation
find_moonraker() {
    local moonraker_path=""

    # Check common locations
    local locations=(
        "$HOME/moonraker"
        "$HOME/klipper_config/moonraker"
        "/home/pi/moonraker"
        "/home/klipper/moonraker"
        "$1"  # User-provided path
    )

    for loc in "${locations[@]}"; do
        if [[ -n "$loc" && -d "$loc/moonraker/components" ]]; then
            moonraker_path="$loc"
            break
        fi
    done

    # Also check if moonraker is installed as a package
    if [[ -z "$moonraker_path" ]]; then
        local pip_loc=$(python3 -c "import moonraker; print(moonraker.__path__[0])" 2>/dev/null || true)
        if [[ -n "$pip_loc" && -d "$pip_loc/components" ]]; then
            moonraker_path="$(dirname "$pip_loc")"
        fi
    fi

    echo "$moonraker_path"
}

# Main installation
main() {
    info "HelixPrint Moonraker Plugin Installer"
    echo ""

    # Check plugin file exists
    if [[ ! -f "$PLUGIN_FILE" ]]; then
        error "Plugin file not found: $PLUGIN_FILE"
    fi

    # Find Moonraker
    local moonraker_path=$(find_moonraker "$1")

    if [[ -z "$moonraker_path" ]]; then
        error "Could not find Moonraker installation.
Please provide the path: ./install.sh /path/to/moonraker"
    fi

    local components_dir="$moonraker_path/moonraker/components"

    if [[ ! -d "$components_dir" ]]; then
        error "Components directory not found: $components_dir"
    fi

    info "Found Moonraker at: $moonraker_path"
    info "Components directory: $components_dir"
    echo ""

    # Create symlink
    local target="$components_dir/helix_print.py"

    if [[ -f "$target" && ! -L "$target" ]]; then
        error "A file (not symlink) exists at $target
Please remove it manually before installing."
    fi

    # Use ln -sf for atomic replacement (removes existing symlink first)
    ln -sf "$PLUGIN_FILE" "$target"
    info "Created symlink: $target -> $PLUGIN_FILE"
    echo ""

    # Remind about configuration
    info "Installation complete!"
    echo ""
    echo "Next steps:"
    echo "  1. Add the following to your moonraker.conf:"
    echo ""
    echo "     [helix_print]"
    echo "     # enabled: True"
    echo "     # temp_dir: .helix_temp"
    echo "     # symlink_dir: .helix_print"
    echo "     # cleanup_delay: 86400"
    echo ""
    echo "  2. Restart Moonraker:"
    echo "     sudo systemctl restart moonraker"
    echo ""
    echo "  3. Verify the plugin is loaded:"
    echo "     curl http://localhost:7125/server/helix/status"
    echo ""
}

# Uninstall function (interactive)
uninstall() {
    info "Uninstalling HelixPrint plugin..."

    local moonraker_path=$(find_moonraker "$1")

    if [[ -z "$moonraker_path" ]]; then
        error "Could not find Moonraker installation."
    fi

    local target="$moonraker_path/moonraker/components/helix_print.py"

    if [[ -L "$target" ]]; then
        rm "$target"
        info "Removed symlink: $target"
    elif [[ -f "$target" ]]; then
        warn "Found regular file (not symlink) at $target"
        warn "Please remove it manually if desired."
    else
        info "Plugin symlink not found (already uninstalled?)"
    fi

    echo ""
    echo "Don't forget to:"
    echo "  1. Remove [helix_print] section from moonraker.conf"
    echo "  2. Restart Moonraker: sudo systemctl restart moonraker"
}

# Auto-uninstall function (non-interactive, for HelixScreen integration)
auto_uninstall() {
    info "HelixPrint Auto-Uninstall Mode"
    echo ""

    # Find Moonraker (auto-detect only)
    local moonraker_path=$(find_moonraker "")

    if [[ -z "$moonraker_path" ]]; then
        error "Could not auto-detect Moonraker installation."
    fi

    local target="$moonraker_path/moonraker/components/helix_print.py"
    local config_dir=""

    # Find config directory
    local config_locations=(
        "$HOME/printer_data/config"
        "$HOME/klipper_config"
        "/home/pi/printer_data/config"
        "/home/pi/klipper_config"
    )

    for loc in "${config_locations[@]}"; do
        if [[ -d "$loc" && -f "$loc/moonraker.conf" ]]; then
            config_dir="$loc"
            break
        fi
    done

    # Remove phase tracking BEFORE removing plugin (need API access)
    remove_phase_tracking "$config_dir"

    # Remove symlink
    if [[ -L "$target" ]]; then
        rm "$target"
        info "Removed symlink: $target"
    elif [[ -f "$target" ]]; then
        error "Found regular file (not symlink) at $target - manual removal required"
    else
        info "Plugin symlink not found (already uninstalled?)"
    fi

    # Remove config section if possible
    if [[ -n "$config_dir" && -f "$config_dir/moonraker.conf" ]]; then
        local moonraker_conf="$config_dir/moonraker.conf"

        if grep -q '^\[helix_print\]' "$moonraker_conf"; then
            # Create backup before modifying config
            local backup_file="${moonraker_conf}.bak.$(date +%Y%m%d_%H%M%S)"
            cp "$moonraker_conf" "$backup_file"
            info "Created backup: $backup_file"

            info "Removing [helix_print] section from moonraker.conf"
            # Use awk for cross-platform config section removal
            # This correctly handles helix_print as the last section in the file
            awk '
                /^\[helix_print\]/ { skip = 1; next }
                /^\[/ { skip = 0 }
                !skip { print }
            ' "$moonraker_conf" > "$moonraker_conf.tmp" && mv "$moonraker_conf.tmp" "$moonraker_conf"
        fi
    fi

    # Restart Moonraker
    info "Restarting Moonraker..."
    if command -v systemctl &> /dev/null && systemctl list-units --type=service | grep -q moonraker; then
        sudo systemctl restart moonraker || warn "Failed to restart Moonraker via systemctl"
    elif command -v service &> /dev/null; then
        sudo service moonraker restart || warn "Failed to restart Moonraker via service"
    else
        warn "Neither systemctl nor service found - please restart Moonraker manually"
    fi

    # Wait for Moonraker to come back up
    wait_for_moonraker

    echo ""
    info "Auto-uninstall complete!"
}

# Wait for Moonraker to become available after restart
wait_for_moonraker() {
    local max_attempts=30
    local attempt=0
    local moonraker_url="${MOONRAKER_URL:-http://localhost:7125}"

    info "Waiting for Moonraker to become available..."

    while [[ $attempt -lt $max_attempts ]]; do
        if curl -s --max-time 2 "$moonraker_url/server/info" > /dev/null 2>&1; then
            info "Moonraker is ready!"
            return 0
        fi
        attempt=$((attempt + 1))
        sleep 1
    done

    warn "Moonraker did not respond within ${max_attempts} seconds"
    return 1
}

# Install phase tracking macros and optionally instrument PRINT_START
install_phase_tracking() {
    local config_dir="$1"
    local moonraker_url="${MOONRAKER_URL:-http://localhost:7125}"

    if [[ -z "$config_dir" ]]; then
        warn "Config directory not found - skipping phase tracking setup"
        return 1
    fi

    info "Setting up detailed print preparation tracking..."

    # Copy helix_phase_tracking.cfg to config directory
    if [[ -f "$PHASE_TRACKING_CFG" ]]; then
        cp "$PHASE_TRACKING_CFG" "$config_dir/helix_phase_tracking.cfg"
        info "Installed helix_phase_tracking.cfg"
    else
        warn "Phase tracking config not found: $PHASE_TRACKING_CFG"
        return 1
    fi

    # Add include to printer.cfg if not already present
    local printer_cfg="$config_dir/printer.cfg"
    if [[ -f "$printer_cfg" ]]; then
        if ! grep -q '\[include helix_phase_tracking.cfg\]' "$printer_cfg"; then
            # Create backup
            local backup_file="${printer_cfg}.bak.$(date +%Y%m%d_%H%M%S)"
            cp "$printer_cfg" "$backup_file"
            info "Created backup: $backup_file"

            # Append include at end of file (safe, simple approach)
            echo "" >> "$printer_cfg"
            echo "[include helix_phase_tracking.cfg]" >> "$printer_cfg"

            info "Added [include helix_phase_tracking.cfg] to printer.cfg"
        else
            info "Phase tracking include already present in printer.cfg"
        fi
    else
        warn "printer.cfg not found - please add [include helix_phase_tracking.cfg] manually"
    fi

    # Call plugin API to instrument PRINT_START macro
    info "Instrumenting PRINT_START macro..."
    local response
    response=$(curl -s -X POST "$moonraker_url/server/helix/phase_tracking/enable" 2>/dev/null || echo '{"error": "API call failed"}')

    if echo "$response" | grep -q '"success".*true'; then
        info "PRINT_START macro instrumented successfully"
    else
        warn "Could not auto-instrument PRINT_START macro"
        warn "You can enable this later from HelixScreen Settings"
        warn "Or manually add HELIX_PRINT_COMPLETE to the end of your PRINT_START macro"
    fi

    return 0
}

# Remove phase tracking from printer config
remove_phase_tracking() {
    local config_dir="$1"
    local moonraker_url="${MOONRAKER_URL:-http://localhost:7125}"

    if [[ -z "$config_dir" ]]; then
        return 0
    fi

    info "Removing phase tracking..."

    # Call plugin API to strip instrumentation first (while Moonraker is still running)
    curl -s -X POST "$moonraker_url/server/helix/phase_tracking/disable" 2>/dev/null || true

    # Remove include from printer.cfg
    local printer_cfg="$config_dir/printer.cfg"
    if [[ -f "$printer_cfg" ]] && grep -q '\[include helix_phase_tracking.cfg\]' "$printer_cfg"; then
        local backup_file="${printer_cfg}.bak.$(date +%Y%m%d_%H%M%S)"
        cp "$printer_cfg" "$backup_file"
        grep -v '\[include helix_phase_tracking.cfg\]' "$printer_cfg" > "$printer_cfg.tmp"
        mv "$printer_cfg.tmp" "$printer_cfg"
        info "Removed phase tracking include from printer.cfg"
    fi

    # Remove the cfg file
    if [[ -f "$config_dir/helix_phase_tracking.cfg" ]]; then
        rm "$config_dir/helix_phase_tracking.cfg"
        info "Removed helix_phase_tracking.cfg"
    fi
}

# Auto-install function (non-interactive, for HelixScreen integration)
auto_install() {
    info "HelixPrint Auto-Install Mode"
    echo ""

    AUTO_MODE=true

    # Check plugin file exists
    if [[ ! -f "$PLUGIN_FILE" ]]; then
        error "Plugin file not found: $PLUGIN_FILE"
    fi

    # Find Moonraker (auto-detect only)
    local moonraker_path=$(find_moonraker "")

    if [[ -z "$moonraker_path" ]]; then
        error "Could not auto-detect Moonraker installation."
    fi

    local components_dir="$moonraker_path/moonraker/components"
    local config_dir=""

    # Find config directory
    local config_locations=(
        "$HOME/printer_data/config"
        "$HOME/klipper_config"
        "/home/pi/printer_data/config"
        "/home/pi/klipper_config"
    )

    for loc in "${config_locations[@]}"; do
        if [[ -d "$loc" && -f "$loc/moonraker.conf" ]]; then
            config_dir="$loc"
            break
        fi
    done

    info "Moonraker: $moonraker_path"
    info "Config: ${config_dir:-not found}"
    if [[ "$ENABLE_PHASE_TRACKING" == "true" ]]; then
        info "Phase tracking: ENABLED"
    fi
    echo ""

    # Pre-flight permission checks
    if [[ ! -w "$components_dir" ]]; then
        error "Cannot write to components directory: $components_dir"
    fi

    if [[ -n "$config_dir" && -f "$config_dir/moonraker.conf" ]]; then
        if [[ ! -w "$config_dir/moonraker.conf" ]]; then
            error "Cannot write to moonraker.conf: $config_dir/moonraker.conf"
        fi
    fi

    # Create symlink
    local target="$components_dir/helix_print.py"

    if [[ -f "$target" && ! -L "$target" ]]; then
        error "A file (not symlink) exists at $target"
    fi

    # Use ln -sf for atomic replacement (removes existing symlink first)
    ln -sf "$PLUGIN_FILE" "$target"
    info "Created symlink: $target"

    # Auto-configure moonraker.conf if possible
    if [[ -n "$config_dir" && -f "$config_dir/moonraker.conf" ]]; then
        local moonraker_conf="$config_dir/moonraker.conf"

        # Check if [helix_print] section already exists
        if grep -q '^\[helix_print\]' "$moonraker_conf"; then
            info "Config section [helix_print] already exists"
        else
            # Create backup before modifying config
            local backup_file="${moonraker_conf}.bak.$(date +%Y%m%d_%H%M%S)"
            cp "$moonraker_conf" "$backup_file"
            info "Created backup: $backup_file"

            info "Adding [helix_print] section to moonraker.conf"
            echo "" >> "$moonraker_conf"
            echo "[helix_print]" >> "$moonraker_conf"
            echo "# HelixScreen plugin - auto-configured" >> "$moonraker_conf"
        fi
    else
        warn "Could not find moonraker.conf - manual config required"
    fi

    # Restart Moonraker
    info "Restarting Moonraker..."
    if command -v systemctl &> /dev/null && systemctl list-units --type=service | grep -q moonraker; then
        sudo systemctl restart moonraker || warn "Failed to restart Moonraker via systemctl"
    elif command -v service &> /dev/null; then
        sudo service moonraker restart || warn "Failed to restart Moonraker via service"
    else
        warn "Neither systemctl nor service found - please restart Moonraker manually"
    fi

    # Wait for Moonraker to come back up
    wait_for_moonraker

    # Install phase tracking if requested
    if [[ "$ENABLE_PHASE_TRACKING" == "true" ]]; then
        install_phase_tracking "$config_dir"

        # Need to restart Klipper to load the new macros
        info "Restarting Klipper to load phase tracking macros..."
        local moonraker_url="${MOONRAKER_URL:-http://localhost:7125}"
        curl -s -X POST "$moonraker_url/printer/restart" 2>/dev/null || warn "Could not restart Klipper via API"

        # Give Klipper time to restart
        sleep 5
    fi

    echo ""
    info "Auto-install complete!"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --auto|-a)
            AUTO_MODE=true
            shift
            ;;
        --with-phase-tracking)
            ENABLE_PHASE_TRACKING=true
            shift
            ;;
        --uninstall|-u)
            uninstall "$2"
            exit 0
            ;;
        --uninstall-auto)
            auto_uninstall
            exit 0
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS] [MOONRAKER_PATH]"
            echo ""
            echo "Options:"
            echo "  --auto, -a              Full auto-install (updates config, restarts Moonraker)"
            echo "  --with-phase-tracking   Enable detailed print preparation tracking"
            echo "  --uninstall, -u         Remove the plugin symlink (interactive)"
            echo "  --uninstall-auto        Full auto-uninstall (removes config, restarts Moonraker)"
            echo "  --help, -h              Show this help message"
            echo ""
            echo "Arguments:"
            echo "  MOONRAKER_PATH     Path to Moonraker installation (auto-detected if not provided)"
            echo ""
            echo "Environment Variables:"
            echo "  MOONRAKER_URL      URL for Moonraker API health check after restart"
            echo "                     Default: http://localhost:7125"
            echo "                     Example: MOONRAKER_URL=http://192.168.1.100:7125 ./install.sh --auto"
            echo ""
            echo "Examples:"
            echo "  ./install.sh --auto                         # Auto-install without phase tracking"
            echo "  ./install.sh --auto --with-phase-tracking   # Auto-install with phase tracking"
            exit 0
            ;;
        *)
            # Assume it's a moonraker path for interactive mode
            if [[ "$AUTO_MODE" == "true" ]]; then
                auto_install
            else
                main "$1"
            fi
            exit 0
            ;;
    esac
done

# No arguments left - run in appropriate mode
if [[ "$AUTO_MODE" == "true" ]]; then
    auto_install
else
    main ""
fi
