// Copyright (c) 2025 Preston Brown <pbrown@brown-house.net>
// SPDX-License-Identifier: GPL-3.0-or-later
//
// G-Code Geometry Builder Implementation

#include "gcode_geometry_builder.h"

#include <spdlog/spdlog.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <cmath>
#include <glm/gtx/norm.hpp>

namespace gcode {

// ============================================================================
// QuantizationParams Implementation
// ============================================================================

void QuantizationParams::calculate_scale(const AABB& bbox) {
    min_bounds = bbox.min;
    max_bounds = bbox.max;

    // Calculate maximum dimension to determine scale factor
    glm::vec3 extents = max_bounds - min_bounds;
    float max_extent = std::max({extents.x, extents.y, extents.z});

    // 16-bit signed int range: -32768 to +32767
    // Quantization formula: (value - min_bound) * scale
    // Maximum quantized value = (max_bound - min_bound) * scale = extent * scale
    // Constraint: extent * scale <= 32767
    // Reserve 10% headroom to avoid edge cases
    constexpr float INT16_MAX_WITH_HEADROOM = 32767.0f * 0.9f;

    if (max_extent > 0.0f) {
        scale_factor = INT16_MAX_WITH_HEADROOM / max_extent;
    } else {
        // Fallback for degenerate bounding box
        scale_factor = 1000.0f; // 1 unit = 1mm
    }

    spdlog::debug("Quantization: bounds=[{:.2f},{:.2f},{:.2f}] to [{:.2f},{:.2f},{:.2f}], "
                  "scale={:.2f} units/mm, resolution={:.4f}mm",
                  min_bounds.x, min_bounds.y, min_bounds.z, max_bounds.x, max_bounds.y,
                  max_bounds.z, scale_factor, 1.0f / scale_factor);
}

int16_t QuantizationParams::quantize(float value, float min_bound) const {
    float normalized = (value - min_bound) * scale_factor;

    // Clamp to int16 range to prevent overflow
    normalized = std::max(-32768.0f, std::min(32767.0f, normalized));

    return static_cast<int16_t>(std::round(normalized));
}

float QuantizationParams::dequantize(int16_t value, float min_bound) const {
    return static_cast<float>(value) / scale_factor + min_bound;
}

QuantizedVertex QuantizationParams::quantize_vec3(const glm::vec3& v) const {
    return QuantizedVertex{quantize(v.x, min_bounds.x), quantize(v.y, min_bounds.y),
                           quantize(v.z, min_bounds.z)};
}

glm::vec3 QuantizationParams::dequantize_vec3(const QuantizedVertex& qv) const {
    return glm::vec3(dequantize(qv.x, min_bounds.x), dequantize(qv.y, min_bounds.y),
                     dequantize(qv.z, min_bounds.z));
}

// ============================================================================
// BuildStats Implementation
// ============================================================================

void GeometryBuilder::BuildStats::log() const {
    spdlog::info("Geometry Build Statistics:");
    spdlog::info("  Input segments:      {:>8}", input_segments);
    spdlog::info("  Simplified segments: {:>8} ({:.1f}% reduction)", output_segments,
                 simplification_ratio * 100.0f);
    spdlog::info("  Vertices generated:  {:>8}", vertices_generated);
    spdlog::info("  Triangles generated: {:>8}", triangles_generated);
    spdlog::info("  Memory usage:        {:>8} KB ({:.2f} MB)", memory_bytes / 1024,
                 memory_bytes / (1024.0 * 1024.0));

    if (input_segments > 0) {
        float bytes_per_segment = static_cast<float>(memory_bytes) / input_segments;
        spdlog::info("  Bytes per segment:   {:.1f}", bytes_per_segment);
    }
}

// ============================================================================
// GeometryBuilder Implementation
// ============================================================================

GeometryBuilder::GeometryBuilder() {
    stats_ = {};
}

// ============================================================================
// Palette Management
// ============================================================================

uint16_t GeometryBuilder::add_to_normal_palette(RibbonGeometry& geometry, const glm::vec3& normal) {
    // Very light quantization (0.001) to merge nearly-identical normals without visible banding
    constexpr float QUANT_STEP = 0.001f;
    glm::vec3 quantized;
    quantized.x = std::round(normal.x / QUANT_STEP) * QUANT_STEP;
    quantized.y = std::round(normal.y / QUANT_STEP) * QUANT_STEP;
    quantized.z = std::round(normal.z / QUANT_STEP) * QUANT_STEP;

    // Renormalize to ensure unit vector
    float length = glm::length(quantized);
    if (length > 0.0001f) {
        quantized /= length;
    } else {
        quantized = normal; // Fallback if quantization created zero vector
    }

    // Search for existing quantized normal in palette
    constexpr float EPSILON = 0.0001f;
    for (size_t i = 0; i < geometry.normal_palette.size(); ++i) {
        const glm::vec3& existing = geometry.normal_palette[i];
        if (glm::length(existing - quantized) < EPSILON) {
            return static_cast<uint16_t>(i); // Found existing
        }
    }

    // Add new quantized normal to palette (supports up to 65536 entries)
    if (geometry.normal_palette.size() >= 65536) {
        spdlog::warn("Normal palette full (65536 entries), reusing last entry");
        return 65535;
    }

    geometry.normal_palette.push_back(quantized); // Store quantized normal

    // Log palette size periodically
    if (geometry.normal_palette.size() % 1000 == 0) {
        spdlog::debug("Normal palette: {} entries", geometry.normal_palette.size());
    }

    return static_cast<uint16_t>(geometry.normal_palette.size() - 1);
}

uint8_t GeometryBuilder::add_to_color_palette(RibbonGeometry& geometry, uint32_t color_rgb) {
    // Search for existing color in palette
    for (size_t i = 0; i < geometry.color_palette.size(); ++i) {
        if (geometry.color_palette[i] == color_rgb) {
            return static_cast<uint8_t>(i); // Found existing
        }
    }

    // Add new color to palette
    if (geometry.color_palette.size() >= 256) {
        spdlog::warn("Color palette full (256 entries), reusing last entry");
        return 255;
    }

    geometry.color_palette.push_back(color_rgb);
    return static_cast<uint8_t>(geometry.color_palette.size() - 1);
}

RibbonGeometry GeometryBuilder::build(const ParsedGCodeFile& gcode,
                                      const SimplificationOptions& options) {
    RibbonGeometry geometry;
    stats_ = {}; // Reset statistics

    // Validate and apply options
    SimplificationOptions validated_opts = options;
    validated_opts.validate();

    spdlog::info("Building G-code geometry (tolerance={:.3f}mm, merging={})",
                 validated_opts.tolerance_mm, validated_opts.enable_merging);

    // Calculate quantization parameters from bounding box
    // IMPORTANT: Expand bounds to account for tube width (vertices extend beyond segment positions)
    // Use sqrt(2) safety factor because rectangular tubes on diagonal segments can expand
    // in multiple dimensions simultaneously (e.g., perp_horizontal + perp_vertical)
    float max_tube_width = std::max(extrusion_width_mm_, travel_width_mm_);
    float expansion_margin = max_tube_width * 1.5f; // Safety factor for diagonal expansion
    AABB expanded_bbox = gcode.global_bounding_box;
    expanded_bbox.min -= glm::vec3(expansion_margin, expansion_margin, expansion_margin);
    expanded_bbox.max += glm::vec3(expansion_margin, expansion_margin, expansion_margin);
    quant_params_.calculate_scale(expanded_bbox);

    spdlog::debug("Expanded quantization bounds by {:.1f}mm for tube width {:.1f}mm",
                  expansion_margin, max_tube_width);

    // Collect all segments from all layers
    std::vector<ToolpathSegment> all_segments;
    for (const auto& layer : gcode.layers) {
        all_segments.insert(all_segments.end(), layer.segments.begin(), layer.segments.end());
    }

    stats_.input_segments = all_segments.size();
    spdlog::debug("Collected {} total segments from {} layers", all_segments.size(),
                  gcode.layers.size());

    // Step 1: Simplify segments (merge collinear lines)
    std::vector<ToolpathSegment> simplified;
    if (validated_opts.enable_merging) {
        simplified = simplify_segments(all_segments, validated_opts);
        stats_.output_segments = simplified.size();
        stats_.simplification_ratio =
            1.0f - (static_cast<float>(simplified.size()) / all_segments.size());

        spdlog::info("Simplified {} → {} segments ({:.1f}% reduction)", all_segments.size(),
                     simplified.size(), stats_.simplification_ratio * 100.0f);
    } else {
        simplified = all_segments;
        stats_.output_segments = simplified.size();
        stats_.simplification_ratio = 0.0f;
    }

    // Step 2: Generate ribbon geometry with vertex sharing
    // Track previous segment end vertices for reuse
    std::optional<TubeCap> prev_end_cap;
    glm::vec3 prev_end_pos{0.0f};

    // DEBUG: Track segment Y range
    float seg_y_min = FLT_MAX, seg_y_max = -FLT_MAX;
    size_t segments_skipped = 0;
    for (size_t i = 0; i < simplified.size(); ++i) {
        const auto& segment = simplified[i];

        // Skip degenerate segments (zero length)
        float segment_length = glm::distance(segment.start, segment.end);
        if (segment_length < 0.0001f) {
            segments_skipped++;
            continue;
        }

        // Track Y range
        seg_y_min = std::min({seg_y_min, segment.start.y, segment.end.y});
        seg_y_max = std::max({seg_y_max, segment.start.y, segment.end.y});

        // Check if we can share vertices with previous segment (OPTIMIZATION ENABLED!)
        bool can_share = false;
        if (prev_end_cap.has_value()) {
            // Segments must connect spatially (within epsilon) and be same type
            float dist = glm::distance(segment.start, prev_end_pos);
            can_share = (dist < 0.001f) && (segment.is_extrusion == simplified[i - 1].is_extrusion);
        }

        // Generate geometry, reusing previous end cap if segments connect
        TubeCap end_cap = generate_ribbon_vertices(segment, geometry, quant_params_,
                                                   can_share ? prev_end_cap : std::nullopt);

        // Store for next iteration
        prev_end_cap = end_cap;
        prev_end_pos = segment.end;
    }

    // Add END cap for the final segment (closes the last tube end)
    if (prev_end_cap.has_value()) {
        const auto& end_cap = prev_end_cap.value();
        // End cap faces forward (blocks view into tube from ahead)
        // Triangle strip: BL, TL, BR, TR creates two triangles forming a square
        geometry.strips.push_back({end_cap[0], end_cap[5], end_cap[2], end_cap[4]});

        // Update triangle count
        if (!simplified.empty() && simplified.back().is_extrusion) {
            geometry.extrusion_triangle_count += 2;
        } else {
            geometry.travel_triangle_count += 2;
        }
    }

    spdlog::debug("Segment Y range: [{:.1f}, {:.1f}]", seg_y_min, seg_y_max);

    // Store quantization parameters for dequantization during rendering
    geometry.quantization = quant_params_;

    // Update final statistics
    stats_.vertices_generated = geometry.vertices.size();
    stats_.triangles_generated = geometry.indices.size();
    stats_.memory_bytes = geometry.memory_usage();

    // Log palette statistics
    spdlog::info("Palette stats: {} normals, {} colors (smooth_shading={})",
                 geometry.normal_palette.size(), geometry.color_palette.size(),
                 use_smooth_shading_);

    if (segments_skipped > 0) {
        spdlog::warn("Skipped {} degenerate segments (zero length)", segments_skipped);
    }

    stats_.log();

    return geometry;
}

// ============================================================================
// Segment Simplification
// ============================================================================

std::vector<ToolpathSegment>
GeometryBuilder::simplify_segments(const std::vector<ToolpathSegment>& segments,
                                   const SimplificationOptions& options) {
    if (segments.empty()) {
        return {};
    }

    std::vector<ToolpathSegment> simplified;
    simplified.reserve(segments.size()); // Upper bound

    // Start with first segment
    ToolpathSegment current = segments[0];

    for (size_t i = 1; i < segments.size(); ++i) {
        const auto& next = segments[i];

        // Can only merge segments if:
        // 1. Same move type (both extrusion or both travel)
        // 2. Endpoints connect (current.end ≈ next.start)
        // 3. Same object (for per-object highlighting)
        // 4. Collinear within tolerance

        bool same_type = (current.is_extrusion == next.is_extrusion);
        bool endpoints_connect = glm::distance2(current.end, next.start) < 0.0001f;
        bool same_object = (current.object_name == next.object_name);

        if (same_type && endpoints_connect && same_object) {
            // Check if current.start, current.end, next.end are collinear
            bool collinear =
                are_collinear(current.start, current.end, next.end, options.tolerance_mm);

            if (collinear) {
                // Merge: extend current segment to end at next.end
                current.end = next.end;
                current.extrusion_amount += next.extrusion_amount;
                continue; // Skip adding next to simplified list
            }
        }

        // Cannot merge - save current and start new segment
        simplified.push_back(current);
        current = next;
    }

    // Add final segment
    simplified.push_back(current);

    return simplified;
}

bool GeometryBuilder::are_collinear(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
                                    float tolerance) const {
    // Vector from p1 to p2
    glm::vec3 v1 = p2 - p1;

    // Vector from p1 to p3
    glm::vec3 v2 = p3 - p1;

    // If either vector is nearly zero-length, points are effectively same point
    float len1_sq = glm::length2(v1);
    float len2_sq = glm::length2(v2);

    if (len1_sq < 1e-8f || len2_sq < 1e-8f) {
        return true; // Degenerate case - treat as collinear
    }

    // Cross product gives vector perpendicular to both v1 and v2
    // If v1 and v2 are collinear, cross product magnitude will be zero
    glm::vec3 cross = glm::cross(v1, v2);
    float cross_mag = glm::length(cross);

    // Distance from p3 to line defined by p1-p2 is:
    // distance = |cross(v1, v2)| / |v1|
    float distance = cross_mag / std::sqrt(len1_sq);

    return distance <= tolerance;
}

// ============================================================================
// Ribbon Geometry Generation
// ============================================================================

GeometryBuilder::TubeCap
GeometryBuilder::generate_ribbon_vertices(const ToolpathSegment& segment, RibbonGeometry& geometry,
                                          const QuantizationParams& quant,
                                          std::optional<TubeCap> prev_start_cap) {
    // Determine tube dimensions based on move type
    // Use per-segment width if available AND reasonable, otherwise use metadata default
    float width;
    if (segment.is_extrusion && segment.width >= 0.1f && segment.width <= 2.0f) {
        width = segment.width; // Use calculated width from E-delta (if in reasonable range)
    } else {
        // Use metadata-based default width (from slicer comments)
        width = segment.is_extrusion ? extrusion_width_mm_ : travel_width_mm_;
    }
    float half_width = width * 0.5f;
    float half_height = width * 0.5f; // Square cross-section

    // Calculate direction vector
    glm::vec3 direction = glm::normalize(segment.end - segment.start);

    // Calculate TWO perpendicular vectors for rectangular cross-section
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 perp_horizontal = glm::cross(direction, up);

    if (glm::length2(perp_horizontal) < 1e-6f) {
        // Direction is vertical - use X-axis as horizontal perpendicular
        perp_horizontal = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        perp_horizontal = glm::normalize(perp_horizontal);
    }

    // Second perpendicular: cross(direction, perp_horizontal) gives vertical component
    glm::vec3 perp_vertical = glm::normalize(glm::cross(direction, perp_horizontal));

    // Compute colors based on Z-height (rainbow gradient)
    float mid_z = (segment.start.z + segment.end.z) * 0.5f;
    uint32_t rgb = compute_color_rgb(mid_z, quant.min_bounds.z, quant.max_bounds.z);

    // Brighten color if this segment belongs to any highlighted object
    if (!highlighted_objects_.empty() && !segment.object_name.empty() &&
        highlighted_objects_.count(segment.object_name) > 0) {
        constexpr float HIGHLIGHT_BRIGHTNESS = 1.8f;
        uint8_t r =
            static_cast<uint8_t>(std::min(255.0f, ((rgb >> 16) & 0xFF) * HIGHLIGHT_BRIGHTNESS));
        uint8_t g =
            static_cast<uint8_t>(std::min(255.0f, ((rgb >> 8) & 0xFF) * HIGHLIGHT_BRIGHTNESS));
        uint8_t b = static_cast<uint8_t>(std::min(255.0f, (rgb & 0xFF) * HIGHLIGHT_BRIGHTNESS));
        rgb = (r << 16) | (g << 8) | b;
    }

    // End cross-section (always need to generate these)
    glm::vec3 end_bl = segment.end - perp_horizontal * half_width - perp_vertical * half_height;
    glm::vec3 end_br = segment.end + perp_horizontal * half_width - perp_vertical * half_height;
    glm::vec3 end_tr = segment.end + perp_horizontal * half_width + perp_vertical * half_height;
    glm::vec3 end_tl = segment.end - perp_horizontal * half_width + perp_vertical * half_height;

    uint32_t idx_start = geometry.vertices.size();

    // Compute normals for each face
    glm::vec3 normal_bottom = -perp_vertical;
    glm::vec3 normal_right = perp_horizontal;
    glm::vec3 normal_top = perp_vertical;
    glm::vec3 normal_left = -perp_horizontal;

    // Compute normals based on shading mode (stored as glm::vec3 for palette)
    // Vertex order: [bl_bottom, br_bottom, br_right, tr_right, tr_top, tl_top, tl_left, bl_left]
    std::array<glm::vec3, 8> vertex_normals;

    if (use_smooth_shading_) {
        // Smooth shading: average adjacent face normals at each corner
        glm::vec3 corner_bl = glm::normalize(normal_bottom + normal_left);  // Bottom-left
        glm::vec3 corner_br = glm::normalize(normal_bottom + normal_right); // Bottom-right
        glm::vec3 corner_tr = glm::normalize(normal_top + normal_right);    // Top-right
        glm::vec3 corner_tl = glm::normalize(normal_top + normal_left);     // Top-left

        vertex_normals[0] = corner_bl; // bl_bottom uses BL corner
        vertex_normals[1] = corner_br; // br_bottom uses BR corner
        vertex_normals[2] = corner_br; // br_right uses BR corner
        vertex_normals[3] = corner_tr; // tr_right uses TR corner
        vertex_normals[4] = corner_tr; // tr_top uses TR corner
        vertex_normals[5] = corner_tl; // tl_top uses TL corner
        vertex_normals[6] = corner_tl; // tl_left uses TL corner
        vertex_normals[7] = corner_bl; // bl_left uses BL corner
    } else {
        // Flat shading: per-face normals
        vertex_normals[0] = normal_bottom; // bl_bottom
        vertex_normals[1] = normal_bottom; // br_bottom
        vertex_normals[2] = normal_right;  // br_right
        vertex_normals[3] = normal_right;  // tr_right
        vertex_normals[4] = normal_top;    // tr_top
        vertex_normals[5] = normal_top;    // tl_top
        vertex_normals[6] = normal_left;   // tl_left
        vertex_normals[7] = normal_left;   // bl_left
    }

    // Add color to palette and get index (computed once per segment)
    uint8_t color_idx = add_to_color_palette(geometry, rgb);

    // Start cap indices: reuse previous segment's end cap if provided, else create new
    // TubeCap order: [bl_bottom, br_bottom, br_right, tr_right, tr_top, tl_top, tl_left, bl_left]
    TubeCap start_cap;

    if (prev_start_cap.has_value()) {
        // OPTIMIZATION: Reuse previous segment's end vertices (50% vertex count reduction!)
        start_cap = prev_start_cap.value();
    } else {
        // Generate new start vertices (first segment or discontinuity)
        glm::vec3 start_bl =
            segment.start - perp_horizontal * half_width - perp_vertical * half_height;
        glm::vec3 start_br =
            segment.start + perp_horizontal * half_width - perp_vertical * half_height;
        glm::vec3 start_tr =
            segment.start + perp_horizontal * half_width + perp_vertical * half_height;
        glm::vec3 start_tl =
            segment.start - perp_horizontal * half_width + perp_vertical * half_height;

        // Add vertices with palette indices
        start_cap[0] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_bl),
                                     add_to_normal_palette(geometry, vertex_normals[0]),
                                     color_idx});

        start_cap[1] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_br),
                                     add_to_normal_palette(geometry, vertex_normals[1]),
                                     color_idx});

        start_cap[2] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_br),
                                     add_to_normal_palette(geometry, vertex_normals[2]),
                                     color_idx});

        start_cap[3] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_tr),
                                     add_to_normal_palette(geometry, vertex_normals[3]),
                                     color_idx});

        start_cap[4] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_tr),
                                     add_to_normal_palette(geometry, vertex_normals[4]),
                                     color_idx});

        start_cap[5] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_tl),
                                     add_to_normal_palette(geometry, vertex_normals[5]),
                                     color_idx});

        start_cap[6] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_tl),
                                     add_to_normal_palette(geometry, vertex_normals[6]),
                                     color_idx});

        start_cap[7] = idx_start++;
        geometry.vertices.push_back({quant.quantize_vec3(start_bl),
                                     add_to_normal_palette(geometry, vertex_normals[7]),
                                     color_idx});
    }

    // Always generate end cap vertices
    TubeCap end_cap;

    end_cap[0] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_bl),
                                 add_to_normal_palette(geometry, vertex_normals[0]), color_idx});

    end_cap[1] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_br),
                                 add_to_normal_palette(geometry, vertex_normals[1]), color_idx});

    end_cap[2] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_br),
                                 add_to_normal_palette(geometry, vertex_normals[2]), color_idx});

    end_cap[3] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_tr),
                                 add_to_normal_palette(geometry, vertex_normals[3]), color_idx});

    end_cap[4] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_tr),
                                 add_to_normal_palette(geometry, vertex_normals[4]), color_idx});

    end_cap[5] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_tl),
                                 add_to_normal_palette(geometry, vertex_normals[5]), color_idx});

    end_cap[6] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_tl),
                                 add_to_normal_palette(geometry, vertex_normals[6]), color_idx});

    end_cap[7] = idx_start++;
    geometry.vertices.push_back({quant.quantize_vec3(end_bl),
                                 add_to_normal_palette(geometry, vertex_normals[7]), color_idx});

    // Generate triangle strips (4 strips, one per face, 2 triangles each)
    // Each strip uses 4 indices instead of 6 (33% reduction!)
    // Strip winding: [v0, v1, v2, v3] → Triangle1(v0,v1,v2) + Triangle2(v1,v3,v2)

    // Bottom face strip: [start_BL, end_BL, start_BR, end_BR]
    geometry.strips.push_back({start_cap[0], end_cap[0], start_cap[1], end_cap[1]});

    // Right face strip: [start_BR, end_BR, start_TR, end_TR]
    geometry.strips.push_back({start_cap[2], end_cap[2], start_cap[3], end_cap[3]});

    // Top face strip: [start_TR, end_TR, start_TL, end_TL]
    geometry.strips.push_back({start_cap[4], end_cap[4], start_cap[5], end_cap[5]});

    // Left face strip: [start_TL, end_TL, start_BL, end_BL]
    geometry.strips.push_back({start_cap[6], end_cap[6], start_cap[7], end_cap[7]});

    // Track triangle count
    int triangle_count = 8; // 4 side faces × 2 triangles each

    // Add START cap if this segment doesn't connect to previous (new vertices were created)
    if (!prev_start_cap.has_value()) {
        // Start cap faces backward (blocks view into tube from behind)
        // Triangle strip: BL, BR, TL, TR creates two triangles forming a square
        geometry.strips.push_back({start_cap[7], start_cap[1], start_cap[5], start_cap[3]});
        triangle_count += 2;
    }

    // Update counters
    if (segment.is_extrusion) {
        geometry.extrusion_triangle_count += triangle_count;
    } else {
        geometry.travel_triangle_count += triangle_count;
    }

    // Return end cap for next segment to reuse
    return end_cap;
}

