// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "ui_fonts.h"

#include "app_globals.h"
#include "grid_layout.h"
#include "panel_widget_config.h"
#include "panel_widget_registry.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace helix {

// MDI icon_xmark glyph (U+F0156)
static constexpr const char* ICON_XMARK = "\xF3\xB0\x85\x96";

// Drag visual constants
static constexpr int GHOST_BORDER_WIDTH = 2;
static constexpr lv_opa_t GHOST_BORDER_OPA = LV_OPA_50;
static constexpr int PREVIEW_BORDER_WIDTH = 3;
static constexpr lv_opa_t DRAG_SHADOW_OPA = LV_OPA_40;
static constexpr int DRAG_SHADOW_WIDTH = 12;
static constexpr int DRAG_SHADOW_OFS = 4;

// Resize corner detection radius (pixels from the corner bracket)
static constexpr int CORNER_HIT_RADIUS = 24;

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
    cleanup_drag_state();
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

int GridEditMode::find_config_index_for_widget(lv_obj_t* widget) const {
    if (!widget || !container_ || !config_) {
        return -1;
    }

    // Container children are in creation order matching enabled entries order.
    // Find which grid-child index this widget corresponds to.
    int widget_child_index = -1;
    int grid_child_count = 0;
    uint32_t total_children = lv_obj_get_child_count(container_);
    for (uint32_t i = 0; i < total_children; ++i) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
        if (!child || child == dots_overlay_ || child == selection_overlay_) {
            continue;
        }
        if (child == drag_ghost_ || child == snap_preview_) {
            continue;
        }
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }
        if (child == widget) {
            widget_child_index = grid_child_count;
        }
        ++grid_child_count;
    }

    if (widget_child_index < 0) {
        return -1;
    }

    // Map child index to config entry: count enabled entries
    const auto& entries = config_->entries();
    int enabled_index = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].enabled) {
            if (enabled_index == widget_child_index) {
                return static_cast<int>(i);
            }
            ++enabled_index;
        }
    }
    return -1;
}

void GridEditMode::remove_selected_widget() {
    if (!selected_ || !config_) {
        spdlog::warn("[GridEditMode] remove_selected_widget: no selection or config");
        return;
    }

    int config_index = find_config_index_for_widget(selected_);
    if (config_index < 0) {
        spdlog::warn("[GridEditMode] Selected widget not found in config");
        select_widget(nullptr);
        return;
    }

    const auto& entries = config_->entries();
    spdlog::info("[GridEditMode] Removing widget '{}' (config index {})",
                 entries[static_cast<size_t>(config_index)].id, config_index);

    // Disable the widget in config
    config_->set_enabled(static_cast<size_t>(config_index), false);

    // Deselect before rebuild (chrome will be destroyed)
    select_widget(nullptr);

    // Save config and trigger rebuild
    config_->save();
    if (rebuild_cb_) {
        rebuild_cb_();
    }
}

// ---------------------------------------------------------------------------
// screen_to_grid_cell
// ---------------------------------------------------------------------------

std::pair<int, int> GridEditMode::screen_to_grid_cell(int screen_x, int screen_y, int container_x,
                                                      int container_y, int container_w,
                                                      int container_h, int ncols, int nrows) {
    // Convert screen coordinates to container-relative
    int rel_x = screen_x - container_x;
    int rel_y = screen_y - container_y;

    // Compute cell indices
    int col = (rel_x * ncols) / container_w;
    int row = (rel_y * nrows) / container_h;

    // Clamp to valid range
    col = std::clamp(col, 0, ncols - 1);
    row = std::clamp(row, 0, nrows - 1);

    return {col, row};
}

// ---------------------------------------------------------------------------
// clamp_span
// ---------------------------------------------------------------------------

std::pair<int, int> GridEditMode::clamp_span(const std::string& widget_id, int desired_colspan,
                                             int desired_rowspan) {
    const auto* def = find_widget_def(widget_id);
    if (!def) {
        // Unknown widget — default to 1x1
        return {std::max(desired_colspan, 1), std::max(desired_rowspan, 1)};
    }

    int min_c = def->effective_min_colspan();
    int max_c = def->effective_max_colspan();
    int min_r = def->effective_min_rowspan();
    int max_r = def->effective_max_rowspan();

    int clamped_c = std::clamp(desired_colspan, min_c, max_c);
    int clamped_r = std::clamp(desired_rowspan, min_r, max_r);

    return {clamped_c, clamped_r};
}

