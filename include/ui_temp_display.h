// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

/**
 * @file ui_temp_display.h
 * @brief Reusable temperature display widget showing "current / target°C"
 *
 * The temp_display widget provides a standardized way to show temperature
 * information across all panels. It displays current and target temps
 * in the format "210 / 245°C" with consistent styling.
 *
 * ## XML Usage
 *
 * Basic usage with reactive binding (uses PrinterState subjects):
 * @code{.xml}
 * <temp_display name="nozzle_temp" size="lg"
 *               bind_current="extruder_temp"
 *               bind_target="extruder_target"/>
 *
 * <temp_display name="bed_temp" size="lg"
 *               bind_current="bed_temp"
 *               bind_target="bed_target"/>
 * @endcode
 *
 * Current-only display (hide target):
 * @code{.xml}
 * <temp_display name="chamber_temp" size="sm" show_target="false"
 *               bind_current="chamber_temp"/>
 * @endcode
 *
 * ## XML Properties (API)
 *
 * | Property       | Type    | Default | Description                                      |
 * |----------------|---------|---------|--------------------------------------------------|
 * | size           | string  | "md"    | Font size: "sm", "md", or "lg"                   |
 * | show_target    | bool    | "true"  | Show "/ target" portion                          |
 * | bind_current   | subject | -       | Subject name for current temp (centidegrees×10) |
 * | bind_target    | subject | -       | Subject name for target temp (centidegrees×10)  |
 *
 * @note The widget expects temperature subjects in **centidegrees** (value×10),
 *       which matches PrinterState's format. The widget converts internally:
 *       - Subject value 2050 → displays as "205°C"
 *       - Subject value 600  → displays as "60°C"
 *
 * ## Size Mapping
 *
 * | Size | Font Token    | Typical Use                      |
 * |------|---------------|----------------------------------|
 * | sm   | font_small    | Compact displays, status bars    |
 * | md   | font_body     | Standard temperature readouts    |
 * | lg   | font_heading  | Hero displays, main panels       |
 *
 * ## Styling
 *
 * The widget uses semantic colors from the theme:
 * - Current temp: text_primary
 * - Separator (" / "): text_secondary
 * - Target temp: text_primary
 * - Unit ("°C"): text_secondary
 *
 * Standard lv_obj style properties can also be applied (align, width, etc.)
 *
 * ## C++ Usage
 *
 * @code{.cpp}
 * // Find widget and set values directly (without subjects)
 * lv_obj_t* temp = lv_obj_find_by_name(panel, "nozzle_temp");
 * ui_temp_display_set(temp, 210, 245);  // Shows "210 / 245°C"
 *
 * // Update only current during heating animation
 * ui_temp_display_set_current(temp, 215);  // Shows "215 / 245°C"
 *
 * // Read values back
 * int current = ui_temp_display_get_current(temp);
 * int target = ui_temp_display_get_target(temp);
 * @endcode
 *
 * @see ui_temp_display_init() - Must be called during startup
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the temp_display widget for XML usage
 *
 * Registers the custom widget so it can be used as <temp_display> in XML.
 * Call this during UI initialization, after fonts are registered but
 * BEFORE loading any XML components that use <temp_display>.
 *
 * Typical call location: main.cpp after ui_icon_register_widget()
 */
void ui_temp_display_init(void);

/**
 * @brief Set both current and target temperature
 *
 * Use this for direct (non-reactive) temperature updates.
 * If using bind_current/bind_target subjects, prefer updating
 * the subjects instead for automatic reactivity.
 *
 * @param obj temp_display widget
 * @param current Current temperature in °C
 * @param target Target temperature in °C
 */
void ui_temp_display_set(lv_obj_t* obj, int current, int target);

/**
 * @brief Update only the current temperature (keeps target unchanged)
 *
 * Use this for efficient updates during heating animation when
 * only the current value is changing.
 *
 * @param obj temp_display widget
 * @param current Current temperature in °C
 */
void ui_temp_display_set_current(lv_obj_t* obj, int current);

/**
 * @brief Get the current temperature value
 *
 * @param obj temp_display widget
 * @return Current temperature in °C, or -1 if not a valid temp_display
 */
int ui_temp_display_get_current(lv_obj_t* obj);

/**
 * @brief Get the target temperature value
 *
 * @param obj temp_display widget
 * @return Target temperature in °C, or -1 if not a valid temp_display
 */
int ui_temp_display_get_target(lv_obj_t* obj);

/**
 * @brief Check if this is a valid temp_display widget
 *
 * @param obj Object to check
 * @return true if this is a temp_display widget created by this module
 */
bool ui_temp_display_is_valid(lv_obj_t* obj);

#ifdef __cplusplus
}
#endif