glm::vec3 GeometryBuilder::compute_perpendicular(const glm::vec3& direction, float width) const {
    // Define "up" vector (Z-axis)
    glm::vec3 up(0.0f, 0.0f, 1.0f);

    // Compute perpendicular in XY plane
    // perpendicular = cross(direction, up)
    glm::vec3 perp = glm::cross(direction, up);

    // If direction is vertical (parallel to up), cross product will be zero
    // Fall back to using X-axis as perpendicular
    if (glm::length2(perp) < 1e-6f) {
        perp = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        perp = glm::normalize(perp);
    }

    return perp * width;
}

uint32_t GeometryBuilder::compute_color_rgb(float z_height, float z_min, float z_max) const {
    if (!use_height_gradient_) {
        // Use solid filament color
        uint32_t color = (static_cast<uint32_t>(filament_r_) << 16) |
                         (static_cast<uint32_t>(filament_g_) << 8) |
                         static_cast<uint32_t>(filament_b_);
        static bool logged_once = false;
        if (!logged_once) {
            spdlog::debug("compute_color_rgb: R={}, G={}, B={} -> 0x{:06X}", filament_r_,
                          filament_g_, filament_b_, color);
            logged_once = true;
        }
        return color;
    }

    // Rainbow gradient from blue (bottom) to red (top)
    // Normalize Z to [0, 1]
    float range = z_max - z_min;
    float t = (range > 0.0f) ? (z_height - z_min) / range : 0.5f;
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to [0, 1]

    // Rainbow spectrum: Blue → Cyan → Green → Yellow → Red
    // Using HSV color space converted to RGB
    float hue = (1.0f - t) * 240.0f; // 240° (blue) to 0° (red)

    // Simple HSV to RGB conversion (assuming S=1.0, V=1.0)
    float c = 1.0f; // Chroma (full saturation)
    float h_prime = hue / 60.0f;
    float x = c * (1.0f - std::abs(std::fmod(h_prime, 2.0f) - 1.0f));

    float r, g, b;
    if (h_prime < 1.0f) {
        r = c;
        g = x;
        b = 0.0f;
    } else if (h_prime < 2.0f) {
        r = x;
        g = c;
        b = 0.0f;
    } else if (h_prime < 3.0f) {
        r = 0.0f;
        g = c;
        b = x;
    } else if (h_prime < 4.0f) {
        r = 0.0f;
        g = x;
        b = c;
    } else if (h_prime < 5.0f) {
        r = x;
        g = 0.0f;
        b = c;
    } else {
        r = c;
        g = 0.0f;
        b = x;
    }

    uint8_t r8 = static_cast<uint8_t>(r * 255.0f);
    uint8_t g8 = static_cast<uint8_t>(g * 255.0f);
    uint8_t b8 = static_cast<uint8_t>(b * 255.0f);

    return (static_cast<uint32_t>(r8) << 16) | (static_cast<uint32_t>(g8) << 8) |
           static_cast<uint32_t>(b8);
}

void GeometryBuilder::set_filament_color(const std::string& hex_color) {
    use_height_gradient_ = false; // Disable gradient

    // Remove '#' prefix if present
    const char* hex_str = hex_color.c_str();
    if (hex_str[0] == '#')
        hex_str++;

    // Parse RGB hex (e.g., "26A69A")
    uint32_t rgb = std::strtol(hex_str, nullptr, 16);
    filament_r_ = (rgb >> 16) & 0xFF;
    filament_g_ = (rgb >> 8) & 0xFF;
    filament_b_ = rgb & 0xFF;

    spdlog::info("Filament color set to #{:02X}{:02X}{:02X} (R={}, G={}, B={})", filament_r_,
                 filament_g_, filament_b_, filament_r_, filament_g_, filament_b_);
}

} // namespace gcode
