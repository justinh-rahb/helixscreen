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

#include "ui_panel_controls.h"

#include "ui_component_keypad.h"
#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_controls_extrusion.h"
#include "ui_panel_controls_temp.h"
#include "ui_panel_motion.h"

#include <spdlog/spdlog.h>

// Panel object references
static lv_obj_t* controls_panel = nullptr;
static lv_obj_t* motion_panel = nullptr;
static lv_obj_t* nozzle_temp_panel = nullptr;
static lv_obj_t* bed_temp_panel = nullptr;
static lv_obj_t* extrusion_panel = nullptr;
static lv_obj_t* parent_screen = nullptr;

// ============================================================================
// Card Click Event Handlers
// ============================================================================

LVGL_SAFE_EVENT_CB(card_motion_clicked, {
    spdlog::debug("Motion card clicked - opening Motion sub-screen");

    // Create motion panel on first access
    if (!motion_panel && parent_screen) {
        spdlog::debug("Creating motion panel...");
        motion_panel = (lv_obj_t*)lv_xml_create(parent_screen, "motion_panel", nullptr);

        if (!motion_panel) {
            LOG_ERROR_INTERNAL("Failed to create motion panel from XML");
            NOTIFY_ERROR("Failed to load motion panel");
            return;
        }

        // Setup event handlers for motion panel
        ui_panel_motion_setup(motion_panel, parent_screen);

        // Initially hidden
        lv_obj_add_flag(motion_panel, LV_OBJ_FLAG_HIDDEN);
        spdlog::info("Motion panel created and initialized");
    }

    // Push motion panel onto navigation history and show it
    if (motion_panel) {
        ui_nav_push_overlay(motion_panel);
    }
})

LVGL_SAFE_EVENT_CB(card_nozzle_temp_clicked, {
    spdlog::debug("Nozzle Temp card clicked - opening Nozzle Temperature sub-screen");

    // Create nozzle temp panel on first access
    if (!nozzle_temp_panel && parent_screen) {
        spdlog::debug("Creating nozzle temperature panel...");
        nozzle_temp_panel = (lv_obj_t*)lv_xml_create(parent_screen, "nozzle_temp_panel", nullptr);

        if (!nozzle_temp_panel) {
            LOG_ERROR_INTERNAL("Failed to create nozzle temp panel from XML");
            NOTIFY_ERROR("Failed to load temperature panel");
            return;
        }

        // Setup event handlers for nozzle temp panel
        ui_panel_controls_temp_nozzle_setup(nozzle_temp_panel, parent_screen);

        // Initially hidden
        lv_obj_add_flag(nozzle_temp_panel, LV_OBJ_FLAG_HIDDEN);
        spdlog::info("Nozzle temp panel created and initialized");
    }

    // Push nozzle temp panel onto navigation history and show it
    if (nozzle_temp_panel) {
        ui_nav_push_overlay(nozzle_temp_panel);
    }
})

LVGL_SAFE_EVENT_CB(card_bed_temp_clicked, {
    spdlog::debug("Bed Temp card clicked - opening Heatbed Temperature sub-screen");

    // Create bed temp panel on first access
    if (!bed_temp_panel && parent_screen) {
        spdlog::debug("Creating bed temperature panel...");
        bed_temp_panel = (lv_obj_t*)lv_xml_create(parent_screen, "bed_temp_panel", nullptr);

        if (!bed_temp_panel) {
            LOG_ERROR_INTERNAL("Failed to create bed temp panel from XML");
            NOTIFY_ERROR("Failed to load temperature panel");
            return;
        }

        // Setup event handlers for bed temp panel
        ui_panel_controls_temp_bed_setup(bed_temp_panel, parent_screen);

        // Initially hidden
        lv_obj_add_flag(bed_temp_panel, LV_OBJ_FLAG_HIDDEN);
        spdlog::info("Bed temp panel created and initialized");
    }

    // Push bed temp panel onto navigation history and show it
    if (bed_temp_panel) {
        ui_nav_push_overlay(bed_temp_panel);
    }
})

