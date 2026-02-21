// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_image_helpers.h"

#include <spdlog/spdlog.h>

namespace helix::ui {

bool image_scale_to_cover(lv_obj_t* image_widget, lv_coord_t target_width,
                          lv_coord_t target_height) {
    if (!image_widget) {
        spdlog::error("[UI Image] Cannot scale image: widget is null");
        return false;
    }

    // Get source image dimensions
    lv_image_header_t header;
    lv_result_t res = lv_image_decoder_get_info(lv_image_get_src(image_widget), &header);

    if (res != LV_RESULT_OK || header.w == 0 || header.h == 0) {
        int w = header.w, h = header.h; // Copy bitfields for formatting
        spdlog::warn("[UI Image] Cannot get image info for scaling (res={}, w={}, h={})",
                     static_cast<int>(res), w, h);
        return false;
    }

    // Calculate scale to cover the target area (like CSS object-fit: cover)
    // Use larger scale factor so image fills entire area (may crop)
    float scale_w = (float)target_width / header.w;
    float scale_h = (float)target_height / header.h;
    float scale = (scale_w > scale_h) ? scale_w : scale_h; // Use larger scale to cover

    // LVGL uses zoom as fixed-point: 256 = 1.0x, 512 = 2.0x, etc.
    uint16_t zoom = (uint16_t)(scale * 256);
    lv_image_set_scale(image_widget, zoom);
    lv_image_set_inner_align(image_widget, LV_IMAGE_ALIGN_CENTER);

    int img_w = header.w, img_h = header.h; // Copy bitfields for formatting
    spdlog::debug("[UI Image] Scale (cover): img={}x{}, target={}x{}, zoom={} ({:.1f}%)", img_w,
                  img_h, target_width, target_height, zoom, scale * 100);

    return true;
}

bool image_scale_to_contain(lv_obj_t* image_widget, lv_coord_t target_width,
                            lv_coord_t target_height, lv_image_align_t align) {
    if (!image_widget) {
        spdlog::error("[UI Image] Cannot scale image: widget is null");
        return false;
    }

    // Get source image dimensions
    lv_image_header_t header;
    lv_result_t res = lv_image_decoder_get_info(lv_image_get_src(image_widget), &header);

    if (res != LV_RESULT_OK || header.w == 0 || header.h == 0) {
        int w = header.w, h = header.h; // Copy bitfields for formatting
        spdlog::warn("[UI Image] Cannot get image info for scaling (res={}, w={}, h={})",
                     static_cast<int>(res), w, h);
        return false;
    }

    // Calculate scale to contain the image (like CSS object-fit: contain)
    // Use smaller scale factor so entire image fits within area (no crop)
    float scale_w = (float)target_width / header.w;
    float scale_h = (float)target_height / header.h;
    float scale = (scale_w < scale_h) ? scale_w : scale_h; // Use smaller scale to contain

    // LVGL uses zoom as fixed-point: 256 = 1.0x, 512 = 2.0x, etc.
    uint16_t zoom = (uint16_t)(scale * 256);
    lv_image_set_scale(image_widget, zoom);
    lv_image_set_inner_align(image_widget, align);

    int img_w = header.w, img_h = header.h; // Copy bitfields for formatting
    spdlog::debug("[UI Image] Scale (contain): img={}x{}, target={}x{}, zoom={} ({:.1f}%)", img_w,
                  img_h, target_width, target_height, zoom, scale * 100);

    return true;
}

} // namespace helix::ui
