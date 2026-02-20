// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "home_widget_config.h"

#include "config.h"
#include "home_widget_registry.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>

namespace helix {

HomeWidgetConfig::HomeWidgetConfig(Config& config) : config_(config) {}

void HomeWidgetConfig::load() {
    entries_.clear();

    auto saved = config_.get<json>("/home_widgets", json::array());
    if (!saved.is_array()) {
        entries_ = build_defaults();
        return;
    }

    std::set<std::string> seen_ids;

    for (const auto& item : saved) {
        if (!item.is_object() || !item.contains("id") || !item.contains("enabled")) {
            continue;
        }

        // Validate field types before extraction
        if (!item["id"].is_string() || !item["enabled"].is_boolean()) {
            spdlog::debug("[HomeWidgetConfig] Skipping malformed widget entry (wrong field types)");
            continue;
        }

        std::string id = item["id"].get<std::string>();
        bool enabled = item["enabled"].get<bool>();

        // Skip duplicates
        if (seen_ids.count(id) > 0) {
            spdlog::debug("[HomeWidgetConfig] Skipping duplicate widget ID: {}", id);
            continue;
        }

        // Skip unknown widget IDs (not in registry)
        if (find_widget_def(id) == nullptr) {
            spdlog::debug("[HomeWidgetConfig] Dropping unknown widget ID: {}", id);
            continue;
        }

        seen_ids.insert(id);
        entries_.push_back({id, enabled});
    }

    // Append any new widgets from registry that are not in saved config
    for (const auto& def : get_all_widget_defs()) {
        if (seen_ids.count(def.id) == 0) {
            spdlog::debug("[HomeWidgetConfig] Appending new widget: {}", def.id);
            entries_.push_back({def.id, true});
        }
    }

    // If saved array was empty or all entries were invalid, we still have
    // the appended defaults from above. If nothing was saved at all,
    // that gives us the full default set.
    if (entries_.empty()) {
        entries_ = build_defaults();
    }
}

void HomeWidgetConfig::save() {
    json widgets_array = json::array();
    for (const auto& entry : entries_) {
        widgets_array.push_back({{"id", entry.id}, {"enabled", entry.enabled}});
    }
    config_.set<json>("/home_widgets", widgets_array);
    config_.save();
}

void HomeWidgetConfig::reorder(size_t from_index, size_t to_index) {
    if (from_index >= entries_.size() || to_index >= entries_.size()) {
        return;
    }
    if (from_index == to_index) {
        return;
    }

    // Extract element, then insert at new position
    auto entry = std::move(entries_[from_index]);
    entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(from_index));
    entries_.insert(entries_.begin() + static_cast<ptrdiff_t>(to_index), std::move(entry));
}

void HomeWidgetConfig::set_enabled(size_t index, bool enabled) {
    if (index >= entries_.size()) {
        return;
    }
    entries_[index].enabled = enabled;
}

void HomeWidgetConfig::reset_to_defaults() {
    entries_ = build_defaults();
}

bool HomeWidgetConfig::is_enabled(const std::string& id) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const HomeWidgetEntry& e) { return e.id == id; });
    return it != entries_.end() && it->enabled;
}

std::vector<HomeWidgetEntry> HomeWidgetConfig::build_defaults() {
    std::vector<HomeWidgetEntry> defaults;
    const auto& defs = get_all_widget_defs();
    defaults.reserve(defs.size());
    for (const auto& def : defs) {
        defaults.push_back({def.id, true});
    }
    return defaults;
}

} // namespace helix