// ---------------------------------------------------------------------------
// Resize helpers
// ---------------------------------------------------------------------------

bool GridEditMode::is_near_bottom_right_corner(int px, int py, const lv_area_t& widget_area) const {
    int br_x = widget_area.x2;
    int br_y = widget_area.y2;
    int dx = px - br_x;
    int dy = py - br_y;
    return (dx * dx + dy * dy) <= (CORNER_HIT_RADIUS * CORNER_HIT_RADIUS);
}

bool GridEditMode::is_selected_widget_resizable() const {
    if (!selected_ || !config_) {
        return false;
    }
    int cfg_idx = find_config_index_for_widget(selected_);
    if (cfg_idx < 0) {
        return false;
    }
    const auto& entry = config_->entries()[static_cast<size_t>(cfg_idx)];
    const auto* def = find_widget_def(entry.id);
    return def && def->is_scalable();
}

// ---------------------------------------------------------------------------
// Drag lifecycle — public entry points
// ---------------------------------------------------------------------------

void GridEditMode::handle_long_press(lv_event_t* e) {
    if (!active_ || !container_ || !selected_) {
        return;
    }
    handle_drag_start(e);
}

void GridEditMode::handle_pressing(lv_event_t* e) {
    if (!active_) {
        return;
    }
    if (resizing_) {
        handle_resize_move(e);
        return;
    }
    if (dragging_) {
        handle_drag_move(e);
    }
}

void GridEditMode::handle_released(lv_event_t* e) {
    if (!active_) {
        return;
    }
    if (resizing_) {
        handle_resize_end(e);
        return;
    }
    if (dragging_) {
        handle_drag_end(e);
    }
}

// ---------------------------------------------------------------------------
// Drag start
// ---------------------------------------------------------------------------

