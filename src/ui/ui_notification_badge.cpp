// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lvgl/lvgl.h"
#include "lvgl/src/xml/lv_xml.h"
#include "lvgl/src/xml/lv_xml_parser.h"
#include "lvgl/src/xml/lv_xml_utils.h"
#include "lvgl/src/xml/lv_xml_widget.h"
#include "lvgl/src/xml/parsers/lv_xml_obj_parser.h"
#include "theme_core.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <cstring>

namespace {

/**
 * @brief Update badge text color based on background luminance
 */
void update_badge_text_contrast(lv_obj_t* badge) {
    // Find the text label child
    lv_obj_t* label = lv_obj_get_child(badge, 0);
    if (!label) {
        return;
    }

    // Get badge background color
    lv_color_t bg = lv_obj_get_style_bg_color(badge, LV_PART_MAIN);
    lv_color_t text_color = theme_core_get_contrast_text_color(bg);

    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);

    spdlog::trace("[notification_badge] contrast update: bg=0x{:06X} text=0x{:06X}",
                  lv_color_to_u32(bg) & 0xFFFFFF, lv_color_to_u32(text_color) & 0xFFFFFF);
}

/**
 * @brief Event callback for style changes - update text contrast
 */
void badge_style_changed_cb(lv_event_t* e) {
    lv_obj_t* badge = lv_event_get_target_obj(e);
    update_badge_text_contrast(badge);
}

/**
 * @brief Observer callback to update label text when subject changes
 */
void badge_text_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    lv_obj_t* label = static_cast<lv_obj_t*>(lv_observer_get_user_data(observer));
    if (!label)
        return;

    const char* text = static_cast<const char*>(lv_subject_get_pointer(subject));
    if (text) {
        lv_label_set_text(label, text);
        // Re-center after text change
        lv_obj_center(label);
    }
}

/**
 * @brief XML create handler for notification_badge
 *
 * Creates a circular badge with:
 * - Background color bound to severity
 * - Auto-contrast text color
 * - Child label for count display
 */
void* notification_badge_create(lv_xml_parser_state_t* state, const char** attrs) {
    lv_obj_t* parent = static_cast<lv_obj_t*>(lv_xml_state_get_parent(state));

    // Create badge container
    lv_obj_t* badge = lv_obj_create(parent);

    // Default styling - circular badge
    lv_obj_set_size(badge, 18, 18);
    lv_obj_set_style_radius(badge, 9, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    // Parse variant for default background color
    const char* variant = lv_xml_get_value_of(attrs, "variant");
    if (!variant) {
        variant = "info";
    }

    // Set background color based on variant
    lv_color_t bg_color;
    if (strcmp(variant, "warning") == 0) {
        bg_color = theme_manager_get_color("warning");
    } else if (strcmp(variant, "error") == 0 || strcmp(variant, "danger") == 0) {
        bg_color = theme_manager_get_color("danger");
    } else {
        bg_color = theme_manager_get_color("info");
    }
    lv_obj_set_style_bg_color(badge, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);

    // Parse text attribute
    const char* text = lv_xml_get_value_of(attrs, "text");
    if (!text) {
        text = "0";
    }

    // Create label for count
    lv_obj_t* label = lv_label_create(badge);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, theme_manager_get_font("font_small"), LV_PART_MAIN);
    lv_obj_center(label);

    // Handle bind_text - connect subject to internal label
    const char* bind_text = lv_xml_get_value_of(attrs, "bind_text");
    if (bind_text && strlen(bind_text) > 0) {
        lv_subject_t* subject = lv_xml_get_subject(&state->scope, bind_text);
        if (subject) {
            // Set initial value
            const char* initial = static_cast<const char*>(lv_subject_get_pointer(subject));
            if (initial) {
                lv_label_set_text(label, initial);
            }
            // Subscribe to updates - observer freed when label is deleted
            lv_subject_add_observer_obj(subject, badge_text_observer_cb, label, label);
            spdlog::trace("[notification_badge] Bound text to subject '{}'", bind_text);
        } else {
            spdlog::warn("[notification_badge] Subject '{}' not found for bind_text", bind_text);
        }
    }

    // Apply initial text contrast
    update_badge_text_contrast(badge);

    // Register for style changes to update contrast when bg changes
    lv_obj_add_event_cb(badge, badge_style_changed_cb, LV_EVENT_STYLE_CHANGED, nullptr);

    spdlog::trace("[notification_badge] Created badge variant='{}' text='{}'", variant, text);
    return badge;
}

} // namespace

extern "C" {

void ui_notification_badge_init() {
    lv_xml_register_widget("notification_badge", notification_badge_create, lv_xml_obj_apply);
    spdlog::debug("[notification_badge] Registered widget");
}

} // extern "C"
