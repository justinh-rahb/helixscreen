// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "panel_widget_config.h"

#include "config.h"
#include "grid_layout.h"
#include "panel_widget_registry.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <set>

namespace helix {

PanelWidgetConfig::PanelWidgetConfig(const std::string& panel_id, Config& config)
    : panel_id_(panel_id), config_(config) {}

void PanelWidgetConfig::load() {
    entries_.clear();

    // Per-panel path: /panel_widgets/<panel_id>
    std::string panel_path = "/panel_widgets/" + panel_id_;
    auto saved = config_.get<json>(panel_path, json());

    // Migration: move legacy "home_widgets" to "panel_widgets.home"
    if (panel_id_ == "home" && (saved.is_null() || !saved.is_array())) {
        auto legacy = config_.get<json>("/home_widgets", json());
        if (legacy.is_array() && !legacy.empty()) {
            spdlog::info("[PanelWidgetConfig] Migrating legacy home_widgets to panel_widgets.home");
            config_.set<json>(panel_path, legacy);
            // Remove legacy key
            config_.get_json("").erase("home_widgets");
            config_.save();
            saved = legacy;
        }
    }

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
            spdlog::debug(
                "[PanelWidgetConfig] Skipping malformed widget entry (wrong field types)");
            continue;
        }

        std::string id = item["id"].get<std::string>();
        bool enabled = item["enabled"].get<bool>();

        // Skip duplicates
        if (seen_ids.count(id) > 0) {
            spdlog::debug("[PanelWidgetConfig] Skipping duplicate widget ID: {}", id);
            continue;
        }

        // Skip unknown widget IDs (not in registry)
        if (find_widget_def(id) == nullptr) {
            spdlog::debug("[PanelWidgetConfig] Dropping unknown widget ID: {}", id);
            continue;
        }

        // Load optional per-widget config
        nlohmann::json widget_config;
        if (item.contains("config") && item["config"].is_object()) {
            widget_config = item["config"];
        }

        // Load grid placement coordinates (default to -1 = auto-place)
        int col = -1;
        int row_val = -1;
        int colspan = 1;
        int rowspan = 1;
        if (item.contains("col") && item["col"].is_number_integer()) {
            col = item["col"].get<int>();
        }
        if (item.contains("row") && item["row"].is_number_integer()) {
            row_val = item["row"].get<int>();
        }
        if (item.contains("colspan") && item["colspan"].is_number_integer()) {
            colspan = item["colspan"].get<int>();
        }
        if (item.contains("rowspan") && item["rowspan"].is_number_integer()) {
            rowspan = item["rowspan"].get<int>();
        }

        seen_ids.insert(id);
        entries_.push_back({id, enabled, widget_config, col, row_val, colspan, rowspan});
    }

    // Append any new widgets from registry that are not in saved config
    for (const auto& def : get_all_widget_defs()) {
        if (seen_ids.count(def.id) == 0) {
            spdlog::debug("[PanelWidgetConfig] Appending new widget: {} (default_enabled={})",
                          def.id, def.default_enabled);
            entries_.push_back({def.id, def.default_enabled, {}, -1, -1, def.colspan, def.rowspan});
        }
    }

    if (entries_.empty()) {
        entries_ = build_defaults();
        return;
    }

    // If no entries have grid positions, this is a pre-grid config — reset to defaults.
    bool has_any_grid =
        std::any_of(entries_.begin(), entries_.end(),
                    [](const PanelWidgetEntry& e) { return e.has_grid_position(); });
    if (!has_any_grid) {
        spdlog::info(
            "[PanelWidgetConfig] Pre-grid config detected, resetting to default grid for '{}'",
            panel_id_);
        entries_ = build_defaults();
    }
}

void PanelWidgetConfig::save() {
    json widgets_array = json::array();
    for (const auto& entry : entries_) {
        json item = {{"id", entry.id}, {"enabled", entry.enabled}};
        if (!entry.config.empty()) {
            item["config"] = entry.config;
        }
        // Write grid coordinates only for entries with explicit placement
        if (entry.has_grid_position()) {
            item["col"] = entry.col;
            item["row"] = entry.row;
            item["colspan"] = entry.colspan;
            item["rowspan"] = entry.rowspan;
        }
        widgets_array.push_back(std::move(item));
    }
    config_.set<json>("/panel_widgets/" + panel_id_, widgets_array);
    config_.save();
}

void PanelWidgetConfig::reorder(size_t from_index, size_t to_index) {
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

void PanelWidgetConfig::set_enabled(size_t index, bool enabled) {
    if (index >= entries_.size()) {
        return;
    }
    entries_[index].enabled = enabled;
}

void PanelWidgetConfig::reset_to_defaults() {
    entries_ = build_defaults();
}

bool PanelWidgetConfig::is_enabled(const std::string& id) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const PanelWidgetEntry& e) { return e.id == id; });
    return it != entries_.end() && it->enabled;
}

