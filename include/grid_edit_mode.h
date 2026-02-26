// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace helix {

class PanelWidgetConfig;

/// Manages in-panel grid editing for the home dashboard.
/// Handles enter/exit transitions, grid intersection dot overlay,
/// widget selection with corner brackets, and (X) removal.
class GridEditMode {
  public:
    using SaveCallback = std::function<void()>;
    using RebuildCallback = std::function<void()>;

    GridEditMode() = default;
    ~GridEditMode();

    GridEditMode(const GridEditMode&) = delete;
    GridEditMode& operator=(const GridEditMode&) = delete;
    GridEditMode(GridEditMode&&) = delete;
    GridEditMode& operator=(GridEditMode&&) = delete;

    void enter(lv_obj_t* container, PanelWidgetConfig* config);
    void exit();

    bool is_active() const {
        return active_;
    }

    void set_save_callback(SaveCallback cb) {
        save_cb_ = std::move(cb);
    }

    void set_rebuild_callback(RebuildCallback cb) {
        rebuild_cb_ = std::move(cb);
    }

    /// Currently selected widget (nullptr if none)
    lv_obj_t* selected_widget() const {
        return selected_;
    }

    /// Select a widget (shows corner brackets + X button), or nullptr to deselect
    void select_widget(lv_obj_t* widget);

    /// Handle a click event on the container — hit-tests children for selection
    void handle_click(lv_event_t* e);

    /// Handle drag lifecycle events (called from container event callbacks)
    void handle_long_press(lv_event_t* e);
    void handle_pressing(lv_event_t* e);
    void handle_released(lv_event_t* e);

    /// Open the widget catalog overlay for adding a new widget.
    /// @param screen  The parent screen to host the overlay
    void open_widget_catalog(lv_obj_t* screen);

    /// Map screen coordinates to grid cell (col, row). Clamps to valid range.
    static std::pair<int, int> screen_to_grid_cell(int screen_x, int screen_y, int container_x,
                                                   int container_y, int container_w,
                                                   int container_h, int ncols, int nrows);

    /// Clamp desired colspan/rowspan to the min/max allowed by the widget registry.
    /// Returns {clamped_colspan, clamped_rowspan}.
    static std::pair<int, int> clamp_span(const std::string& widget_id, int desired_colspan,
                                          int desired_rowspan);

  private:
    void create_dots_overlay();
    void destroy_dots_overlay();
    void create_selection_chrome(lv_obj_t* widget);
    void destroy_selection_chrome();
    void remove_selected_widget();

    /// Find the config entry index for a given container child widget.
    /// Returns -1 if not found.
    int find_config_index_for_widget(lv_obj_t* widget) const;

    // Drag helpers
    void handle_drag_start(lv_event_t* e);
    void handle_drag_move(lv_event_t* e);
    void handle_drag_end(lv_event_t* e);
    void create_drag_ghost(int col, int row, int colspan, int rowspan);
    void destroy_drag_ghost();
    void update_snap_preview(int col, int row, int colspan, int rowspan, bool valid);
    void destroy_snap_preview();
    void cleanup_drag_state();

    // Resize helpers
    bool is_near_bottom_right_corner(int px, int py, const lv_area_t& widget_area) const;
    bool is_selected_widget_resizable() const;
    void handle_resize_move(lv_event_t* e);
    void handle_resize_end(lv_event_t* e);

    // Widget catalog placement
    void place_widget_from_catalog(const std::string& widget_id);
    bool hit_test_any_widget(int screen_x, int screen_y) const;

    bool active_ = false;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* dots_overlay_ = nullptr;
    lv_obj_t* selected_ = nullptr;
    lv_obj_t* selection_overlay_ = nullptr;
    PanelWidgetConfig* config_ = nullptr;
    SaveCallback save_cb_;
    RebuildCallback rebuild_cb_;

    // Drag state
    bool dragging_ = false;
    int drag_orig_col_ = -1;
    int drag_orig_row_ = -1;
    int drag_orig_colspan_ = 1;
    int drag_orig_rowspan_ = 1;
    lv_point_t drag_offset_ = {0, 0};
    lv_obj_t* drag_ghost_ = nullptr;
    lv_obj_t* snap_preview_ = nullptr;
    int snap_preview_col_ = -1;
    int snap_preview_row_ = -1;

    // Resize state
    bool resizing_ = false;
    int resize_preview_colspan_ = -1;
    int resize_preview_rowspan_ = -1;

    // Widget catalog placement: grid cell where the long-press originated
    int catalog_origin_col_ = -1;
    int catalog_origin_row_ = -1;
};

} // namespace helix
