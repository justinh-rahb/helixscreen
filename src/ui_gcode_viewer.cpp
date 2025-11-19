// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 HelixScreen Contributors
 *
 * This file is part of HelixScreen, which is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * See <https://www.gnu.org/licenses/>.
 */

#include "ui_gcode_viewer.h"

#include "gcode_camera.h"
#include "gcode_parser.h"

#ifdef ENABLE_TINYGL_3D
#include "gcode_tinygl_renderer.h"
#else
#include "gcode_renderer.h"
#endif

#include <lvgl/src/xml/lv_xml_parser.h>
#include <lvgl/src/xml/parsers/lv_xml_obj_parser.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <unordered_set>

// Widget state (stored in LVGL object user_data)
struct gcode_viewer_state_t {
    // G-code data
    std::unique_ptr<gcode::ParsedGCodeFile> gcode_file;
    gcode_viewer_state_enum_t viewer_state{GCODE_VIEWER_STATE_EMPTY};

    // Rendering components
    std::unique_ptr<gcode::GCodeCamera> camera;
#ifdef ENABLE_TINYGL_3D
    std::unique_ptr<gcode::GCodeTinyGLRenderer> renderer;
#else
    std::unique_ptr<gcode::GCodeRenderer> renderer;
#endif

    // Gesture state (for touch interaction)
    bool is_dragging{false};
    lv_point_t drag_start{0, 0};
    lv_point_t last_drag_pos{0, 0};

    // Selection state (future-proofed for multi-select)
    std::unordered_set<std::string> selected_objects;

    // Rendering settings
    bool use_filament_color{true}; // Auto-apply filament color from metadata

    // Constructor
    gcode_viewer_state_t() {
        camera = std::make_unique<gcode::GCodeCamera>();
#ifdef ENABLE_TINYGL_3D
        renderer = std::make_unique<gcode::GCodeTinyGLRenderer>();
        spdlog::info("GCode viewer using TinyGL 3D renderer");
#else
        renderer = std::make_unique<gcode::GCodeRenderer>();
        spdlog::info("GCode viewer using LVGL 2D renderer");
#endif
    }
};

// Helper: Get widget state from object
static gcode_viewer_state_t* get_state(lv_obj_t* obj) {
    return static_cast<gcode_viewer_state_t*>(lv_obj_get_user_data(obj));
}

// ==============================================
// Event Callbacks
// ==============================================

/**
 * @brief Main draw callback - renders G-code using custom renderer
 */
static void gcode_viewer_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (!st || !layer) {
        return;
    }

    // If no G-code loaded, draw placeholder message
    if (st->viewer_state != GCODE_VIEWER_STATE_LOADED || !st->gcode_file) {
        // TODO: Draw "No G-code loaded" message
        return;
    }

    // Render G-code (viewport size is already set by SIZE_CHANGED event)
    st->renderer->render(layer, *st->gcode_file, *st->camera);
}

/**
 * @brief Touch press callback - start drag gesture
 */
static void gcode_viewer_press_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (!st)
        return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev)
        return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    st->is_dragging = true;
    st->drag_start = point;
    st->last_drag_pos = point;

    spdlog::trace("GCodeViewer: Press at ({}, {})", point.x, point.y);
}

/**
 * @brief Touch pressing callback - handle drag for camera rotation
 */
static void gcode_viewer_pressing_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (!st || !st->is_dragging)
        return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev)
        return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // Calculate delta from last position
    int dx = point.x - st->last_drag_pos.x;
    int dy = point.y - st->last_drag_pos.y;

    if (dx != 0 || dy != 0) {
        // Convert pixel movement to rotation angles
        // Scale factor: ~0.5 degrees per pixel
        float delta_azimuth = dx * 0.5f;
        float delta_elevation = -dy * 0.5f; // Flip Y for intuitive control

        st->camera->rotate(delta_azimuth, delta_elevation);

        // Trigger redraw
        lv_obj_invalidate(obj);

        st->last_drag_pos = point;

        spdlog::trace("GCodeViewer: Drag ({}, {}) -> rotate({:.1f}, {:.1f})", dx, dy, delta_azimuth,
                      delta_elevation);
    }
}

