// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_utils.h"

#include "ui_theme.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>

// ============================================================================
// Filename Utilities
// ============================================================================

std::string get_filename_basename(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    // Find last path separator
    size_t last_sep = path.find_last_of("/\\");
    if (last_sep == std::string::npos) {
        return path; // No separator, already just a filename
    }

    return path.substr(last_sep + 1);
}

std::string strip_gcode_extension(const std::string& filename) {
    // Common G-code extensions (case-insensitive check)
    static const std::vector<std::string> extensions = {".gcode", ".GCODE", ".Gcode", ".gco",
                                                        ".GCO",   ".Gco",   ".g",     ".G"};

    for (const auto& ext : extensions) {
        if (filename.size() > ext.size()) {
            size_t pos = filename.size() - ext.size();
            if (filename.compare(pos, ext.size(), ext) == 0) {
                return filename.substr(0, pos);
            }
        }
    }

    return filename;
}

std::string get_display_filename(const std::string& path) {
    return strip_gcode_extension(get_filename_basename(path));
}

std::string resolve_gcode_filename(const std::string& path) {
    // Pattern: .helix_temp/modified_123456789_OriginalName.gcode
    // Also handles: /tmp/helixscreen_mod_XXXXXX_filename.gcode
    static const std::string helix_temp_prefix = ".helix_temp/modified_";
    static const std::string local_temp_prefix = "/tmp/helixscreen_mod_";

    size_t underscore_pos = std::string::npos;

    if (path.find(helix_temp_prefix) != std::string::npos) {
        // Extract original: .helix_temp/modified_123456789_OriginalName.gcode -> OriginalName.gcode
        size_t prefix_end = path.find(helix_temp_prefix) + helix_temp_prefix.size();
        underscore_pos = path.find('_', prefix_end);
    } else if (path.find(local_temp_prefix) != std::string::npos) {
        // Extract original: /tmp/helixscreen_mod_123456_OriginalName.gcode -> OriginalName.gcode
        size_t prefix_end = path.find(local_temp_prefix) + local_temp_prefix.size();
        underscore_pos = path.find('_', prefix_end);
    }

    if (underscore_pos != std::string::npos && underscore_pos + 1 < path.size()) {
        std::string original = path.substr(underscore_pos + 1);
        spdlog::debug("[resolve_gcode_filename] '{}' -> '{}'", path, original);
        return original;
    }

    return path;
}

// ============================================================================
// Time Formatting
// ============================================================================

std::string format_print_time(int minutes) {
    char buf[32];
    if (minutes < 60) {
        snprintf(buf, sizeof(buf), "%dm", minutes);
    } else {
        int hours = minutes / 60;
        int mins = minutes % 60;
        if (mins == 0) {
            snprintf(buf, sizeof(buf), "%dh", hours);
        } else {
            snprintf(buf, sizeof(buf), "%dh%dm", hours, mins);
        }
    }
    return std::string(buf);
}

std::string format_filament_weight(float grams) {
    char buf[32];
    if (grams < 1.0f) {
        snprintf(buf, sizeof(buf), "%.1fg", grams);
    } else if (grams < 10.0f) {
        snprintf(buf, sizeof(buf), "%.1fg", grams);
    } else {
        snprintf(buf, sizeof(buf), "%.0fg", grams);
    }
    return std::string(buf);
}

std::string format_file_size(size_t bytes) {
    char buf[32];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        double kb = bytes / 1024.0;
        snprintf(buf, sizeof(buf), "%.1f KB", kb);
    } else if (bytes < 1024 * 1024 * 1024) {
        double mb = bytes / (1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "%.1f MB", mb);
    } else {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "%.2f GB", gb);
    }
    return std::string(buf);
}

std::string format_modified_date(time_t timestamp) {
    char buf[64];
    struct tm* timeinfo = localtime(&timestamp);
    if (timeinfo) {
        // Format: "Jan 15 14:30"
        strftime(buf, sizeof(buf), "%b %d %H:%M", timeinfo);
    } else {
        snprintf(buf, sizeof(buf), "Unknown");
    }
    return std::string(buf);
}

lv_coord_t ui_get_header_content_padding(lv_coord_t screen_height) {
    (void)screen_height; // Parameter kept for API stability

    // Use unified space_* system - values are already responsive based on breakpoint
    // set during theme initialization (space_lg = 12/16/20px at small/medium/large)
    int32_t spacing = ui_theme_get_spacing("space_lg");

    // Fallback if theme not initialized (e.g., in unit tests)
    constexpr int32_t DEFAULT_SPACE_LG = 16; // Medium breakpoint value
    if (spacing == 0) {
        spacing = DEFAULT_SPACE_LG;
    }

    return spacing;
}

lv_coord_t ui_get_responsive_header_height(lv_coord_t screen_height) {
    // Responsive header heights for space efficiency:
    // Large/Medium (≥600px): 60px (comfortable)
    // Small (480-599px): 48px (compact)
    // Tiny (≤479px): 40px (minimal)

    if (screen_height >= UI_SCREEN_MEDIUM_H) {
        return 60;
    } else if (screen_height >= UI_SCREEN_SMALL_H) {
        return 48;
    } else {
        return 40;
    }
}

// ============================================================================
// Color Utilities
// ============================================================================

