// Copyright (c) 2025 Preston Brown <pbrown@brown-house.net>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TinyGL-based G-code 3D Renderer
// High-quality 3D visualization with lighting, smooth shading, and optimized geometry.

#pragma once

#include "gcode_camera.h"
#include "gcode_geometry_builder.h"
#include "gcode_parser.h"

#include <lvgl/lvgl.h>

#include <glm/glm.hpp>
#include <memory>
#include <optional>

namespace gcode {

/**
 * @brief TinyGL-based 3D renderer for G-code visualization
 *
 * Provides high-quality 3D rendering with:
 * - Smooth Gouraud shading
 * - Two-point studio lighting
 * - Optimized triangle strip geometry
 * - Sub-5MB memory footprint
 *
 * Designed as a drop-in replacement for GCodeRenderer with dramatically
 * improved visual quality.
 */
class GCodeTinyGLRenderer {
  public:
    GCodeTinyGLRenderer();
    ~GCodeTinyGLRenderer();

    // ==============================================
    // Main Rendering Interface (compatible with GCodeRenderer)
    // ==============================================

    /**
     * @brief Render G-code to LVGL layer
     * @param layer LVGL draw layer (from draw event callback)
     * @param gcode Parsed G-code file
     * @param camera Camera with view/projection matrices
     *
     * Main rendering function. Call from LVGL draw event callback.
     */
    void render(lv_layer_t* layer, const ParsedGCodeFile& gcode, const GCodeCamera& camera);

    /**
     * @brief Set viewport size
     * @param width Viewport width in pixels
     * @param height Viewport height in pixels
     */
    void set_viewport_size(int width, int height);

    // ==============================================
    // Configuration
    // ==============================================

    /**
     * @brief Set filament color from hex string
     * @param hex_color Color in hex format (e.g., "#26A69A")
     */
    void set_filament_color(const std::string& hex_color);

    /**
     * @brief Enable/disable smooth shading (Gouraud)
     * @param enable true for smooth shading, false for flat shading
     */
    void set_smooth_shading(bool enable);

    /**
     * @brief Set extrusion width (ribbon tube width)
     * @param width_mm Width in millimeters (default: 0.26mm)
     */
    void set_extrusion_width(float width_mm);

    /**
     * @brief Set geometry simplification tolerance
     * @param tolerance_mm Merge tolerance in mm (default: 0.15mm)
     */
    void set_simplification_tolerance(float tolerance_mm);

    // ==============================================
    // Compatibility Methods (for LVGL renderer interface)
    // ==============================================

    /**
     * @brief Set extrusion color (TinyGL uses filament color instead)
     * @param color Ignored - use set_filament_color() instead
     */
    void set_extrusion_color(lv_color_t color) {
        (void)color;
    }

    /**
     * @brief Set travel color (TinyGL doesn't render travel moves)
     * @param color Ignored
     */
    void set_travel_color(lv_color_t color) {
        (void)color;
    }

    /**
     * @brief Set brightness factor (not implemented for TinyGL)
     * @param factor Ignored
     */
    void set_brightness_factor(float factor) {
        (void)factor;
    }

    /**
     * @brief Get rendering options (layer range, etc.)
     */
    struct RenderingOptions {
        bool show_extrusions;
        bool show_travels;
        int layer_start;
        int layer_end;
        std::string highlighted_object;
    };
    RenderingOptions get_options() const;

    /**
     * @brief Pick object at screen position
     * @param screen_pos Screen coordinates (pixels)
     * @param gcode Parsed G-code file
     * @param camera Current camera
     * @return Object name if picked, nullopt otherwise
     */
    std::optional<std::string> pick_object(const glm::vec2& screen_pos,
                                           const ParsedGCodeFile& gcode,
                                           const GCodeCamera& camera) const;

    /**
     * @brief Show/hide travel moves
     * @param show true to show travel moves, false to hide
     */
    void set_show_travels(bool show);

    /**
     * @brief Show/hide extrusion moves
     * @param show true to show extrusion moves, false to hide
     */
    void set_show_extrusions(bool show);

    /**
     * @brief Set visible layer range
     * @param start First layer to render (0-based)
     * @param end Last layer to render (-1 for all)
     */
    void set_layer_range(int start, int end);

    /**
     * @brief Set highlighted object name
     * @param name Object to highlight (empty to clear)
     */
    void set_highlighted_object(const std::string& name);

    /**
     * @brief Reset to default rendering settings
     */
    void reset_colors();

    /**
     * @brief Set global rendering opacity
     * @param opacity Opacity value (0-255)
     */
    void set_global_opacity(lv_opa_t opacity);

    // ==============================================
    // Statistics
    // ==============================================

    /**
     * @brief Get number of segments rendered (returns triangle count / 2)
     */
    size_t get_segments_rendered() const {
        return get_triangle_count() / 2;
    }

    /**
     * @brief Get memory usage of last rendered geometry
     * @return Memory in bytes
     */
    size_t get_memory_usage() const;

    /**
     * @brief Get triangle count of last rendered geometry
     * @return Triangle count
     */
    size_t get_triangle_count() const;

  private:
    // ==============================================
    // Internal Rendering
    // ==============================================

    /**
     * @brief Initialize TinyGL context for current viewport
     */
    void init_tinygl();

    /**
     * @brief Shutdown TinyGL context
     */
    void shutdown_tinygl();

    /**
     * @brief Build or rebuild geometry from G-code
     * @param gcode Parsed G-code file
     */
    void build_geometry(const ParsedGCodeFile& gcode);

    /**
     * @brief Render geometry with TinyGL
     * @param camera Camera with view/projection
     */
    void render_geometry(const GCodeCamera& camera);

    /**
     * @brief Convert TinyGL framebuffer to LVGL image and draw to layer
     * @param layer LVGL draw layer
     */
    void draw_to_lvgl(lv_layer_t* layer);

    /**
     * @brief Setup lighting (two-point studio setup)
     */
    void setup_lighting();

    /**
     * @brief Render bounding box wireframe for highlighted object
     * @param gcode Parsed G-code file (contains object bounds)
     */
    void render_bounding_box(const ParsedGCodeFile& gcode);

    // Configuration
    int viewport_width_{800};
    int viewport_height_{600};
    bool smooth_shading_{true};   // Smooth shading with wider extrusions
    float extrusion_width_{0.5f}; // Wider for solid appearance
    SimplificationOptions simplification_;

    // Rendering options
    bool show_extrusions_{true};
    bool show_travels_{false};
    int layer_start_{0};
    int layer_end_{-1}; // -1 = all layers
    std::string highlighted_object_;
    lv_opa_t global_opacity_{LV_OPA_100};
    float brightness_factor_{1.0f};

    // TinyGL context (opaque pointer to avoid header dependency)
    void* zbuffer_{nullptr};
    unsigned int* framebuffer_{nullptr};

    // Geometry
    std::unique_ptr<GeometryBuilder> geometry_builder_;
    std::optional<RibbonGeometry> geometry_;
    std::string current_gcode_filename_; // Track if we need to rebuild

    // LVGL image buffer for display
    lv_draw_buf_t* draw_buf_{nullptr};
};

} // namespace gcode
