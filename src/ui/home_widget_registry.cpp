// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "home_widget_registry.h"

#include <algorithm>
#include <string_view>

namespace helix {

// Vector order defines the default display order on the home panel.
// clang-format off
static const std::vector<HomeWidgetDef> s_widget_defs = {
    {"power",            "Power",            "power_cycle",      "Moonraker power device controls",              "Power",            "power_device_count"},
    {"network",          "Network",          "wifi_strength_4",  "Wi-Fi and ethernet connection status",         "Network",          nullptr},
    {"firmware_restart", "Firmware Restart",  "refresh",          "Restart Klipper firmware",                     "Firmware Restart", nullptr},
    {"ams",              "AMS Status",        "filament",         "Multi-material spool status and control",      "AMS Status",       "ams_slot_count"},
    {"temperature",      "Temperature",       "thermometer",      "Monitor and set nozzle and bed temps",         "Temperature",      nullptr},
    {"led",              "LED Light",         "lightbulb_outline","Quick toggle, long press for full control",    "LED Light",        "printer_has_led"},
    {"humidity",         "Humidity",          "water",            "Enclosure humidity sensor readings",           "Humidity",         "humidity_sensor_count"},
    {"width_sensor",     "Width Sensor",      "ruler",            "Filament width sensor readings",               "Width Sensor",     "width_sensor_count"},
    {"probe",            "Probe",             "target",           "Z probe status and offset",                    "Probe",            "probe_count"},
    {"filament",         "Filament Sensor",   "filament_alert",   "Filament runout detection status",             "Filament Sensor",  "show_filament_status"},
    {"notifications",    "Notifications",     "notifications",    "Pending alerts and system messages",           "Notifications",    nullptr},
};
// clang-format on

const std::vector<HomeWidgetDef>& get_all_widget_defs() {
    return s_widget_defs;
}

const HomeWidgetDef* find_widget_def(std::string_view id) {
    auto it = std::find_if(s_widget_defs.begin(), s_widget_defs.end(),
                           [&id](const HomeWidgetDef& def) { return id == def.id; });
    return it != s_widget_defs.end() ? &*it : nullptr;
}

size_t widget_def_count() {
    return s_widget_defs.size();
}

} // namespace helix
