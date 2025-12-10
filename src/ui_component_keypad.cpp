// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen

/**
 * @file ui_component_keypad.cpp
 * @brief Numeric keypad overlay with reactive Subject-Observer pattern
 *
 * The keypad is shown/hidden directly via LVGL flags (NOT through ui_nav) so it
 * appears on top of the current overlay panel without hiding it. This allows
 * users to see the temperature graph while entering a custom temperature value.
 *
 * The XML binds to the keypad_display subject, so updating the subject
 * automatically updates the UI.
 */

#include "ui_component_keypad.h"

#include "ui_event_safety.h"

#include "lvgl/lvgl.h"
#include "lvgl/src/xml/lv_xml.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Reactive State (Subject for XML binding)
// ============================================================================
static lv_subject_t keypad_display_subject;
static char keypad_display_buf[16] = "0";
static bool subjects_initialized = false;

// Widget reference (for showing/hiding via nav system)
static lv_obj_t* keypad_widget = nullptr;
static lv_obj_t* title_label = nullptr;

// Current config and input state
static ui_keypad_config_t current_config;
static char input_buffer[16] = "0";

// ============================================================================
// Forward declarations
// ============================================================================
static void update_display();
static void append_digit(int digit);
static void handle_backspace();
static void handle_cancel();
static void handle_confirm();
static void wire_button_events();

// ============================================================================
// Subject Initialization (call BEFORE XML creation)
// ============================================================================
void ui_keypad_init_subjects() {
    if (subjects_initialized) {
        return;
    }

    // Initialize display subject for reactive binding
    lv_subject_init_string(&keypad_display_subject, keypad_display_buf, nullptr,
                           sizeof(keypad_display_buf), "0");

    // Register with XML binding system so <lv_label-bind_text subject="keypad_display"/> works
    lv_xml_register_subject(nullptr, "keypad_display", &keypad_display_subject);

    subjects_initialized = true;
    spdlog::debug("Keypad subjects initialized");
}

// ============================================================================
// Widget Initialization (call AFTER XML creation)
// ============================================================================
void ui_keypad_init(lv_obj_t* parent) {
    if (!parent) {
        spdlog::error("Cannot init keypad: parent is null");
        return;
    }

    if (keypad_widget) {
        spdlog::warn("Keypad already initialized");
        return;
    }

    // Ensure subjects are initialized first
    ui_keypad_init_subjects();

    // Create keypad from XML component
    const char* attrs[] = {"title", "Enter Value", "unit_label", "", NULL};

    keypad_widget = (lv_obj_t*)lv_xml_create(parent, "numeric_keypad_modal", attrs);
    if (!keypad_widget) {
        spdlog::error("Failed to create keypad from XML");
        return;
    }

    // Find title label in header_bar for dynamic updates
    title_label = lv_obj_find_by_name(keypad_widget, "header_title");
    if (!title_label) {
        spdlog::warn("Keypad: header_title not found (title updates disabled)");
    }

    // Wire button events
    wire_button_events();

    spdlog::debug("Numeric keypad initialized");
}

// ============================================================================
// Public API
// ============================================================================
void ui_keypad_show(const ui_keypad_config_t* config) {
    if (!keypad_widget || !config) {
        spdlog::error("Cannot show keypad: not initialized or invalid config");
        return;
    }

    // Store config
    current_config = *config;

    // Format initial value
    if (config->allow_decimal) {
        snprintf(input_buffer, sizeof(input_buffer), "%.1f", config->initial_value);
    } else {
        snprintf(input_buffer, sizeof(input_buffer), "%d", (int)config->initial_value);
    }

    // Update display via subject (reactive binding updates XML automatically)
    update_display();

    // Update title if provided
    if (title_label && config->title_label) {
        lv_label_set_text(title_label, config->title_label);
    }

    // Show directly (NOT via nav system) so underlying panel stays visible
    // This allows users to see the temperature graph while entering custom temp
    lv_obj_remove_flag(keypad_widget, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keypad_widget);

    spdlog::info("Keypad: showing (initial={:.1f}, range={:.0f}-{:.0f})", config->initial_value,
                 config->min_value, config->max_value);
}

void ui_keypad_hide() {
    if (keypad_widget && ui_keypad_is_visible()) {
        // Hide directly (NOT via nav system)
        lv_obj_add_flag(keypad_widget, LV_OBJ_FLAG_HIDDEN);
    }
}

