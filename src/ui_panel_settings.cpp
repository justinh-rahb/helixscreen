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

#include "ui_panel_settings.h"

#include "ui_nav.h"
#include "ui_panel_bed_mesh.h"

#include <spdlog/spdlog.h>

// Panel object references
static lv_obj_t* settings_panel = nullptr;
static lv_obj_t* bed_mesh_panel = nullptr;
static lv_obj_t* parent_screen = nullptr;

// Card click event handlers
static void card_network_clicked(lv_event_t* e);
static void card_display_clicked(lv_event_t* e);
static void card_bed_mesh_clicked(lv_event_t* e);
static void card_z_offset_clicked(lv_event_t* e);
static void card_printer_info_clicked(lv_event_t* e);
static void card_about_clicked(lv_event_t* e);

void ui_panel_settings_init_subjects() {
    // Initialize bed mesh panel subjects
    ui_panel_bed_mesh_init_subjects();
    spdlog::info("Settings panel subjects initialized");
}

void ui_panel_settings_wire_events(lv_obj_t* panel_obj, lv_obj_t* screen) {
    if (!panel_obj) {
        spdlog::error("Cannot wire settings panel events: null panel object");
        return;
    }

    // Store screen reference (needed for overlay panel creation)
    parent_screen = screen;

    // Find launcher card objects by name
    lv_obj_t* card_network = lv_obj_find_by_name(panel_obj, "card_network");
    lv_obj_t* card_display = lv_obj_find_by_name(panel_obj, "card_display");
    lv_obj_t* card_bed_mesh = lv_obj_find_by_name(panel_obj, "card_bed_mesh");
    lv_obj_t* card_z_offset = lv_obj_find_by_name(panel_obj, "card_z_offset");
    lv_obj_t* card_printer_info = lv_obj_find_by_name(panel_obj, "card_printer_info");
    lv_obj_t* card_about = lv_obj_find_by_name(panel_obj, "card_about");

    // Verify all cards found
    if (!card_network || !card_display || !card_bed_mesh || !card_z_offset || !card_printer_info ||
        !card_about) {
        spdlog::error("Failed to find all settings panel launcher cards");
        return;
    }

    // Wire click event handlers
    lv_obj_add_event_cb(card_network, card_network_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_display, card_display_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_bed_mesh, card_bed_mesh_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_z_offset, card_z_offset_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_printer_info, card_printer_info_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card_about, card_about_clicked, LV_EVENT_CLICKED, NULL);

    // Make only active cards clickable (bed_mesh is the only active one for now)
    lv_obj_add_flag(card_bed_mesh, LV_OBJ_FLAG_CLICKABLE);
    // Others are disabled placeholders - no clickable flag

    spdlog::info("Settings panel events wired");
}

lv_obj_t* ui_panel_settings_get() {
    return settings_panel;
}

void ui_panel_settings_set(lv_obj_t* panel_obj) {
    settings_panel = panel_obj;
}

// ============================================================================
// Card Click Event Handlers
// ============================================================================

static void card_network_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("Network card clicked (placeholder - not yet implemented)");
    // TODO: Open network settings panel
}

static void card_display_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("Display card clicked (placeholder - not yet implemented)");
    // TODO: Open display settings panel
}

static void card_bed_mesh_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("Bed Mesh card clicked - opening Bed Mesh Visualization");

    // Create bed mesh panel on first access
    if (!bed_mesh_panel && parent_screen) {
        spdlog::debug("Creating bed mesh visualization panel...");

        // Create from XML
        bed_mesh_panel = (lv_obj_t*)lv_xml_create(parent_screen, "bed_mesh_panel", nullptr);
        if (bed_mesh_panel) {
            // Setup event handlers and renderer
            ui_panel_bed_mesh_setup(bed_mesh_panel, parent_screen);

            // Initially hidden
            lv_obj_add_flag(bed_mesh_panel, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("Bed mesh visualization panel created");
        } else {
            spdlog::error("Failed to create bed mesh panel from XML");
            return;
        }
    }

    // Push bed mesh panel onto navigation history and show it
    if (bed_mesh_panel) {
        ui_nav_push_overlay(bed_mesh_panel);
    }
}

static void card_z_offset_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("Z-Offset card clicked (placeholder - not yet implemented)");
    // TODO: Open Z-offset adjustment panel
}

static void card_printer_info_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("Printer Info card clicked (placeholder - not yet implemented)");
    // TODO: Open printer info panel
}

static void card_about_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("About card clicked (placeholder - not yet implemented)");
    // TODO: Open about panel
}
