// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

namespace helix {

class Config;

struct HomeWidgetEntry {
    std::string id;
    bool enabled;

    bool operator==(const HomeWidgetEntry& other) const {
        return id == other.id && enabled == other.enabled;
    }
};

class HomeWidgetConfig {
  public:
    explicit HomeWidgetConfig(Config& config);

    /// Load widget order from config, merging with registry defaults
    void load();

    /// Save current order to config
    void save();

    const std::vector<HomeWidgetEntry>& entries() const {
        return entries_;
    }

    /// Move widget between positions. No-op if indices are equal or out of bounds.
    void reorder(size_t from_index, size_t to_index);

    /// No-op if index out of bounds.
    void set_enabled(size_t index, bool enabled);

    void reset_to_defaults();

    bool is_enabled(const std::string& id) const;

  private:
    Config& config_;
    std::vector<HomeWidgetEntry> entries_;

    static std::vector<HomeWidgetEntry> build_defaults();
};

} // namespace helix
