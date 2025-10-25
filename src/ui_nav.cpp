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

#include "ui_nav.h"
#include "ui_theme.h"
#include "ui_fonts.h"
#include "lvgl/lvgl.h"
#include <cstdio>
#include <cstdlib>  // for atoi
#include <vector>

// Active panel tracking
static lv_subject_t active_panel_subject;
static ui_panel_id_t active_panel = UI_PANEL_HOME;

// Icon color subjects (one per navbar button)
static lv_subject_t icon_color_subjects[UI_PANEL_COUNT];

// Panel widget tracking for show/hide
static lv_obj_t* panel_widgets[UI_PANEL_COUNT] = {nullptr};

// App layout widget reference (contains navbar + panels, must never be hidden)
static lv_obj_t* app_layout_widget = nullptr;

// Subjects initialization flag
static bool subjects_initialized = false;

// Panel stack: tracks ALL visible panels in z-order (bottom to top)
// Last element is the currently visible top panel
// This replaces the old nav_history approach and eliminates guessing
static std::vector<lv_obj_t*> panel_stack;

// Observer callback - updates all icon colors when active panel changes
static void active_panel_observer_cb(lv_observer_t* /*observer*/, lv_subject_t* subject) {
    int32_t new_active_panel = lv_subject_get_int(subject);

    // Update all icon color subjects based on which panel is active
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (i == new_active_panel) {
            lv_subject_set_color(&icon_color_subjects[i], UI_COLOR_PRIMARY);
        } else {
            lv_subject_set_color(&icon_color_subjects[i], UI_COLOR_NAV_INACTIVE);
        }
    }

    // Show/hide panels if widgets are set
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (panel_widgets[i]) {
            if (i == new_active_panel) {
                lv_obj_remove_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

// Observer callback for icon color changes - updates image recolor style
static void icon_image_color_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    lv_obj_t* image = (lv_obj_t*)lv_observer_get_target(observer);
    lv_color_t color = lv_subject_get_color(subject);
    // Material Design icons are white - use recolor to tint them red/gray
    lv_obj_set_style_img_recolor(image, color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(image, 255, LV_PART_MAIN);
}

// Button click event handler - switches active panel
static void nav_button_clicked_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    int panel_id = (int)(uintptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        // DEFENSIVE: Hide ALL visible overlay panels (not in panel_widgets)
        // This handles overlays shown via command line, push_overlay, or other means
        lv_obj_t* screen = lv_screen_active();
        if (screen) {
            for (uint32_t i = 0; i < lv_obj_get_child_count(screen); i++) {
                lv_obj_t* child = lv_obj_get_child(screen, i);
                if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
                    continue;  // Already hidden
                }

                // Don't hide app_layout (contains navbar + panels)
                if (child == app_layout_widget) {
                    continue;
                }

                // Check if this is NOT one of the main nav panels
                bool is_main_panel = false;
                for (int j = 0; j < UI_PANEL_COUNT; j++) {
                    if (panel_widgets[j] == child) {
                        is_main_panel = true;
                        break;
                    }
                }

                // Hide any visible overlay panel
                if (!is_main_panel) {
                    lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
                    LV_LOG_USER("Hiding overlay panel %p (nav button clicked)", child);
                }
            }
        }

        // Hide all main panels
        for (int i = 0; i < UI_PANEL_COUNT; i++) {
            if (panel_widgets[i]) {
                lv_obj_add_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Clear panel stack when switching via nav bar
        panel_stack.clear();
        LV_LOG_USER("Panel stack cleared (nav button clicked)");

        // Show the clicked panel and add it to stack
        lv_obj_t* new_panel = panel_widgets[panel_id];
        if (new_panel) {
            lv_obj_remove_flag(new_panel, LV_OBJ_FLAG_HIDDEN);
            panel_stack.push_back(new_panel);
            LV_LOG_USER("Showing panel %p (stack depth: %zu)", new_panel, panel_stack.size());
        }

        // Update active panel state (triggers icon colors, etc.)
        ui_nav_set_active((ui_panel_id_t)panel_id);
    }
}

void ui_nav_init() {
    if (subjects_initialized) {
        LV_LOG_WARN("Navigation subjects already initialized");
        return;
    }

    LV_LOG_USER("Initializing navigation reactive subjects...");

    // Initialize active panel subject (starts at home)
    lv_subject_init_int(&active_panel_subject, UI_PANEL_HOME);

    // Initialize icon color subjects (all inactive except home)
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (i == UI_PANEL_HOME) {
            lv_subject_init_color(&icon_color_subjects[i], UI_COLOR_PRIMARY);
        } else {
            lv_subject_init_color(&icon_color_subjects[i], UI_COLOR_NAV_INACTIVE);
        }
    }

    // Register subjects for XML binding
    lv_xml_register_subject(NULL, "active_panel", &active_panel_subject);
    lv_xml_register_subject(NULL, "nav_icon_0_color", &icon_color_subjects[0]);
    lv_xml_register_subject(NULL, "nav_icon_1_color", &icon_color_subjects[1]);
    lv_xml_register_subject(NULL, "nav_icon_2_color", &icon_color_subjects[2]);
    lv_xml_register_subject(NULL, "nav_icon_3_color", &icon_color_subjects[3]);
    lv_xml_register_subject(NULL, "nav_icon_4_color", &icon_color_subjects[4]);
    lv_xml_register_subject(NULL, "nav_icon_5_color", &icon_color_subjects[5]);

    // Add observer to active panel subject to update icon colors
    lv_subject_add_observer(&active_panel_subject, active_panel_observer_cb, NULL);

    subjects_initialized = true;

    LV_LOG_USER("Navigation subjects initialized successfully");
}

void ui_nav_set_app_layout(lv_obj_t* app_layout) {
    app_layout_widget = app_layout;
    LV_LOG_USER("App layout widget registered");
}

void ui_nav_wire_events(lv_obj_t* navbar) {
    if (!navbar) {
        LV_LOG_ERROR("NULL navbar provided to ui_nav_wire_events");
        return;
    }

    if (!subjects_initialized) {
        LV_LOG_ERROR("Navigation subjects not initialized! Call ui_nav_init() first!");
        return;
    }

    // Ensure navbar container doesn't block clicks to children
    lv_obj_remove_flag(navbar, LV_OBJ_FLAG_CLICKABLE);

    // Determine responsive sizing based on screen height using theme constants
    lv_display_t* display = lv_display_get_default();
    int32_t screen_height = lv_display_get_vertical_resolution(display);
    uint16_t icon_scale;  // 256 = 100%, 128 = 50%, etc.
    lv_coord_t button_size;
    lv_coord_t nav_padding;
    lv_coord_t nav_width;

    if (screen_height <= UI_SCREEN_TINY_H) {
        // Tiny screens (320px): 42px buttons, 0px padding, 52px width
        // Icons scaled to 60% (64px → 38px) to fit comfortably in 42px buttons
        icon_scale = 154;
        button_size = UI_NAV_BUTTON_SIZE_TINY;
        nav_padding = UI_NAV_PADDING_TINY;
        nav_width = UI_NAV_WIDTH_TINY;
        LV_LOG_USER("Tiny nav sizing (h=%d): width=%d, buttons=%d, padding=%d, icon_scale=%d",
                    screen_height, nav_width, button_size, nav_padding, icon_scale);
    } else if (screen_height <= UI_SCREEN_SMALL_H) {
        // Small screens (480px): 60px buttons, 8px padding, 76px width
        // Icons scaled to 60% (64px → 38px)
        icon_scale = 154;
        button_size = UI_NAV_BUTTON_SIZE_SMALL;
        nav_padding = UI_NAV_PADDING_SMALL;
        nav_width = UI_NAV_WIDTH_SMALL;
        LV_LOG_USER("Small nav sizing (h=%d): width=%d, buttons=%d, padding=%d",
                    screen_height, nav_width, button_size, nav_padding);
    } else if (screen_height <= UI_SCREEN_MEDIUM_H) {
        // Medium screens (600px): 70px buttons, 12px padding, 94px width
        icon_scale = 192;
        button_size = UI_NAV_BUTTON_SIZE_MEDIUM;
        nav_padding = UI_NAV_PADDING_MEDIUM;
        nav_width = UI_NAV_WIDTH_MEDIUM;
        LV_LOG_USER("Medium nav sizing (h=%d): width=%d, buttons=%d, padding=%d",
                    screen_height, nav_width, button_size, nav_padding);
    } else {
        // Large screens (720px+): 70px buttons, 16px padding, 102px width
        icon_scale = 256;
        button_size = UI_NAV_BUTTON_SIZE_LARGE;
        nav_padding = UI_NAV_PADDING_LARGE;
        nav_width = UI_NAV_WIDTH_LARGE;
        LV_LOG_USER("Large nav sizing (h=%d): width=%d, buttons=%d, padding=%d",
                    screen_height, nav_width, button_size, nav_padding);
    }

    // Apply responsive width and padding to navbar container
    lv_obj_set_width(navbar, nav_width);
    lv_obj_set_style_pad_all(navbar, nav_padding, LV_PART_MAIN);

    // Name-based widget lookup for navigation buttons and icons (order matches ui_panel_id_t enum)
    const char* button_names[] = {"nav_btn_home", "nav_btn_print_select", "nav_btn_controls", "nav_btn_filament", "nav_btn_settings", "nav_btn_advanced"};
    const char* icon_names[] = {"nav_icon_home", "nav_icon_print_select", "nav_icon_controls", "nav_icon_filament", "nav_icon_settings", "nav_icon_advanced"};

    // Bind colors to icon widgets and add click event handlers to buttons
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        lv_obj_t* btn = lv_obj_find_by_name(navbar, button_names[i]);
        lv_obj_t* icon_widget = lv_obj_find_by_name(navbar, icon_names[i]);

        if (!btn || !icon_widget) {
            LV_LOG_ERROR("Failed to find nav button/icon %d: btn=%p, icon=%p", i, btn, icon_widget);
            continue;
        }

        // All navigation icons are now Material Design images
        if (!lv_obj_check_type(icon_widget, &lv_image_class)) {
            LV_LOG_ERROR("Nav icon %d is not an image widget!", i);
            continue;
        }

        // Apply responsive sizing to button (touch target)
        lv_obj_set_size(btn, button_size, button_size);

        // Apply responsive scaling to Material Design image
        lv_image_set_scale(icon_widget, icon_scale);

        // Bind img_recolor to icon color subject
        lv_subject_add_observer_obj(&icon_color_subjects[i], icon_image_color_observer_cb, icon_widget, NULL);

        // Make icon widget non-clickable so clicks pass through to button
        lv_obj_add_flag(icon_widget, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_remove_flag(icon_widget, LV_OBJ_FLAG_CLICKABLE);

        // Ensure button is clickable and add event handler
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, nav_button_clicked_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }

    // Force update all icon color subjects now that bindings exist
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (i == active_panel) {
            lv_subject_set_color(&icon_color_subjects[i], UI_COLOR_PRIMARY);
        } else {
            lv_subject_set_color(&icon_color_subjects[i], UI_COLOR_NAV_INACTIVE);
        }
    }
}

