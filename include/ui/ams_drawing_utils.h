// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ams_types.h"
#include "lvgl/lvgl.h"

#include <string>

/**
 * @brief Shared AMS drawing utilities
 *
 * Consolidates duplicated drawing code used by ui_ams_mini_status,
 * ui_panel_ams_overview, ui_ams_slot, and ui_spool_canvas.
 */
namespace ams_draw {

// ============================================================================
// Color Utilities
// ============================================================================

/** Lighten a color by adding amount to each channel (clamped to 255) */
lv_color_t lighten_color(lv_color_t c, uint8_t amount);

/** Darken a color by subtracting amount from each channel (clamped to 0) */
lv_color_t darken_color(lv_color_t c, uint8_t amount);

/** Blend two colors: factor=0 -> c1, factor=1 -> c2 (clamped to [0,1]) */
lv_color_t blend_color(lv_color_t c1, lv_color_t c2, float factor);

// ============================================================================
// Severity & Error Helpers
// ============================================================================

/** Map error severity to theme color (danger/warning/text_muted) */
lv_color_t severity_color(SlotError::Severity severity);

/** Get worst error severity across all slots in a unit */
SlotError::Severity worst_unit_severity(const AmsUnit& unit);

// ============================================================================
// Data Helpers
// ============================================================================

/** Calculate fill percentage from SlotInfo weight data (returns min_pct..100, or 100 if unknown) */
int fill_percent_from_slot(const SlotInfo& slot, int min_pct = 5);

/**
 * Calculate bar width to fit slot_count bars in container_width.
 * @param container_pct Percentage of container_width to use (default 100)
 */
int32_t calc_bar_width(int32_t container_width, int slot_count, int32_t gap, int32_t min_width,
                       int32_t max_width, int container_pct = 100);

// ============================================================================
// Presentation Helpers
// ============================================================================

/** Get display name for a unit (uses unit.name, falls back to "Unit N") */
std::string get_unit_display_name(const AmsUnit& unit, int unit_index);

} // namespace ams_draw
