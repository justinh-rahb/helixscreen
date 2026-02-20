// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "prerendered_images.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>

namespace helix {

bool prerendered_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

const char* get_splash_size_name(int screen_width) {
    if (screen_width < 600) {
        return "tiny"; // 480x320 class
    } else if (screen_width < 900) {
        return "small"; // 800x480 class (AD5M)
    } else if (screen_width < 1100) {
        return "medium"; // 1024x600 class
    } else {
        return "large"; // 1280x720+ class
    }
}

const char* get_splash_3d_size_name(int screen_width, int screen_height) {
    // Ultra-wide displays (e.g. 1920x440): wide but very short
    if (screen_width >= 1100 && screen_height < 500) {
        return "ultrawide";
    }

    if (screen_width < 600) {
        // Distinguish K1 (480x400) from generic tiny (480x320)
        return (screen_height >= 380) ? "tiny_alt" : "tiny";
    } else if (screen_width < 900) {
        return "small"; // 800x480 class (AD5M)
    } else if (screen_width < 1100) {
        return "medium"; // 1024x600 class
    } else {
        return "large"; // 1280x720+ class
    }
}

int get_splash_3d_target_height(const char* size_name) {
    // Known heights for pre-rendered splash images (from gen_splash_3d.py SCREEN_SIZES)
    if (strcmp(size_name, "tiny") == 0)
        return 320;
    if (strcmp(size_name, "tiny_alt") == 0)
        return 400;
    if (strcmp(size_name, "small") == 0)
        return 480;
    if (strcmp(size_name, "medium") == 0)
        return 600;
    if (strcmp(size_name, "large") == 0)
        return 720;
    if (strcmp(size_name, "ultrawide") == 0)
        return 440;
    return 0; // Unknown — caller should fall back to runtime scaling
}

std::string get_prerendered_splash_3d_path(int screen_width, int screen_height, bool dark_mode) {
    const char* size_name = get_splash_3d_size_name(screen_width, screen_height);
    const char* mode_name = dark_mode ? "dark" : "light";

    // Path relative to install directory
    std::string path = "assets/images/prerendered/splash-3d-";
    path += mode_name;
    path += "-";
    path += size_name;
    path += ".bin";

    if (prerendered_exists(path)) {
        spdlog::debug("[Prerendered] Using 3D splash: {}", path);
        return "A:" + path;
    }

    // Fallback: try base "tiny" if tiny_alt not found (backward compat)
    if (std::string(size_name) == "tiny_alt") {
        path = "assets/images/prerendered/splash-3d-";
        path += mode_name;
        path += "-tiny.bin";
        if (prerendered_exists(path)) {
            spdlog::debug("[Prerendered] Using 3D splash (tiny fallback): {}", path);
            return "A:" + path;
        }
    }

    spdlog::debug("[Prerendered] 3D splash not found for {} {} ({}x{}), falling back", mode_name,
                  size_name, screen_width, screen_height);
    return "";
}

std::string get_prerendered_splash_path(int screen_width) {
    const char* size_name = get_splash_size_name(screen_width);

    // Path relative to install directory
    std::string path = "assets/images/prerendered/splash-logo-";
    path += size_name;
    path += ".bin";

    if (prerendered_exists(path)) {
        spdlog::debug("[Prerendered] Using splash: {}", path);
        return "A:" + path;
    }

    spdlog::debug("[Prerendered] Splash fallback to PNG ({}px screen)", screen_width);
    return "A:assets/images/helixscreen-logo.png";
}

int get_printer_image_size(int screen_width) {
    // 300px for medium-large displays (800x480+)
    // 150px for small displays (480x320)
    return (screen_width >= 600) ? 300 : 150;
}

std::string get_prerendered_printer_path(const std::string& printer_name, int screen_width) {
    int size = get_printer_image_size(screen_width);

    // Path relative to install directory
    std::string path = "assets/images/printers/prerendered/";
    path += printer_name;
    path += "-";
    path += std::to_string(size);
    path += ".bin";

    if (prerendered_exists(path)) {
        spdlog::debug("[Prerendered] Using printer image: {}", path);
        return "A:" + path;
    }

    // Fall back to original PNG, but verify it exists
    std::string png_path = "assets/images/printers/" + printer_name + ".png";
    if (prerendered_exists(png_path)) {
        spdlog::trace("[Prerendered] Printer {} fallback to PNG (no {}px)", printer_name, size);
        return "A:" + png_path;
    }

    // Neither prerendered nor PNG exists — fall back to generic
    spdlog::debug("[Prerendered] Printer {} has no image, using generic fallback", printer_name);
    std::string generic_bin =
        "assets/images/printers/prerendered/generic-corexy-" + std::to_string(size) + ".bin";
    if (prerendered_exists(generic_bin)) {
        return "A:" + generic_bin;
    }
    return "A:assets/images/printers/generic-corexy.png";
}

std::string get_prerendered_placeholder_path(const std::string& placeholder_name) {
    // Path relative to install directory
    std::string bin_path = "assets/images/prerendered/";
    bin_path += placeholder_name;
    bin_path += ".bin";

    if (prerendered_exists(bin_path)) {
        spdlog::debug("[Prerendered] Using placeholder: {}", bin_path);
        return "A:" + bin_path;
    }

    // Fallback to original PNG
    std::string png_path = "assets/images/" + placeholder_name + ".png";
    spdlog::trace("[Prerendered] Placeholder fallback to PNG: {}", png_path);
    return "A:" + png_path;
}

} // namespace helix
