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

#include "bed_mesh_renderer.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// Internal renderer state
struct bed_mesh_renderer {
    // Mesh data storage
    std::vector<std::vector<double>> mesh; // mesh[row][col] = Z height
    int rows;
    int cols;
    double mesh_min_z;
    double mesh_max_z;
    bool has_mesh_data;

    // Color range configuration
    bool auto_color_range;
    double color_min_z;
    double color_max_z;

    // View/camera state
    bed_mesh_view_state_t view_state;

    // Computed rendering state
    std::vector<bed_mesh_quad_3d_t> quads; // Generated geometry
};

// Helper functions (forward declarations)
static void compute_mesh_bounds(bed_mesh_renderer_t* renderer);
static double compute_dynamic_z_scale(double z_range);
static double compute_fov_scale(int rows, int cols, int canvas_width, int canvas_height);
static bed_mesh_point_3d_t project_3d_to_2d(double x, double y, double z, int canvas_width,
                                            int canvas_height, const bed_mesh_view_state_t* view);
static lv_color_t height_to_color(double value, double min_val, double max_val);
static bed_mesh_rgb_t lerp_color(bed_mesh_rgb_t a, bed_mesh_rgb_t b, double t);
static void fill_triangle_solid(lv_obj_t* canvas, int x1, int y1, int x2, int y2, int x3, int y3,
                                lv_color_t color);
static void fill_triangle_gradient(lv_obj_t* canvas, int x1, int y1, lv_color_t c1, int x2, int y2,
                                   lv_color_t c2, int x3, int y3, lv_color_t c3);
static void generate_mesh_quads(bed_mesh_renderer_t* renderer);
static void sort_quads_by_depth(std::vector<bed_mesh_quad_3d_t>& quads);
static void render_quad(lv_obj_t* canvas, const bed_mesh_quad_3d_t& quad, int canvas_width,
                        int canvas_height, const bed_mesh_view_state_t* view, bool use_gradient);
static void render_grid_lines(lv_obj_t* canvas, const bed_mesh_renderer_t* renderer,
                              int canvas_width, int canvas_height);

// Public API implementation

bed_mesh_renderer_t* bed_mesh_renderer_create(void) {
    bed_mesh_renderer_t* renderer = new (std::nothrow) bed_mesh_renderer_t;
    if (!renderer) {
        spdlog::error("Failed to allocate bed mesh renderer");
        return nullptr;
    }

    // Initialize state
    renderer->rows = 0;
    renderer->cols = 0;
    renderer->mesh_min_z = 0.0;
    renderer->mesh_max_z = 0.0;
    renderer->has_mesh_data = false;

    renderer->auto_color_range = true;
    renderer->color_min_z = 0.0;
    renderer->color_max_z = 0.0;

    // Default view state (looking down from above at an angle)
    renderer->view_state.angle_x = -85.0;
    renderer->view_state.angle_z = 10.0;
    renderer->view_state.z_scale = BED_MESH_DEFAULT_Z_SCALE;
    renderer->view_state.fov_scale = 100.0;
    renderer->view_state.is_dragging = false;

    spdlog::debug("Created bed mesh renderer");
    return renderer;
}

void bed_mesh_renderer_destroy(bed_mesh_renderer_t* renderer) {
    if (!renderer) {
        return;
    }

    spdlog::debug("Destroying bed mesh renderer");
    delete renderer;
}

bool bed_mesh_renderer_set_mesh_data(bed_mesh_renderer_t* renderer, const float* const* mesh,
                                     int rows, int cols) {
    if (!renderer || !mesh || rows <= 0 || cols <= 0) {
        spdlog::error(
            "Invalid parameters for set_mesh_data: renderer={}, mesh={}, rows={}, cols={}",
            (void*)renderer, (void*)mesh, rows, cols);
        return false;
    }

    spdlog::debug("Setting mesh data: {}x{} points", rows, cols);

    // Allocate storage
    renderer->mesh.clear();
    renderer->mesh.resize(rows);
    for (int row = 0; row < rows; row++) {
        renderer->mesh[row].resize(cols);
        for (int col = 0; col < cols; col++) {
            renderer->mesh[row][col] = static_cast<double>(mesh[row][col]);
        }
    }

    renderer->rows = rows;
    renderer->cols = cols;
    renderer->has_mesh_data = true;

    // Compute bounds
    compute_mesh_bounds(renderer);

    // If auto color range, update it
    if (renderer->auto_color_range) {
        renderer->color_min_z = renderer->mesh_min_z;
        renderer->color_max_z = renderer->mesh_max_z;
    }

    spdlog::debug("Mesh bounds: min_z={:.3f}, max_z={:.3f}, range={:.3f}", renderer->mesh_min_z,
                  renderer->mesh_max_z, renderer->mesh_max_z - renderer->mesh_min_z);

    return true;
}

