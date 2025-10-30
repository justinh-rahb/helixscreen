/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "lvgl/lvgl.h"

/**
 * Wizard WiFi Setup Screen
 *
 * Handles WiFi configuration during first-run wizard:
 * - WiFi on/off toggle
 * - Network scanning and selection
 * - Password entry for secured networks
 * - Connection status feedback
 * - Ethernet status display
 *
 * Initialization Order (CRITICAL):
 *   1. Register XML components (wizard_wifi_setup.xml, wifi_password_modal.xml)
 *   2. ui_wizard_wifi_init_subjects()
 *   3. ui_wizard_wifi_register_callbacks()
 *   4. ui_wizard_wifi_create(parent)
 *   5. ui_wizard_wifi_init_wifi_manager() <- Start WiFi backend integration
 */

/**
 * Initialize WiFi screen subjects
 *
 * Creates and registers reactive subjects:
 * - wifi_enabled (int, 0=off 1=on)
 * - wifi_status (string, e.g. "Scanning...", "Connected to MyNetwork")
 * - ethernet_status (string, e.g. "Connected", "Disconnected")
 *
 * MUST be called BEFORE creating XML components.
 */
void ui_wizard_wifi_init_subjects();

/**
 * Register event callbacks
 *
 * Registers callbacks for:
 * - on_wifi_toggle_changed (WiFi enable/disable)
 * - on_network_item_clicked (Network selection)
 * - on_modal_cancel_clicked (Password modal cancel)
 * - on_modal_connect_clicked (Password modal connect)
 *
 * MUST be called BEFORE creating XML components.
 */
void ui_wizard_wifi_register_callbacks();

/**
 * Register responsive constants for WiFi screen
 *
 * Detects screen size and registers size-specific constants:
 * - wifi_card_height, wifi_ethernet_height, wifi_toggle_height
 * - wifi_title_font, wifi_status_font, wifi_help_font
 *
 * MUST be called BEFORE creating XML components.
 */
void ui_wizard_wifi_register_responsive_constants();

/**
 * Create WiFi setup screen
 *
 * Creates the WiFi UI from wizard_wifi_setup.xml.
 * Returns the root WiFi screen object.
 *
 * Prerequisites:
 * - ui_wizard_wifi_init_subjects() called
 * - ui_wizard_wifi_register_callbacks() called
 *
 * @param parent Parent container (typically wizard_content area)
 * @return The WiFi screen root object, or NULL on failure
 */
lv_obj_t* ui_wizard_wifi_create(lv_obj_t* parent);

/**
 * Initialize WiFi manager integration
 *
 * Sets up WiFiManager callbacks for:
 * - Network scan results
 * - Connection status updates
 * - WiFi enable/disable events
 *
 * MUST be called AFTER ui_wizard_wifi_create().
 */
void ui_wizard_wifi_init_wifi_manager();

/**
 * Cleanup WiFi screen resources
 *
 * Stops WiFi scanning, disconnects callbacks, and cleans up subjects.
 * Called when leaving the WiFi setup step.
 */
void ui_wizard_wifi_cleanup();

/**
 * Show password entry modal
 *
 * Displays the password modal for a secured network.
 *
 * @param ssid Network SSID to display
 */
void ui_wizard_wifi_show_password_modal(const char* ssid);

/**
 * Hide password entry modal
 */
void ui_wizard_wifi_hide_password_modal();