/**
 * @brief Touch release callback - handle click vs drag gesture
 */
static void gcode_viewer_release_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (!st)
        return;

    // Get release position
    lv_indev_t* indev = lv_indev_active();
    if (!indev) {
        st->is_dragging = false;
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    // Calculate total drag distance from initial press
    int dx = abs(point.x - st->drag_start.x);
    int dy = abs(point.y - st->drag_start.y);

    const int CLICK_THRESHOLD = 10; // pixels - distinguish click from drag

    // If movement was minimal, treat as click and try to pick object
    if (dx < CLICK_THRESHOLD && dy < CLICK_THRESHOLD && st->gcode_file) {
        spdlog::debug("GCodeViewer: Click detected at ({}, {})", point.x, point.y);
        const char* picked = ui_gcode_viewer_pick_object(obj, point.x, point.y);

        if (picked && picked[0] != '\0') {
            // Object clicked - toggle selection
            std::string picked_name(picked);

            if (st->selected_objects.count(picked_name) > 0) {
                // Already selected - deselect
                st->selected_objects.erase(picked_name);
                spdlog::info("GCodeViewer: Deselected object '{}'", picked_name);

                // Clear highlighting
                ui_gcode_viewer_set_highlighted_object(obj, nullptr);
            } else {
                // Not selected - select it (single selection mode)
                st->selected_objects.clear(); // Clear any previous selection
                st->selected_objects.insert(picked_name);
                spdlog::info("GCodeViewer: Selected object '{}'", picked_name);

                // Highlight the selected object
                ui_gcode_viewer_set_highlighted_object(obj, picked_name.c_str());
            }
        } else {
            spdlog::debug("GCodeViewer: Click at ({}, {}) - no object found (G-code may lack EXCLUDE_OBJECT metadata)",
                         point.x, point.y);
        }
        // Note: If no object picked, keep current selection (per user requirements)
    }

    st->is_dragging = false;
    spdlog::trace("GCodeViewer: Release at ({}, {}), drag=({}, {})", point.x, point.y, dx, dy);
}

/**
 * @brief Size changed callback - update camera aspect ratio on resize
 */
static void gcode_viewer_size_changed_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (!st)
        return;

    // Get new widget dimensions
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int width = lv_area_get_width(&coords);
    int height = lv_area_get_height(&coords);

    // Update camera and renderer viewport to match new size
    st->camera->set_viewport_size(width, height);
    st->renderer->set_viewport_size(width, height);

    // Trigger redraw with new aspect ratio
    lv_obj_invalidate(obj);

    spdlog::info("GCodeViewer SIZE_CHANGED: {}x{}, aspect={:.3f}",
                 width, height, (float)width / (float)height);
}

/**
 * @brief Cleanup callback - free resources on widget deletion
 */
static void gcode_viewer_delete_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    gcode_viewer_state_t* st = get_state(obj);

    if (st) {
        spdlog::debug("GCodeViewer: Widget destroyed");
        delete st; // C++ destructor will clean up unique_ptrs
        lv_obj_set_user_data(obj, nullptr);
    }
}

// ==============================================
// Public API Implementation
// ==============================================

