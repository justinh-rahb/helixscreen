// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_widget.h"

#include "ui_event_safety.h"
#include "ui_icon.h"
#include "ui_nav_manager.h"
#include "ui_panel_home.h"
#include "ui_panel_power.h"
#include "ui_update_queue.h"

#include "moonraker_api.h"

#include <spdlog/spdlog.h>

#include <set>

extern HomePanel& get_global_home_panel();

namespace helix {

PowerWidget::PowerWidget(MoonrakerAPI* api) : api_(api) {}

PowerWidget::~PowerWidget() {
    detach();
}

void PowerWidget::attach(lv_obj_t* widget_obj, lv_obj_t* parent_screen) {
    widget_obj_ = widget_obj;
    parent_screen_ = parent_screen;

    if (widget_obj_) {
        lv_obj_set_user_data(widget_obj_, this);
    }

    power_icon_ = lv_obj_find_by_name(widget_obj_, "power_icon");
    if (!power_icon_) {
        spdlog::warn("[PowerWidget] Could not find 'power_icon' in widget XML");
    }

    // Register XML event callbacks (transition: still delegate to HomePanel global)
    lv_xml_register_event_cb(nullptr, "power_toggle_cb", power_toggle_cb);
    lv_xml_register_event_cb(nullptr, "power_long_press_cb", power_long_press_cb);

    refresh_power_state();
}

void PowerWidget::detach() {
    if (widget_obj_) {
        lv_obj_set_user_data(widget_obj_, nullptr);
    }
    widget_obj_ = nullptr;
    parent_screen_ = nullptr;
    power_icon_ = nullptr;
}

void PowerWidget::handle_power_toggle() {
    // Suppress click that follows a long-press gesture
    if (power_long_pressed_) {
        power_long_pressed_ = false;
        spdlog::debug("[PowerWidget] Power click suppressed (follows long-press)");
        return;
    }

    spdlog::info("[PowerWidget] Power button clicked");

    if (!api_) {
        spdlog::warn("[PowerWidget] Power toggle: no API available");
        return;
    }

    // Get selected devices from power panel config
    auto& power_panel = get_global_power_panel();
    const auto& selected = power_panel.get_selected_devices();
    if (selected.empty()) {
        spdlog::warn("[PowerWidget] Power toggle: no devices selected");
        return;
    }

    // Determine action: if currently on -> turn off, else turn on
    const char* action = power_on_ ? "off" : "on";
    bool new_state = !power_on_;

    for (const auto& device : selected) {
        api_->set_device_power(
            device, action,
            [this, device]() {
                spdlog::debug("[PowerWidget] Power device '{}' set successfully", device);
            },
            [this, device](const MoonrakerError& err) {
                spdlog::error("[PowerWidget] Failed to set power device '{}': {}", device,
                              err.message);
                // On error, refresh from actual state
                refresh_power_state();
            });
    }

    // Optimistically update icon state
    power_on_ = new_state;
    update_power_icon(power_on_);
}

void PowerWidget::handle_power_long_press() {
    spdlog::info("[PowerWidget] Power long-press: opening power panel overlay");

    auto& panel = get_global_power_panel();
    lv_obj_t* overlay = panel.get_or_create_overlay(parent_screen_);
    if (overlay) {
        power_long_pressed_ = true; // Suppress the click that follows long-press
        NavigationManager::instance().push_overlay(overlay);
    }
}

void PowerWidget::update_power_icon(bool is_on) {
    if (!power_icon_)
        return;

    ui_icon_set_variant(power_icon_, is_on ? "danger" : "muted");
}

void PowerWidget::refresh_power_state() {
    if (!api_)
        return;

    // Capture selected devices on UI thread before async API call
    auto& power_panel = get_global_power_panel();
    const auto& selected = power_panel.get_selected_devices();
    if (selected.empty())
        return;
    std::set<std::string> selected_set(selected.begin(), selected.end());

    // Query power devices to determine if selected ones are on
    api_->get_power_devices(
        [this, selected_set](const std::vector<PowerDevice>& devices) {
            // Check if any selected device is on
            bool any_on = false;
            for (const auto& dev : devices) {
                if (selected_set.count(dev.device) > 0 && dev.status == "on") {
                    any_on = true;
                    break;
                }
            }

            helix::ui::queue_update([this, any_on]() {
                power_on_ = any_on;
                update_power_icon(power_on_);
                spdlog::debug("[PowerWidget] Power state refreshed: {}", power_on_ ? "on" : "off");
            });
        },
        [](const MoonrakerError& err) {
            spdlog::warn("[PowerWidget] Failed to refresh power state: {}", err.message);
        });
}

// Static callbacks — transition pattern: delegate to global HomePanel
// These are registered as XML event callbacks and route through the global instance
void PowerWidget::power_toggle_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PowerWidget] power_toggle_cb");
    (void)e;
    get_global_home_panel().handle_power_toggle();
    LVGL_SAFE_EVENT_CB_END();
}

void PowerWidget::power_long_press_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PowerWidget] power_long_press_cb");
    (void)e;
    get_global_home_panel().handle_power_long_press();
    LVGL_SAFE_EVENT_CB_END();
}

} // namespace helix
