// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "ui_fonts.h"
#include "ui_widget_catalog_overlay.h"

#include "app_globals.h"
#include "display_settings_manager.h"
#include "grid_layout.h"
#include "panel_widget_config.h"
#include "panel_widget_registry.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <unordered_set>

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

/// Recursively remove CLICKABLE flag from all descendants of obj.

static void disable_widget_clicks_recursive(lv_obj_t* obj) {
    if (!obj) {
        return;
    }
    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_t* child = lv_obj_get_child(obj, static_cast<int32_t>(i));
        lv_obj_remove_flag(child, LV_OBJ_FLAG_CLICKABLE);
        disable_widget_clicks_recursive(child);
    }
}

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

    // Disable clickability on all widget descendants so their individual
    // click handlers and press animations don't fire during edit mode.
    disable_widget_clicks_recursive(container_);

    // Create dots overlay (event shield + visual grid dots)
    create_dots_overlay();

    // Sync config grid positions from actual screen positions.
    // populate_widgets() may have been called many times during startup
    // (hardware gates firing), and config positions may not match the
    // final visual layout. This ensures config reflects reality.
    sync_config_from_screen();

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

    // Clickability is restored by the rebuild callback (save_cb_) which
    // recreates all widget children fresh with their original flags.

    lv_subject_set_int(&get_home_edit_mode_subject(), 0);

    if (config_) {
        config_->save();
    }
    // Rebuild widgets to restore normal click behavior (the dots overlay
    // absorbed all events during edit mode; rebuild creates fresh widgets).
    if (rebuild_cb_) {
        rebuild_cb_();
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

    // Ensure layout is up-to-date (enter() may have just modified the container)
    lv_obj_update_layout(container_);

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
        lv_area_t hit_area;
        lv_obj_get_coords(hit, &hit_area);
        const char* wname = lv_obj_get_name(hit);
        spdlog::debug(
            "[GridEditMode] Hit widget '{}': screen=({},{})→({},{}) size={}x{} click=({},{}) "
            "state=0x{:x} clickable={} pressed={} floating={}",
            wname ? wname : "?", hit_area.x1, hit_area.y1, hit_area.x2, hit_area.y2,
            hit_area.x2 - hit_area.x1, hit_area.y2 - hit_area.y1, point.x, point.y,
            static_cast<uint32_t>(lv_obj_get_state(hit)),
            lv_obj_has_flag(hit, LV_OBJ_FLAG_CLICKABLE),
            (lv_obj_get_state(hit) & LV_STATE_PRESSED) != 0,
            lv_obj_has_flag(hit, LV_OBJ_FLAG_FLOATING));

        // Log widget position BEFORE select
        lv_area_t pre_coords;
        lv_obj_get_coords(hit, &pre_coords);

        select_widget(hit);

        // Log widget position AFTER select (chrome created)
        lv_area_t post_coords;
        lv_obj_get_coords(hit, &post_coords);
        if (pre_coords.x1 != post_coords.x1 || pre_coords.y1 != post_coords.y1) {
            spdlog::warn("[GridEditMode] WIDGET SHIFTED after select! "
                         "before=({},{}) after=({},{}) delta=({},{})",
                         pre_coords.x1, pre_coords.y1, post_coords.x1, post_coords.y1,
                         post_coords.x1 - pre_coords.x1, post_coords.y1 - pre_coords.y1);
        } else {
            spdlog::debug("[GridEditMode] Widget position stable after select: ({},{})",
                          post_coords.x1, post_coords.y1);
        }

        // Also check all grid children for any position changes
        for (uint32_t i = 0; i < child_count; ++i) {
            lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
            if (!child || lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING))
                continue;
            lv_area_t ccoords;
            lv_obj_get_coords(child, &ccoords);
            const char* cname = lv_obj_get_name(child);
            spdlog::debug("[GridEditMode]   child '{}': ({},{})→({},{}) state=0x{:x}",
                          cname ? cname : "?", ccoords.x1, ccoords.y1, ccoords.x2, ccoords.y2,
                          static_cast<uint32_t>(lv_obj_get_state(child)));
        }
    } else {
        spdlog::debug("[GridEditMode] handle_click: no widget at ({},{}) — {} children checked",
                      point.x, point.y, child_count);
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

    // Get container's outer coords (selection overlay is positioned relative to these,
    // since it uses LV_OBJ_FLAG_FLOATING which ignores content padding)
    lv_area_t container_area;
    lv_obj_get_coords(container_, &container_area);

    // LVGL adds parent padding even for FLOATING objects (see lv_obj_move_to),
    // so subtract it to get the correct screen position.
    int pad_left = lv_obj_get_style_space_left(container_, LV_PART_MAIN);
    int pad_top = lv_obj_get_style_space_top(container_, LV_PART_MAIN);
    int rel_x1 = widget_area.x1 - container_area.x1 - pad_left;
    int rel_y1 = widget_area.y1 - container_area.y1 - pad_top;
    int widget_w = widget_area.x2 - widget_area.x1;
    int widget_h = widget_area.y2 - widget_area.y1;

    spdlog::debug("[GridEditMode] Chrome coords: widget_screen=({},{})→({},{}) "
                  "container_screen=({},{}) pad=({},{}) rel=({},{}) size={}x{}",
                  widget_area.x1, widget_area.y1, widget_area.x2, widget_area.y2, container_area.x1,
                  container_area.y1, pad_left, pad_top, rel_x1, rel_y1, widget_w, widget_h);

    // Create floating overlay container for selection chrome.
    // The overlay itself draws the connecting border (rounded, muted).
    lv_color_t accent = theme_get_accent_color();
    int radius = theme_manager_get_spacing("border_radius");

    selection_overlay_ = lv_obj_create(container_);
    lv_obj_set_pos(selection_overlay_, rel_x1, rel_y1);
    lv_obj_set_size(selection_overlay_, widget_w, widget_h);
    lv_obj_add_flag(selection_overlay_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(selection_overlay_, LV_OBJ_FLAG_SCROLLABLE);
    // Let touch events pass through to the container for long-press drag detection.
    // The X button child is independently clickable.
    lv_obj_remove_flag(selection_overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(selection_overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(selection_overlay_, accent, 0);
    lv_obj_set_style_border_opa(selection_overlay_, LV_OPA_30, 0);
    lv_obj_set_style_border_width(selection_overlay_, 1, 0);
    lv_obj_set_style_radius(selection_overlay_, radius, 0);
    lv_obj_set_style_pad_all(selection_overlay_, 0, 0);

    // Corner bracket styling: two bars per corner forming a square L-bracket
    constexpr int BAR_LEN = 16;
    constexpr int BAR_THICK = 3;

    int bar_count = 0;
    auto make_bar = [&](int x, int y, int w, int h) {
        lv_obj_t* bar = lv_obj_create(selection_overlay_);
        lv_obj_set_pos(bar, x, y);
        lv_obj_set_size(bar, w, h);
        lv_obj_set_style_bg_color(bar, accent, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
        bar_count++;
    };

    // Top-left
    make_bar(0, 0, BAR_LEN, BAR_THICK);
    make_bar(0, 0, BAR_THICK, BAR_LEN);
    // Top-right
    make_bar(widget_w - BAR_LEN, 0, BAR_LEN, BAR_THICK);
    make_bar(widget_w - BAR_THICK, 0, BAR_THICK, BAR_LEN);
    // Bottom-left
    make_bar(0, widget_h - BAR_THICK, BAR_LEN, BAR_THICK);
    make_bar(0, widget_h - BAR_LEN, BAR_THICK, BAR_LEN);
    // Bottom-right
    make_bar(widget_w - BAR_LEN, widget_h - BAR_THICK, BAR_LEN, BAR_THICK);
    make_bar(widget_w - BAR_THICK, widget_h - BAR_LEN, BAR_THICK, BAR_LEN);

    (void)bar_count; // 8 bars created — pulse targets these, not the X button

    // Pulse animation on corner brackets when animations are enabled
    if (DisplaySettingsManager::instance().get_animations_enabled()) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, selection_overlay_);
        lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_20);
        lv_anim_set_duration(&anim, 1000);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_playback_duration(&anim, 1000);
        lv_anim_set_exec_cb(&anim, [](void* obj, int32_t val) {
            auto* overlay = static_cast<lv_obj_t*>(obj);
            // Pulse only the first 8 children (corner bars), not the X button
            uint32_t count = std::min(lv_obj_get_child_count(overlay), uint32_t(8));
            for (uint32_t i = 0; i < count; ++i) {
                lv_obj_t* child = lv_obj_get_child(overlay, static_cast<int32_t>(i));
                lv_obj_set_style_bg_opa(child, static_cast<lv_opa_t>(val), 0);
            }
        });
        lv_anim_start(&anim);
    }

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

    // Verify: where did the overlay actually end up on screen?
    lv_obj_update_layout(selection_overlay_);
    lv_area_t overlay_area;
    lv_obj_get_coords(selection_overlay_, &overlay_area);
    spdlog::debug("[GridEditMode] Chrome verify: overlay_screen=({},{})→({},{}) "
                  "widget_screen=({},{})→({},{}) delta=({},{})",
                  overlay_area.x1, overlay_area.y1, overlay_area.x2, overlay_area.y2,
                  widget_area.x1, widget_area.y1, widget_area.x2, widget_area.y2,
                  overlay_area.x1 - widget_area.x1, overlay_area.y1 - widget_area.y1);
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

    // Look up the widget's name (set by populate_widgets via lv_obj_set_name)
    const char* name = lv_obj_get_name(widget);
    if (!name || name[0] == '\0') {
        spdlog::debug("[GridEditMode] find_config: widget {} has no name",
                      static_cast<void*>(widget));
        return -1;
    }

    // Find the config entry with this widget ID
    const auto& entries = config_->entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id == name) {
            return static_cast<int>(i);
        }
    }

    spdlog::debug("[GridEditMode] find_config: no config entry for name '{}'", name);
    return -1;
}