lv_obj_t* ui_gcode_viewer_create(lv_obj_t* parent) {
    // Create base object
    lv_obj_t* obj = lv_obj_create(parent);
    if (!obj) {
        return nullptr;
    }

    // Allocate state (C++ object)
    gcode_viewer_state_t* st = new gcode_viewer_state_t();
    if (!st) {
        lv_obj_delete(obj);
        return nullptr;
    }

    lv_obj_set_user_data(obj, st);

    // Configure object appearance
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);

    // Register event handlers
    lv_obj_add_event_cb(obj, gcode_viewer_draw_cb, LV_EVENT_DRAW_POST, nullptr);
    lv_obj_add_event_cb(obj, gcode_viewer_size_changed_cb, LV_EVENT_SIZE_CHANGED, nullptr);
    lv_obj_add_event_cb(obj, gcode_viewer_press_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(obj, gcode_viewer_pressing_cb, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(obj, gcode_viewer_release_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(obj, gcode_viewer_delete_cb, LV_EVENT_DELETE, nullptr);

    // Initialize viewport size based on current widget dimensions
    // This ensures correct aspect ratio from the start
    lv_obj_update_layout(obj); // Force layout calculation
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int width = lv_area_get_width(&coords);
    int height = lv_area_get_height(&coords);

    if (width > 0 && height > 0) {
        st->camera->set_viewport_size(width, height);
        st->renderer->set_viewport_size(width, height);
        spdlog::info("GCodeViewer INIT: viewport={}x{}, aspect={:.3f}",
                     width, height, (float)width / (float)height);
    } else {
        spdlog::error("GCodeViewer INIT: Invalid size {}x{}, using defaults", width, height);
    }

    spdlog::debug("GCodeViewer: Widget created");
    return obj;
}

void ui_gcode_viewer_load_file(lv_obj_t* obj, const char* file_path) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !file_path)
        return;

    spdlog::info("GCodeViewer: Loading file: {}", file_path);
    st->viewer_state = GCODE_VIEWER_STATE_LOADING;

    // Parse file (synchronous in Phase 1)
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            spdlog::error("GCodeViewer: Failed to open file: {}", file_path);
            st->viewer_state = GCODE_VIEWER_STATE_ERROR;
            return;
        }

        gcode::GCodeParser parser;
        std::string line;

        while (std::getline(file, line)) {
            parser.parse_line(line);
        }

        file.close();

        // Finalize and store result
        st->gcode_file = std::make_unique<gcode::ParsedGCodeFile>(parser.finalize());

        // Set filename
        st->gcode_file->filename = file_path;

        // Fit camera to model bounds
        st->camera->fit_to_bounds(st->gcode_file->global_bounding_box);
        st->camera->set_isometric_view();

        st->viewer_state = GCODE_VIEWER_STATE_LOADED;

        spdlog::info("GCodeViewer: Loaded {} layers, {} segments, {} objects",
                     st->gcode_file->layers.size(), st->gcode_file->total_segments,
                     st->gcode_file->objects.size());

        // Auto-apply filament color if enabled and available
        if (st->use_filament_color && st->gcode_file->filament_color_hex.length() >= 2) {
            lv_color_t color = lv_color_hex(
                std::strtol(st->gcode_file->filament_color_hex.c_str() + 1, nullptr, 16));
            st->renderer->set_extrusion_color(color);
            spdlog::info("GCodeViewer: Auto-applied filament color: {}",
                         st->gcode_file->filament_color_hex);
        }

        // Trigger redraw
        lv_obj_invalidate(obj);

    } catch (const std::exception& ex) {
        spdlog::error("GCodeViewer: Exception during parsing: {}", ex.what());
        st->viewer_state = GCODE_VIEWER_STATE_ERROR;
        st->gcode_file.reset();
    }
}

void ui_gcode_viewer_set_gcode_data(lv_obj_t* obj, void* gcode_data) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !gcode_data)
        return;

    // Take ownership of the data (caller must use new to allocate)
    st->gcode_file.reset(static_cast<gcode::ParsedGCodeFile*>(gcode_data));

    // Fit camera to model
    st->camera->fit_to_bounds(st->gcode_file->global_bounding_box);
    st->camera->set_isometric_view();

    st->viewer_state = GCODE_VIEWER_STATE_LOADED;

    spdlog::info("GCodeViewer: Set G-code data: {} layers, {} segments",
                 st->gcode_file->layers.size(), st->gcode_file->total_segments);

    // Auto-apply filament color if enabled and available
    if (st->use_filament_color && st->gcode_file->filament_color_hex.length() >= 2) {
        lv_color_t color =
            lv_color_hex(std::strtol(st->gcode_file->filament_color_hex.c_str() + 1, nullptr, 16));
        st->renderer->set_extrusion_color(color);
        spdlog::info("GCodeViewer: Auto-applied filament color: {}",
                     st->gcode_file->filament_color_hex);
    }

    // Trigger redraw
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_clear(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->gcode_file.reset();
    st->viewer_state = GCODE_VIEWER_STATE_EMPTY;

    lv_obj_invalidate(obj);
    spdlog::debug("GCodeViewer: Cleared");
}

