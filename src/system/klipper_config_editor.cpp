// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "klipper_config_editor.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>
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

namespace {

// Split content into lines, preserving the ability to rejoin with \n
std::vector<std::string> split_lines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Rejoin lines with \n, adding trailing newline if original had one
std::string join_lines(const std::vector<std::string>& lines, bool trailing_newline) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i + 1 < lines.size() || trailing_newline) {
            result += '\n';
        }
    }
    return result;
}

} // namespace

std::optional<std::string> KlipperConfigEditor::set_value(const std::string& content,
                                                          const std::string& section,
                                                          const std::string& key,
                                                          const std::string& new_value) const {
    auto structure = parse_structure(content);
    auto found = structure.find_key(section, key);
    if (!found.has_value())
        return std::nullopt;

    auto lines = split_lines(content);
    int target = found->line_number;
    if (target < 0 || target >= static_cast<int>(lines.size()))
        return std::nullopt;

    const auto& raw_line = lines[target];

    // Find the delimiter position in the raw line (first : or =)
    size_t delim_pos = std::string::npos;
    size_t colon_pos = raw_line.find(':');
    size_t equals_pos = raw_line.find('=');
    if (colon_pos != std::string::npos && equals_pos != std::string::npos)
        delim_pos = std::min(colon_pos, equals_pos);
    else if (colon_pos != std::string::npos)
        delim_pos = colon_pos;
    else if (equals_pos != std::string::npos)
        delim_pos = equals_pos;

    if (delim_pos == std::string::npos)
        return std::nullopt;

    // Preserve everything up to and including the delimiter plus any whitespace after it
    size_t value_start = delim_pos + 1;
    // Preserve the spacing between delimiter and value
    while (value_start < raw_line.size() &&
           (raw_line[value_start] == ' ' || raw_line[value_start] == '\t')) {
        ++value_start;
    }

    // Reconstruct: key + delimiter + spacing + new_value
    std::string prefix = raw_line.substr(0, delim_pos + 1);
    // Restore the original spacing between delimiter and old value
    std::string spacing = raw_line.substr(delim_pos + 1, value_start - (delim_pos + 1));
    lines[target] = prefix + spacing + new_value;

    bool trailing = !content.empty() && content.back() == '\n';
    return join_lines(lines, trailing);
}

std::optional<std::string> KlipperConfigEditor::add_key(const std::string& content,
                                                        const std::string& section,
                                                        const std::string& key,
                                                        const std::string& value,
                                                        const std::string& delimiter) const {
    auto structure = parse_structure(content);
    auto sec_it = structure.sections.find(section);
    if (sec_it == structure.sections.end())
        return std::nullopt;

    auto lines = split_lines(content);
    const auto& sec = sec_it->second;

    // Find insert position: after the last key line, or after section header if no keys
    int insert_after = sec.line_start;
    if (!sec.keys.empty()) {
        // Use the end_line of the last key (handles multi-line values)
        for (const auto& k : sec.keys) {
            if (k.end_line > insert_after)
                insert_after = k.end_line;
        }
    }

    // Insert the new line after insert_after
    std::string new_line = key + delimiter + value;
    lines.insert(lines.begin() + insert_after + 1, new_line);

    bool trailing = !content.empty() && content.back() == '\n';
    return join_lines(lines, trailing);
}

std::optional<std::string> KlipperConfigEditor::remove_key(const std::string& content,
                                                           const std::string& section,
                                                           const std::string& key) const {
    auto structure = parse_structure(content);
    auto found = structure.find_key(section, key);
    if (!found.has_value())
        return std::nullopt;

    auto lines = split_lines(content);
    int start = found->line_number;
    int end = found->end_line;

    // Comment out the key line and any continuation lines
    for (int i = start; i <= end && i < static_cast<int>(lines.size()); ++i) {
        lines[i] = "#" + lines[i];
    }

    bool trailing = !content.empty() && content.back() == '\n';
    return join_lines(lines, trailing);
}

namespace {

/// Get the directory portion of a file path (everything before the last '/')
std::string get_directory(const std::string& path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos)
        return "";
    return path.substr(0, pos);
}

/// Resolve a relative include path against the directory of the including file
std::string resolve_path(const std::string& current_file, const std::string& include_path) {
    std::string dir = get_directory(current_file);
    if (dir.empty())
        return include_path;
    return dir + "/" + include_path;
}

/// Simple glob pattern matching for Klipper include patterns (supports '*' wildcard)
bool glob_match(const std::string& pattern, const std::string& text) {
    size_t pi = 0, ti = 0;
    size_t star_pi = std::string::npos, star_ti = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern[pi] == text[ti] || pattern[pi] == '?')) {
            ++pi;
            ++ti;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_pi = pi;
            star_ti = ti;
            ++pi;
        } else if (star_pi != std::string::npos) {
            pi = star_pi + 1;
            ++star_ti;
            ti = star_ti;
        } else {
            return false;
        }
    }

    while (pi < pattern.size() && pattern[pi] == '*')
        ++pi;

    return pi == pattern.size();
}

/// Find all files in the map that match a glob pattern (resolved relative to current file)
std::vector<std::string> match_glob(const std::map<std::string, std::string>& files,
                                    const std::string& current_file,
                                    const std::string& include_pattern) {
    std::string resolved = resolve_path(current_file, include_pattern);
    std::vector<std::string> matches;

    for (const auto& [filename, _] : files) {
        if (glob_match(resolved, filename)) {
            matches.push_back(filename);
        }
    }

    // Sort for deterministic ordering
    std::sort(matches.begin(), matches.end());
    return matches;
}

} // namespace

std::map<std::string, SectionLocation>
KlipperConfigEditor::resolve_includes(const std::map<std::string, std::string>& files,
                                      const std::string& root_file, int max_depth) const {
    std::map<std::string, SectionLocation> result;
    std::set<std::string> visited;

    // Recursive lambda: process a file and its includes
    // depth starts at 0 for the root file
    std::function<void(const std::string&, int)> process_file;
    process_file = [&](const std::string& file_path, int depth) {
        // Cycle detection
        if (visited.count(file_path))
            return;
        visited.insert(file_path);

        // Depth check — root is depth 0, max_depth=5 allows depths 0..5 (6 levels total)
        if (depth > max_depth) {
            spdlog::debug("klipper_config_editor: max include depth {} reached at {}", max_depth,
                          file_path);
            return;
        }

        // Find file content
        auto it = files.find(file_path);
        if (it == files.end()) {
            spdlog::debug("klipper_config_editor: included file not found: {}", file_path);
            return;
        }

        auto structure = parse_structure(it->second);

        // Process includes first (so the current file's sections override included ones)
        for (const auto& include_pattern : structure.includes) {
            bool has_wildcard = include_pattern.find('*') != std::string::npos;

            if (has_wildcard) {
                auto matched = match_glob(files, file_path, include_pattern);
                for (const auto& match : matched) {
                    process_file(match, depth + 1);
                }
            } else {
                std::string resolved = resolve_path(file_path, include_pattern);
                process_file(resolved, depth + 1);
            }
        }

        // Add this file's sections (overwrites any from includes — last wins)
        for (const auto& [name, section] : structure.sections) {
            SectionLocation loc;
            loc.file_path = file_path;
            loc.section = section;
            result[name] = loc;
        }
    };

    process_file(root_file, 0);
    return result;
}

} // namespace helix::system
