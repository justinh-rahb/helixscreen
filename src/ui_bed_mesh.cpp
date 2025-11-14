// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ui_bed_mesh.h"

#include "bed_mesh_renderer.h"
#include "lvgl/lvgl.h"
#include "lvgl/src/xml/lv_xml.h"
#include "lvgl/src/xml/lv_xml_parser.h"
#include "lvgl/src/xml/lv_xml_widget.h"
#include "lvgl/src/xml/parsers/lv_xml_obj_parser.h"

#include <spdlog/spdlog.h>

#include <cstdlib>

// Canvas dimensions (600×400 RGB888 = 720,000 bytes)
#define BED_MESH_CANVAS_WIDTH 600
#define BED_MESH_CANVAS_HEIGHT 400

// Rotation angle defaults
#define ROTATION_X_DEFAULT (-45)
#define ROTATION_Z_DEFAULT 45

/**
 * Widget instance data stored in user_data
 */
typedef struct {
    void* buffer;                  // Canvas buffer (for cleanup)
    bed_mesh_renderer_t* renderer; // 3D renderer instance
    int rotation_x;                // Current tilt angle (degrees)
    int rotation_z;                // Current spin angle (degrees)
} bed_mesh_widget_data_t;

/**
 * Delete event handler - cleanup resources
 */
static void bed_mesh_delete_cb(lv_event_t* e) {
    lv_obj_t* canvas = (lv_obj_t*)lv_event_get_target(e);
    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)lv_obj_get_user_data(canvas);

    if (data) {
        // Destroy renderer
        if (data->renderer) {
            bed_mesh_renderer_destroy(data->renderer);
            data->renderer = nullptr;
            spdlog::debug("[bed_mesh] Destroyed renderer");
        }

        // Free canvas buffer
        if (data->buffer) {
            free(data->buffer);
            data->buffer = nullptr;
            spdlog::debug("[bed_mesh] Freed buffer memory");
        }

        // Free widget data struct
        free(data);
        lv_obj_set_user_data(canvas, NULL);
    }
}

/**
 * Size changed event handler - reallocate buffer to match new canvas size
 */
static void bed_mesh_size_changed_cb(lv_event_t* e) {
    lv_obj_t* canvas = (lv_obj_t*)lv_event_get_target(e);
    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)lv_obj_get_user_data(canvas);

    if (!data) {
        spdlog::warn("[bed_mesh] SIZE_CHANGED: no widget data");
        return;
    }

    int new_width = lv_obj_get_width(canvas);
    int new_height = lv_obj_get_height(canvas);

    spdlog::debug("[bed_mesh] SIZE_CHANGED: {}x{}", new_width, new_height);

    // Skip reallocation if dimensions are invalid (flex layout not ready)
    if (new_width <= 0 || new_height <= 0) {
        spdlog::debug("[bed_mesh] Skipping buffer reallocation: invalid dimensions {}x{}",
                      new_width, new_height);
        return;
    }

    // Reallocate buffer to match new canvas size
    size_t new_buffer_size = LV_CANVAS_BUF_SIZE(new_width, new_height, 24, 1);
    void* new_buffer = realloc(data->buffer, new_buffer_size);

    if (!new_buffer) {
        spdlog::error("[bed_mesh] Failed to reallocate buffer for {}x{} ({}bytes)", new_width,
                      new_height, new_buffer_size);
        return;
    }

    data->buffer = new_buffer;

    // Update canvas buffer
    lv_canvas_set_buffer(canvas, data->buffer, new_width, new_height, LV_COLOR_FORMAT_RGB888);

    spdlog::debug("[bed_mesh] Reallocated buffer: {}x{} RGB888 ({} bytes)", new_width, new_height,
                  new_buffer_size);

    // Re-render mesh with new dimensions
    ui_bed_mesh_redraw(canvas);
}

/**
 * XML create handler for <bed_mesh>
 * Creates canvas widget with RGB888 buffer and renderer
 */
static void* bed_mesh_xml_create(lv_xml_parser_state_t* state, const char** attrs) {
    LV_UNUSED(attrs);

    void* parent = lv_xml_state_get_parent(state);
    lv_obj_t* canvas = lv_canvas_create((lv_obj_t*)parent);

    if (!canvas) {
        spdlog::error("[bed_mesh] Failed to create canvas");
        return NULL;
    }

    // Allocate widget data struct
    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)malloc(sizeof(bed_mesh_widget_data_t));
    if (!data) {
        spdlog::error("[bed_mesh] Failed to allocate widget data");
        lv_obj_delete(canvas);
        return NULL;
    }

    // Allocate buffer (600×400 RGB888, 24 bpp, stride=1)
    size_t buffer_size = LV_CANVAS_BUF_SIZE(BED_MESH_CANVAS_WIDTH, BED_MESH_CANVAS_HEIGHT, 24, 1);
    data->buffer = malloc(buffer_size);

    if (!data->buffer) {
        spdlog::error("[bed_mesh] Failed to allocate buffer ({} bytes)", buffer_size);
        free(data);
        lv_obj_delete(canvas);
        return NULL;
    }

    // Create renderer
    data->renderer = bed_mesh_renderer_create();
    if (!data->renderer) {
        spdlog::error("[bed_mesh] Failed to create renderer");
        free(data->buffer);
        free(data);
        lv_obj_delete(canvas);
        return NULL;
    }

    // Set default rotation angles
    data->rotation_x = ROTATION_X_DEFAULT;
    data->rotation_z = ROTATION_Z_DEFAULT;
    bed_mesh_renderer_set_rotation(data->renderer, data->rotation_x, data->rotation_z);

    // Set canvas buffer
    lv_canvas_set_buffer(canvas, data->buffer, BED_MESH_CANVAS_WIDTH, BED_MESH_CANVAS_HEIGHT,
                         LV_COLOR_FORMAT_RGB888);

    // Store widget data in user_data for cleanup and API access
    lv_obj_set_user_data(canvas, data);

    // Register event handlers
    lv_obj_add_event_cb(canvas, bed_mesh_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(canvas, bed_mesh_size_changed_cb, LV_EVENT_SIZE_CHANGED, NULL);

    // Set default size (will be overridden by XML width/height attributes)
    lv_obj_set_size(canvas, BED_MESH_CANVAS_WIDTH, BED_MESH_CANVAS_HEIGHT);

    spdlog::debug("[bed_mesh] Created canvas: {}x{} RGB888 ({} bytes), renderer initialized",
                  BED_MESH_CANVAS_WIDTH, BED_MESH_CANVAS_HEIGHT, buffer_size);

    return (void*)canvas;
}

