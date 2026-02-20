// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lvgl/lvgl.h"

/// Draw a spool box centered at (cx, cy).
/// Filled 3D box when has_spool=true (shadow + body + highlight).
/// Hollow outline with "+" indicator when has_spool=false.
/// sensor_r controls sizing: box is sensor_r*3 wide, sensor_r*4 tall.
void ui_draw_spool_box(lv_layer_t* layer, int32_t cx, int32_t cy, lv_color_t color, bool has_spool,
                       int32_t sensor_r);

/// Color manipulation utilities for 3D spool effects
lv_color_t ui_color_darken(lv_color_t c, uint8_t amt);
lv_color_t ui_color_lighten(lv_color_t c, uint8_t amt);