LVGL_SAFE_EVENT_CB(card_extrusion_clicked, {
    spdlog::debug("Extrusion card clicked - opening Extrusion sub-screen");

    // Create extrusion panel on first access
    if (!extrusion_panel && parent_screen) {
        spdlog::debug("Creating extrusion panel...");
        extrusion_panel = (lv_obj_t*)lv_xml_create(parent_screen, "extrusion_panel", nullptr);

        if (!extrusion_panel) {
            LOG_ERROR_INTERNAL("Failed to create extrusion panel from XML");
            NOTIFY_ERROR("Failed to load extrusion panel");
            return;
        }

        // Setup event handlers for extrusion panel
        ui_panel_controls_extrusion_setup(extrusion_panel, parent_screen);

        // Initially hidden
        lv_obj_add_flag(extrusion_panel, LV_OBJ_FLAG_HIDDEN);
        spdlog::info("Extrusion panel created and initialized");
    }

    // Push extrusion panel onto navigation history and show it
    if (extrusion_panel) {
        ui_nav_push_overlay(extrusion_panel);
    }
})

LVGL_SAFE_EVENT_CB(card_fan_clicked, {
    spdlog::debug("Fan card clicked - Phase 2 feature");
    // TODO: Create and show fan control sub-screen (Phase 2)
})

LVGL_SAFE_EVENT_CB(card_motors_clicked, {
    spdlog::debug("Motors Disable card clicked");
    // TODO: Show confirmation dialog, then send motors disable command
})

// ============================================================================
// Public Functions
// ============================================================================

void ui_panel_controls_init_subjects() {
    // TODO: Initialize subjects for sub-screens
    // For now, no subjects needed at launcher level
    spdlog::debug("Controls panel subjects initialized");
}

void ui_panel_controls_wire_events(lv_obj_t* panel_obj, lv_obj_t* screen) {
    if (!panel_obj) {
        spdlog::error("Cannot wire controls panel events: null panel object");
        return;
    }

    // Store screen reference (needed for overlay panel creation)
    parent_screen = screen;

    // Find launcher card objects by name
    lv_obj_t* card_motion = lv_obj_find_by_name(panel_obj, "card_motion");
    lv_obj_t* card_nozzle_temp = lv_obj_find_by_name(panel_obj, "card_nozzle_temp");
    lv_obj_t* card_bed_temp = lv_obj_find_by_name(panel_obj, "card_bed_temp");
    lv_obj_t* card_extrusion = lv_obj_find_by_name(panel_obj, "card_extrusion");
    lv_obj_t* card_fan = lv_obj_find_by_name(panel_obj, "card_fan");
    lv_obj_t* card_motors = lv_obj_find_by_name(panel_obj, "card_motors");

    // Verify all cards found
    if (!card_motion || !card_nozzle_temp || !card_bed_temp || !card_extrusion || !card_fan ||
        !card_motors) {
        spdlog::error("Failed to find all controls panel launcher cards");
        return;
    }

    // Wire click event handlers
    lv_obj_add_event_cb(card_motion, card_motion_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_nozzle_temp, card_nozzle_temp_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_bed_temp, card_bed_temp_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_extrusion, card_extrusion_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_fan, card_fan_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_motors, card_motors_clicked, LV_EVENT_CLICKED, NULL);

    // Make cards clickable
    lv_obj_add_flag(card_motion, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card_nozzle_temp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card_bed_temp, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card_extrusion, LV_OBJ_FLAG_CLICKABLE);
    // card_fan is disabled (Phase 2), don't make clickable
    lv_obj_add_flag(card_motors, LV_OBJ_FLAG_CLICKABLE);

    spdlog::debug("Controls panel events wired");
}

lv_obj_t* ui_panel_controls_get() {
    return controls_panel;
}

void ui_panel_controls_set(lv_obj_t* panel_obj) {
    controls_panel = panel_obj;
}