/**
 * XML apply handler for <bed_mesh>
 * Applies standard lv_obj attributes from XML
 */
static void bed_mesh_xml_apply(lv_xml_parser_state_t* state, const char** attrs) {
    void* item = lv_xml_state_get_item(state);
    lv_obj_t* canvas = (lv_obj_t*)item;

    if (!canvas) {
        spdlog::error("[bed_mesh] NULL canvas in xml_apply");
        return;
    }

    // Apply standard lv_obj properties from XML (size, style, align, etc.)
    lv_xml_obj_apply(state, attrs);

    spdlog::trace("[bed_mesh] Applied XML attributes");
}

/**
 * Register <bed_mesh> widget with LVGL XML system
 */
void ui_bed_mesh_register(void) {
    lv_xml_register_widget("bed_mesh", bed_mesh_xml_create, bed_mesh_xml_apply);
    spdlog::info("[bed_mesh] Registered <bed_mesh> widget with XML system");
}

/**
 * Set mesh data for rendering
 */
bool ui_bed_mesh_set_data(lv_obj_t* canvas, const float* const* mesh, int rows, int cols) {
    if (!canvas) {
        spdlog::error("[bed_mesh] ui_bed_mesh_set_data: NULL canvas");
        return false;
    }

    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)lv_obj_get_user_data(canvas);
    if (!data || !data->renderer) {
        spdlog::error("[bed_mesh] ui_bed_mesh_set_data: widget data or renderer not initialized");
        return false;
    }

    if (!mesh || rows <= 0 || cols <= 0) {
        spdlog::error("[bed_mesh] ui_bed_mesh_set_data: invalid mesh data (rows={}, cols={})", rows,
                      cols);
        return false;
    }

    // Set mesh data in renderer
    if (!bed_mesh_renderer_set_mesh_data(data->renderer, mesh, rows, cols)) {
        spdlog::error("[bed_mesh] Failed to set mesh data in renderer");
        return false;
    }

    spdlog::info("[bed_mesh] Mesh data loaded: {}x{}", rows, cols);

    // Automatically redraw after setting new data
    ui_bed_mesh_redraw(canvas);

    return true;
}

/**
 * Set camera rotation angles
 */
void ui_bed_mesh_set_rotation(lv_obj_t* canvas, int angle_x, int angle_z) {
    if (!canvas) {
        spdlog::error("[bed_mesh] ui_bed_mesh_set_rotation: NULL canvas");
        return;
    }

    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)lv_obj_get_user_data(canvas);
    if (!data || !data->renderer) {
        spdlog::error("[bed_mesh] ui_bed_mesh_set_rotation: widget data or renderer not "
                      "initialized");
        return;
    }

    // Update stored rotation angles
    data->rotation_x = angle_x;
    data->rotation_z = angle_z;

    // Update renderer
    bed_mesh_renderer_set_rotation(data->renderer, angle_x, angle_z);

    spdlog::debug("[bed_mesh] Rotation updated: tilt={}°, spin={}°", angle_x, angle_z);

    // Automatically redraw after rotation change
    ui_bed_mesh_redraw(canvas);
}

/**
 * Force redraw of mesh visualization
 */
void ui_bed_mesh_redraw(lv_obj_t* canvas) {
    if (!canvas) {
        spdlog::warn("[bed_mesh] ui_bed_mesh_redraw: NULL canvas");
        return;
    }

    bed_mesh_widget_data_t* data = (bed_mesh_widget_data_t*)lv_obj_get_user_data(canvas);
    if (!data || !data->renderer) {
        spdlog::warn("[bed_mesh] ui_bed_mesh_redraw: widget data or renderer not initialized");
        return;
    }

    // Force layout update before rendering (LVGL deferred layout)
    lv_obj_update_layout(canvas);

    // Clear canvas
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    // Render mesh
    if (!bed_mesh_renderer_render(data->renderer, canvas)) {
        spdlog::error("[bed_mesh] Render failed");
        return;
    }

    spdlog::debug("[bed_mesh] Render complete");
}