void bed_mesh_renderer_set_rotation(bed_mesh_renderer_t* renderer, double angle_x, double angle_z) {
    if (!renderer) {
        return;
    }

    renderer->view_state.angle_x = angle_x;
    renderer->view_state.angle_z = angle_z;
}

const bed_mesh_view_state_t* bed_mesh_renderer_get_view_state(bed_mesh_renderer_t* renderer) {
    if (!renderer) {
        return nullptr;
    }
    return &renderer->view_state;
}

void bed_mesh_renderer_set_view_state(bed_mesh_renderer_t* renderer,
                                      const bed_mesh_view_state_t* state) {
    if (!renderer || !state) {
        return;
    }
    renderer->view_state = *state;
}

void bed_mesh_renderer_set_dragging(bed_mesh_renderer_t* renderer, bool is_dragging) {
    if (!renderer) {
        return;
    }
    renderer->view_state.is_dragging = is_dragging;
}

void bed_mesh_renderer_set_z_scale(bed_mesh_renderer_t* renderer, double z_scale) {
    if (!renderer) {
        return;
    }
    // Clamp to valid range
    z_scale = std::max(BED_MESH_MIN_Z_SCALE, std::min(BED_MESH_MAX_Z_SCALE, z_scale));
    renderer->view_state.z_scale = z_scale;
}

void bed_mesh_renderer_set_fov_scale(bed_mesh_renderer_t* renderer, double fov_scale) {
    if (!renderer) {
        return;
    }
    renderer->view_state.fov_scale = fov_scale;
}

void bed_mesh_renderer_set_color_range(bed_mesh_renderer_t* renderer, double min_z, double max_z) {
    if (!renderer) {
        return;
    }

    renderer->auto_color_range = false;
    renderer->color_min_z = min_z;
    renderer->color_max_z = max_z;

    spdlog::debug("Manual color range set: min={:.3f}, max={:.3f}", min_z, max_z);
}

void bed_mesh_renderer_auto_color_range(bed_mesh_renderer_t* renderer) {
    if (!renderer) {
        return;
    }

    renderer->auto_color_range = true;
    if (renderer->has_mesh_data) {
        renderer->color_min_z = renderer->mesh_min_z;
        renderer->color_max_z = renderer->mesh_max_z;
    }

    spdlog::debug("Auto color range enabled");
}

