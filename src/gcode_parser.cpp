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

#include "gcode_parser.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace gcode {

// ============================================================================
// ParsedGCodeFile Methods
// ============================================================================

int ParsedGCodeFile::find_layer_at_z(float z) const {
    if (layers.empty()) {
        return -1;
    }

    // Binary search for closest Z height
    int left = 0;
    int right = static_cast<int>(layers.size()) - 1;
    int closest = 0;
    float min_diff = std::abs(layers[0].z_height - z);

    while (left <= right) {
        int mid = left + (right - left) / 2;
        float diff = std::abs(layers[mid].z_height - z);

        if (diff < min_diff) {
            min_diff = diff;
            closest = mid;
        }

        if (layers[mid].z_height < z) {
            left = mid + 1;
        } else if (layers[mid].z_height > z) {
            right = mid - 1;
        } else {
            return mid; // Exact match
        }
    }

    return closest;
}

// ============================================================================
// GCodeParser Implementation
// ============================================================================

GCodeParser::GCodeParser() {
    reset();
}

void GCodeParser::reset() {
    current_position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    current_e_ = 0.0f;
    current_object_.clear();
    is_absolute_positioning_ = true;
    is_absolute_extrusion_ = true;
    layers_.clear();
    objects_.clear();
    global_bounds_ = AABB();
    lines_parsed_ = 0;

    // Start with first layer at Z=0
    start_new_layer(0.0f);
}

void GCodeParser::parse_line(const std::string& line) {
    lines_parsed_++;

    // Extract and parse metadata comments before trimming
    size_t comment_pos = line.find(';');
    if (comment_pos != std::string::npos) {
        std::string comment = line.substr(comment_pos);
        parse_metadata_comment(comment);
    }

    std::string trimmed = trim_line(line);
    if (trimmed.empty()) {
        return;
    }

    // Check for EXCLUDE_OBJECT commands first
    if (trimmed.find("EXCLUDE_OBJECT") == 0) {
        parse_exclude_object_command(trimmed);
        return;
    }

    // Parse positioning mode commands
    if (trimmed == "G90") {
        is_absolute_positioning_ = true;
        return;
    } else if (trimmed == "G91") {
        is_absolute_positioning_ = false;
        return;
    } else if (trimmed == "M82") {
        is_absolute_extrusion_ = true;
        return;
    } else if (trimmed == "M83") {
        is_absolute_extrusion_ = false;
        return;
    }

    // Parse movement commands (G0, G1)
    if (trimmed[0] == 'G' && (trimmed.find("G0 ") == 0 || trimmed.find("G1 ") == 0 ||
                              trimmed == "G0" || trimmed == "G1")) {
        parse_movement_command(trimmed);
    }
}

bool GCodeParser::parse_movement_command(const std::string& line) {
    glm::vec3 new_position = current_position_;
    float new_e = current_e_;
    bool has_movement = false;
    bool has_extrusion = false;

    // Extract X, Y, Z parameters
    float value;
    if (extract_param(line, 'X', value)) {
        new_position.x = is_absolute_positioning_ ? value : current_position_.x + value;
        has_movement = true;
    }
    if (extract_param(line, 'Y', value)) {
        new_position.y = is_absolute_positioning_ ? value : current_position_.y + value;
        has_movement = true;
    }
    if (extract_param(line, 'Z', value)) {
        new_position.z = is_absolute_positioning_ ? value : current_position_.z + value;
        has_movement = true;

        // Layer change detected
        if (std::abs(new_position.z - current_position_.z) > 0.001f) {
            start_new_layer(new_position.z);
        }
    }

    // Extract E (extrusion) parameter
    if (extract_param(line, 'E', value)) {
        new_e = is_absolute_extrusion_ ? value : current_e_ + value;
        has_extrusion = true;
    }

    // Add segment if there's XY movement
    if (has_movement &&
        (new_position.x != current_position_.x || new_position.y != current_position_.y)) {
        // Determine if this is an extrusion move
        bool is_extruding = false;
        if (has_extrusion) {
            float e_delta = new_e - current_e_;
            is_extruding = (e_delta > 0.00001f); // Small threshold for floating point
        }

        add_segment(current_position_, new_position, is_extruding);
    }

    // Update state
    current_position_ = new_position;
    if (has_extrusion) {
        current_e_ = new_e;
    }

    return has_movement;
}