void GridEditMode::handle_drag_start(lv_event_t* /*e*/) {
    if (dragging_ || resizing_) {
        return;
    }

    // Verify pointer is on the selected widget
    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t sel_area;
    lv_obj_get_coords(selected_, &sel_area);
    if (point.x < sel_area.x1 || point.x > sel_area.x2 || point.y < sel_area.y1 ||
        point.y > sel_area.y2) {
        return; // Long-press not on selected widget
    }

    // Look up config entry for the selected widget
    int cfg_idx = find_config_index_for_widget(selected_);
    if (cfg_idx < 0) {
        spdlog::warn("[GridEditMode] Drag start: widget not in config");
        return;
    }

    const auto& entry = config_->entries()[static_cast<size_t>(cfg_idx)];
    drag_orig_col_ = entry.col;
    drag_orig_row_ = entry.row;
    drag_orig_colspan_ = entry.colspan;
    drag_orig_rowspan_ = entry.rowspan;

    // Check if pointer is near the bottom-right corner of a resizable widget
    if (is_near_bottom_right_corner(point.x, point.y, sel_area) && is_selected_widget_resizable()) {
        // Start resize mode instead of drag
        resizing_ = true;
        resize_preview_colspan_ = drag_orig_colspan_;
        resize_preview_rowspan_ = drag_orig_rowspan_;

        // Hide selection chrome during resize (will rebuild after)
        destroy_selection_chrome();

        // Show initial resize preview at current size
        update_snap_preview(drag_orig_col_, drag_orig_row_, drag_orig_colspan_, drag_orig_rowspan_,
                            true);

        spdlog::info("[GridEditMode] Resize started: widget '{}' at ({},{}) span {}x{}", entry.id,
                     drag_orig_col_, drag_orig_row_, drag_orig_colspan_, drag_orig_rowspan_);
        return;
    }

    // Record drag offset: distance from pointer to widget top-left
    drag_offset_.x = point.x - sel_area.x1;
    drag_offset_.y = point.y - sel_area.y1;

    // Hide selection chrome during drag (will rebuild after)
    destroy_selection_chrome();

    // Make widget float above the grid so it can be freely positioned
    lv_obj_add_flag(selected_, LV_OBJ_FLAG_FLOATING);

    // Elevation: shadow + slight visual lift
    lv_obj_set_style_shadow_width(selected_, DRAG_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_ofs_x(selected_, DRAG_SHADOW_OFS, 0);
    lv_obj_set_style_shadow_ofs_y(selected_, DRAG_SHADOW_OFS, 0);
    lv_obj_set_style_shadow_opa(selected_, DRAG_SHADOW_OPA, 0);
    lv_obj_set_style_shadow_color(selected_, lv_color_black(), 0);
    lv_obj_set_style_transform_scale(selected_, 260, 0); // ~1.02x (256 = 1.0x)

    // Create ghost outline at original position
    create_drag_ghost(drag_orig_col_, drag_orig_row_, drag_orig_colspan_, drag_orig_rowspan_);

    dragging_ = true;
    snap_preview_col_ = -1;
    snap_preview_row_ = -1;

    spdlog::info("[GridEditMode] Drag started: widget '{}' from ({},{}) span {}x{}", entry.id,
                 drag_orig_col_, drag_orig_row_, drag_orig_colspan_, drag_orig_rowspan_);
}

// ---------------------------------------------------------------------------
// Drag move
// ---------------------------------------------------------------------------

void GridEditMode::handle_drag_move(lv_event_t* /*e*/) {
    if (!selected_ || !container_) {
        return;
    }

    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // Move the widget to follow the pointer (adjusted by drag offset)
    lv_area_t container_area;
    lv_obj_get_coords(container_, &container_area);
    int new_x = point.x - drag_offset_.x - container_area.x1;
    int new_y = point.y - drag_offset_.y - container_area.y1;
    lv_obj_set_pos(selected_, new_x, new_y);

    // Compute target grid cell from pointer position
    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    auto [target_col, target_row] = screen_to_grid_cell(point.x, point.y, content_area.x1,
                                                        content_area.y1, cw, ch, ncols, nrows);

    // Only update preview if target cell changed
    if (target_col == snap_preview_col_ && target_row == snap_preview_row_) {
        return;
    }

    // Skip if hovering over the original position
    if (target_col == drag_orig_col_ && target_row == drag_orig_row_) {
        destroy_snap_preview();
        snap_preview_col_ = target_col;
        snap_preview_row_ = target_row;
        return;
    }

    // Check placement validity: build a temporary grid with all widgets except the dragged one
    GridLayout temp_grid(breakpoint);
    const auto& entries = config_->entries();
    std::string dragged_id;
    int cfg_idx = find_config_index_for_widget(selected_);
    if (cfg_idx >= 0) {
        dragged_id = entries[static_cast<size_t>(cfg_idx)].id;
    }

    // Occupant at target position (for potential swap)
    std::string occupant_id;
    int occupant_colspan = 0;
    int occupant_rowspan = 0;

    for (const auto& entry : entries) {
        if (!entry.enabled || !entry.has_grid_position()) {
            continue;
        }
        if (entry.id == dragged_id) {
            continue; // Skip the widget being dragged
        }
        temp_grid.place({entry.id, entry.col, entry.row, entry.colspan, entry.rowspan});

        // Check if this entry occupies the target cell
        if (target_col >= entry.col && target_col < entry.col + entry.colspan &&
            target_row >= entry.row && target_row < entry.row + entry.rowspan) {
            occupant_id = entry.id;
            occupant_colspan = entry.colspan;
            occupant_rowspan = entry.rowspan;
        }
    }

    bool valid = false;
    if (occupant_id.empty()) {
        // Target is empty — check if the dragged widget fits
        // Remove all placements temporarily to check just the target position bounds
        valid = (target_col + drag_orig_colspan_ <= ncols) &&
                (target_row + drag_orig_rowspan_ <= nrows) &&
                temp_grid.can_place(target_col, target_row, drag_orig_colspan_, drag_orig_rowspan_);
    } else {
        // Target occupied — allow swap only if same size
        valid = (occupant_colspan == drag_orig_colspan_ && occupant_rowspan == drag_orig_rowspan_);
    }

    update_snap_preview(target_col, target_row, drag_orig_colspan_, drag_orig_rowspan_, valid);
    snap_preview_col_ = target_col;
    snap_preview_row_ = target_row;
}

// ---------------------------------------------------------------------------
// Drag end
// ---------------------------------------------------------------------------

void GridEditMode::handle_drag_end(lv_event_t* /*e*/) {
    if (!selected_ || !container_ || !config_) {
        cleanup_drag_state();
        return;
    }

    lv_indev_t* indev = lv_indev_active();
    lv_point_t point = {0, 0};
    if (indev) {
        lv_indev_get_point(indev, &point);
    }

    // Compute final target cell
    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    auto [target_col, target_row] = screen_to_grid_cell(point.x, point.y, content_area.x1,
                                                        content_area.y1, cw, ch, ncols, nrows);

    bool did_move = false;

    // Don't allow drop on the same position
    if (target_col != drag_orig_col_ || target_row != drag_orig_row_) {
        int cfg_idx = find_config_index_for_widget(selected_);
        if (cfg_idx >= 0) {
            auto& entries = const_cast<std::vector<PanelWidgetEntry>&>(config_->entries());
            auto& dragged_entry = entries[static_cast<size_t>(cfg_idx)];

            // Check for occupant at target
            int occupant_cfg_idx = -1;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (!entries[i].enabled || !entries[i].has_grid_position()) {
                    continue;
                }
                if (static_cast<int>(i) == cfg_idx) {
                    continue;
                }
                if (target_col >= entries[i].col &&
                    target_col < entries[i].col + entries[i].colspan &&
                    target_row >= entries[i].row &&
                    target_row < entries[i].row + entries[i].rowspan) {
                    occupant_cfg_idx = static_cast<int>(i);
                    break;
                }
            }

            if (occupant_cfg_idx >= 0) {
                // Swap: only if same size
                auto& occupant = entries[static_cast<size_t>(occupant_cfg_idx)];
                if (occupant.colspan == drag_orig_colspan_ &&
                    occupant.rowspan == drag_orig_rowspan_) {
                    spdlog::info("[GridEditMode] Swapping '{}' ({},{}) <-> '{}' ({},{})",
                                 dragged_entry.id, drag_orig_col_, drag_orig_row_, occupant.id,
                                 occupant.col, occupant.row);
                    // Swap grid positions
                    occupant.col = drag_orig_col_;
                    occupant.row = drag_orig_row_;
                    dragged_entry.col = target_col;
                    dragged_entry.row = target_row;
                    did_move = true;
                }
            } else {
                // Empty cell — check bounds and collision
                GridLayout temp_grid(breakpoint);
                for (const auto& entry : entries) {
                    if (!entry.enabled || !entry.has_grid_position()) {
                        continue;
                    }
                    if (entry.id == dragged_entry.id) {
                        continue;
                    }
                    temp_grid.place({entry.id, entry.col, entry.row, entry.colspan, entry.rowspan});
                }
                if (temp_grid.can_place(target_col, target_row, drag_orig_colspan_,
                                        drag_orig_rowspan_)) {
                    spdlog::info("[GridEditMode] Moving '{}' from ({},{}) to ({},{})",
                                 dragged_entry.id, drag_orig_col_, drag_orig_row_, target_col,
                                 target_row);
                    dragged_entry.col = target_col;
                    dragged_entry.row = target_row;
                    did_move = true;
                }
            }
        }
    }

    if (!did_move) {
        spdlog::debug("[GridEditMode] Drag cancelled, snapping back to ({},{})", drag_orig_col_,
                      drag_orig_row_);
    }

    // Clean up visual state before rebuild
    lv_obj_t* was_selected = selected_;
    cleanup_drag_state();

    if (did_move) {
        // Deselect and rebuild
        selected_ = nullptr;
        config_->save();
        if (rebuild_cb_) {
            rebuild_cb_();
        }
    } else {
        // Re-select to show chrome again (widget snaps back via grid layout)
        selected_ = nullptr;
        // Force LVGL to recalculate positions
        lv_obj_invalidate(container_);
        // Reselect the widget to show chrome at snapped-back position
        lv_obj_update_layout(container_);
        select_widget(was_selected);
    }
}