void GridEditMode::sync_config_from_screen() {
    if (!container_ || !config_) {
        return;
    }

    lv_obj_update_layout(container_);

    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    if (cw <= 0 || ch <= 0) {
        return;
    }

    auto& mut_entries = config_->mutable_entries();
    bool any_changed = false;

    // Walk container children and match by name (widget_id) set during populate_widgets.
    uint32_t total_children = lv_obj_get_child_count(container_);
    for (uint32_t i = 0; i < total_children; ++i) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
        if (!child || child == dots_overlay_ || child == selection_overlay_) {
            continue;
        }
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }

        // Look up config entry by widget name
        const char* name = lv_obj_get_name(child);
        if (!name || name[0] == '\0') {
            continue;
        }

        auto it = std::find_if(mut_entries.begin(), mut_entries.end(),
                               [name](const PanelWidgetEntry& e) { return e.id == name; });
        if (it == mut_entries.end()) {
            continue;
        }

        // Use the widget's top-left corner (center of the widget's first cell)
        lv_area_t widget_area;
        lv_obj_get_coords(child, &widget_area);

        // Compute which cell the top-left corner falls in
        int cell_w = cw / ncols;
        int cell_h = ch / nrows;
        int cx = widget_area.x1 + cell_w / 2; // center of first cell
        int cy = widget_area.y1 + cell_h / 2;

        auto [col, row] =
            screen_to_grid_cell(cx, cy, content_area.x1, content_area.y1, cw, ch, ncols, nrows);

        if (it->col != col || it->row != row) {
            spdlog::debug("[GridEditMode] sync_config: '{}' config ({},{}) -> screen ({},{})",
                          it->id, it->col, it->row, col, row);
            it->col = col;
            it->row = row;
            any_changed = true;
        }
    }

    if (any_changed) {
        config_->save();
        spdlog::info("[GridEditMode] Synced config positions from screen layout");
    }
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

    // Deselect before rebuild. Null out overlay pointers since
    // lv_obj_clean in the rebuild will delete them.
    select_widget(nullptr);
    dots_overlay_ = nullptr;

    // Save config and trigger rebuild
    config_->save();
    if (rebuild_cb_) {
        rebuild_cb_();
    }
    // Recreate dots overlay (rebuild destroyed all container children)
    if (active_) {
        create_dots_overlay();
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
    if (!active_ || !container_) {
        return;
    }

    spdlog::debug("[GridEditMode] handle_long_press: selected_={}", (void*)selected_);

    // If no widget is selected, try to select the one under the finger
    if (!selected_) {
        handle_click(e); // Select widget under finger (if any)
    }

    // If still no widget selected, check if we're on empty area for catalog
    if (!selected_) {
        lv_indev_t* indev = lv_indev_active();
        if (!indev) {
            return;
        }
        lv_point_t point;
        lv_indev_get_point(indev, &point);

        // Long-press on empty area — open the widget catalog
        lv_area_t content_area;
        lv_obj_get_content_coords(container_, &content_area);
        int cw = content_area.x2 - content_area.x1;
        int ch = content_area.y2 - content_area.y1;

        lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
        int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
        int ncols = GridLayout::get_cols(breakpoint);
        int nrows = GridLayout::get_rows(breakpoint);

        auto [col, row] = screen_to_grid_cell(point.x, point.y, content_area.x1, content_area.y1,
                                              cw, ch, ncols, nrows);
        catalog_origin_col_ = col;
        catalog_origin_row_ = row;

        lv_obj_t* screen = lv_obj_get_screen(container_);
        if (screen) {
            open_widget_catalog(screen);
        }
        return;
    }

    drag_pending_ = false; // Long-press bypasses threshold
    handle_drag_start(e);
}

