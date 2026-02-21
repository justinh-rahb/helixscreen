// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "home_widget.h"

class MoonrakerAPI;

namespace helix {

class PowerWidget : public HomeWidget {
  public:
    explicit PowerWidget(MoonrakerAPI* api);
    ~PowerWidget() override;

    void attach(lv_obj_t* widget_obj, lv_obj_t* parent_screen) override;
    void detach() override;
    const char* id() const override {
        return "power";
    }

    /// Refresh power button state from actual device status (called on panel activate)
    void refresh_power_state();

  private:
    MoonrakerAPI* api_;

    lv_obj_t* widget_obj_ = nullptr;
    lv_obj_t* parent_screen_ = nullptr;
    lv_obj_t* power_icon_ = nullptr;

    bool power_on_ = false;
    bool power_long_pressed_ = false;

    void handle_power_toggle();
    void handle_power_long_press();
    void update_power_icon(bool is_on);

    static void power_toggle_cb(lv_event_t* e);
    static void power_long_press_cb(lv_event_t* e);
};

} // namespace helix