void ui_nav_set_active(ui_panel_id_t panel_id) {
    if (panel_id >= UI_PANEL_COUNT) {
        LV_LOG_ERROR("Invalid panel ID: %d", panel_id);
        return;
    }

    if (panel_id == active_panel) {
        return;
    }

    // Update active panel subject - this triggers observer and icon color updates
    lv_subject_set_int(&active_panel_subject, panel_id);
    active_panel = panel_id;
}

ui_panel_id_t ui_nav_get_active() {
    return active_panel;
}

void ui_nav_set_panels(lv_obj_t** panels) {
    if (!panels) {
        LV_LOG_ERROR("NULL panels array provided");
        return;
    }

    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        panel_widgets[i] = panels[i];
    }

    // Hide all panels except active one
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (panel_widgets[i]) {
            if (i == active_panel) {
                lv_obj_remove_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Initialize panel stack with the active panel
    panel_stack.clear();
    if (panel_widgets[active_panel]) {
        panel_stack.push_back(panel_widgets[active_panel]);
        LV_LOG_USER("Panel stack initialized with active panel %p", panel_widgets[active_panel]);
    }

    LV_LOG_USER("Panel widgets registered for show/hide management");
}

void ui_nav_push_overlay(lv_obj_t* overlay_panel) {
    if (!overlay_panel) {
        LV_LOG_ERROR("Cannot push NULL overlay panel");
        return;
    }

    // Hide current top panel (if any)
    if (!panel_stack.empty()) {
        lv_obj_t* current_top = panel_stack.back();
        lv_obj_add_flag(current_top, LV_OBJ_FLAG_HIDDEN);
        LV_LOG_USER("Hiding current top panel %p (pushing overlay)", current_top);
    }

    // Show the new overlay and push it to stack
    lv_obj_remove_flag(overlay_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_panel);
    panel_stack.push_back(overlay_panel);

    LV_LOG_USER("Showing overlay panel %p (stack depth: %zu)", overlay_panel, panel_stack.size());
}

bool ui_nav_go_back() {
    LV_LOG_USER("=== ui_nav_go_back() called, stack depth: %zu ===", panel_stack.size());

    // DEFENSIVE: Always hide any overlay panels (not in panel_widgets)
    // This handles cases where panels were shown via command line or other means
    lv_obj_t* screen = lv_screen_active();
    if (screen) {
        LV_LOG_USER("Scanning %u screen children for overlays to hide", lv_obj_get_child_count(screen));
        for (uint32_t i = 0; i < lv_obj_get_child_count(screen); i++) {
            lv_obj_t* child = lv_obj_get_child(screen, i);

            // Don't hide app_layout (contains navbar + panels)
            if (child == app_layout_widget) {
                LV_LOG_USER("  Child %u: app_layout (skip)", i);
                continue;
            }

            // Check if this is NOT one of the main nav panels
            bool is_main_panel = false;
            for (int j = 0; j < UI_PANEL_COUNT; j++) {
                if (panel_widgets[j] == child) {
                    is_main_panel = true;
                    break;
                }
            }

            // Hide any overlay panel (even if already hidden, for consistency)
            if (!is_main_panel) {
                lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
                LV_LOG_USER("  Child %u: %p - HIDING overlay panel", i, child);
            } else {
                LV_LOG_USER("  Child %u: %p - main panel (skip)", i, child);
            }
        }
    }

    // Pop current panel from stack if present
    if (!panel_stack.empty()) {
        panel_stack.pop_back();
        LV_LOG_USER("Popped panel from stack (remaining depth: %zu)", panel_stack.size());
    }

    // Need at least one panel in stack to show
    if (panel_stack.empty()) {
        LV_LOG_USER("Panel stack empty after pop, falling back to home panel");

        // Hide all main panels
        for (int i = 0; i < UI_PANEL_COUNT; i++) {
            if (panel_widgets[i]) {
                lv_obj_add_flag(panel_widgets[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Show home panel
        if (panel_widgets[UI_PANEL_HOME]) {
            lv_obj_remove_flag(panel_widgets[UI_PANEL_HOME], LV_OBJ_FLAG_HIDDEN);
            panel_stack.push_back(panel_widgets[UI_PANEL_HOME]);
            active_panel = UI_PANEL_HOME;
            lv_subject_set_int(&active_panel_subject, UI_PANEL_HOME);
            LV_LOG_USER("Fallback: showing home panel");
            return true;
        }

        LV_LOG_ERROR("Cannot show home panel - widget not found!");
        return false;
    }

    // Show previous panel (new top of stack)
    lv_obj_t* previous_panel = panel_stack.back();

    // If previous panel is one of the main nav panels, hide all other main panels
    bool is_main_panel = false;
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        if (panel_widgets[i] == previous_panel) {
            is_main_panel = true;
            // Hide all main panels except this one
            for (int j = 0; j < UI_PANEL_COUNT; j++) {
                if (j != i && panel_widgets[j]) {
                    lv_obj_add_flag(panel_widgets[j], LV_OBJ_FLAG_HIDDEN);
                }
            }
            // Update active panel state
            active_panel = (ui_panel_id_t)i;
            lv_subject_set_int(&active_panel_subject, i);
            LV_LOG_USER("Updated active panel to %d", i);
            break;
        }
    }

    lv_obj_remove_flag(previous_panel, LV_OBJ_FLAG_HIDDEN);
    LV_LOG_USER("Showing previous panel %p (stack depth: %zu, is_main=%d)",
                previous_panel, panel_stack.size(), is_main_panel);

    return true;
}
