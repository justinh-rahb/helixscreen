// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file bed_mesh_buffer.cpp
 * @brief ARGB8888 pixel buffer with drawing primitives for off-screen rendering
 *
 * All drawing operations use BGRA byte order to match LVGL's ARGB8888 format.
 * Alpha blending uses the standard formula:
 *   result = (src * src_alpha + dst * (255 - src_alpha)) / 255
 */

#include "bed_mesh_buffer.h"

#include <algorithm>
#include <cstring>

namespace helix {
namespace mesh {

// ============================================================================
// Construction
// ============================================================================

PixelBuffer::PixelBuffer(int width, int height)
    : width_(std::max(0, width)), height_(std::max(0, height)),
      data_(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4, 0) {}

// ============================================================================
// Pixel access
// ============================================================================

uint8_t* PixelBuffer::pixel_at(int x, int y) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return nullptr;
    }
    return data_.data() + (static_cast<size_t>(y) * width_ + x) * 4;
}

const uint8_t* PixelBuffer::pixel_at(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return nullptr;
    }
    return data_.data() + (static_cast<size_t>(y) * width_ + x) * 4;
}

// ============================================================================
// Clear
// ============================================================================

void PixelBuffer::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (data_.empty()) {
        return;
    }

    // Write BGRA pattern to first pixel, then replicate
    uint8_t pattern[4] = {b, g, r, a};

    if (pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3]) {
        // All bytes the same -- memset is fastest
        std::memset(data_.data(), pattern[0], data_.size());
    } else {
        // Write pattern to each pixel
        auto* dst = data_.data();
        const auto total_pixels = static_cast<size_t>(width_) * height_;
        for (size_t i = 0; i < total_pixels; i++) {
            dst[0] = pattern[0];
            dst[1] = pattern[1];
            dst[2] = pattern[2];
            dst[3] = pattern[3];
            dst += 4;
        }
    }
}

// ============================================================================
// Alpha blending
// ============================================================================

void PixelBuffer::blend_pixel(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 255) {
        // Fully opaque: direct write, skip blending
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        dst[3] = a;
        return;
    }

    // Standard alpha blend: result = (src * alpha + dst * (255 - alpha)) / 255
    const uint16_t inv_a = 255 - a;
    dst[0] = static_cast<uint8_t>((b * a + dst[0] * inv_a) / 255);
    dst[1] = static_cast<uint8_t>((g * a + dst[1] * inv_a) / 255);
    dst[2] = static_cast<uint8_t>((r * a + dst[2] * inv_a) / 255);
    // Keep destination alpha (compositing onto an existing surface)
}

// ============================================================================
// set_pixel
// ============================================================================

void PixelBuffer::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 0) {
        return;
    }
    auto* p = pixel_at(x, y);
    if (p == nullptr) {
        return;
    }
    blend_pixel(p, r, g, b, a);
}

// ============================================================================
// fill_hline
// ============================================================================

void PixelBuffer::fill_hline(int x, int w, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (a == 0 || w <= 0 || y < 0 || y >= height_) {
        return;
    }

    // Clamp X range to buffer bounds
    int x_end = x + w;
    if (x < 0) {
        x = 0;
    }
    if (x_end > width_) {
        x_end = width_;
    }
    if (x >= x_end) {
        return;
    }

    auto* dst = pixel_at(x, y);
    if (a == 255) {
        // Fully opaque fast path
        for (int px = x; px < x_end; px++) {
            dst[0] = b;
            dst[1] = g;
            dst[2] = r;
            dst[3] = a;
            dst += 4;
        }
    } else {
        const uint16_t inv_a = 255 - a;
        for (int px = x; px < x_end; px++) {
            dst[0] = static_cast<uint8_t>((b * a + dst[0] * inv_a) / 255);
            dst[1] = static_cast<uint8_t>((g * a + dst[1] * inv_a) / 255);
            dst[2] = static_cast<uint8_t>((r * a + dst[2] * inv_a) / 255);
            dst += 4;
        }
    }
}

// ============================================================================
// draw_line (Bresenham's algorithm)
// ============================================================================

void PixelBuffer::draw_line(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
                            uint8_t a, int thickness) {
    if (a == 0 || width_ == 0 || height_ == 0) {
        return;
    }

    if (thickness <= 1) {
        // Standard Bresenham
        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;

        for (;;) {
            set_pixel(x0, y0, r, g, b, a);

            if (x0 == x1 && y0 == y1) {
                break;
            }

            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    } else {
        // Thick line: draw the center line, then offset lines perpendicular
        // For simplicity, expand by drawing parallel lines offset in the
        // perpendicular direction. For mostly-horizontal lines, offset in Y;
        // for mostly-vertical lines, offset in X.
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int half = (thickness - 1) / 2;

        if (dx >= dy) {
            // Mostly horizontal: expand in Y direction
            for (int offset = -half; offset <= half; offset++) {
                draw_line(x0, y0 + offset, x1, y1 + offset, r, g, b, a, 1);
            }
        } else {
            // Mostly vertical: expand in X direction
            for (int offset = -half; offset <= half; offset++) {
                draw_line(x0 + offset, y0, x1 + offset, y1, r, g, b, a, 1);
            }
        }
    }
}

} // namespace mesh
} // namespace helix