std::optional<uint32_t> ui_parse_hex_color(const std::string& hex_str) {
    if (hex_str.empty()) {
        return std::nullopt;
    }

    std::string hex = hex_str;
    if (hex[0] == '#') {
        hex = hex.substr(1);
    }

    if (hex.length() != 6) {
        return std::nullopt;
    }

    try {
        return static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    } catch (...) {
        return std::nullopt;
    }
}

int ui_color_distance(uint32_t color1, uint32_t color2) {
    int r1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int b1 = color1 & 0xFF;

    int r2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int b2 = color2 & 0xFF;

    // Weighted distance - green is most perceptible to human eye
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;

    // Weights: R=0.30, G=0.59, B=0.11 (standard luminance)
    // Squared for distance calculation, then sqrt
    int dist_sq = (dr * dr * 30 + dg * dg * 59 + db * db * 11) / 100;
    return static_cast<int>(std::sqrt(static_cast<double>(dist_sq)));
}

// ============================================================================
// App-level Resize Handling
// ============================================================================

// Storage for registered resize callbacks
static std::vector<ui_resize_callback_t> resize_callbacks;

// Debounce timer and period
static lv_timer_t* resize_debounce_timer = nullptr;
static constexpr uint32_t RESIZE_DEBOUNCE_MS = 250;

// Debounce timer callback - invokes all registered callbacks
static void resize_timer_cb(lv_timer_t* timer) {
    (void)timer;

    spdlog::debug("[UI Utils] Resize debounce complete, calling {} registered callbacks",
                  resize_callbacks.size());

    // Call all registered callbacks
    for (auto callback : resize_callbacks) {
        if (callback) {
            callback();
        }
    }

    // Delete one-shot timer
    if (resize_debounce_timer) {
        lv_timer_delete(resize_debounce_timer);
        resize_debounce_timer = nullptr;
    }
}

// Screen resize event handler - starts/resets debounce timer
static void resize_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SIZE_CHANGED) {
        lv_obj_t* screen = (lv_obj_t*)lv_event_get_target(e);
        lv_coord_t width = lv_obj_get_width(screen);
        lv_coord_t height = lv_obj_get_height(screen);

        spdlog::debug("[UI Utils] Screen size changed to {}x{}, resetting debounce timer", width,
                      height);

        // Reset or create debounce timer
        if (resize_debounce_timer) {
            lv_timer_reset(resize_debounce_timer);
        } else {
            resize_debounce_timer = lv_timer_create(resize_timer_cb, RESIZE_DEBOUNCE_MS, nullptr);
            lv_timer_set_repeat_count(resize_debounce_timer, 1); // One-shot
        }
    }
}

void ui_resize_handler_init(lv_obj_t* screen) {
    if (!screen) {
        spdlog::error("[UI Utils] Cannot init resize handler: screen is null");
        return;
    }

    // Add SIZE_CHANGED event listener to screen
    lv_obj_add_event_cb(screen, resize_event_cb, LV_EVENT_SIZE_CHANGED, nullptr);

    spdlog::debug("[UI Utils] Resize handler initialized on screen");
}

void ui_resize_handler_register(ui_resize_callback_t callback) {
    if (!callback) {
        spdlog::warn("[UI Utils] Attempted to register null resize callback");
        return;
    }

    resize_callbacks.push_back(callback);
    spdlog::debug("[UI Utils] Registered resize callback ({} total)", resize_callbacks.size());
}

// ============================================================================
// Image Scaling Utilities
// ============================================================================

bool ui_image_scale_to_cover(lv_obj_t* image_widget, lv_coord_t target_width,
                             lv_coord_t target_height) {
    if (!image_widget) {
        spdlog::error("[UI Utils] Cannot scale image: widget is null");
        return false;
    }

    // Get source image dimensions
    lv_image_header_t header;
    lv_result_t res = lv_image_decoder_get_info(lv_image_get_src(image_widget), &header);

    if (res != LV_RESULT_OK || header.w == 0 || header.h == 0) {
        int w = header.w, h = header.h; // Copy bitfields for formatting
        spdlog::warn("[UI Utils] Cannot get image info for scaling (res={}, w={}, h={})",
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
    spdlog::debug("[UI Utils] Image scale (cover): img={}x{}, target={}x{}, zoom={} ({:.1f}%)",
                  img_w, img_h, target_width, target_height, zoom, scale * 100);

    return true;
}

bool ui_image_scale_to_contain(lv_obj_t* image_widget, lv_coord_t target_width,
                               lv_coord_t target_height, lv_image_align_t align) {
    if (!image_widget) {
        spdlog::error("[UI Utils] Cannot scale image: widget is null");
        return false;
    }

    // Get source image dimensions
    lv_image_header_t header;
    lv_result_t res = lv_image_decoder_get_info(lv_image_get_src(image_widget), &header);

    if (res != LV_RESULT_OK || header.w == 0 || header.h == 0) {
        int w = header.w, h = header.h; // Copy bitfields for formatting
        spdlog::warn("[UI Utils] Cannot get image info for scaling (res={}, w={}, h={})",
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
    spdlog::debug("[UI Utils] Image scale (contain): img={}x{}, target={}x{}, zoom={} ({:.1f}%)",
                  img_w, img_h, target_width, target_height, zoom, scale * 100);

    return true;
}