bool bed_mesh_renderer_render(bed_mesh_renderer_t* renderer, lv_obj_t* canvas) {
    if (!renderer || !canvas) {
        spdlog::error("Invalid parameters for render: renderer={}, canvas={}", (void*)renderer,
                      (void*)canvas);
        return false;
    }

    if (!renderer->has_mesh_data) {
        spdlog::warn("No mesh data loaded, cannot render");
        return false;
    }

    // Get canvas dimensions
    int canvas_width = lv_obj_get_width(canvas);
    int canvas_height = lv_obj_get_height(canvas);

    // Skip rendering if canvas dimensions are invalid (flex layout not yet calculated)
    if (canvas_width <= 0 || canvas_height <= 0) {
        spdlog::debug("Skipping render: invalid canvas dimensions {}x{} (layout not ready)",
                      canvas_width, canvas_height);
        return false;
    }

    spdlog::debug("Canvas dimensions: {}x{}", canvas_width, canvas_height);

    spdlog::debug("Rendering mesh to {}x{} canvas (dragging={})", canvas_width, canvas_height,
                  renderer->view_state.is_dragging);

    // Clear canvas
    lv_canvas_fill_bg(canvas, lv_color_make(40, 40, 40), LV_OPA_COVER);

    // Compute dynamic Z scale if needed
    double z_range = renderer->mesh_max_z - renderer->mesh_min_z;
    if (z_range < 1e-6) {
        // Flat mesh, use default scale
        renderer->view_state.z_scale = BED_MESH_DEFAULT_Z_SCALE;
    } else {
        // Compute dynamic scale to fit mesh in reasonable height
        double computed_scale = compute_dynamic_z_scale(z_range);
        // Keep current scale if user has adjusted it manually
        // (This is a simplification - could track manual vs auto scale separately)
        renderer->view_state.z_scale = computed_scale;
    }

    // Compute FOV scale to fit mesh in canvas
    renderer->view_state.fov_scale =
        compute_fov_scale(renderer->rows, renderer->cols, canvas_width, canvas_height);

    // Generate geometry quads with colors
    generate_mesh_quads(renderer);

    // Project quads to screen space and compute depths
    for (auto& quad : renderer->quads) {
        double total_depth = 0.0;
        for (int i = 0; i < 4; i++) {
            bed_mesh_point_3d_t projected =
                project_3d_to_2d(quad.vertices[i].x, quad.vertices[i].y, quad.vertices[i].z,
                                 canvas_width, canvas_height, &renderer->view_state);
            total_depth += projected.depth;
        }
        quad.avg_depth = total_depth / 4.0;
    }

    // Sort quads by depth (painter's algorithm - furthest first)
    sort_quads_by_depth(renderer->quads);

    spdlog::debug("Rendering {} quads with {} mode", renderer->quads.size(),
                  renderer->view_state.is_dragging ? "solid" : "gradient");

    // Render quads
    bool use_gradient = !renderer->view_state.is_dragging;
    for (const auto& quad : renderer->quads) {
        render_quad(canvas, quad, canvas_width, canvas_height, &renderer->view_state, use_gradient);
    }

    // Render wireframe grid on top
    render_grid_lines(canvas, renderer, canvas_width, canvas_height);

    // Invalidate canvas for LVGL redraw
    lv_obj_invalidate(canvas);

    spdlog::debug("Mesh rendering complete");
    return true;
}

// Helper function implementations

static void compute_mesh_bounds(bed_mesh_renderer_t* renderer) {
    if (!renderer || !renderer->has_mesh_data) {
        return;
    }

    double min_z = renderer->mesh[0][0];
    double max_z = renderer->mesh[0][0];

    for (int row = 0; row < renderer->rows; row++) {
        for (int col = 0; col < renderer->cols; col++) {
            double z = renderer->mesh[row][col];
            if (z < min_z)
                min_z = z;
            if (z > max_z)
                max_z = z;
        }
    }

    renderer->mesh_min_z = min_z;
    renderer->mesh_max_z = max_z;
}

static double compute_dynamic_z_scale(double z_range) {
    // Compute scale to amplify Z range to target height
    double z_scale = BED_MESH_DEFAULT_Z_TARGET_HEIGHT / z_range;

    // Clamp to valid range
    z_scale = std::max(BED_MESH_MIN_Z_SCALE, std::min(BED_MESH_MAX_Z_SCALE, z_scale));

    return z_scale;
}

static double compute_fov_scale(int rows, int cols, int canvas_width, int canvas_height) {
    // Compute diagonal of mesh in world space
    double mesh_width = (cols - 1) * BED_MESH_SCALE;
    double mesh_height = (rows - 1) * BED_MESH_SCALE;
    double mesh_diagonal = std::sqrt(mesh_width * mesh_width + mesh_height * mesh_height);

    // Compute available canvas space (with 10% padding)
    double available_width = canvas_width * 0.9;
    double available_height = canvas_height * 0.9;
    double available_diagonal =
        std::sqrt(available_width * available_width + available_height * available_height);

    // Scale to fit mesh in canvas
    double fov_scale = (available_diagonal * BED_MESH_CAMERA_DISTANCE) / mesh_diagonal;

    return fov_scale;
}

