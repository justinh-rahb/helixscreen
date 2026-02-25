// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

#include <functional>
#include <string>

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

  private:
    void create_dots_overlay();
    void destroy_dots_overlay();
    void create_selection_chrome(lv_obj_t* widget);
    void destroy_selection_chrome();
    void remove_selected_widget();

    bool active_ = false;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* dots_overlay_ = nullptr;
    lv_obj_t* selected_ = nullptr;
    lv_obj_t* selection_overlay_ = nullptr;
    PanelWidgetConfig* config_ = nullptr;
    SaveCallback save_cb_;
    RebuildCallback rebuild_cb_;
};

} // namespace helix
