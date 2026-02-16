// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <optional>
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

class KlipperConfigEditor {
  public:
    ConfigStructure parse_structure(const std::string& content) const;
};

} // namespace helix::system