bool GCodeParser::parse_exclude_object_command(const std::string& line) {
    // EXCLUDE_OBJECT_DEFINE NAME=... CENTER=... POLYGON=...
    if (line.find("EXCLUDE_OBJECT_DEFINE") == 0) {
        std::string name;
        if (!extract_string_param(line, "NAME", name)) {
            return false;
        }

        GCodeObject obj;
        obj.name = name;

        // Extract CENTER (format: "X,Y")
        std::string center_str;
        if (extract_string_param(line, "CENTER", center_str)) {
            size_t comma = center_str.find(',');
            if (comma != std::string::npos) {
                try {
                    obj.center.x = std::stof(center_str.substr(0, comma));
                    obj.center.y = std::stof(center_str.substr(comma + 1));
                } catch (...) {
                    spdlog::warn("Failed to parse CENTER for object: {}", name);
                }
            }
        }

        // Extract POLYGON (format: "[[x1,y1],[x2,y2],...]")
        // For now, we'll do basic parsing - full JSON parsing would be better
        std::string polygon_str;
        if (extract_string_param(line, "POLYGON", polygon_str)) {
            // Simple extraction of number pairs
            std::istringstream iss(polygon_str);
            char ch;
            float x, y;
            while (iss >> ch) {
                if (ch == '[') {
                    if (iss >> x >> ch && ch == ',' && iss >> y >> ch && ch == ']') {
                        obj.polygon.push_back(glm::vec2(x, y));
                    }
                }
            }
        }

        objects_[name] = obj;
        spdlog::debug("Defined object: {} at ({}, {})", name, obj.center.x, obj.center.y);
        return true;
    }
    // EXCLUDE_OBJECT_START NAME=...
    else if (line.find("EXCLUDE_OBJECT_START") == 0) {
        if (!extract_string_param(line, "NAME", current_object_)) {
            current_object_.clear();
            return false;
        }
        spdlog::debug("Started object: {}", current_object_);
        return true;
    }
    // EXCLUDE_OBJECT_END NAME=...
    else if (line.find("EXCLUDE_OBJECT_END") == 0) {
        std::string name;
        if (extract_string_param(line, "NAME", name) && name == current_object_) {
            spdlog::debug("Ended object: {}", current_object_);
            current_object_.clear();
            return true;
        }
    }

    return false;
}

