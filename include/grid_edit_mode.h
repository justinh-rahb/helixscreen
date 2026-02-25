// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

#include <functional>

namespace helix {

class PanelWidgetConfig;

/// Manages in-panel grid editing for the home dashboard.
/// Pure state machine: enter/exit transitions, no visuals (yet).
class GridEditMode {
  public:
    using SaveCallback = std::function<void()>;

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

  private:
    bool active_ = false;
    lv_obj_t* container_ = nullptr;
    PanelWidgetConfig* config_ = nullptr;
    SaveCallback save_cb_;
};

} // namespace helix
