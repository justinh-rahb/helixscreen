// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "klipper_config_editor.h"

#include <algorithm>
#include <sstream>

namespace helix::system {

std::optional<ConfigKey> ConfigStructure::find_key(const std::string& section,
                                                   const std::string& key) const {
    auto it = sections.find(section);
    if (it == sections.end())
        return std::nullopt;

    for (const auto& k : it->second.keys) {
        if (k.name == key)
            return k;
    }
    return std::nullopt;
}

ConfigStructure KlipperConfigEditor::parse_structure(const std::string& content) const {
    ConfigStructure result;

    if (content.empty()) {
        result.total_lines = 0;
        return result;
    }

    // Split content into lines
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    result.total_lines = static_cast<int>(lines.size());

    std::string current_section;
    ConfigKey* current_multiline_key = nullptr;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const auto& raw_line = lines[i];

        // Check for SAVE_CONFIG boundary
        if (raw_line.find("#*# <") != std::string::npos &&
            raw_line.find("SAVE_CONFIG") != std::string::npos) {
            result.save_config_line = i;
            // Stop parsing structured content after SAVE_CONFIG
            break;
        }

        // Check if this is a continuation of a multi-line value
        if (current_multiline_key != nullptr) {
            // Empty lines are preserved within multi-line values
            if (raw_line.empty()) {
                current_multiline_key->end_line = i;
                continue;
            }
            // Indented lines (space or tab) continue the multi-line value
            if (raw_line[0] == ' ' || raw_line[0] == '\t') {
                current_multiline_key->end_line = i;
                continue;
            }
            // Non-indented, non-empty line ends the multi-line value
            current_multiline_key = nullptr;
        }

        // Skip empty lines outside multi-line values
        if (raw_line.empty())
            continue;

        // Check for section header: [section_name]
        if (raw_line[0] == '[') {
            auto close_bracket = raw_line.find(']');
            if (close_bracket != std::string::npos) {
                // Finalize previous section's line_end
                if (!current_section.empty()) {
                    result.sections[current_section].line_end = i - 1;
                }

                std::string section_name = raw_line.substr(1, close_bracket - 1);

                // Check for include directive
                const std::string include_prefix = "include ";
                if (section_name.substr(0, include_prefix.size()) == include_prefix) {
                    std::string path = section_name.substr(include_prefix.size());
                    result.includes.push_back(path);
                    current_section.clear();
                    continue;
                }

                current_section = section_name;
                auto& sec = result.sections[current_section];
                sec.name = current_section;
                sec.line_start = i;
                continue;
            }
        }

        // Skip full-line comments
        if (raw_line[0] == '#' || raw_line[0] == ';')
            continue;

        // If we're not in a section, skip
        if (current_section.empty())
            continue;

        // Parse key-value pair: find first ':' or '='
        std::string delimiter;
        size_t delim_pos = std::string::npos;

        // Find the first delimiter (: or =)
        size_t colon_pos = raw_line.find(':');
        size_t equals_pos = raw_line.find('=');

        if (colon_pos != std::string::npos && equals_pos != std::string::npos) {
            delim_pos = std::min(colon_pos, equals_pos);
        } else if (colon_pos != std::string::npos) {
            delim_pos = colon_pos;
        } else if (equals_pos != std::string::npos) {
            delim_pos = equals_pos;
        }

        if (delim_pos == std::string::npos)
            continue;

        delimiter = std::string(1, raw_line[delim_pos]);

        // Extract key name and lowercase it
        std::string key_name = raw_line.substr(0, delim_pos);
        // Trim trailing whitespace from key
        while (!key_name.empty() && (key_name.back() == ' ' || key_name.back() == '\t')) {
            key_name.pop_back();
        }
        std::transform(key_name.begin(), key_name.end(), key_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // Extract value (after delimiter, trimming leading whitespace)
        std::string value;
        if (delim_pos + 1 < raw_line.size()) {
            value = raw_line.substr(delim_pos + 1);
            // Trim leading whitespace from value
            size_t first_non_space = value.find_first_not_of(" \t");
            if (first_non_space != std::string::npos) {
                value = value.substr(first_non_space);
            } else {
                value.clear();
            }
        }

        ConfigKey key;
        key.name = key_name;
        key.value = value;
        key.delimiter = delimiter;
        key.line_number = i;
        key.end_line = i;

        // Check if this is the start of a multi-line value
        // (empty value or value that will have indented continuation lines)
        if (value.empty()) {
            key.is_multiline = true;
        }

        result.sections[current_section].keys.push_back(key);

        // Track pointer for multi-line continuation detection
        // Even non-empty values can have continuations
        current_multiline_key = &result.sections[current_section].keys.back();
    }

    // Finalize the last section's line_end
    if (!current_section.empty()) {
        int last_line =
            result.save_config_line >= 0 ? result.save_config_line - 1 : result.total_lines - 1;
        result.sections[current_section].line_end = last_line;
    }

    return result;
}

} // namespace helix::system