gcode_viewer_state_enum_t ui_gcode_viewer_get_state(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    return st ? st->viewer_state : GCODE_VIEWER_STATE_EMPTY;
}

// ==============================================
// Camera Controls
// ==============================================

void ui_gcode_viewer_rotate(lv_obj_t* obj, float delta_azimuth, float delta_elevation) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->camera->rotate(delta_azimuth, delta_elevation);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_pan(lv_obj_t* obj, float delta_x, float delta_y) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->camera->pan(delta_x, delta_y);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_zoom(lv_obj_t* obj, float factor) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->camera->zoom(factor);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_reset_camera(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->camera->reset();

    // Re-fit to model if loaded
    if (st->gcode_file) {
        st->camera->fit_to_bounds(st->gcode_file->global_bounding_box);
    }

    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_view(lv_obj_t* obj, gcode_viewer_preset_view_t preset) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    switch (preset) {
    case GCODE_VIEWER_VIEW_ISOMETRIC:
        st->camera->set_isometric_view();
        break;
    case GCODE_VIEWER_VIEW_TOP:
        st->camera->set_top_view();
        break;
    case GCODE_VIEWER_VIEW_FRONT:
        st->camera->set_front_view();
        break;
    case GCODE_VIEWER_VIEW_SIDE:
        st->camera->set_side_view();
        break;
    }

    lv_obj_invalidate(obj);
}

// ==============================================
// Rendering Options
// ==============================================

void ui_gcode_viewer_set_show_travels(lv_obj_t* obj, bool show) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_show_travels(show);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_show_extrusions(lv_obj_t* obj, bool show) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_show_extrusions(show);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_layer_range(lv_obj_t* obj, int start_layer, int end_layer) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_layer_range(start_layer, end_layer);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_highlighted_object(lv_obj_t* obj, const char* object_name) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_highlighted_object(object_name ? object_name : "");
    lv_obj_invalidate(obj);
}

// ==============================================
// Color & Rendering Control
// ==============================================

void ui_gcode_viewer_set_extrusion_color(lv_obj_t* obj, lv_color_t color) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_extrusion_color(color);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_travel_color(lv_obj_t* obj, lv_color_t color) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_travel_color(color);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_use_filament_color(lv_obj_t* obj, bool enable) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->use_filament_color = enable;

    // If enabling and we have a loaded file with filament color, apply it now
    if (enable && st->gcode_file && st->gcode_file->filament_color_hex.length() >= 2) {
        lv_color_t color =
            lv_color_hex(std::strtol(st->gcode_file->filament_color_hex.c_str() + 1, nullptr, 16));
        st->renderer->set_extrusion_color(color);
        lv_obj_invalidate(obj);
        spdlog::debug("GCodeViewer: Applied filament color: {}",
                      st->gcode_file->filament_color_hex);
    } else if (!enable) {
        // Reset to theme default
        st->renderer->reset_colors();
        lv_obj_invalidate(obj);
    }
}

void ui_gcode_viewer_set_opacity(lv_obj_t* obj, lv_opa_t opacity) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_global_opacity(opacity);
    lv_obj_invalidate(obj);
}

void ui_gcode_viewer_set_brightness(lv_obj_t* obj, float factor) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return;

    st->renderer->set_brightness_factor(factor);
    lv_obj_invalidate(obj);
}

// ==============================================
// Layer Control Extensions
// ==============================================

void ui_gcode_viewer_set_single_layer(lv_obj_t* obj, int layer) {
    ui_gcode_viewer_set_layer_range(obj, layer, layer);
}

int ui_gcode_viewer_get_current_layer_start(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return 0;

    return st->renderer->get_options().layer_start;
}

int ui_gcode_viewer_get_current_layer_end(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return -1;

    return st->renderer->get_options().layer_end;
}