static bed_mesh_point_3d_t project_3d_to_2d(double x, double y, double z, int canvas_width,
                                            int canvas_height, const bed_mesh_view_state_t* view) {
    bed_mesh_point_3d_t result;

    // Step 1: Z-axis rotation (spin around vertical axis)
    double z_angle_rad = view->angle_z * M_PI / 180.0;
    double cos_z = std::cos(z_angle_rad);
    double sin_z = std::sin(z_angle_rad);
    double rotated_x = x * cos_z - y * sin_z;
    double rotated_y = x * sin_z + y * cos_z;
    double rotated_z = z;

    // Step 2: X-axis rotation (tilt up/down)
    double x_angle_rad = view->angle_x * M_PI / 180.0;
    double cos_x = std::cos(x_angle_rad);
    double sin_x = std::sin(x_angle_rad);
    double final_x = rotated_x;
    double final_y = rotated_y * cos_x + rotated_z * sin_x;
    double final_z = rotated_y * sin_x - rotated_z * cos_x;

    // Step 3: Translate camera back
    final_z += BED_MESH_CAMERA_DISTANCE;

    // Step 4: Perspective projection (similar triangles)
    double perspective_x = (final_x * view->fov_scale) / final_z;
    double perspective_y = (final_y * view->fov_scale) / final_z;

    // Step 5: Convert to screen coordinates
    result.screen_x = static_cast<int>(canvas_width / 2 + perspective_x);
    result.screen_y =
        static_cast<int>(canvas_height * BED_MESH_Z_ORIGIN_VERTICAL_POS + perspective_y);
    result.depth = final_z;

    return result;
}

static lv_color_t height_to_color(double value, double min_val, double max_val) {
    // Apply color compression for enhanced contrast
    double data_range = max_val - min_val;
    double adjusted_range = data_range * BED_MESH_COLOR_COMPRESSION;
    double data_center = (min_val + max_val) / 2.0;
    double color_min = data_center - (adjusted_range / 2.0);

    // Normalize to [0, 1]
    double normalized = (value - color_min) / adjusted_range;
    normalized = std::max(0.0, std::min(1.0, normalized));

    // Compute RGB using segmented heat-map
    uint8_t r, g, b;

    if (normalized < 0.125) {
        // Purple to Blue
        double t = normalized / 0.125;
        r = static_cast<uint8_t>(128 * (1.0 - t));
        g = static_cast<uint8_t>(128 * t);
        b = 255;
    } else if (normalized < 0.375) {
        // Blue to Cyan
        double t = (normalized - 0.125) / 0.25;
        r = 0;
        g = static_cast<uint8_t>(128 + 127 * t);
        b = 255;
    } else if (normalized < 0.625) {
        // Cyan to Yellow
        double t = (normalized - 0.375) / 0.25;
        r = static_cast<uint8_t>(255 * t);
        g = 255;
        b = static_cast<uint8_t>(255 * (1.0 - t));
    } else if (normalized < 0.875) {
        // Yellow to Red
        double t = (normalized - 0.625) / 0.25;
        r = 255;
        g = static_cast<uint8_t>(255 * (1.0 - t));
        b = 0;
    } else {
        // Deep Red
        r = 255;
        g = 0;
        b = 0;
    }

    // Desaturate by 35% for muted appearance
    uint8_t gray = (r + g + b) / 3;
    r = static_cast<uint8_t>(r * 0.65 + gray * 0.35);
    g = static_cast<uint8_t>(g * 0.65 + gray * 0.35);
    b = static_cast<uint8_t>(b * 0.65 + gray * 0.35);

    return lv_color_make(r, g, b);
}

static bed_mesh_rgb_t lerp_color(bed_mesh_rgb_t a, bed_mesh_rgb_t b, double t) {
    bed_mesh_rgb_t result;
    result.r = static_cast<uint8_t>(a.r + t * (b.r - a.r));
    result.g = static_cast<uint8_t>(a.g + t * (b.g - a.g));
    result.b = static_cast<uint8_t>(a.b + t * (b.b - a.b));
    return result;
}

