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

#include "ui_wizard.h"
#include "lvgl/lvgl.h"
#include <spdlog/spdlog.h>
#include <cstdio>

// Subject declarations (static/global scope required)
static lv_subject_t current_step;
static lv_subject_t total_steps;
static lv_subject_t wizard_title;
static lv_subject_t wizard_progress;
static lv_subject_t wizard_next_button_text;

// String buffers (must be persistent)
static char wizard_title_buffer[64];
static char wizard_progress_buffer[32];
static char wizard_next_button_text_buffer[16];

// Wizard container instance
static lv_obj_t* wizard_root = nullptr;

// Forward declarations
static void on_back_clicked(lv_event_t* e);
static void on_next_clicked(lv_event_t* e);

void ui_wizard_init_subjects() {
    spdlog::debug("[Wizard] Initializing subjects");

    // Initialize subjects with defaults
    lv_subject_init_int(&current_step, 1);
    lv_subject_init_int(&total_steps, 7);

    lv_subject_init_string(&wizard_title, wizard_title_buffer, nullptr,
                           sizeof(wizard_title_buffer), "Welcome");

    lv_subject_init_string(&wizard_progress, wizard_progress_buffer, nullptr,
                           sizeof(wizard_progress_buffer), "Step 1 of 7");

    lv_subject_init_string(&wizard_next_button_text, wizard_next_button_text_buffer,
                           nullptr, sizeof(wizard_next_button_text_buffer), "Next");

    // Register subjects globally
    lv_xml_register_subject(nullptr, "current_step", &current_step);
    lv_xml_register_subject(nullptr, "total_steps", &total_steps);
    lv_xml_register_subject(nullptr, "wizard_title", &wizard_title);
    lv_xml_register_subject(nullptr, "wizard_progress", &wizard_progress);
    lv_xml_register_subject(nullptr, "wizard_next_button_text", &wizard_next_button_text);

    spdlog::info("[Wizard] Subjects initialized");
}

void ui_wizard_register_responsive_constants() {
    spdlog::debug("[Wizard] Registering responsive constants");

    // Detect screen size
    int width = lv_display_get_horizontal_resolution(lv_display_get_default());

    // Calculate responsive values
    const char* padding_value;
    const char* gap_value;
    const char* header_height;
    const char* button_width;
    const char* header_font;
    const char* title_font;

    if (width < 600) {  // TINY (480x320)
        padding_value = "6";
        gap_value = "4";
        header_height = "32";  // Increased for better text fit
        button_width = "110";
        header_font = "montserrat_14";
        title_font = "montserrat_16";
        spdlog::info("[Wizard] Screen size: TINY ({}px)", width);
    } else if (width < 900) {  // SMALL (800x480)
        padding_value = "12";
        gap_value = "8";
        header_height = "36";  // Increased for better text fit
        button_width = "140";
        header_font = "montserrat_16";
        title_font = "montserrat_20";
        spdlog::info("[Wizard] Screen size: SMALL ({}px)", width);
    } else {  // LARGE (1024x600+)
        padding_value = "20";
        gap_value = "12";
        header_height = "44";  // Increased for better text fit
        button_width = "160";
        header_font = "montserrat_20";
        title_font = "montserrat_24";
        spdlog::info("[Wizard] Screen size: LARGE ({}px)", width);
    }

    // Get globals scope
    lv_xml_component_scope_t* scope = lv_xml_component_get_scope("globals");

    // Register constants BEFORE creating wizard
    lv_xml_register_const(scope, "wizard_padding", padding_value);
    lv_xml_register_const(scope, "wizard_gap", gap_value);
    lv_xml_register_const(scope, "wizard_header_height", header_height);
    lv_xml_register_const(scope, "wizard_button_width", button_width);
    lv_xml_register_const(scope, "wizard_header_font", header_font);
    lv_xml_register_const(scope, "wizard_title_font", title_font);

    spdlog::debug("[Wizard] Registered constants: padding={}, gap={}, header_height={}, button_width={}",
                  padding_value, gap_value, header_height, button_width);
}

void ui_wizard_register_event_callbacks() {
    spdlog::debug("[Wizard] Registering event callbacks");
    lv_xml_register_event_cb(nullptr, "on_back_clicked", on_back_clicked);
    lv_xml_register_event_cb(nullptr, "on_next_clicked", on_next_clicked);
}

lv_obj_t* ui_wizard_create(lv_obj_t* parent) {
    spdlog::debug("[Wizard] Creating wizard container");

    // Create wizard from XML (constants already registered)
    wizard_root = (lv_obj_t*)lv_xml_create(parent, "wizard_container", nullptr);

    if (!wizard_root) {
        spdlog::error("[Wizard] Failed to create wizard_container from XML");
        return nullptr;
    }

    // Update layout to ensure SIZE_CONTENT calculates correctly
    lv_obj_update_layout(wizard_root);

    spdlog::info("[Wizard] Wizard container created successfully");
    return wizard_root;
}

void ui_wizard_navigate_to_step(int step) {
    spdlog::debug("[Wizard] Navigating to step {}", step);

    // Clamp step to valid range
    int total = lv_subject_get_int(&total_steps);
    if (step < 1) step = 1;
    if (step > total) step = total;

    // Update current_step subject
    lv_subject_set_int(&current_step, step);

    // Update next button text based on step
    if (step == total) {
        lv_subject_copy_string(&wizard_next_button_text, "Finish");
    } else {
        lv_subject_copy_string(&wizard_next_button_text, "Next");
    }

    // Update progress text
    char progress_buf[32];
    snprintf(progress_buf, sizeof(progress_buf), "Step %d of %d", step, total);
    lv_subject_copy_string(&wizard_progress, progress_buf);

    spdlog::debug("[Wizard] Updated to step {}/{}, button: {}",
                  step, total, (step == total) ? "Finish" : "Next");
}

void ui_wizard_set_title(const char* title) {
    if (!title) {
        spdlog::warn("[Wizard] set_title called with nullptr, ignoring");
        return;
    }

    spdlog::debug("[Wizard] Setting title: {}", title);
    lv_subject_copy_string(&wizard_title, title);
}

// ============================================================================
// Event Handlers
// ============================================================================

static void on_back_clicked(lv_event_t* e) {
    (void)e;
    int current = lv_subject_get_int(&current_step);
    if (current > 1) {
        ui_wizard_navigate_to_step(current - 1);
    }
    spdlog::debug("[Wizard] Back button clicked, step: {}", current - 1);
}

static void on_next_clicked(lv_event_t* e) {
    (void)e;
    int current = lv_subject_get_int(&current_step);
    int total = lv_subject_get_int(&total_steps);

    if (current < total) {
        ui_wizard_navigate_to_step(current + 1);
        spdlog::debug("[Wizard] Next button clicked, step: {}", current + 1);
    } else {
        spdlog::info("[Wizard] Finish button clicked, completing wizard");
        // TODO: Handle wizard completion (emit event or callback)
    }
}