void GridEditMode::handle_pressing(lv_event_t* e) {
    (void)e;
    if (!active_) {
        return;
    }
    if (resizing_) {
        handle_resize_move(e);
        return;
    }
    if (dragging_) {
        handle_drag_move(e);
        return;
    }

    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    // First press frame: select widget and start watching for drag threshold
    if (!drag_pending_ && !selected_) {
        handle_click(e);
        if (selected_) {
            drag_pending_ = true;
            press_origin_ = pt;
        }
        return;
    }

    // If already selected but pressing on a different widget, re-select
    if (!drag_pending_ && selected_) {
        lv_area_t sel_area;
        lv_obj_get_coords(selected_, &sel_area);
        if (pt.x < sel_area.x1 || pt.x > sel_area.x2 || pt.y < sel_area.y1 || pt.y > sel_area.y2) {
            handle_click(e);
        }
        if (selected_) {
            drag_pending_ = true;
            press_origin_ = pt;
        }
        return;
    }

    // Drag threshold check: start real drag when finger moves enough
    if (drag_pending_ && selected_) {
        int dx = pt.x - press_origin_.x;
        int dy = pt.y - press_origin_.y;
        if (dx * dx + dy * dy > DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX) {
            drag_pending_ = false;
            handle_drag_start(e);
        }
    }
}