static void fill_triangle_solid(lv_obj_t* canvas, int x1, int y1, int x2, int y2, int x3, int y3,
                                lv_color_t color) {
    // Get canvas dimensions for bounds checking
    int canvas_width = lv_obj_get_width(canvas);
    int canvas_height = lv_obj_get_height(canvas);

    // Sort vertices by Y coordinate (bubble sort for 3 elements)
    if (y1 > y2) {
        std::swap(y1, y2);
        std::swap(x1, x2);
    }
    if (y2 > y3) {
        std::swap(y2, y3);
        std::swap(x2, x3);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
        std::swap(x1, x2);
    }

    // Skip degenerate triangles
    if (y1 == y3)
        return;

    // Skip triangles completely outside canvas bounds
    if (y3 < 0 || y1 >= canvas_height)
        return;
    if (std::max({x1, x2, x3}) < 0 || std::min({x1, x2, x3}) >= canvas_width)
        return;

    // Prepare draw descriptor
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_90;
    dsc.border_width = 0;

    // Scanline fill with bounds clipping
    int y_start = std::max(y1, 0);
    int y_end = std::min(y3, canvas_height - 1);

    for (int y = y_start; y <= y_end; y++) {
        // Compute left/right edges
        double t_long = (y - y1) / static_cast<double>(y3 - y1);
        int x_long = x1 + static_cast<int>(t_long * (x3 - x1));

        int x_short;
        if (y < y2) {
            // Upper half: interpolate along y1->y2
            if (y2 == y1) {
                x_short = x1;
            } else {
                double t = (y - y1) / static_cast<double>(y2 - y1);
                x_short = x1 + static_cast<int>(t * (x2 - x1));
            }
        } else {
            // Lower half: interpolate along y2->y3
            if (y3 == y2) {
                x_short = x2;
            } else {
                double t = (y - y2) / static_cast<double>(y3 - y2);
                x_short = x2 + static_cast<int>(t * (x3 - x2));
            }
        }

        // Ensure x_left <= x_right and clip to canvas bounds
        int x_left = std::max(std::min(x_long, x_short), 0);
        int x_right = std::min(std::max(x_long, x_short), canvas_width - 1);

        // Draw horizontal line pixel by pixel (only if valid range)
        if (x_left <= x_right) {
            for (int px = x_left; px <= x_right; px++) {
                lv_canvas_set_px(canvas, px, y, color, LV_OPA_90);
            }
        }
    }
}