bool ui_keypad_is_visible() {
    if (!keypad_widget)
        return false;
    return !lv_obj_has_flag(keypad_widget, LV_OBJ_FLAG_HIDDEN);
}

lv_subject_t* ui_keypad_get_display_subject() {
    return &keypad_display_subject;
}

// ============================================================================
// Input Logic
// ============================================================================
static void update_display() {
    lv_subject_copy_string(&keypad_display_subject, input_buffer);
}

static void append_digit(int digit) {
    size_t len = strlen(input_buffer);

    // Replace initial "0" with first digit
    if (len == 1 && input_buffer[0] == '0') {
        input_buffer[0] = '0' + digit;
        update_display();
        return;
    }

    // Count digits (ignore decimal/minus)
    int digit_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (input_buffer[i] >= '0' && input_buffer[i] <= '9') {
            digit_count++;
        }
    }

    // Max 3 digits
    if (digit_count >= 3) {
        return;
    }

    // Append
    if (len < sizeof(input_buffer) - 1) {
        input_buffer[len] = '0' + digit;
        input_buffer[len + 1] = '\0';
        update_display();
    }
}

static void handle_backspace() {
    size_t len = strlen(input_buffer);
    if (len > 0) {
        input_buffer[len - 1] = '\0';
    }

    // Reset to "0" if empty
    if (strlen(input_buffer) == 0 || strcmp(input_buffer, "-") == 0) {
        strcpy(input_buffer, "0");
    }

    update_display();
}

static void handle_cancel() {
    ui_keypad_hide();
    spdlog::debug("Keypad: cancelled");
}

static void handle_confirm() {
    // Parse and clamp value
    float value = static_cast<float>(atof(input_buffer));
    if (value < current_config.min_value)
        value = current_config.min_value;
    if (value > current_config.max_value)
        value = current_config.max_value;

    // Hide first (before callback, in case callback shows something else)
    ui_keypad_hide();

    // Invoke callback
    if (current_config.callback) {
        current_config.callback(value, current_config.user_data);
        spdlog::info("Keypad: confirmed value={:.1f}", value);
    }
}

// ============================================================================
// Event Wiring
// ============================================================================
static void wire_button_events() {
    if (!keypad_widget)
        return;

    // Number buttons 0-9
    const char* btn_names[] = {"btn_0", "btn_1", "btn_2", "btn_3", "btn_4",
                               "btn_5", "btn_6", "btn_7", "btn_8", "btn_9"};

    for (int i = 0; i < 10; i++) {
        lv_obj_t* btn = lv_obj_find_by_name(keypad_widget, btn_names[i]);
        if (btn) {
            lv_obj_add_event_cb(
                btn,
                [](lv_event_t* e) {
                    ui_event_safe_call("keypad_digit", [e]() {
                        int digit = (int)(intptr_t)lv_event_get_user_data(e);
                        append_digit(digit);
                    });
                },
                LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
    }

    // Backspace button
    lv_obj_t* btn_back = lv_obj_find_by_name(keypad_widget, "btn_backspace");
    if (btn_back) {
        lv_obj_add_event_cb(
            btn_back,
            [](lv_event_t*) {
                ui_event_safe_call("keypad_backspace", []() { handle_backspace(); });
            },
            LV_EVENT_CLICKED, nullptr);
    }

    // Back button (in header_bar) → cancel
    lv_obj_t* back_btn = lv_obj_find_by_name(keypad_widget, "back_button");
    if (back_btn) {
        lv_obj_add_event_cb(
            back_btn,
            [](lv_event_t*) { ui_event_safe_call("keypad_cancel", []() { handle_cancel(); }); },
            LV_EVENT_CLICKED, nullptr);
    }

    // Action button (OK in header_bar) → confirm
    lv_obj_t* ok_btn = lv_obj_find_by_name(keypad_widget, "action_button");
    if (ok_btn) {
        lv_obj_add_event_cb(
            ok_btn,
            [](lv_event_t*) { ui_event_safe_call("keypad_confirm", []() { handle_confirm(); }); },
            LV_EVENT_CLICKED, nullptr);
    }

    spdlog::debug("Keypad: events wired");
}
