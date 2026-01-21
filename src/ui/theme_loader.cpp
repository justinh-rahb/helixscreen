// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme_loader.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "hv/json.hpp"

namespace helix {

const std::array<const char*, 16>& ThemePalette::color_names() {
    static const std::array<const char*, 16> names = {
        "bg_darkest",     "bg_dark",          "bg_dark_highlight", "border_muted",
        "text_light",     "bg_light",         "bg_lightest",       "accent_highlight",
        "accent_primary", "accent_secondary", "accent_tertiary",   "status_error",
        "status_danger",  "status_warning",   "status_success",    "status_special"};
    return names;
}

const std::string& ThemePalette::at(size_t index) const {
    switch (index) {
    case 0:
        return bg_darkest;
    case 1:
        return bg_dark;
    case 2:
        return bg_dark_highlight;
    case 3:
        return border_muted;
    case 4:
        return text_light;
    case 5:
        return bg_light;
    case 6:
        return bg_lightest;
    case 7:
        return accent_highlight;
    case 8:
        return accent_primary;
    case 9:
        return accent_secondary;
    case 10:
        return accent_tertiary;
    case 11:
        return status_error;
    case 12:
        return status_danger;
    case 13:
        return status_warning;
    case 14:
        return status_success;
    case 15:
        return status_special;
    default:
        throw std::out_of_range("ThemePalette index out of range");
    }
}

std::string& ThemePalette::at(size_t index) {
    return const_cast<std::string&>(static_cast<const ThemePalette*>(this)->at(index));
}

bool ThemeData::is_valid() const {
    // Check all colors are non-empty and start with #
    for (size_t i = 0; i < 16; ++i) {
        const auto& color = colors.at(i);
        if (color.empty() || color[0] != '#' || color.size() != 7) {
            return false;
        }
    }
    return !name.empty();
}

ThemeData get_default_nord_theme() {
    ThemeData theme;
    theme.name = "Nord";
    theme.filename = "nord";

    theme.colors.bg_darkest = "#2e3440";
    theme.colors.bg_dark = "#3b4252";
    theme.colors.bg_dark_highlight = "#434c5e";
    theme.colors.border_muted = "#4c566a";
    theme.colors.text_light = "#d8dee9";
    theme.colors.bg_light = "#e5e9f0";
    theme.colors.bg_lightest = "#eceff4";
    theme.colors.accent_highlight = "#8fbcbb";
    theme.colors.accent_primary = "#88c0d0";
    theme.colors.accent_secondary = "#81a1c1";
    theme.colors.accent_tertiary = "#5e81ac";
    theme.colors.status_error = "#bf616a";
    theme.colors.status_danger = "#d08770";
    theme.colors.status_warning = "#ebcb8b";
    theme.colors.status_success = "#a3be8c";
    theme.colors.status_special = "#b48ead";

    theme.properties.border_radius = 12;
    theme.properties.border_width = 1;
    theme.properties.border_opacity = 40;
    theme.properties.shadow_intensity = 0;

    return theme;
}

ThemeData parse_theme_json(const std::string& json_str, const std::string& filename) {
    ThemeData theme;
    theme.filename = filename;

    // Remove .json extension if present
    if (theme.filename.size() > 5 && theme.filename.substr(theme.filename.size() - 5) == ".json") {
        theme.filename = theme.filename.substr(0, theme.filename.size() - 5);
    }

    try {
        auto json = nlohmann::json::parse(json_str);

        theme.name = json.value("name", "Unnamed Theme");

        // Parse colors
        if (json.contains("colors")) {
            auto& colors = json["colors"];
            auto& names = ThemePalette::color_names();
            auto defaults = get_default_nord_theme();

            for (size_t i = 0; i < 16; ++i) {
                const char* name = names[i];
                if (colors.contains(name)) {
                    theme.colors.at(i) = colors[name].get<std::string>();
                } else {
                    // Fall back to Nord default
                    theme.colors.at(i) = defaults.colors.at(i);
                    spdlog::warn("[ThemeLoader] Missing color '{}' in {}, using Nord default", name,
                                 filename);
                }
            }
        } else {
            spdlog::error("[ThemeLoader] No 'colors' object in {}", filename);
            return get_default_nord_theme();
        }

        // Parse properties with defaults
        theme.properties.border_radius = json.value("border_radius", 12);
        theme.properties.border_width = json.value("border_width", 1);
        theme.properties.border_opacity = json.value("border_opacity", 40);
        theme.properties.shadow_intensity = json.value("shadow_intensity", 0);

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("[ThemeLoader] Failed to parse {}: {}", filename, e.what());
        return get_default_nord_theme();
    }

    return theme;
}

ThemeData load_theme_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("[ThemeLoader] Failed to open {}", filepath);
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    // Extract filename from path
    std::string filename = filepath;
    size_t slash = filepath.rfind('/');
    if (slash != std::string::npos) {
        filename = filepath.substr(slash + 1);
    }

    return parse_theme_json(buffer.str(), filename);
}

bool save_theme_to_file(const ThemeData& theme, const std::string& filepath) {
    nlohmann::json json;

    json["name"] = theme.name;

    // Build colors object
    nlohmann::json colors;
    auto& names = ThemePalette::color_names();
    for (size_t i = 0; i < 16; ++i) {
        colors[names[i]] = theme.colors.at(i);
    }
    json["colors"] = colors;

    // Properties
    json["border_radius"] = theme.properties.border_radius;
    json["border_width"] = theme.properties.border_width;
    json["border_opacity"] = theme.properties.border_opacity;
    json["shadow_intensity"] = theme.properties.shadow_intensity;

    // Write with pretty formatting
    std::ofstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("[ThemeLoader] Failed to write {}", filepath);
        return false;
    }

    file << json.dump(2);
    return true;
}

} // namespace helix
