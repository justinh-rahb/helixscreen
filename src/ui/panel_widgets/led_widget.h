// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_observer_guard.h"

#include "panel_widget.h"

class MoonrakerAPI;

namespace helix {
class PrinterState;
}

namespace helix {

class LedWidget : public PanelWidget {
  public:
    LedWidget(PrinterState& printer_state, MoonrakerAPI* api);
    ~LedWidget() override;

    void attach(lv_obj_t* widget_obj, lv_obj_t* parent_screen) override;
    void detach() override;
    const char* id() const override {
        return "led";
    }

  private:
    PrinterState& printer_state_;
    MoonrakerAPI* api_;

    lv_obj_t* widget_obj_ = nullptr;
    lv_obj_t* parent_screen_ = nullptr;
    lv_obj_t* light_icon_ = nullptr;
    lv_obj_t* led_control_panel_ = nullptr;

    bool light_on_ = false;
    bool light_long_pressed_ = false;

    ObserverGuard led_state_observer_;
    ObserverGuard led_brightness_observer_;

    void handle_light_toggle();
    void handle_light_long_press();
    void update_light_icon();
    void flash_light_icon();
    void ensure_led_observers();
    void on_led_state_changed(int state);

    static void light_toggle_cb(lv_event_t* e);
    static void light_long_press_cb(lv_event_t* e);
};

} // namespace helix
