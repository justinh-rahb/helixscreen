// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

/**
 * @file ui_ams_slot.h
 * @brief Custom LVGL XML widget for AMS filament slot display
 *
 * The ams_slot widget encapsulates a single AMS slot with:
 * - Color swatch showing filament color (bound to ams_slot_N_color subject)
 * - Status icon (available, loaded, blocked, etc.)
 * - Material label (PLA, PETG, etc.)
 * - Slot number badge
 * - Active slot highlight border
 *
 * XML usage:
 * @code{.xml}
 * <ams_slot slot_index="0"/>
 * <ams_slot slot_index="1"/>
 * @endcode
 *
 * The widget automatically creates observers on AmsState subjects based on
 * slot_index and cleans them up when the widget is deleted.
 */

/**
 * @brief Register the ams_slot widget with LVGL's XML system
 *
 * Must be called AFTER AmsState::init_subjects() and BEFORE any XML files
 * using <ams_slot> are registered.
 */
void ui_ams_slot_register(void);

/**
 * @brief Get the slot index from an ams_slot widget
 *
 * @param obj The ams_slot widget
 * @return Slot index (0-15), or -1 if not an ams_slot widget
 */
int ui_ams_slot_get_index(lv_obj_t* obj);

/**
 * @brief Set the slot index on an ams_slot widget
 *
 * This re-creates the observers for the new slot index.
 * Generally only called during initial XML parsing.
 *
 * @param obj The ams_slot widget
 * @param slot_index Slot index (0-15)
 */
void ui_ams_slot_set_index(lv_obj_t* obj, int slot_index);

/**
 * @brief Force refresh of all visual elements from current AmsState
 *
 * Useful after backend sync or when slot becomes visible.
 *
 * @param obj The ams_slot widget
 */
void ui_ams_slot_refresh(lv_obj_t* obj);

/**
 * @brief Set the fill level of the spool visualization
 *
 * Used to show remaining filament when integrated with Spoolman.
 * The filament ring scales from full (near outer edge) to empty (near hub).
 *
 * @param obj        The ams_slot widget
 * @param fill_level Fill level from 0.0 (empty) to 1.0 (full)
 */
void ui_ams_slot_set_fill_level(lv_obj_t* obj, float fill_level);

/**
 * @brief Get the current fill level of a slot
 *
 * @param obj The ams_slot widget
 * @return Fill level (0.0-1.0), or 1.0 if not an ams_slot widget
 */
float ui_ams_slot_get_fill_level(lv_obj_t* obj);

#ifdef __cplusplus
}
#endif
