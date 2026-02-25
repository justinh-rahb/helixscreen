// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "app_globals.h"
#include "grid_layout.h"
#include "panel_widget_config.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

namespace helix {

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
