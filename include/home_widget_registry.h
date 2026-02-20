// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace helix {

struct HomeWidgetDef {
    const char* id;                    // Stable string for JSON config
    const char* display_name;          // For settings overlay UI
    const char* icon;                  // Icon name
    const char* description;           // Short description for settings overlay
    const char* translation_tag;       // For i18n
    const char* hardware_gate_subject; // nullptr = always available
};

const std::vector<HomeWidgetDef>& get_all_widget_defs();
const HomeWidgetDef* find_widget_def(std::string_view id);
size_t widget_def_count();

} // namespace helix
