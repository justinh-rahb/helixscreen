// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "ui_fonts.h"

#include "app_globals.h"
#include "grid_layout.h"
#include "panel_widget_config.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

namespace helix {

// MDI icon_xmark glyph (U+F0156)
static constexpr const char* ICON_XMARK = "\xF3\xB0\x85\x96";

GridEditMode::~GridEditMode() {
    if (active_) {
        exit();
    }
}

void GridEditMode::enter(lv_obj_t* container, PanelWidgetConfig* config) {
    if (active_) {
        spdlog::debug("[GridEditMode] Already active, ignoring enter()");
        return;
    }
    active_ = true;
    container_ = container;
    config_ = config;
    lv_subject_set_int(&get_home_edit_mode_subject(), 1);
    create_dots_overlay();
    spdlog::info("[GridEditMode] Entered edit mode");
}

void GridEditMode::exit() {
    if (!active_) {
        return;
    }
    active_ = false;
    destroy_selection_chrome();
    selected_ = nullptr;
    destroy_dots_overlay();
    lv_subject_set_int(&get_home_edit_mode_subject(), 0);

    if (config_) {
        config_->save();
    }
    if (save_cb_) {
        save_cb_();
    }

    container_ = nullptr;
    config_ = nullptr;
    spdlog::info("[GridEditMode] Exited edit mode");
}

void GridEditMode::select_widget(lv_obj_t* widget) {
    if (!active_) {
        return;
    }
    if (widget == selected_) {
        return;
    }
    destroy_selection_chrome();
    selected_ = widget;
    if (widget && container_) {
        create_selection_chrome(widget);
    }
    spdlog::debug("[GridEditMode] Selected widget: {}", static_cast<void*>(widget));
}

void GridEditMode::handle_click(lv_event_t* /*e*/) {
    if (!active_ || !container_) {
        return;
    }

    // Get click point in screen coordinates
    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // Hit-test child widgets (skip floating overlays like dots_overlay_ and selection_overlay_)
    lv_obj_t* hit = nullptr;
    uint32_t child_count = lv_obj_get_child_count(container_);
    for (uint32_t i = 0; i < child_count; ++i) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
        if (!child) {
            continue;
        }
        // Skip our overlay objects
        if (child == dots_overlay_ || child == selection_overlay_) {
            continue;
        }
        // Skip floating objects (they are overlays, not grid widgets)
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }

        lv_area_t coords;
        lv_obj_get_coords(child, &coords);
        if (point.x >= coords.x1 && point.x <= coords.x2 && point.y >= coords.y1 &&
            point.y <= coords.y2) {
            hit = child;
            // Keep searching — later children are drawn on top, so prefer last match
        }
    }

    if (hit) {
        select_widget(hit);
    } else {
        select_widget(nullptr);
    }
}

