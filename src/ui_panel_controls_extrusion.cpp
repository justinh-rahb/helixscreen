// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

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

#include "ui_panel_controls_extrusion.h"

#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_common.h"
#include "ui_subject_registry.h"
#include "ui_temperature_utils.h"
#include "ui_theme.h"
#include "ui_utils.h"

#include "app_constants.h"

#include <spdlog/spdlog.h>

#include <stdio.h>
#include <string.h>

// Temperature subjects (reactive data binding)
static lv_subject_t temp_status_subject;
static lv_subject_t warning_temps_subject;

// Subject storage buffers
static char temp_status_buf[64];
static char warning_temps_buf[64];

// Current state
static int nozzle_current = 25;
static int nozzle_target = 0;
static int selected_amount = 10; // Default: 10mm

// Temperature limits (can be updated from Moonraker heater config)
static int nozzle_min_temp = AppConstants::Temperature::DEFAULT_MIN_TEMP;
static int nozzle_max_temp = AppConstants::Temperature::DEFAULT_NOZZLE_MAX;

// Panel widgets
static lv_obj_t* extrusion_panel = nullptr;
static lv_obj_t* parent_obj = nullptr;
static lv_obj_t* btn_extrude = nullptr;
static lv_obj_t* btn_retract = nullptr;
static lv_obj_t* safety_warning = nullptr;

// Amount button widgets (for visual feedback)
static lv_obj_t* amount_buttons[4] = {nullptr, nullptr, nullptr, nullptr};
static const int amount_values[4] = {5, 10, 25, 50};

// Forward declarations
static void update_temp_status();
static void update_warning_text();
static void update_safety_state();
static void update_amount_buttons_visual();

void ui_panel_controls_extrusion_init_subjects() {
    // Initialize subjects with default values
    char temp_status_val[64], warning_temps_val[64];
    snprintf(temp_status_val, sizeof(temp_status_val), "%d / %d°C", nozzle_current, nozzle_target);
    snprintf(warning_temps_val, sizeof(warning_temps_val), "Current: %d°C\nTarget: %d°C",
             nozzle_current, nozzle_target);

    UI_SUBJECT_INIT_AND_REGISTER_STRING(temp_status_subject, temp_status_buf, temp_status_val, "extrusion_temp_status");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(warning_temps_subject, warning_temps_buf, warning_temps_val, "extrusion_warning_temps");

    spdlog::debug("[0: temp={}/{}°C, amount={}mm", nozzle_current,
                 nozzle_target, selected_amount);
}

// Update temperature status display text
static void update_temp_status() {
    // Status indicator: ✓ (ready), ⚠ (heating), ✗ (too cold)
    const char* status_icon;
    if (UITemperatureUtils::is_extrusion_safe(nozzle_current,
                                              AppConstants::Temperature::MIN_EXTRUSION_TEMP)) {
        // Within 5°C of target and hot enough (safe range check without overflow)
        if (nozzle_target > 0 && nozzle_current >= nozzle_target - 5 &&
            nozzle_current <= nozzle_target + 5) {
            status_icon = "✓"; // Ready
        } else {
            status_icon = "✓"; // Hot enough
        }
    } else if (nozzle_target >= AppConstants::Temperature::MIN_EXTRUSION_TEMP) {
        status_icon = "⚠"; // Heating
    } else {
        status_icon = "✗"; // Too cold
    }

    snprintf(temp_status_buf, sizeof(temp_status_buf), "%d / %d°C %s", nozzle_current,
             nozzle_target, status_icon);
    lv_subject_copy_string(&temp_status_subject, temp_status_buf);
}

// Update warning card text
static void update_warning_text() {
    snprintf(warning_temps_buf, sizeof(warning_temps_buf), "Current: %d°C\nTarget: %d°C",
             nozzle_current, nozzle_target);
    lv_subject_copy_string(&warning_temps_subject, warning_temps_buf);
}

