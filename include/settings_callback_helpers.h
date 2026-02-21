// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

namespace helix::settings {

/**
 * @brief Factory for creating dropdown changed callbacks
 *
 * Reduces repetitive boilerplate in settings panels where each dropdown
 * callback follows the same pattern: get selected index, log it, call setter.
 *
 * Usage:
 * @code
 * // Instead of writing a full static callback function:
 * static void on_my_dropdown_changed(lv_event_t* e) {
 *     auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
 *     int index = static_cast<int>(lv_dropdown_get_selected(dropdown));
 *     spdlog::info("[SettingsPanel] My setting changed: {}", index);
 *     SomeManager::instance().set_my_setting(index);
 * }
 *
 * // You can use the factory:
 * auto on_my_dropdown_changed = helix::settings::make_dropdown_callback(
 *     "My setting",
 *     [](int index) { SomeManager::instance().set_my_setting(index); });
 * @endcode
 *
 * @param log_name Name to use in log messages (e.g., "Volume", "Timeout")
 * @param setter Callable taking int index, applies the new setting value
 * @return lv_event_cb_t compatible function pointer (stored in static lambda)
 */
template <typename Setter> auto make_dropdown_callback(const char* log_name, Setter setter) {
    // Return a lambda that captures by value (setter is typically small/stateless)
    return [log_name, setter](lv_event_t* e) {
        auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        int index = static_cast<int>(lv_dropdown_get_selected(dropdown));
        spdlog::info("[Settings] {} changed: {}", log_name, index);
        setter(index);
    };
}

/**
 * @brief Factory for creating toggle (switch) changed callbacks
 *
 * @param log_name Name to use in log messages
 * @param setter Callable taking bool enabled
 * @return Lambda compatible with lv_event_cb_t pattern
 */
template <typename Setter> auto make_toggle_callback(const char* log_name, Setter setter) {
    return [log_name, setter](lv_event_t* e) {
        auto* toggle = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        bool enabled = lv_obj_has_state(toggle, LV_STATE_CHECKED);
        spdlog::info("[Settings] {} changed: {}", log_name, enabled ? "ON" : "OFF");
        setter(enabled);
    };
}

} // namespace helix::settings