void GCodeParser::parse_metadata_comment(const std::string& line) {
    // OrcaSlicer/PrusaSlicer format: "; key = value"
    // Use fuzzy matching to handle variations across slicers

    if (line.length() < 3 || line[0] != ';') {
        return;
    }

    // Skip '; ' to get key=value part
    std::string content = line.substr(1);

    // Trim leading whitespace
    size_t start = 0;
    while (start < content.length() && std::isspace(content[start])) {
        start++;
    }
    content = content.substr(start);

    // Look for '=' separator
    size_t eq_pos = content.find('=');
    if (eq_pos == std::string::npos) {
        return;
    }

    // Extract key and value
    std::string key = content.substr(0, eq_pos);
    std::string value = content.substr(eq_pos + 1);

    // Trim whitespace from key and value
    auto trim = [](std::string& s) {
        size_t start = 0;
        while (start < s.length() && std::isspace(s[start]))
            start++;
        size_t end = s.length();
        while (end > start && std::isspace(s[end - 1]))
            end--;
        s = s.substr(start, end - start);
    };
    trim(key);
    trim(value);

    // Convert key to lowercase for case-insensitive matching
    std::string key_lower = key;
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

    // Helper to check if key contains all substrings (fuzzy match)
    auto contains_all = [&key_lower](std::initializer_list<const char*> terms) {
        for (const char* term : terms) {
            if (key_lower.find(term) == std::string::npos) {
                return false;
            }
        }
        return true;
    };

    // Parse specific metadata fields with fuzzy matching
    if (contains_all({"filament", "col"})) { // "color" or "colour"
        metadata_filament_color_ = value;
        spdlog::trace("Parsed filament color: {}", value);
    } else if (contains_all({"filament", "type"})) {
        metadata_filament_type_ = value;
        spdlog::trace("Parsed filament type: {}", value);
    } else if (contains_all({"printer", "model"}) || contains_all({"printer", "name"})) {
        metadata_printer_model_ = value;
        spdlog::trace("Parsed printer model: {}", value);
    } else if (contains_all({"nozzle", "diameter"})) {
        try {
            metadata_nozzle_diameter_ = std::stof(value);
            spdlog::trace("Parsed nozzle diameter: {}mm", metadata_nozzle_diameter_);
        } catch (...) {
        }
    } else if (contains_all({"filament"}) &&
               (key_lower.find("[mm]") != std::string::npos || contains_all({"length"}))) {
        try {
            metadata_filament_length_ = std::stof(value);
            spdlog::trace("Parsed filament length: {}mm", metadata_filament_length_);
        } catch (...) {
        }
    } else if (contains_all({"filament"}) &&
               (key_lower.find("[g]") != std::string::npos || contains_all({"weight"}))) {
        try {
            metadata_filament_weight_ = std::stof(value);
            spdlog::trace("Parsed filament weight: {}g", metadata_filament_weight_);
        } catch (...) {
        }
    } else if (contains_all({"filament", "cost"}) || contains_all({"material", "cost"})) {
        try {
            metadata_filament_cost_ = std::stof(value);
            spdlog::trace("Parsed filament cost: ${}", metadata_filament_cost_);
        } catch (...) {
        }
    } else if (contains_all({"layer"}) &&
               (contains_all({"total"}) || contains_all({"number"}) || contains_all({"count"}))) {
        try {
            metadata_layer_count_ = std::stoi(value);
            spdlog::trace("Parsed total layer count: {}", metadata_layer_count_);
        } catch (...) {
        }
    } else if ((contains_all({"time"}) &&
                (contains_all({"print"}) || contains_all({"estimated"}))) ||
               contains_all({"print", "time"})) {
        // Parse various time formats: "29m 25s", "1h 23m", "45s", etc.
        float minutes = 0.0f;

        // Try to find hours
        size_t h_pos = value.find('h');
        if (h_pos != std::string::npos) {
            try {
                float hours = std::stof(value.substr(0, h_pos));
                minutes += hours * 60.0f;
            } catch (...) {
            }
        }

        // Try to find minutes
        size_t m_pos = value.find('m');
        if (m_pos != std::string::npos) {
            try {
                size_t start_pos = (h_pos != std::string::npos) ? h_pos + 1 : 0;
                std::string min_str = value.substr(start_pos, m_pos - start_pos);
                // Trim spaces
                size_t min_start = 0;
                while (min_start < min_str.length() && std::isspace(min_str[min_start]))
                    min_start++;
                if (min_start < min_str.length()) {
                    minutes += std::stof(min_str.substr(min_start));
                }
            } catch (...) {
            }
        }

        // Try to find seconds
        size_t s_pos = value.find('s');
        if (s_pos != std::string::npos) {
            try {
                size_t start_pos = (m_pos != std::string::npos)   ? m_pos + 1
                                   : (h_pos != std::string::npos) ? h_pos + 1
                                                                  : 0;
                std::string sec_str = value.substr(start_pos, s_pos - start_pos);
                // Trim spaces
                size_t sec_start = 0;
                while (sec_start < sec_str.length() && std::isspace(sec_str[sec_start]))
                    sec_start++;
                if (sec_start < sec_str.length()) {
                    float seconds = std::stof(sec_str.substr(sec_start));
                    minutes += seconds / 60.0f;
                }
            } catch (...) {
            }
        }

        if (minutes > 0.0f) {
            metadata_print_time_ = minutes;
            spdlog::trace("Parsed estimated time: {:.2f} minutes", minutes);
        }
    } else if (contains_all({"generated"}) || contains_all({"slicer"})) {
        metadata_slicer_name_ = value;
        spdlog::trace("Parsed slicer: {}", value);
    }
}

bool GCodeParser::extract_param(const std::string& line, char param, float& out_value) {
    size_t pos = line.find(param);
    if (pos == std::string::npos) {
        return false;
    }

    // Make sure it's a parameter (preceded by space or at start after command)
    if (pos > 0 && line[pos - 1] != ' ' && line[pos - 1] != '\t') {
        return false;
    }

    // Extract number after parameter letter
    size_t start = pos + 1;
    if (start >= line.length()) {
        return false;
    }

    // Find end of number (space, end of string, or another letter)
    size_t end = start;
    while (end < line.length() &&
           (std::isdigit(line[end]) || line[end] == '.' || line[end] == '-' || line[end] == '+')) {
        end++;
    }

    if (end == start) {
        return false;
    }

    try {
        out_value = std::stof(line.substr(start, end - start));
        return true;
    } catch (...) {
        return false;
    }
}

bool GCodeParser::extract_string_param(const std::string& line, const std::string& param,
                                       std::string& out_value) {
    size_t pos = line.find(param + "=");
    if (pos == std::string::npos) {
        return false;
    }

    size_t start = pos + param.length() + 1; // Skip "PARAM="
    if (start >= line.length()) {
        return false;
    }

    // Find end of value (space or end of line)
    size_t end = line.find(' ', start);
    if (end == std::string::npos) {
        end = line.length();
    }

    out_value = line.substr(start, end - start);
    return true;
}

