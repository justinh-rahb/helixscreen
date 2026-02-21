// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_callback_helpers.h
 * @brief Helpers to reduce boilerplate in panel/overlay callback registration and widget lookup
 *
 * @pattern Batch registration replaces repetitive lv_xml_register_event_cb() calls
 * @threading Main thread only
 */

#pragma once

#include "lvgl/lvgl.h"

#include <spdlog/spdlog.h>

#include <initializer_list>

/**
 * @brief Entry for batch XML event callback registration
 *
 * Pairs a callback name (matching XML event_cb attribute) with
 * its C++ function pointer.
 */
struct XmlCallbackEntry {
    const char* name;
    lv_event_cb_t callback;
};

/**
 * @brief Register multiple XML event callbacks in a single call
 *
 * Replaces repetitive blocks of lv_xml_register_event_cb() calls with
 * a compact table format. All callbacks are registered in the global
 * scope (nullptr component scope).
 *
 * @param callbacks Initializer list of {name, callback} pairs
 *
 * Example:
 * @code
 * register_xml_callbacks({
 *     {"on_home_all",  on_home_all},
 *     {"on_home_x",    on_home_x},
 *     {"on_home_y",    on_home_y},
 * });
 * @endcode
 */
inline void register_xml_callbacks(std::initializer_list<XmlCallbackEntry> callbacks) {
    for (const auto& cb : callbacks) {
        lv_xml_register_event_cb(nullptr, cb.name, cb.callback);
    }
}

/**
 * @brief Find a widget by name with error logging on failure
 *
 * Combines lv_obj_find_by_name() + error log into a single call.
 * Returns nullptr if the widget is not found.
 *
 * @param parent    Parent object to search within
 * @param name      Widget name to find (matches XML name= attribute)
 * @param panel_tag Log tag for error messages (e.g., "[BedMesh]")
 * @return Widget pointer, or nullptr if not found
 *
 * Example:
 * @code
 * lv_obj_t* btn = find_required_widget(overlay_root_, "save_btn", "[BedMesh]");
 * if (!btn) return;
 * @endcode
 */
inline lv_obj_t* find_required_widget(lv_obj_t* parent, const char* name, const char* panel_tag) {
    lv_obj_t* obj = lv_obj_find_by_name(parent, name);
    if (!obj) {
        spdlog::error("{} Required widget '{}' not found", panel_tag, name);
    }
    return obj;
}