static void fill_triangle_gradient(lv_obj_t* canvas, int x1, int y1, lv_color_t c1, int x2, int y2,
                                   lv_color_t c2, int x3, int y3, lv_color_t c3) {
    // Get canvas dimensions for bounds checking
    int canvas_width = lv_obj_get_width(canvas);
    int canvas_height = lv_obj_get_height(canvas);

    // Sort vertices by Y coordinate, keeping colors aligned
    struct Vertex {
        int x, y;
        bed_mesh_rgb_t color;
    };
    Vertex v[3] = {{x1, y1, {c1.red, c1.green, c1.blue}},
                   {x2, y2, {c2.red, c2.green, c2.blue}},
                   {x3, y3, {c3.red, c3.green, c3.blue}}};

    if (v[0].y > v[1].y)
        std::swap(v[0], v[1]);
    if (v[1].y > v[2].y)
        std::swap(v[1], v[2]);
    if (v[0].y > v[1].y)
        std::swap(v[0], v[1]);

    // Skip degenerate triangles
    if (v[0].y == v[2].y)
        return;

    // Skip triangles completely outside canvas bounds
    if (v[2].y < 0 || v[0].y >= canvas_height)
        return;
    if (std::max({v[0].x, v[1].x, v[2].x}) < 0 ||
        std::min({v[0].x, v[1].x, v[2].x}) >= canvas_width)
        return;

    // Prepare draw descriptor
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_90;
    dsc.border_width = 0;

    // Scanline fill with color interpolation and bounds clipping
    int y_start = std::max(v[0].y, 0);
    int y_end = std::min(v[2].y, canvas_height - 1);

    for (int y = y_start; y <= y_end; y++) {
        // Interpolate along long edge (v0 -> v2)
        double t_long = (y - v[0].y) / static_cast<double>(v[2].y - v[0].y);
        int x_long = v[0].x + static_cast<int>(t_long * (v[2].x - v[0].x));
        bed_mesh_rgb_t c_long = lerp_color(v[0].color, v[2].color, t_long);

        // Interpolate along short edge
        int x_short;
        bed_mesh_rgb_t c_short;
        if (y < v[1].y) {
            // Upper half: v0 -> v1
            if (v[1].y == v[0].y) {
                x_short = v[0].x;
                c_short = v[0].color;
            } else {
                double t = (y - v[0].y) / static_cast<double>(v[1].y - v[0].y);
                x_short = v[0].x + static_cast<int>(t * (v[1].x - v[0].x));
                c_short = lerp_color(v[0].color, v[1].color, t);
            }
        } else {
            // Lower half: v1 -> v2
            if (v[2].y == v[1].y) {
                x_short = v[1].x;
                c_short = v[1].color;
            } else {
                double t = (y - v[1].y) / static_cast<double>(v[2].y - v[1].y);
                x_short = v[1].x + static_cast<int>(t * (v[2].x - v[1].x));
                c_short = lerp_color(v[1].color, v[2].color, t);
            }
        }

        // Ensure left/right ordering and clip to canvas bounds
        int x_left = std::max(std::min(x_long, x_short), 0);
        int x_right = std::min(std::max(x_long, x_short), canvas_width - 1);
        bed_mesh_rgb_t c_left = (x_long < x_short) ? c_long : c_short;
        bed_mesh_rgb_t c_right = (x_long < x_short) ? c_short : c_long;

        int line_width = x_right - x_left + 1;
        if (line_width <= 0)
            continue;

        // Performance: use solid color for thin lines
        if (line_width < BED_MESH_GRADIENT_MIN_LINE_WIDTH) {
            bed_mesh_rgb_t avg = lerp_color(c_left, c_right, 0.5);
            lv_color_t avg_color = lv_color_make(avg.r, avg.g, avg.b);
            for (int px = x_left; px <= x_right; px++) {
                lv_canvas_set_px(canvas, px, y, avg_color, LV_OPA_90);
            }
        } else {
            // Gradient: divide into segments
            int segments = std::min(BED_MESH_GRADIENT_SEGMENTS, line_width / 4);
            if (segments < 1)
                segments = 1;

            for (int seg = 0; seg < segments; seg++) {
                int seg_x_start = x_left + (seg * line_width) / segments;
                int seg_x_end = x_left + ((seg + 1) * line_width) / segments;
                if (seg_x_start >= seg_x_end)
                    continue;

                double t = (seg + 0.5) / segments;
                bed_mesh_rgb_t seg_color = lerp_color(c_left, c_right, t);
                lv_color_t color = lv_color_make(seg_color.r, seg_color.g, seg_color.b);
                for (int px = seg_x_start; px < seg_x_end; px++) {
                    lv_canvas_set_px(canvas, px, y, color, LV_OPA_90);
                }
            }
        }
    }
}