void GridEditMode::handle_released(lv_event_t* e) {
    if (!active_) {
        return;
    }
    // Clear drag pending (finger released before threshold = just a tap/select)
    drag_pending_ = false;

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
    spdlog::debug("[GridEditMode] handle_drag_start: dragging_={} resizing_={} selected_={}",
                  dragging_, resizing_, (void*)selected_);
    if (dragging_ || resizing_ || !selected_) {
        return;
    }

    // Verify pointer is on the selected widget
    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        spdlog::debug("[GridEditMode] handle_drag_start: no active indev");
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t sel_area;
    lv_obj_get_coords(selected_, &sel_area);
    // Allow some tolerance for finger drift during long-press
    constexpr int TOUCH_MARGIN = 15;
    spdlog::debug("[GridEditMode] handle_drag_start: point=({},{}) sel=({},{})→({},{}) margin={}",
                  point.x, point.y, sel_area.x1, sel_area.y1, sel_area.x2, sel_area.y2,
                  TOUCH_MARGIN);
    if (point.x < sel_area.x1 - TOUCH_MARGIN || point.x > sel_area.x2 + TOUCH_MARGIN ||
        point.y < sel_area.y1 - TOUCH_MARGIN || point.y > sel_area.y2 + TOUCH_MARGIN) {
        spdlog::debug("[GridEditMode] handle_drag_start: point not on selected widget");
        return; // Long-press not on selected widget
    }

    // Look up config entry for the selected widget
    int cfg_idx = find_config_index_for_widget(selected_);
    if (cfg_idx < 0) {
        spdlog::warn("[GridEditMode] Drag start: widget not in config");
        return;
    }

    const auto& entry = config_->entries()[static_cast<size_t>(cfg_idx)];
    spdlog::debug("[GridEditMode] handle_drag_start: cfg_idx={} id='{}' col={} row={} "
                  "colspan={} rowspan={} has_grid={}",
                  cfg_idx, entry.id, entry.col, entry.row, entry.colspan, entry.rowspan,
                  entry.has_grid_position());
    drag_cfg_idx_ = cfg_idx;
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

    // Keep selection chrome visible during drag — it moves with the widget
    // in handle_drag_move(). No ghost outline at the origin needed.

    // Make widget float above the grid so it can be freely positioned.
    // Compensate position: FLOATING changes coordinate reference frame
    // from content area to parent outer coords + padding. Without compensation,
    // the widget visually shifts by the container's padding amount.
    lv_area_t cont_area;
    lv_obj_get_coords(container_, &cont_area);
    int pad_left = lv_obj_get_style_space_left(container_, LV_PART_MAIN);
    int pad_top = lv_obj_get_style_space_top(container_, LV_PART_MAIN);

    lv_obj_add_flag(selected_, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(selected_, sel_area.x1 - cont_area.x1 - pad_left,
                   sel_area.y1 - cont_area.y1 - pad_top);

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

    // Move selection chrome overlay to track the widget.
    // Recompute from the widget's actual screen coords (same math as create_selection_chrome).
    if (selection_overlay_) {
        lv_obj_update_layout(selected_);
        lv_area_t widget_area;
        lv_obj_get_coords(selected_, &widget_area);
        int pad_left = lv_obj_get_style_space_left(container_, LV_PART_MAIN);
        int pad_top = lv_obj_get_style_space_top(container_, LV_PART_MAIN);
        int chrome_x = widget_area.x1 - container_area.x1 - pad_left;
        int chrome_y = widget_area.y1 - container_area.y1 - pad_top;
        lv_obj_set_pos(selection_overlay_, chrome_x, chrome_y);
    }

    // Compute target grid cell from the widget's center position.
    // Using raw touch point is wrong for multi-cell widgets (grab point varies).
    // Using top-left overcorrects. The center gives the most intuitive mapping.
    lv_area_t content_area;
    lv_obj_get_content_coords(container_, &content_area);
    int cw = content_area.x2 - content_area.x1;
    int ch = content_area.y2 - content_area.y1;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;
    int ncols = GridLayout::get_cols(breakpoint);
    int nrows = GridLayout::get_rows(breakpoint);

    // Widget top-left in screen coords, then add half the span to get center
    int widget_left = point.x - drag_offset_.x;
    int widget_top = point.y - drag_offset_.y;
    int half_w = (cw * drag_orig_colspan_) / (ncols * 2);
    int half_h = (ch * drag_orig_rowspan_) / (nrows * 2);
    int widget_cx = widget_left + half_w;
    int widget_cy = widget_top + half_h;
    auto [target_col, target_row] = screen_to_grid_cell(widget_cx, widget_cy, content_area.x1,
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

    // Check placement validity: build a temporary grid with only VISIBLE widgets
    // (hardware-gated widgets may be enabled in config but not placed on screen)
    GridLayout temp_grid(breakpoint);
    const auto& entries = config_->entries();
    std::string dragged_id;
    if (drag_cfg_idx_ >= 0 && static_cast<size_t>(drag_cfg_idx_) < entries.size()) {
        dragged_id = entries[static_cast<size_t>(drag_cfg_idx_)].id;
    }

    // Collect IDs of widgets actually visible on screen
    std::unordered_set<std::string> visible_ids;
    uint32_t nchildren = lv_obj_get_child_count(container_);
    for (uint32_t ci = 0; ci < nchildren; ++ci) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(ci));
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }
        const char* cname = lv_obj_get_name(child);
        if (cname && cname[0] != '\0') {
            visible_ids.insert(cname);
        }
    }

    // Occupant at target position (reject drop on occupied cells)
    std::string occupant_id;

    for (const auto& entry : entries) {
        if (!entry.enabled || !entry.has_grid_position()) {
            continue;
        }
        if (entry.id == dragged_id) {
            continue; // Skip the widget being dragged
        }
        if (visible_ids.find(entry.id) == visible_ids.end()) {
            continue; // Skip hardware-gated widgets not on screen
        }
        temp_grid.place({entry.id, entry.col, entry.row, entry.colspan, entry.rowspan});

        // Check if this entry occupies the target cell
        if (target_col >= entry.col && target_col < entry.col + entry.colspan &&
            target_row >= entry.row && target_row < entry.row + entry.rowspan) {
            occupant_id = entry.id;
        }
    }

    bool valid = false;
    if (occupant_id.empty()) {
        // Target is empty — check if the dragged widget fits (bounds + collision)
        valid = (target_col + drag_orig_colspan_ <= ncols) &&
                (target_row + drag_orig_rowspan_ <= nrows) &&
                temp_grid.can_place(target_col, target_row, drag_orig_colspan_, drag_orig_rowspan_);
    } else {
        // Target occupied — reject drop (no swapping for now)
        valid = false;
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

    // Use the last snap preview position — this is what the user saw highlighted.
    // Recomputing from the release point can differ (finger moves during release).
    int target_col = snap_preview_col_;
    int target_row = snap_preview_row_;

    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;

    bool did_move = false;

    spdlog::debug("[GridEditMode] Drag end: target=({},{}) orig=({},{})", target_col, target_row,
                  drag_orig_col_, drag_orig_row_);

    // Don't allow drop if no preview was shown or on the same position
    if (target_col >= 0 && target_row >= 0 &&
        (target_col != drag_orig_col_ || target_row != drag_orig_row_)) {
        int cfg_idx = drag_cfg_idx_;
        spdlog::debug("[GridEditMode] Drag end: cfg_idx={} selected_={}", cfg_idx,
                      (void*)selected_);
        if (cfg_idx >= 0) {
            auto& entries = config_->mutable_entries();
            auto& dragged_entry = entries[static_cast<size_t>(cfg_idx)];

            // Collect IDs of widgets actually visible on screen (not hardware-gated)
            std::unordered_set<std::string> visible_ids;
            uint32_t nchildren = lv_obj_get_child_count(container_);
            for (uint32_t ci = 0; ci < nchildren; ++ci) {
                lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(ci));
                if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
                    continue;
                }
                const char* cname = lv_obj_get_name(child);
                if (cname && cname[0] != '\0') {
                    visible_ids.insert(cname);
                }
            }

            // Check for occupant at target (only visible widgets)
            int occupant_cfg_idx = -1;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (!entries[i].enabled || !entries[i].has_grid_position()) {
                    continue;
                }
                if (static_cast<int>(i) == cfg_idx) {
                    continue;
                }
                if (visible_ids.find(entries[i].id) == visible_ids.end()) {
                    continue; // Skip hardware-gated widgets not on screen
                }
                if (target_col >= entries[i].col &&
                    target_col < entries[i].col + entries[i].colspan &&
                    target_row >= entries[i].row &&
                    target_row < entries[i].row + entries[i].rowspan) {
                    occupant_cfg_idx = static_cast<int>(i);
                    break;
                }
            }

            spdlog::debug("[GridEditMode] Drag end: occupant_cfg_idx={}", occupant_cfg_idx);

            if (occupant_cfg_idx >= 0) {
                // Target occupied — reject drop (swap logic disabled for now)
                // TODO: re-enable swap when UX is polished
                spdlog::debug("[GridEditMode] Drag end: target occupied by '{}', rejecting drop",
                              entries[static_cast<size_t>(occupant_cfg_idx)].id);
            } else {
                // Empty cell — check bounds and collision (only visible widgets)
                GridLayout temp_grid(breakpoint);

                for (const auto& entry : entries) {
                    if (!entry.enabled || !entry.has_grid_position()) {
                        continue;
                    }
                    if (entry.id == dragged_entry.id) {
                        continue;
                    }
                    if (visible_ids.find(entry.id) == visible_ids.end()) {
                        continue; // Skip hardware-gated widgets not on screen
                    }
                    temp_grid.place({entry.id, entry.col, entry.row, entry.colspan, entry.rowspan});
                }
                bool ok = temp_grid.can_place(target_col, target_row, drag_orig_colspan_,
                                              drag_orig_rowspan_);
                spdlog::debug("[GridEditMode] Drag end: can_place({},{} {}x{})={}", target_col,
                              target_row, drag_orig_colspan_, drag_orig_rowspan_, ok);
                if (ok) {
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
        // Deselect before rebuild. The rebuild (lv_obj_clean) will destroy
        // selection_overlay_ and dots_overlay_ since they're container children.
        // Null them out first to prevent dangling pointer access.
        selected_ = nullptr;
        selection_overlay_ = nullptr;
        dots_overlay_ = nullptr;
        config_->save();
        spdlog::debug("[GridEditMode] Config saved, rebuilding widgets...");
        if (rebuild_cb_) {
            rebuild_cb_();
        }
        spdlog::debug("[GridEditMode] Rebuild complete, recreating dots overlay");
        // Recreate dots overlay (rebuild destroyed all container children)
        if (active_) {
            create_dots_overlay();
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
        auto& entries = config_->mutable_entries();
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
        // Null out overlay pointers before rebuild (lv_obj_clean will delete them)
        selected_ = nullptr;
        selection_overlay_ = nullptr;
        dots_overlay_ = nullptr;
        config_->save();
        if (rebuild_cb_) {
            rebuild_cb_();
        }
        // Recreate dots overlay (rebuild destroyed all container children)
        if (active_) {
            create_dots_overlay();
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
    drag_pending_ = false;
    if (!dragging_ && !resizing_) {
        return;
    }

    // Remove floating flag from the widget (only for drag, not resize)
    if (dragging_ && selected_) {
        lv_obj_remove_flag(selected_, LV_OBJ_FLAG_FLOATING);
    }

    destroy_drag_ghost();
    destroy_snap_preview();

    dragging_ = false;
    resizing_ = false;
    drag_cfg_idx_ = -1;
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

    // Create transparent overlay that floats above grid children.
    // This overlay serves two purposes:
    // 1. Draws grid intersection dots for visual feedback
    // 2. Acts as an event shield — absorbs ALL touches so widgets underneath
    //    never receive click/press events during edit mode. Events bubble up
    //    to the container where our drag/click handlers process them.
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
    // Use contrast text color so dots are visible on both light and dark backgrounds
    lv_color_t screen_bg = ThemeManager::instance().current_palette().screen_bg;
    lv_color_t dot_color = theme_manager_get_contrast_color(screen_bg);

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

// ---------------------------------------------------------------------------
// Hit-test: check if screen coordinates land on any grid widget
// ---------------------------------------------------------------------------

bool GridEditMode::hit_test_any_widget(int screen_x, int screen_y) const {
    if (!container_) {
        return false;
    }
    uint32_t child_count = lv_obj_get_child_count(container_);
    for (uint32_t i = 0; i < child_count; ++i) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(i));
        if (!child) {
            continue;
        }
        if (child == dots_overlay_ || child == selection_overlay_) {
            continue;
        }
        if (child == drag_ghost_ || child == snap_preview_) {
            continue;
        }
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }
        lv_area_t coords;
        lv_obj_get_coords(child, &coords);
        if (screen_x >= coords.x1 && screen_x <= coords.x2 && screen_y >= coords.y1 &&
            screen_y <= coords.y2) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Widget catalog integration
// ---------------------------------------------------------------------------

void GridEditMode::open_widget_catalog(lv_obj_t* screen) {
    if (!config_) {
        spdlog::warn("[GridEditMode] Cannot open catalog: no config");
        return;
    }

    spdlog::info("[GridEditMode] Opening widget catalog (origin cell: {}, {})", catalog_origin_col_,
                 catalog_origin_row_);

    WidgetCatalogOverlay::show(screen, *config_, [this](const std::string& widget_id) {
        place_widget_from_catalog(widget_id);
    });
}

void GridEditMode::place_widget_from_catalog(const std::string& widget_id) {
    if (!config_ || !container_) {
        spdlog::warn("[GridEditMode] place_widget_from_catalog: no config or container");
        return;
    }

    const auto* def = find_widget_def(widget_id);
    if (!def) {
        spdlog::warn("[GridEditMode] Unknown widget ID: {}", widget_id);
        return;
    }

    int colspan = def->colspan;
    int rowspan = def->rowspan;

    // Determine current breakpoint and build a temporary grid
    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2;

    GridLayout temp_grid(breakpoint);
    const auto& entries = config_->entries();

    // Only include widgets actually visible on screen (not hardware-gated invisible ones)
    std::unordered_set<std::string> visible_ids;
    uint32_t nchildren = lv_obj_get_child_count(container_);
    for (uint32_t ci = 0; ci < nchildren; ++ci) {
        lv_obj_t* child = lv_obj_get_child(container_, static_cast<int32_t>(ci));
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_FLOATING)) {
            continue;
        }
        const char* cname = lv_obj_get_name(child);
        if (cname && cname[0] != '\0') {
            visible_ids.insert(cname);
        }
    }

    for (const auto& entry : entries) {
        if (!entry.enabled || !entry.has_grid_position()) {
            continue;
        }
        if (visible_ids.find(entry.id) == visible_ids.end()) {
            continue;
        }
        temp_grid.place({entry.id, entry.col, entry.row, entry.colspan, entry.rowspan});
    }

    int place_col = -1;
    int place_row = -1;

    // Try the catalog origin cell first
    if (catalog_origin_col_ >= 0 && catalog_origin_row_ >= 0 &&
        temp_grid.can_place(catalog_origin_col_, catalog_origin_row_, colspan, rowspan)) {
        place_col = catalog_origin_col_;
        place_row = catalog_origin_row_;
    } else {
        // Fall back to first available position
        auto pos = temp_grid.find_available(colspan, rowspan);
        if (pos) {
            place_col = pos->first;
            place_row = pos->second;
        }
    }

    if (place_col < 0 || place_row < 0) {
        spdlog::warn("[GridEditMode] No available grid position for widget '{}' ({}x{})", widget_id,
                     colspan, rowspan);
        return;
    }

    // Enable the widget in config with the computed grid position.
    // Find the entry by ID — it should exist as disabled.
    auto& mutable_entries = config_->mutable_entries();
    bool found = false;
    for (auto& entry : mutable_entries) {
        if (entry.id == widget_id) {
            entry.enabled = true;
            entry.col = place_col;
            entry.row = place_row;
            entry.colspan = colspan;
            entry.rowspan = rowspan;
            found = true;
            break;
        }
    }

    if (!found) {
        spdlog::warn("[GridEditMode] Widget '{}' not found in config entries", widget_id);
        return;
    }

    spdlog::info("[GridEditMode] Placed widget '{}' at ({},{}) {}x{}", widget_id, place_col,
                 place_row, colspan, rowspan);

    // Reset catalog origin
    catalog_origin_col_ = -1;
    catalog_origin_row_ = -1;

    // Deselect, save, and rebuild
    select_widget(nullptr);
    config_->save();
    if (rebuild_cb_) {
        rebuild_cb_();
    }
    // Recreate dots overlay (rebuild destroys all container children)
    if (active_) {
        create_dots_overlay();
    }
}

} // namespace helix