void GCodeParser::add_segment(const glm::vec3& start, const glm::vec3& end, bool is_extrusion) {
    if (layers_.empty()) {
        start_new_layer(start.z);
    }

    ToolpathSegment segment;
    segment.start = start;
    segment.end = end;
    segment.is_extrusion = is_extrusion;
    segment.object_name = current_object_;
    segment.extrusion_amount = 0.0f; // TODO: Calculate from E delta

    // Update layer data
    Layer& current_layer = layers_.back();
    current_layer.segments.push_back(segment);
    current_layer.bounding_box.expand(start);
    current_layer.bounding_box.expand(end);

    if (is_extrusion) {
        current_layer.segment_count_extrusion++;
    } else {
        current_layer.segment_count_travel++;
    }

    // Update global bounds
    global_bounds_.expand(start);
    global_bounds_.expand(end);

    // Update object bounding box (only for extrusion moves, not travels)
    if (!current_object_.empty() && objects_.count(current_object_) > 0 && is_extrusion) {
        objects_[current_object_].bounding_box.expand(start);
        objects_[current_object_].bounding_box.expand(end);

        // Debug: Log first few extrusion segments per object
        static std::map<std::string, int> segment_counts;
        segment_counts[current_object_]++;
        if (segment_counts[current_object_] <= 3) {
            spdlog::trace("Object '{}' extrusion segment: start=({:.2f},{:.2f},{:.2f}) "
                          "end=({:.2f},{:.2f},{:.2f})",
                          current_object_, start.x, start.y, start.z, end.x, end.y, end.z);
        }
    }
}

void GCodeParser::start_new_layer(float z) {
    // Don't create duplicate layers at same Z
    if (!layers_.empty() && std::abs(layers_.back().z_height - z) < 0.001f) {
        return;
    }

    Layer layer;
    layer.z_height = z;
    layers_.push_back(layer);

    spdlog::debug("Started layer {} at Z={:.3f}", layers_.size() - 1, z);
}

std::string GCodeParser::trim_line(const std::string& line) {
    if (line.empty()) {
        return line;
    }

    // Remove comments (everything after ';')
    size_t comment_pos = line.find(';');
    std::string without_comment =
        (comment_pos != std::string::npos) ? line.substr(0, comment_pos) : line;

    // Trim leading/trailing whitespace
    size_t start = 0;
    while (start < without_comment.length() && std::isspace(without_comment[start])) {
        start++;
    }

    if (start == without_comment.length()) {
        return "";
    }

    size_t end = without_comment.length();
    while (end > start && std::isspace(without_comment[end - 1])) {
        end--;
    }

    return without_comment.substr(start, end - start);
}

ParsedGCodeFile GCodeParser::finalize() {
    ParsedGCodeFile result;
    result.filename = "";
    result.layers = std::move(layers_);
    result.objects = std::move(objects_);
    result.global_bounding_box = global_bounds_;

    // Calculate statistics
    for (const auto& layer : result.layers) {
        result.total_segments += layer.segments.size();
    }

    // Transfer metadata
    result.slicer_name = metadata_slicer_name_;
    result.filament_type = metadata_filament_type_;
    result.filament_color_hex = metadata_filament_color_;
    result.printer_model = metadata_printer_model_;
    result.nozzle_diameter_mm = metadata_nozzle_diameter_;
    result.total_filament_mm = metadata_filament_length_;
    result.filament_weight_g = metadata_filament_weight_;
    result.filament_cost = metadata_filament_cost_;
    result.estimated_print_time_minutes = metadata_print_time_;
    result.total_layer_count = metadata_layer_count_;

    spdlog::info("Parsed G-code: {} layers, {} segments, {} objects", result.layers.size(),
                 result.total_segments, result.objects.size());

    // Debug: Log object bounding boxes
    for (const auto& [name, obj] : result.objects) {
        spdlog::debug("Object '{}' AABB: min=({:.2f},{:.2f},{:.2f}) max=({:.2f},{:.2f},{:.2f}) "
                      "center=({:.2f},{:.2f},{:.2f})",
                      name, obj.bounding_box.min.x, obj.bounding_box.min.y, obj.bounding_box.min.z,
                      obj.bounding_box.max.x, obj.bounding_box.max.y, obj.bounding_box.max.z,
                      obj.bounding_box.center().x, obj.bounding_box.center().y,
                      obj.bounding_box.center().z);
    }

    // Reset state for potential reuse
    reset();

    return result;
}

} // namespace gcode