static void generate_mesh_quads(bed_mesh_renderer_t* renderer) {
    if (!renderer || !renderer->has_mesh_data) {
        return;
    }

    renderer->quads.clear();

    // Center mesh around origin for rotation
    double z_center = (renderer->mesh_min_z + renderer->mesh_max_z) / 2.0;

    // Generate quads for each mesh cell
    for (int row = 0; row < renderer->rows - 1; row++) {
        for (int col = 0; col < renderer->cols - 1; col++) {
            bed_mesh_quad_3d_t quad;

            // Compute base X,Y positions (centered around origin)
            // Note: Y is inverted because mesh[0] = front edge
            double base_x_0 = (col - renderer->cols / 2.0) * BED_MESH_SCALE;
            double base_x_1 = (col + 1 - renderer->cols / 2.0) * BED_MESH_SCALE;
            double base_y_0 = ((renderer->rows - 1 - row) - renderer->rows / 2.0) * BED_MESH_SCALE;
            double base_y_1 =
                ((renderer->rows - 1 - (row + 1)) - renderer->rows / 2.0) * BED_MESH_SCALE;

            // Create 4 vertices with Z values (centered and scaled)
            // Vertex order: [0]=bottom-left, [1]=bottom-right, [2]=top-left, [3]=top-right
            quad.vertices[0].x = base_x_0;
            quad.vertices[0].y = base_y_1;
            quad.vertices[0].z =
                (renderer->mesh[row + 1][col] - z_center) * renderer->view_state.z_scale;
            quad.vertices[0].color = height_to_color(renderer->mesh[row + 1][col],
                                                     renderer->color_min_z, renderer->color_max_z);

            quad.vertices[1].x = base_x_1;
            quad.vertices[1].y = base_y_1;
            quad.vertices[1].z =
                (renderer->mesh[row + 1][col + 1] - z_center) * renderer->view_state.z_scale;
            quad.vertices[1].color = height_to_color(renderer->mesh[row + 1][col + 1],
                                                     renderer->color_min_z, renderer->color_max_z);

            quad.vertices[2].x = base_x_0;
            quad.vertices[2].y = base_y_0;
            quad.vertices[2].z =
                (renderer->mesh[row][col] - z_center) * renderer->view_state.z_scale;
            quad.vertices[2].color = height_to_color(renderer->mesh[row][col],
                                                     renderer->color_min_z, renderer->color_max_z);

            quad.vertices[3].x = base_x_1;
            quad.vertices[3].y = base_y_0;
            quad.vertices[3].z =
                (renderer->mesh[row][col + 1] - z_center) * renderer->view_state.z_scale;
            quad.vertices[3].color = height_to_color(renderer->mesh[row][col + 1],
                                                     renderer->color_min_z, renderer->color_max_z);

            // Compute center color for fast rendering
            bed_mesh_rgb_t avg_color = {
                static_cast<uint8_t>((quad.vertices[0].color.red + quad.vertices[1].color.red +
                                      quad.vertices[2].color.red + quad.vertices[3].color.red) /
                                     4),
                static_cast<uint8_t>((quad.vertices[0].color.green + quad.vertices[1].color.green +
                                      quad.vertices[2].color.green + quad.vertices[3].color.green) /
                                     4),
                static_cast<uint8_t>((quad.vertices[0].color.blue + quad.vertices[1].color.blue +
                                      quad.vertices[2].color.blue + quad.vertices[3].color.blue) /
                                     4)};
            quad.center_color = lv_color_make(avg_color.r, avg_color.g, avg_color.b);

            quad.avg_depth = 0.0; // Will be computed during projection

            renderer->quads.push_back(quad);
        }
    }

    spdlog::debug("Generated {} quads from {}x{} mesh", renderer->quads.size(), renderer->rows,
                  renderer->cols);
}

static void sort_quads_by_depth(std::vector<bed_mesh_quad_3d_t>& quads) {
    std::sort(quads.begin(), quads.end(),
              [](const bed_mesh_quad_3d_t& a, const bed_mesh_quad_3d_t& b) {
                  // Descending order: furthest (largest depth) first
                  return a.avg_depth > b.avg_depth;
              });
}

/**
 * Render wireframe grid lines over the mesh surface
 * Draws horizontal and vertical lines connecting mesh vertices
 */