// ---------------------------------------------------------------------------
// Resize move
// ---------------------------------------------------------------------------

void GridEditMode::handle_resize_move(lv_event_t* /*e*/) {
    if (!selected_ || !container_ || !config_) {
        return;
    }

    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // Compute which grid cell the pointer is over
    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    auto [target_col, target_row] = screen_to_grid_cell(point.x, point.y, content_area.x1,
                                                        content_area.y1, cw, ch, ncols, nrows);

    // Desired span: from original top-left to the cell the pointer is in (inclusive)
    int desired_colspan = target_col - drag_orig_col_ + 1;
    int desired_rowspan = target_row - drag_orig_row_ + 1;

    // Clamp to min 1 before registry clamping
    desired_colspan = std::max(desired_colspan, 1);
    desired_rowspan = std::max(desired_rowspan, 1);

    // Clamp via registry min/max
    int cfg_idx = find_config_index_for_widget(selected_);
    if (cfg_idx < 0) {
        return;
    }
    const auto& entry = config_->entries()[static_cast<size_t>(cfg_idx)];
    auto [clamped_c, clamped_r] = clamp_span(entry.id, desired_colspan, desired_rowspan);

    // Also clamp to grid bounds
    clamped_c = std::min(clamped_c, ncols - drag_orig_col_);
    clamped_r = std::min(clamped_r, nrows - drag_orig_row_);

    // Only update if changed
    if (clamped_c == resize_preview_colspan_ && clamped_r == resize_preview_rowspan_) {
        return;
    }

    // Check if the new size overlaps other widgets
    GridLayout temp_grid(breakpoint);
    const auto& entries = config_->entries();
    for (const auto& e : entries) {
        if (!e.enabled || !e.has_grid_position()) {
            continue;
        }
        if (e.id == entry.id) {
            continue; // Skip the widget being resized
        }
        temp_grid.place({e.id, e.col, e.row, e.colspan, e.rowspan});
    }

    bool valid = temp_grid.can_place(drag_orig_col_, drag_orig_row_, clamped_c, clamped_r);

    update_snap_preview(drag_orig_col_, drag_orig_row_, clamped_c, clamped_r, valid);
    resize_preview_colspan_ = clamped_c;
    resize_preview_rowspan_ = clamped_r;

    spdlog::debug("[GridEditMode] Resize preview: {}x{} valid={}", clamped_c, clamped_r, valid);
}

