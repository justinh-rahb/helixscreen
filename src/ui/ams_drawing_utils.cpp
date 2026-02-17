// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/ams_drawing_utils.h"

#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace ams_draw {

// ============================================================================
// Color Utilities
// ============================================================================

lv_color_t lighten_color(lv_color_t c, uint8_t amount) {
    return lv_color_make(std::min(255, c.red + amount), std::min(255, c.green + amount),
                         std::min(255, c.blue + amount));
}

lv_color_t darken_color(lv_color_t c, uint8_t amount) {
    return lv_color_make(c.red > amount ? c.red - amount : 0,
                         c.green > amount ? c.green - amount : 0,
                         c.blue > amount ? c.blue - amount : 0);
}

lv_color_t blend_color(lv_color_t c1, lv_color_t c2, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    return lv_color_make(static_cast<uint8_t>(c1.red + (c2.red - c1.red) * factor),
                         static_cast<uint8_t>(c1.green + (c2.green - c1.green) * factor),
                         static_cast<uint8_t>(c1.blue + (c2.blue - c1.blue) * factor));
}

// ============================================================================
// Severity & Error Helpers
// ============================================================================

lv_color_t severity_color(SlotError::Severity severity) {
    switch (severity) {
    case SlotError::ERROR:
        return theme_manager_get_color("danger");
    case SlotError::WARNING:
        return theme_manager_get_color("warning");
    default:
        return theme_manager_get_color("text_muted");
    }
}

SlotError::Severity worst_unit_severity(const AmsUnit& unit) {
    SlotError::Severity worst = SlotError::INFO;
    for (const auto& slot : unit.slots) {
        if (slot.error.has_value() && slot.error->severity > worst) {
            worst = slot.error->severity;
        }
    }
    return worst;
}

// ============================================================================
// Data Helpers
// ============================================================================

int fill_percent_from_slot(const SlotInfo& slot, int min_pct) {
    float pct = slot.get_remaining_percent();
    if (pct < 0) {
        return 100;
    }
    return std::clamp(static_cast<int>(pct), min_pct, 100);
}

int32_t calc_bar_width(int32_t container_width, int slot_count, int32_t gap, int32_t min_width,
                       int32_t max_width, int container_pct) {
    int32_t usable = (container_width * container_pct) / 100;
    int count = std::max(1, slot_count);
    int32_t total_gaps = (count > 1) ? (count - 1) * gap : 0;
    int32_t width = (usable - total_gaps) / count;
    return std::clamp(width, min_width, max_width);
}

// ============================================================================
// Presentation Helpers
// ============================================================================

std::string get_unit_display_name(const AmsUnit& unit, int unit_index) {
    if (!unit.name.empty()) {
        return unit.name;
    }
    return "Unit " + std::to_string(unit_index + 1);
}

} // namespace ams_draw