static void render_grid_lines(lv_obj_t* canvas, const bed_mesh_renderer_t* renderer,
                              int canvas_width, int canvas_height) {
    if (!renderer || !renderer->has_mesh_data) {
        return;
    }

    // Initialize canvas layer for drawing
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // Configure line drawing style
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_make(80, 80, 80); // Dark gray
    line_dsc.width = 1;
    line_dsc.opa = LV_OPA_60; // 60% opacity

    // Center mesh Z values (must match quad generation for proper alignment)
    double z_center = (renderer->mesh_min_z + renderer->mesh_max_z) / 2.0;

    // Project all mesh vertices to screen space once
    std::vector<std::vector<bed_mesh_point_3d_t>> projected_points(renderer->rows);
    for (int row = 0; row < renderer->rows; row++) {
        projected_points[row].resize(renderer->cols);
        for (int col = 0; col < renderer->cols; col++) {
            // Convert mesh coordinates to world space (matching quad generation exactly)
            double world_x = (col - renderer->cols / 2.0) * BED_MESH_SCALE;
            // Y is inverted: mesh[0] = front edge (far from viewer after rotation)
            double world_y = ((renderer->rows - 1 - row) - renderer->rows / 2.0) * BED_MESH_SCALE;
            // Z is centered and scaled (matching quad vertex Z calculation)
            double world_z = (renderer->mesh[row][col] - z_center) * renderer->view_state.z_scale;

            // Project to screen space
            projected_points[row][col] = project_3d_to_2d(world_x, world_y, world_z, canvas_width,
                                                          canvas_height, &renderer->view_state);
        }
    }

    // Draw horizontal grid lines (connect points in same row)
    for (int row = 0; row < renderer->rows; row++) {
        for (int col = 0; col < renderer->cols - 1; col++) {
            const auto& p1 = projected_points[row][col];
            const auto& p2 = projected_points[row][col + 1];

            // Bounds check (allow some margin for partially visible lines)
            if (p1.screen_x >= -10 && p1.screen_x < canvas_width + 10 && p1.screen_y >= -10 &&
                p1.screen_y < canvas_height + 10 && p2.screen_x >= -10 &&
                p2.screen_x < canvas_width + 10 && p2.screen_y >= -10 &&
                p2.screen_y < canvas_height + 10) {
                // Set line endpoints in descriptor
                line_dsc.p1.x = static_cast<lv_value_precise_t>(p1.screen_x);
                line_dsc.p1.y = static_cast<lv_value_precise_t>(p1.screen_y);
                line_dsc.p2.x = static_cast<lv_value_precise_t>(p2.screen_x);
                line_dsc.p2.y = static_cast<lv_value_precise_t>(p2.screen_y);

                lv_draw_line(&layer, &line_dsc);
            }
        }
    }

    // Draw vertical grid lines (connect points in same column)
    for (int col = 0; col < renderer->cols; col++) {
        for (int row = 0; row < renderer->rows - 1; row++) {
            const auto& p1 = projected_points[row][col];
            const auto& p2 = projected_points[row + 1][col];

            // Bounds check
            if (p1.screen_x >= -10 && p1.screen_x < canvas_width + 10 && p1.screen_y >= -10 &&
                p1.screen_y < canvas_height + 10 && p2.screen_x >= -10 &&
                p2.screen_x < canvas_width + 10 && p2.screen_y >= -10 &&
                p2.screen_y < canvas_height + 10) {
                // Set line endpoints in descriptor
                line_dsc.p1.x = static_cast<lv_value_precise_t>(p1.screen_x);
                line_dsc.p1.y = static_cast<lv_value_precise_t>(p1.screen_y);
                line_dsc.p2.x = static_cast<lv_value_precise_t>(p2.screen_x);
                line_dsc.p2.y = static_cast<lv_value_precise_t>(p2.screen_y);

                lv_draw_line(&layer, &line_dsc);
            }
        }
    }

    // Finish layer and apply to canvas
    lv_canvas_finish_layer(canvas, &layer);
}

static void render_quad(lv_obj_t* canvas, const bed_mesh_quad_3d_t& quad, int canvas_width,
                        int canvas_height, const bed_mesh_view_state_t* view, bool use_gradient) {
    // Project all 4 vertices to screen space
    bed_mesh_point_3d_t projected[4];
    for (int i = 0; i < 4; i++) {
        projected[i] = project_3d_to_2d(quad.vertices[i].x, quad.vertices[i].y, quad.vertices[i].z,
                                        canvas_width, canvas_height, view);
    }

    // Render quad as 2 triangles
    // Triangle 1: vertices[0], vertices[1], vertices[2]
    if (use_gradient) {
        fill_triangle_gradient(canvas, projected[0].screen_x, projected[0].screen_y,
                               quad.vertices[0].color, projected[1].screen_x, projected[1].screen_y,
                               quad.vertices[1].color, projected[2].screen_x, projected[2].screen_y,
                               quad.vertices[2].color);
    } else {
        fill_triangle_solid(canvas, projected[0].screen_x, projected[0].screen_y,
                            projected[1].screen_x, projected[1].screen_y, projected[2].screen_x,
                            projected[2].screen_y, quad.center_color);
    }

    // Triangle 2: vertices[1], vertices[2], vertices[3]
    if (use_gradient) {
        fill_triangle_gradient(canvas, projected[1].screen_x, projected[1].screen_y,
                               quad.vertices[1].color, projected[2].screen_x, projected[2].screen_y,
                               quad.vertices[2].color, projected[3].screen_x, projected[3].screen_y,
                               quad.vertices[3].color);
    } else {
        fill_triangle_solid(canvas, projected[1].screen_x, projected[1].screen_y,
                            projected[2].screen_x, projected[2].screen_y, projected[3].screen_x,
                            projected[3].screen_y, quad.center_color);
    }
}