// Update safety state (button enable/disable, warning visibility)
static void update_safety_state() {
    bool allowed = UITemperatureUtils::is_extrusion_safe(
        nozzle_current, AppConstants::Temperature::MIN_EXTRUSION_TEMP);

    // Enable/disable extrude and retract buttons
    if (btn_extrude) {
        if (allowed) {
            lv_obj_clear_state(btn_extrude, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_extrude, LV_STATE_DISABLED);
        }
    }

    if (btn_retract) {
        if (allowed) {
            lv_obj_clear_state(btn_retract, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_retract, LV_STATE_DISABLED);
        }
    }

    // Show/hide safety warning
    if (safety_warning) {
        if (allowed) {
            lv_obj_add_flag(safety_warning, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(safety_warning, LV_OBJ_FLAG_HIDDEN);
        }
    }

    spdlog::debug("[Extrusion] Safety state updated: allowed={} (temp={}°C)", allowed,
                  nozzle_current);
}

// Update visual feedback for amount selector buttons
static void update_amount_buttons_visual() {
    for (int i = 0; i < 4; i++) {
        if (amount_buttons[i]) {
            if (amount_values[i] == selected_amount) {
                // Selected state - theme handles colors
                lv_obj_add_state(amount_buttons[i], LV_STATE_CHECKED);
            } else {
                // Unselected state - theme handles colors
                lv_obj_remove_state(amount_buttons[i], LV_STATE_CHECKED);
            }
        }
    }
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================


// Event handler: Amount selector buttons
LVGL_SAFE_EVENT_CB_WITH_EVENT(amount_button_cb, event, {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(event);
    const char* name = lv_obj_get_name(btn);

    if (!name)
        return;

    if (strcmp(name, "amount_5mm") == 0) {
        selected_amount = 5;
    } else if (strcmp(name, "amount_10mm") == 0) {
        selected_amount = 10;
    } else if (strcmp(name, "amount_25mm") == 0) {
        selected_amount = 25;
    } else if (strcmp(name, "amount_50mm") == 0) {
        selected_amount = 50;
    }

    update_amount_buttons_visual();
    spdlog::debug("[Extrusion] Amount selected: {}mm", selected_amount);
})

// Event handler: Extrude button
LVGL_SAFE_EVENT_CB(extrude_button_cb, {
    if (!UITemperatureUtils::is_extrusion_safe(nozzle_current,
                                               AppConstants::Temperature::MIN_EXTRUSION_TEMP)) {
        NOTIFY_WARNING("Nozzle too cold for extrusion ({}°C, min: {}°C)", nozzle_current,
                       AppConstants::Temperature::MIN_EXTRUSION_TEMP);
        return;
    }

    spdlog::info("[Extrusion] Extruding {}mm of filament", selected_amount);
    // TODO: Send command to printer (moonraker_extrude(selected_amount))
})

// Event handler: Retract button
LVGL_SAFE_EVENT_CB(retract_button_cb, {
    if (!UITemperatureUtils::is_extrusion_safe(nozzle_current,
                                               AppConstants::Temperature::MIN_EXTRUSION_TEMP)) {
        NOTIFY_WARNING("Nozzle too cold for retraction ({}°C, min: {}°C)", nozzle_current,
                       AppConstants::Temperature::MIN_EXTRUSION_TEMP);
        return;
    }

    spdlog::info("[Extrusion] Retracting {}mm of filament", selected_amount);
    // TODO: Send command to printer (moonraker_retract(selected_amount))
})

// ============================================================================
// PUBLIC API
// ============================================================================

// Resize callback for responsive padding
void ui_panel_controls_extrusion_setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    extrusion_panel = panel;
    parent_obj = parent_screen;

    spdlog::info("[Extrusion] Setting up panel event handlers");

    // Use standard overlay panel setup (wires header, back button, handles responsive padding)
    ui_overlay_panel_setup_standard(panel, parent_screen, "overlay_header", "overlay_content");

    // Find overlay_content to access panel widgets
    lv_obj_t* overlay_content = lv_obj_find_by_name(panel, "overlay_content");
    if (!overlay_content) {
        spdlog::error("[Extrusion] overlay_content not found!");
        return;
    }

    // Amount selector buttons
    const char* amount_names[] = {"amount_5mm", "amount_10mm", "amount_25mm", "amount_50mm"};
    for (int i = 0; i < 4; i++) {
        amount_buttons[i] = lv_obj_find_by_name(overlay_content, amount_names[i]);
        if (amount_buttons[i]) {
            lv_obj_add_event_cb(amount_buttons[i], amount_button_cb, LV_EVENT_CLICKED, nullptr);
        }
    }
    spdlog::debug("[Extrusion]   ✓ Amount buttons (4)");

    // Extrude button
    btn_extrude = lv_obj_find_by_name(overlay_content, "btn_extrude");
    if (btn_extrude) {
        lv_obj_add_event_cb(btn_extrude, extrude_button_cb, LV_EVENT_CLICKED, nullptr);
        spdlog::debug("[Extrusion]   ✓ Extrude button");
    }

    // Retract button
    btn_retract = lv_obj_find_by_name(overlay_content, "btn_retract");
    if (btn_retract) {
        lv_obj_add_event_cb(btn_retract, retract_button_cb, LV_EVENT_CLICKED, nullptr);
        spdlog::debug("[Extrusion]   ✓ Retract button");
    }

    // Safety warning card
    safety_warning = lv_obj_find_by_name(overlay_content, "safety_warning");

    // Initialize visual state
    update_amount_buttons_visual();
    update_temp_status();
    update_warning_text();
    update_safety_state();

    spdlog::debug("[0!");
}

void ui_panel_controls_extrusion_set_temp(int current, int target) {
    // Validate temperature ranges using dynamic limits
    if (current < nozzle_min_temp || current > nozzle_max_temp) {
        spdlog::warn(
            "[Extrusion] Invalid nozzle current temperature {}°C (valid: {}-{}°C), clamping",
            current, nozzle_min_temp, nozzle_max_temp);
        current = (current < nozzle_min_temp) ? nozzle_min_temp : nozzle_max_temp;
    }
    if (target < nozzle_min_temp || target > nozzle_max_temp) {
        spdlog::warn(
            "[Extrusion] Invalid nozzle target temperature {}°C (valid: {}-{}°C), clamping", target,
            nozzle_min_temp, nozzle_max_temp);
        target = (target < nozzle_min_temp) ? nozzle_min_temp : nozzle_max_temp;
    }

    nozzle_current = current;
    nozzle_target = target;
    update_temp_status();
    update_warning_text();
    update_safety_state();
}

int ui_panel_controls_extrusion_get_amount() {
    return selected_amount;
}

bool ui_panel_controls_extrusion_is_allowed() {
    return UITemperatureUtils::is_extrusion_safe(nozzle_current,
                                                 AppConstants::Temperature::MIN_EXTRUSION_TEMP);
}

void ui_panel_controls_extrusion_set_limits(int min_temp, int max_temp) {
    nozzle_min_temp = min_temp;
    nozzle_max_temp = max_temp;
    spdlog::info("[Extrusion] Nozzle temperature limits updated: {}-{}°C", min_temp, max_temp);
}