// ==============================================
// Metadata Access
// ==============================================

const char* ui_gcode_viewer_get_filament_color(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file || st->gcode_file->filament_color_hex.empty())
        return nullptr;

    return st->gcode_file->filament_color_hex.c_str();
}

const char* ui_gcode_viewer_get_filament_type(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file || st->gcode_file->filament_type.empty())
        return nullptr;

    return st->gcode_file->filament_type.c_str();
}

const char* ui_gcode_viewer_get_printer_model(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file || st->gcode_file->printer_model.empty())
        return nullptr;

    return st->gcode_file->printer_model.c_str();
}

float ui_gcode_viewer_get_estimated_time_minutes(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0.0f;

    return st->gcode_file->estimated_print_time_minutes;
}

float ui_gcode_viewer_get_filament_weight_g(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0.0f;

    return st->gcode_file->filament_weight_g;
}

float ui_gcode_viewer_get_filament_length_mm(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0.0f;

    return st->gcode_file->total_filament_mm;
}

float ui_gcode_viewer_get_filament_cost(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0.0f;

    return st->gcode_file->filament_cost;
}

float ui_gcode_viewer_get_nozzle_diameter_mm(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0.0f;

    return st->gcode_file->nozzle_diameter_mm;
}

// ==============================================
// Object Picking
// ==============================================

const char* ui_gcode_viewer_pick_object(lv_obj_t* obj, int x, int y) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return nullptr;

    auto result = st->renderer->pick_object(glm::vec2(x, y), *st->gcode_file, *st->camera);

    if (result) {
        // Store in static buffer (safe for single-threaded LVGL)
        static std::string picked_name;
        picked_name = *result;
        return picked_name.c_str();
    }

    return nullptr;
}

// ==============================================
// Statistics
// ==============================================

int ui_gcode_viewer_get_layer_count(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st || !st->gcode_file)
        return 0;

    return static_cast<int>(st->gcode_file->layers.size());
}

int ui_gcode_viewer_get_segments_rendered(lv_obj_t* obj) {
    gcode_viewer_state_t* st = get_state(obj);
    if (!st)
        return 0;

    return static_cast<int>(st->renderer->get_segments_rendered());
}

// ==============================================
// LVGL XML Component Registration
// ==============================================

/**
 * @brief XML create handler for gcode_viewer widget
 */
static void* gcode_viewer_xml_create(lv_xml_parser_state_t* state, const char** attrs) {
    void* parent = lv_xml_state_get_parent(state);
    if (!parent) {
        spdlog::error("[GCodeViewer] XML create: no parent object");
        return nullptr;
    }

    lv_obj_t* obj = ui_gcode_viewer_create((lv_obj_t*)parent);
    if (!obj) {
        spdlog::error("[GCodeViewer] XML create: failed to create widget");
        return nullptr;
    }

    spdlog::trace("[GCodeViewer] XML created widget");
    return (void*)obj;
}

/**
 * @brief XML apply handler for gcode_viewer widget
 * Applies XML attributes to the widget
 */
static void gcode_viewer_xml_apply(lv_xml_parser_state_t* state, const char** attrs) {
    void* item = lv_xml_state_get_item(state);
    lv_obj_t* obj = (lv_obj_t*)item;

    if (!obj) {
        spdlog::error("[GCodeViewer] NULL object in xml_apply");
        return;
    }

    // Apply standard lv_obj properties from XML (size, style, align, name, etc.)
    lv_xml_obj_apply(state, attrs);

    spdlog::trace("[GCodeViewer] Applied XML attributes");
}

/**
 * @brief Register gcode_viewer widget with LVGL XML system
 *
 * Call this during application initialization before loading any XML.
 * Typically called from main() or ui_init().
 */
extern "C" void ui_gcode_viewer_register(void) {
    lv_xml_register_widget("gcode_viewer", gcode_viewer_xml_create, gcode_viewer_xml_apply);
    spdlog::info("[GCodeViewer] Registered <gcode_viewer> widget with LVGL XML system");
}
