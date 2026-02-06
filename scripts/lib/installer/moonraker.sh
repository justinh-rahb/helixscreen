#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Module: moonraker
# Moonraker update_manager configuration
#
# Reads: PLATFORM, INSTALL_DIR, SUDO
# Writes: -

# Source guard
[ -n "${_HELIX_MOONRAKER_SOURCED:-}" ] && return 0
_HELIX_MOONRAKER_SOURCED=1

# Common moonraker.conf locations
MOONRAKER_CONF_PATHS="
/home/pi/printer_data/config/moonraker.conf
/home/biqu/printer_data/config/moonraker.conf
/home/mks/printer_data/config/moonraker.conf
/root/printer_data/config/moonraker.conf
/opt/config/moonraker.conf
/usr/data/printer_data/config/moonraker.conf
"

# Find moonraker.conf
# Returns: path to moonraker.conf or empty string
find_moonraker_conf() {
    # Dynamic: check detected user's home first
    if [ -n "${KLIPPER_HOME:-}" ]; then
        local user_conf="${KLIPPER_HOME}/printer_data/config/moonraker.conf"
        if [ -f "$user_conf" ]; then
            echo "$user_conf"
            return 0
        fi
    fi

    # Static fallback
    for conf in $MOONRAKER_CONF_PATHS; do
        if [ -f "$conf" ]; then
            echo "$conf"
            return 0
        fi
    done
    echo ""
}

# Check if update_manager section for helixscreen already exists
# Args: $1 = moonraker.conf path
# Returns: 0 if exists, 1 if not
has_update_manager_section() {
    local conf="$1"
    grep -q '^\[update_manager helixscreen\]' "$conf" 2>/dev/null
}

# Generate update_manager configuration block
# Uses git_repo type for tracking updates
# Note: Uses INSTALL_DIR which must be set before calling
generate_update_manager_config() {
    cat << EOF

# HelixScreen Update Manager
# Added by HelixScreen installer - enables one-click updates from Mainsail/Fluidd
[update_manager helixscreen]
type: git_repo
channel: stable
path: ${INSTALL_DIR}
origin: https://github.com/prestonbrown/helixscreen.git
primary_branch: main
managed_services: helixscreen
install_script: scripts/install.sh
EOF
}

# Add update_manager section to moonraker.conf
# Args: $1 = moonraker.conf path
add_update_manager_section() {
    local conf="$1"

    # Create backup
    $SUDO cp "$conf" "${conf}.bak.helixscreen" 2>/dev/null || true

    # Append configuration
    generate_update_manager_config | $SUDO tee -a "$conf" >/dev/null

    log_success "Added update_manager section to $conf"
    log_info "You can now update HelixScreen from the Mainsail/Fluidd web interface!"
}

# Configure Moonraker update_manager
# Called during installation on platforms with web UI (Pi, K1 with Simple AF)
configure_moonraker_updates() {
    local platform=$1

    # Skip on AD5M (typically no Mainsail/Fluidd web UI)
    if [ "$platform" = "ad5m" ]; then
        log_info "Skipping Moonraker update_manager on AD5M (typically no web UI)"
        return 0
    fi

    # Pi and K1 (Simple AF) both commonly use Mainsail/Fluidd
    log_info "Configuring Moonraker update_manager..."

    local conf
    conf=$(find_moonraker_conf)

    if [ -z "$conf" ]; then
        log_warn "Could not find moonraker.conf"
        log_warn "To enable web UI updates, manually add to your moonraker.conf:"
        echo ""
        generate_update_manager_config
        echo ""
        return 0
    fi

    if has_update_manager_section "$conf"; then
        log_info "update_manager section already exists in $conf"
        return 0
    fi

    add_update_manager_section "$conf"

    # Restart Moonraker to pick up the new configuration
    if command -v systemctl >/dev/null 2>&1 && $SUDO systemctl is-active --quiet moonraker 2>/dev/null; then
        log_info "Restarting Moonraker to apply configuration..."
        $SUDO systemctl restart moonraker || true
    elif [ -x "/etc/init.d/S56moonraker_service" ]; then
        # K1/Simple AF uses SysV init
        log_info "Restarting Moonraker to apply configuration..."
        if ! $SUDO /etc/init.d/S56moonraker_service restart 2>/dev/null; then
            log_warn "Could not restart Moonraker - you may need to restart it manually"
        fi
    fi
}

# Remove update_manager section from moonraker.conf
# Called during uninstallation
remove_update_manager_section() {
    local conf
    conf=$(find_moonraker_conf)

    if [ -z "$conf" ]; then
        return 0
    fi

    if ! has_update_manager_section "$conf"; then
        return 0
    fi

    log_info "Removing update_manager section from $conf..."

    # Create backup
    $SUDO cp "$conf" "${conf}.bak.helixscreen-uninstall" 2>/dev/null || true

    # Remove the section (from [update_manager helixscreen] to next section or EOF)
    # This uses awk to skip lines between [update_manager helixscreen] and the next [section]
    # Note: Need to run awk through sudo to handle permission on output file
    $SUDO sh -c "awk '
        /^\[update_manager helixscreen\]/ { skip=1; next }
        /^\[/ { skip=0 }
        !skip { print }
    ' \"$conf\" > \"${conf}.tmp\"" && $SUDO mv "${conf}.tmp" "$conf"

    # Also remove any "Added by HelixScreen" comment lines that precede it
    $SUDO sed -i '/# HelixScreen Update Manager/d' "$conf" 2>/dev/null || \
    $SUDO sed -i '' '/# HelixScreen Update Manager/d' "$conf" 2>/dev/null || true

    $SUDO sed -i '/# Added by HelixScreen installer/d' "$conf" 2>/dev/null || \
    $SUDO sed -i '' '/# Added by HelixScreen installer/d' "$conf" 2>/dev/null || true

    log_success "Removed update_manager section from $conf"
}