// ---------------------------------------------------------------------------
// Resize end
// ---------------------------------------------------------------------------

void GridEditMode::handle_resize_end(lv_event_t* /*e*/) {
    if (!selected_ || !container_ || !config_) {
        resizing_ = false;
        destroy_snap_preview();
        return;
    }

    bool did_resize = false;
    int cfg_idx = find_config_index_for_widget(selected_);

    if (cfg_idx >= 0 && resize_preview_colspan_ > 0 && resize_preview_rowspan_ > 0) {
        // Check if size actually changed
        auto& entries = const_cast<std::vector<PanelWidgetEntry>&>(config_->entries());
        auto& entry = entries[static_cast<size_t>(cfg_idx)];

        if (resize_preview_colspan_ != drag_orig_colspan_ ||
            resize_preview_rowspan_ != drag_orig_rowspan_) {
            // Validate the new size doesn't collide
            lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
            int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;

            GridLayout temp_grid(breakpoint);
            for (const auto& e : entries) {
                if (!e.enabled || !e.has_grid_position()) {
                    continue;
                }
                if (e.id == entry.id) {
                    continue;
                }
                temp_grid.place({e.id, e.col, e.row, e.colspan, e.rowspan});
            }

            if (temp_grid.can_place(drag_orig_col_, drag_orig_row_, resize_preview_colspan_,
                                    resize_preview_rowspan_)) {
                spdlog::info("[GridEditMode] Resized '{}' from {}x{} to {}x{}", entry.id,
                             drag_orig_colspan_, drag_orig_rowspan_, resize_preview_colspan_,
                             resize_preview_rowspan_);
                entry.colspan = resize_preview_colspan_;
                entry.rowspan = resize_preview_rowspan_;
                did_resize = true;
            }
        }
    }

    // Clean up resize state
    lv_obj_t* was_selected = selected_;
    destroy_snap_preview();
    resizing_ = false;
    resize_preview_colspan_ = -1;
    resize_preview_rowspan_ = -1;
    drag_orig_col_ = -1;
    drag_orig_row_ = -1;
    drag_orig_colspan_ = 1;
    drag_orig_rowspan_ = 1;

    if (did_resize) {
        selected_ = nullptr;
        config_->save();
        if (rebuild_cb_) {
            rebuild_cb_();
        }
    } else {
        // Reselect to restore chrome
        selected_ = nullptr;
        lv_obj_update_layout(container_);
        select_widget(was_selected);
    }
}

// ---------------------------------------------------------------------------
// Drag visual helpers
// ---------------------------------------------------------------------------

