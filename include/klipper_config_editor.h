// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace helix::system {

struct ConfigKey {
    std::string name;      // Key name (lowercased)
    std::string value;     // Raw value string (first line only for multi-line)
    std::string delimiter; // ":" or "=" — preserved for round-trip fidelity
    int line_number = 0;   // 0-indexed line number
    bool is_multiline = false;
    int end_line = 0; // Last line of value (for multi-line)
};

struct ConfigSection {
    std::string name;
    int line_start = 0; // Line of [section] header
    int line_end = 0;   // Last line before next section or EOF
    std::vector<ConfigKey> keys;
};

struct ConfigStructure {
    std::map<std::string, ConfigSection> sections;
    std::vector<std::string> includes;
    int save_config_line = -1;
    int total_lines = 0;

    std::optional<ConfigKey> find_key(const std::string& section, const std::string& key) const;
};

/// Which file a section was found in (for include resolution)
struct SectionLocation {
    std::string file_path; ///< Path relative to config root
    ConfigSection section; ///< Section info from that file
};

class KlipperConfigEditor {
  public:
    ConfigStructure parse_structure(const std::string& content) const;

    /// Set a value for an existing key within a file's content
    /// Returns modified content, or std::nullopt if key not found
    std::optional<std::string> set_value(const std::string& content, const std::string& section,
                                         const std::string& key,
                                         const std::string& new_value) const;

    /// Add a new key to an existing section
    /// Returns modified content, or std::nullopt if section not found
    std::optional<std::string> add_key(const std::string& content, const std::string& section,
                                       const std::string& key, const std::string& value,
                                       const std::string& delimiter = ": ") const;

    /// Resolve all includes and build a section -> file mapping
    /// @param files Map of filename -> content (for unit testing without Moonraker)
    /// @param root_file Starting file to resolve from
    /// @param max_depth Maximum include recursion depth (default 5)
    /// @return Map of section_name -> SectionLocation
    std::map<std::string, SectionLocation>
    resolve_includes(const std::map<std::string, std::string>& files, const std::string& root_file,
                     int max_depth = 5) const;

    /// Comment out a key (prefix with #) — safer than deleting
    /// Returns modified content, or std::nullopt if key not found
    std::optional<std::string> remove_key(const std::string& content, const std::string& section,
                                          const std::string& key) const;
};

} // namespace helix::system