void GridEditMode::create_selection_chrome(lv_obj_t* widget) {
    if (!container_) {
        return;
    }

    // Get widget coordinates (screen-absolute)
    lv_area_t widget_area;
    lv_obj_get_coords(widget, &widget_area);

    // Get container coordinates to compute relative position
    lv_area_t container_area;
    lv_obj_get_coords(container_, &container_area);

    int rel_x1 = widget_area.x1 - container_area.x1;
    int rel_y1 = widget_area.y1 - container_area.y1;
    int rel_x2 = widget_area.x2 - container_area.x1;
    int rel_y2 = widget_area.y2 - container_area.y1;
    int widget_w = rel_x2 - rel_x1;
    int widget_h = rel_y2 - rel_y1;

    // Create floating overlay container for selection chrome
    selection_overlay_ = lv_obj_create(container_);
    lv_obj_set_pos(selection_overlay_, rel_x1, rel_y1);
    lv_obj_set_size(selection_overlay_, widget_w, widget_h);
    lv_obj_add_flag(selection_overlay_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(selection_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(selection_overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(selection_overlay_, 0, 0);
    lv_obj_set_style_pad_all(selection_overlay_, 0, 0);

    // Bracket styling constants
    constexpr int BAR_LEN = 12;
    constexpr int BAR_THICK = 3;
    lv_color_t accent = theme_get_accent_color();

    // Helper to create one bar of an L-bracket
    auto make_bar = [&](int x, int y, int w, int h) {
        lv_obj_t* bar = lv_obj_create(selection_overlay_);
        lv_obj_set_pos(bar, x, y);
        lv_obj_set_size(bar, w, h);
        lv_obj_set_style_bg_color(bar, accent, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    };

    // Top-left bracket: horizontal bar + vertical bar
    make_bar(0, 0, BAR_LEN, BAR_THICK);
    make_bar(0, 0, BAR_THICK, BAR_LEN);

    // Top-right bracket
    make_bar(widget_w - BAR_LEN, 0, BAR_LEN, BAR_THICK);
    make_bar(widget_w - BAR_THICK, 0, BAR_THICK, BAR_LEN);

    // Bottom-left bracket
    make_bar(0, widget_h - BAR_THICK, BAR_LEN, BAR_THICK);
    make_bar(0, widget_h - BAR_LEN, BAR_THICK, BAR_LEN);

    // Bottom-right bracket
    make_bar(widget_w - BAR_LEN, widget_h - BAR_THICK, BAR_LEN, BAR_THICK);
    make_bar(widget_w - BAR_THICK, widget_h - BAR_LEN, BAR_THICK, BAR_LEN);

    // (X) removal button — top-right corner, slightly inset
    constexpr int BTN_SIZE = 24;
    constexpr int BTN_INSET = 4;
    lv_obj_t* x_btn = lv_obj_create(selection_overlay_);
    lv_obj_set_pos(x_btn, widget_w - BTN_SIZE - BTN_INSET, BTN_INSET);
    lv_obj_set_size(x_btn, BTN_SIZE, BTN_SIZE);
    lv_obj_set_style_radius(x_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(x_btn, ThemeManager::instance().get_color("danger"), 0);
    lv_obj_set_style_bg_opa(x_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(x_btn, 0, 0);
    lv_obj_set_style_pad_all(x_btn, 0, 0);
    lv_obj_add_flag(x_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(x_btn, LV_OBJ_FLAG_SCROLLABLE);

    // X icon label inside the button
    lv_obj_t* x_label = lv_label_create(x_btn);
    lv_label_set_text(x_label, ICON_XMARK);
    lv_obj_set_style_text_font(x_label, &mdi_icons_16, 0);
    lv_obj_set_style_text_color(x_label, lv_color_white(), 0);
    lv_obj_center(x_label);

    // (X) button click handler — exception: dynamic overlay chrome uses lv_obj_add_event_cb
    lv_obj_add_event_cb(
        x_btn,
        [](lv_event_t* ev) {
            auto* self = static_cast<GridEditMode*>(lv_event_get_user_data(ev));
            self->remove_selected_widget();
        },
        LV_EVENT_CLICKED, this);

    spdlog::debug("[GridEditMode] Created selection chrome for widget at ({},{} {}x{})", rel_x1,
                  rel_y1, widget_w, widget_h);
}

void GridEditMode::destroy_selection_chrome() {
    if (selection_overlay_) {
        lv_obj_delete(selection_overlay_);
        selection_overlay_ = nullptr;
    }
}

void GridEditMode::remove_selected_widget() {
    if (!selected_ || !config_) {
        spdlog::warn("[GridEditMode] remove_selected_widget: no selection or config");
        return;
    }

    // Find config entry matching the selected widget by comparing grid positions.
    // Container children are in creation order which matches enabled entries order.
    // Iterate container children to find the index of the selected widget.
    int widget_child_index = -1;
    int grid_child_count = 0;
    uint32_t total_children = lv_obj_get_child_count(container_);
    for (uint32_t i = 0; i < total_children; ++i) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
        if (!child || child == dots_overlay_ || child == selection_overlay_) {
            continue;
        }
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }
        if (child == selected_) {
            widget_child_index = grid_child_count;
        }
        ++grid_child_count;
    }

    if (widget_child_index < 0) {
        spdlog::warn("[GridEditMode] Selected widget not found among container children");
        select_widget(nullptr);
        return;
    }

    // Map child index to config entry: count enabled entries
    const auto& entries = config_->entries();
    int enabled_index = 0;
    size_t config_index = 0;
    bool found = false;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].enabled) {
            if (enabled_index == widget_child_index) {
                config_index = i;
                found = true;
                break;
            }
            ++enabled_index;
        }
    }

    if (!found) {
        spdlog::warn("[GridEditMode] No config entry for child index {}", widget_child_index);
        select_widget(nullptr);
        return;
    }

    spdlog::info("[GridEditMode] Removing widget '{}' (config index {})", entries[config_index].id,
                 config_index);

    // Disable the widget in config
    config_->set_enabled(config_index, false);

    // Deselect before rebuild (chrome will be destroyed)
    select_widget(nullptr);

    // Save config and trigger rebuild
    config_->save();
    if (rebuild_cb_) {
        rebuild_cb_();
    }
}

void GridEditMode::create_dots_overlay() {
    if (!container_)
        return;

    // Get current breakpoint for grid dimensions
    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2; // Default MEDIUM
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    // Create transparent overlay that floats above grid children
    dots_overlay_ = lv_obj_create(container_);
    lv_obj_set_size(dots_overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(dots_overlay_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(dots_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(dots_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(dots_overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_overlay_, 0, 0);
    lv_obj_set_style_pad_all(dots_overlay_, 0, 0);

    // Get container content area dimensions
    lv_area_t area;
    lv_obj_get_content_coords(container_, &area);
    int w = area.x2 - area.x1;
    int h = area.y2 - area.y1;

    if (w <= 0 || h <= 0) {
        spdlog::warn("[GridEditMode] Container content area {}x{}, skipping dots", w, h);
        return;
    }

    constexpr int DOT_SIZE = 4;
    constexpr int DOT_HALF = DOT_SIZE / 2;
    lv_color_t dot_color = theme_manager_get_color("text_secondary");

    // Place a dot at each grid intersection (ncols+1 x nrows+1 points)
    for (int r = 0; r <= nrows; ++r) {
        for (int c = 0; c <= ncols; ++c) {
            lv_obj_t* dot = lv_obj_create(dots_overlay_);
            lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, dot_color, 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_30, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

            int x = (c * w / ncols) - DOT_HALF;
            int y = (r * h / nrows) - DOT_HALF;
            lv_obj_set_pos(dot, x, y);
        }
    }

    spdlog::debug("[GridEditMode] Created dots overlay: {}x{} grid, {}x{} area", ncols, nrows, w,
                  h);
}

void GridEditMode::destroy_dots_overlay() {
    if (dots_overlay_) {
        lv_obj_delete(dots_overlay_);
        dots_overlay_ = nullptr;
    }
}

} // namespace helix