void GridEditMode::create_drag_ghost(int col, int row, int colspan, int rowspan) {
    if (!container_) {
        return;
    }

    // Compute cell pixel positions from container content area
    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    int cell_w = cw / ncols;
    int cell_h = ch / nrows;

    int ghost_x = col * cell_w;
    int ghost_y = row * cell_h;
    int ghost_w = colspan * cell_w;
    int ghost_h = rowspan * cell_h;

    drag_ghost_ = lv_obj_create(container_);
    lv_obj_set_pos(drag_ghost_, ghost_x, ghost_y);
    lv_obj_set_size(drag_ghost_, ghost_w, ghost_h);
    lv_obj_add_flag(drag_ghost_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(drag_ghost_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(drag_ghost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(drag_ghost_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(drag_ghost_, GHOST_BORDER_WIDTH, 0);
    lv_obj_set_style_border_color(drag_ghost_, theme_manager_get_color("text_secondary"), 0);
    lv_obj_set_style_border_opa(drag_ghost_, GHOST_BORDER_OPA, 0);
    lv_obj_set_style_radius(drag_ghost_, 8, 0);
    lv_obj_set_style_pad_all(drag_ghost_, 0, 0);

    spdlog::debug("[GridEditMode] Created drag ghost at ({},{}) {}x{}", col, row, colspan, rowspan);
}

void GridEditMode::destroy_drag_ghost() {
    if (drag_ghost_) {
        lv_obj_delete(drag_ghost_);
        drag_ghost_ = nullptr;
    }
}

void GridEditMode::update_snap_preview(int col, int row, int colspan, int rowspan, bool valid) {
    destroy_snap_preview();
    if (!container_) {
        return;
    }

    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    int cell_w = cw / ncols;
    int cell_h = ch / nrows;

    int px = col * cell_w;
    int py = row * cell_h;
    int pw = colspan * cell_w;
    int ph = rowspan * cell_h;

    snap_preview_ = lv_obj_create(container_);
    lv_obj_set_pos(snap_preview_, px, py);
    lv_obj_set_size(snap_preview_, pw, ph);
    lv_obj_add_flag(snap_preview_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(snap_preview_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(snap_preview_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(snap_preview_, PREVIEW_BORDER_WIDTH, 0);
    lv_obj_set_style_radius(snap_preview_, 8, 0);
    lv_obj_set_style_pad_all(snap_preview_, 0, 0);

    if (valid) {
        lv_obj_set_style_bg_color(snap_preview_, theme_get_accent_color(), 0);
        lv_obj_set_style_bg_opa(snap_preview_, LV_OPA_10, 0);
        lv_obj_set_style_border_color(snap_preview_, theme_get_accent_color(), 0);
        lv_obj_set_style_border_opa(snap_preview_, LV_OPA_70, 0);
    } else {
        lv_obj_set_style_bg_color(snap_preview_, ThemeManager::instance().get_color("danger"), 0);
        lv_obj_set_style_bg_opa(snap_preview_, LV_OPA_10, 0);
        lv_obj_set_style_border_color(snap_preview_, ThemeManager::instance().get_color("danger"),
                                      0);
        lv_obj_set_style_border_opa(snap_preview_, LV_OPA_50, 0);
    }
}

void GridEditMode::destroy_snap_preview() {
    if (snap_preview_) {
        lv_obj_delete(snap_preview_);
        snap_preview_ = nullptr;
    }
    snap_preview_col_ = -1;
    snap_preview_row_ = -1;
}

void GridEditMode::cleanup_drag_state() {
    if (!dragging_ && !resizing_) {
        return;
    }

    // Remove floating flag and drag styling from the widget (only for drag, not resize)
    if (dragging_ && selected_) {
        lv_obj_remove_flag(selected_, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_style_shadow_width(selected_, 0, 0);
        lv_obj_set_style_shadow_ofs_x(selected_, 0, 0);
        lv_obj_set_style_shadow_ofs_y(selected_, 0, 0);
        lv_obj_set_style_shadow_opa(selected_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_transform_scale(selected_, 256, 0); // Reset to 1.0x
    }

    destroy_drag_ghost();
    destroy_snap_preview();

    dragging_ = false;
    resizing_ = false;
    drag_orig_col_ = -1;
    drag_orig_row_ = -1;
    drag_orig_colspan_ = 1;
    drag_orig_rowspan_ = 1;
    drag_offset_ = {0, 0};
    resize_preview_colspan_ = -1;
    resize_preview_rowspan_ = -1;
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