nlohmann::json PanelWidgetConfig::get_widget_config(const std::string& id) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const PanelWidgetEntry& e) { return e.id == id; });
    if (it != entries_.end() && !it->config.empty()) {
        return it->config;
    }
    return nlohmann::json::object();
}

void PanelWidgetConfig::set_widget_config(const std::string& id, const nlohmann::json& config) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&id](const PanelWidgetEntry& e) { return e.id == id; });
    if (it != entries_.end()) {
        it->config = config;
        save();
    } else {
        spdlog::debug("[PanelWidgetConfig] set_widget_config: widget '{}' not found", id);
    }
}

std::vector<PanelWidgetEntry> PanelWidgetConfig::build_default_grid() {
    const auto& defs = get_all_widget_defs();

    // Default layout (6×4, MEDIUM breakpoint):
    //   Col 0-1: printer_image (2×2) top, print_status (2×2) bottom
    //   Col 2-3: tips (2×1) top-right of printer image
    //   Col 2-5, rows 1-3: 1×1 widgets fill remaining cells
    constexpr int default_breakpoint = 2;
    GridLayout grid(default_breakpoint);

    // Fixed positions for key widgets
    struct FixedPlacement {
        const char* id;
        int col, row, colspan, rowspan;
    };
    const FixedPlacement fixed[] = {
        {"printer_image", 0, 0, 2, 2},
        {"print_status", 0, 2, 2, 2},
        {"tips", 2, 0, 3, 1},
    };

    std::vector<PanelWidgetEntry> result;
    result.reserve(defs.size());
    std::set<std::string> placed_ids;

    // Place fixed widgets first
    for (const auto& fp : fixed) {
        const auto* def = find_widget_def(fp.id);
        if (!def)
            continue;
        if (grid.place({fp.id, fp.col, fp.row, fp.colspan, fp.rowspan})) {
            result.push_back({fp.id, true, {}, fp.col, fp.row, fp.colspan, fp.rowspan});
            placed_ids.insert(fp.id);
        }
    }

    // Collect remaining enabled widgets
    std::vector<const PanelWidgetDef*> small_widgets;
    for (const auto& def : defs) {
        if (placed_ids.count(def.id) > 0)
            continue;
        if (!def.default_enabled)
            continue;
        small_widgets.push_back(&def);
    }

    // Place small widgets in the right side of the grid, filling bottom rows first.
    // Available area: cols 2..5 in rows 1-3 (row 0 has tips, cols 0-1 occupied by fixed widgets)
    int grid_cols = GridLayout::get_cols(default_breakpoint);
    int grid_rows = GridLayout::get_rows(default_breakpoint);
    int avail_cols = grid_cols - 2;
    int n = static_cast<int>(small_widgets.size());

    // Build slot positions: bottom row first, right-justified, then rows above
    std::vector<std::pair<int, int>> slots; // (col, row)
    for (int r = grid_rows - 1; r >= 1 && static_cast<int>(slots.size()) < n; --r) {
        int need = std::min(avail_cols, n - static_cast<int>(slots.size()));
        int start_col = grid_cols - need;
        for (int c = start_col; c < grid_cols; ++c) {
            slots.push_back({c, r});
        }
    }

    // Reverse so first widget gets the top-left of the bottom-right block
    std::reverse(slots.begin(), slots.end());

    for (size_t i = 0; i < small_widgets.size(); ++i) {
        const auto* def = small_widgets[i];
        if (i < slots.size() && grid.place({def->id, slots[i].first, slots[i].second, 1, 1})) {
            result.push_back({def->id, true, {}, slots[i].first, slots[i].second, 1, 1});
        } else {
            // Overflow: enabled but auto-place at runtime
            result.push_back({def->id, true, {}, -1, -1, def->colspan, def->rowspan});
        }
        placed_ids.insert(def->id);
    }

    // Disabled widgets (no grid position)
    for (const auto& def : defs) {
        if (placed_ids.count(def.id) > 0)
            continue;
        result.push_back({def.id, false, {}, -1, -1, def.colspan, def.rowspan});
    }

    return result;
}

bool PanelWidgetConfig::is_grid_format() const {
    return std::any_of(entries_.begin(), entries_.end(),
                       [](const PanelWidgetEntry& e) { return e.has_grid_position(); });
}

std::vector<PanelWidgetEntry> PanelWidgetConfig::build_defaults() {
    return build_default_grid();
}

} // namespace helix
