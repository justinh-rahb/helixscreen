// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

namespace helix::ui {

/**
 * Scale image to cover a target area (like CSS object-fit: cover)
 * Image may be cropped but will fill the entire area with no empty space
 *
 * @param image_widget The lv_image widget to scale
 * @param target_width Target width in pixels
 * @param target_height Target height in pixels
 * @return true if scaling succeeded, false if image info could not be obtained
 */
bool image_scale_to_cover(lv_obj_t* image_widget, lv_coord_t target_width,
                          lv_coord_t target_height);

/**
 * Scale image to fit within a target area (like CSS object-fit: contain)
 * Entire image will be visible, may have empty space around it
 *
 * @param image_widget The lv_image widget to scale
 * @param target_width Target width in pixels
 * @param target_height Target height in pixels
 * @param align Alignment within the target area (default: LV_IMAGE_ALIGN_CENTER)
 * @return true if scaling succeeded, false if image info could not be obtained
 */
bool image_scale_to_contain(lv_obj_t* image_widget, lv_coord_t target_width,
                            lv_coord_t target_height,
                            lv_image_align_t align = LV_IMAGE_ALIGN_CENTER);

} // namespace helix::ui
