// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "panel_widget_manager.h"

#include "ui_ams_mini_status.h"

#include "config.h"
#include "grid_layout.h"
#include "observer_factory.h"
#include "panel_widget.h"
#include "panel_widget_config.h"
#include "panel_widget_registry.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace helix {

PanelWidgetManager& PanelWidgetManager::instance() {
    static PanelWidgetManager instance;
    return instance;
}

void PanelWidgetManager::clear_shared_resources() {
    shared_resources_.clear();
}

void PanelWidgetManager::init_widget_subjects() {
    if (widget_subjects_initialized_) {
        return;
    }

    // Register all widget factories explicitly (avoids SIOF from file-scope statics)
    init_widget_registrations();

    for (const auto& def : get_all_widget_defs()) {
        if (def.init_subjects) {
            spdlog::debug("[PanelWidgetManager] Initializing subjects for widget '{}'", def.id);
            def.init_subjects();
        }
    }

    widget_subjects_initialized_ = true;
    spdlog::debug("[PanelWidgetManager] Widget subjects initialized");
}

void PanelWidgetManager::register_rebuild_callback(const std::string& panel_id,
                                                   RebuildCallback cb) {
    rebuild_callbacks_[panel_id] = std::move(cb);
}

void PanelWidgetManager::unregister_rebuild_callback(const std::string& panel_id) {
    rebuild_callbacks_.erase(panel_id);
}

void PanelWidgetManager::notify_config_changed(const std::string& panel_id) {
    auto it = rebuild_callbacks_.find(panel_id);
    if (it != rebuild_callbacks_.end()) {
        it->second();
    }
}

static PanelWidgetConfig& get_widget_config(const std::string& panel_id) {
    // Per-panel config instances cached by panel ID
    static std::unordered_map<std::string, PanelWidgetConfig> configs;
    auto it = configs.find(panel_id);
    if (it == configs.end()) {
        it = configs.emplace(panel_id, PanelWidgetConfig(panel_id, *Config::get_instance())).first;
    }
    // Always reload to pick up changes from settings overlay
    it->second.load();
    return it->second;
}

std::vector<std::unique_ptr<PanelWidget>>
PanelWidgetManager::populate_widgets(const std::string& panel_id, lv_obj_t* container) {
    if (!container) {
        spdlog::debug("[PanelWidgetManager] populate_widgets: null container for '{}'", panel_id);
        return {};
    }

    if (populating_) {
        spdlog::debug(
            "[PanelWidgetManager] populate_widgets: already in progress for '{}', skipping",
            panel_id);
        return {};
    }
    populating_ = true;

    // Clear existing children (for repopulation)
    lv_obj_clean(container);

    auto& widget_config = get_widget_config(panel_id);

    // Resolved widget slot: holds the widget ID, resolved XML component name,
    // per-widget config, and optionally a pre-created PanelWidget instance.
    struct WidgetSlot {
        std::string widget_id;
        std::string component_name;
        nlohmann::json config;
        std::unique_ptr<PanelWidget> instance; // nullptr for pure-XML widgets
    };

    // Collect enabled + hardware-available widgets
    std::vector<WidgetSlot> enabled_widgets;
    for (const auto& entry : widget_config.entries()) {
        if (!entry.enabled) {
            continue;
        }

        // Check hardware gate — skip widgets whose hardware isn't present.
        // Gates are defined in PanelWidgetDef::hardware_gate_subject and checked
        // here instead of XML bind_flag_if_eq to avoid orphaned dividers.
        const auto* def = find_widget_def(entry.id);
        if (def && def->hardware_gate_subject) {
            lv_subject_t* gate = lv_xml_get_subject(nullptr, def->hardware_gate_subject);
            if (gate && lv_subject_get_int(gate) == 0) {
                continue;
            }
        }

        WidgetSlot slot;
        slot.widget_id = entry.id;
        slot.config = entry.config;

        // If this widget has a factory, create the instance early so it can
        // resolve the XML component name (e.g. carousel vs stack mode).
        if (def && def->factory) {
            slot.instance = def->factory();
            if (slot.instance) {
                slot.instance->set_config(entry.config);
                slot.component_name = slot.instance->get_component_name();
            } else {
                slot.component_name = "panel_widget_" + entry.id;
            }
        } else {
            slot.component_name = "panel_widget_" + entry.id;
        }

        enabled_widgets.push_back(std::move(slot));
    }

    // If firmware_restart is NOT already in the list (user disabled it),
    // conditionally inject it as the LAST widget when Klipper is NOT READY.
    // This ensures the restart button is always reachable during shutdown, error,
    // or startup (e.g., stuck trying to connect to an MCU).
    bool has_firmware_restart = false;
    for (const auto& slot : enabled_widgets) {
        if (slot.widget_id == "firmware_restart") {
            has_firmware_restart = true;
            break;
        }
    }
    if (!has_firmware_restart) {
        lv_subject_t* klippy = lv_xml_get_subject(nullptr, "klippy_state");
        if (klippy) {
            int state = lv_subject_get_int(klippy);
            if (state != static_cast<int>(KlippyState::READY)) {
                const char* state_names[] = {"READY", "STARTUP", "SHUTDOWN", "ERROR"};
                const char* name = (state >= 0 && state <= 3) ? state_names[state] : "UNKNOWN";
                WidgetSlot slot;
                slot.widget_id = "firmware_restart";
                slot.component_name = "panel_widget_firmware_restart";
                enabled_widgets.push_back(std::move(slot));
                spdlog::debug("[PanelWidgetManager] Injected firmware_restart (Klipper {})", name);
            }
        }
    }

    if (enabled_widgets.empty()) {
        populating_ = false;
        return {};
    }

    // --- Grid layout: compute placements first, then build minimal grid ---

    // Get current breakpoint for column count
    lv_subject_t* bp_subj = theme_manager_get_breakpoint_subject();
    int breakpoint = bp_subj ? lv_subject_get_int(bp_subj) : 2; // Default to MEDIUM

    // Build grid placement tracker to compute positions
    GridLayout grid(breakpoint);

    // Correlate widget entries with config entries to get grid positions
    const auto& entries = widget_config.entries();

    // First pass: compute all placements (determine grid positions)
    struct PlacedSlot {
        size_t slot_index; // Index into enabled_widgets
        int col, row, colspan, rowspan;
    };
    std::vector<PlacedSlot> placed;

    for (size_t i = 0; i < enabled_widgets.size(); ++i) {
        auto& slot = enabled_widgets[i];
        int col, row, colspan, rowspan;

        // Look up the widget entry from config for explicit grid coords
        auto entry_it =
            std::find_if(entries.begin(), entries.end(),
                         [&](const PanelWidgetEntry& e) { return e.id == slot.widget_id; });

        if (entry_it != entries.end() && entry_it->has_grid_position()) {
            col = entry_it->col;
            row = entry_it->row;
            colspan = entry_it->colspan;
            rowspan = entry_it->rowspan;
        } else {
            // Auto-place: get span from registry def, then find first available spot
            const auto* def = find_widget_def(slot.widget_id);
            colspan = def ? def->colspan : 1;
            rowspan = def ? def->rowspan : 1;

            auto pos = grid.find_available(colspan, rowspan);
            if (!pos) {
                spdlog::warn("[PanelWidgetManager] No grid space for widget '{}'", slot.widget_id);
                continue;
            }
            col = pos->first;
            row = pos->second;
        }

        // Validate and register placement in the grid tracker
        if (!grid.place({slot.widget_id, col, row, colspan, rowspan})) {
            spdlog::warn("[PanelWidgetManager] Cannot place widget '{}' at ({},{} {}x{})",
                         slot.widget_id, col, row, colspan, rowspan);
            continue;
        }

        placed.push_back({i, col, row, colspan, rowspan});
    }

    // Compute the actual number of rows used (not the full breakpoint row count)
    int max_row_used = 0;
    for (const auto& p : placed) {
        int bottom = p.row + p.rowspan;
        if (bottom > max_row_used) {
            max_row_used = bottom;
        }
    }
    if (max_row_used == 0) {
        max_row_used = 1; // At least 1 row if any widgets placed
    }

    // Generate grid descriptors sized to actual content
    // Columns: use breakpoint column count (fills available width)
    // Rows: only create rows that are actually occupied (avoids empty rows stealing space)
    auto& dsc = grid_descriptors_[panel_id];
    dsc.col_dsc = GridLayout::make_col_dsc(breakpoint);
    dsc.row_dsc.clear();
    for (int r = 0; r < max_row_used; ++r) {
        dsc.row_dsc.push_back(LV_GRID_FR(1));
    }
    dsc.row_dsc.push_back(LV_GRID_TEMPLATE_LAST);

    // Set up grid on container
    lv_obj_set_layout(container, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(container, dsc.col_dsc.data(), dsc.row_dsc.data());
    lv_obj_set_style_pad_column(container, theme_manager_get_spacing("space_xs"), 0);
    lv_obj_set_style_pad_row(container, theme_manager_get_spacing("space_xs"), 0);

    spdlog::debug("[PanelWidgetManager] Grid layout: {}cols x {}rows (bp={}) for '{}'",
                  GridLayout::get_cols(breakpoint), max_row_used, breakpoint, panel_id);

    // Second pass: create XML components and place in grid cells
    std::vector<std::unique_ptr<PanelWidget>> result;

    for (const auto& p : placed) {
        auto& slot = enabled_widgets[p.slot_index];

        // Create XML component
        auto* widget =
            static_cast<lv_obj_t*>(lv_xml_create(container, slot.component_name.c_str(), nullptr));
        if (!widget) {
            spdlog::warn("[PanelWidgetManager] Failed to create widget: {} (component: {})",
                         slot.widget_id, slot.component_name);
            continue;
        }

        // Place in grid cell
        lv_obj_set_grid_cell(widget, LV_GRID_ALIGN_STRETCH, p.col, p.colspan, LV_GRID_ALIGN_STRETCH,
                             p.row, p.rowspan);

        spdlog::debug("[PanelWidgetManager] Placed widget '{}' at ({},{} {}x{})", slot.widget_id,
                      p.col, p.row, p.colspan, p.rowspan);

        // Attach the pre-created PanelWidget instance if present
        if (slot.instance) {
            slot.instance->attach(widget, lv_scr_act());
            // Row density: approximate as widgets per row in the grid
            int cols = GridLayout::get_cols(breakpoint);
            slot.instance->set_row_density(static_cast<size_t>(cols));
            result.push_back(std::move(slot.instance));
        }

        // Propagate row density to AMS mini status (pure XML widget, no PanelWidget)
        if (slot.widget_id == "ams") {
            lv_obj_t* ams_child = lv_obj_get_child(widget, 0);
            if (ams_child && ui_ams_mini_status_is_valid(ams_child)) {
                ui_ams_mini_status_set_row_density(ams_child, GridLayout::get_cols(breakpoint));
            }
        }
    }

    spdlog::debug("[PanelWidgetManager] Populated {} widgets ({} with factories) via grid for '{}'",
                  placed.size(), result.size(), panel_id);

    populating_ = false;
    return result;
}

void PanelWidgetManager::setup_gate_observers(const std::string& panel_id,
                                              RebuildCallback rebuild_cb) {
    using helix::ui::observe_int_sync;

    // Clear any existing observers for this panel
    gate_observers_.erase(panel_id);
    auto& observers = gate_observers_[panel_id];

    // Collect unique gate subject names from the widget registry
    std::vector<const char*> gate_names;
    for (const auto& def : get_all_widget_defs()) {
        if (def.hardware_gate_subject) {
            // Avoid duplicates
            bool found = false;
            for (const auto* existing : gate_names) {
                if (std::strcmp(existing, def.hardware_gate_subject) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                gate_names.push_back(def.hardware_gate_subject);
            }
        }
    }

    // Also observe klippy_state for firmware_restart conditional injection
    gate_names.push_back("klippy_state");

    for (const auto* name : gate_names) {
        lv_subject_t* subject = lv_xml_get_subject(nullptr, name);
        if (!subject) {
            spdlog::trace("[PanelWidgetManager] Gate subject '{}' not registered yet", name);
            continue;
        }

        // Use observe_int_sync with PanelWidgetManager as the class template parameter.
        // The callback ignores the value and just triggers the panel's rebuild.
        observers.push_back(observe_int_sync<PanelWidgetManager>(
            subject, this,
            [rebuild_cb](PanelWidgetManager* /*self*/, int /*value*/) { rebuild_cb(); }));

        spdlog::trace("[PanelWidgetManager] Observing gate subject '{}' for panel '{}'", name,
                      panel_id);
    }

    spdlog::debug("[PanelWidgetManager] Set up {} gate observers for panel '{}'", observers.size(),
                  panel_id);
}

void PanelWidgetManager::clear_gate_observers(const std::string& panel_id) {
    auto it = gate_observers_.find(panel_id);
    if (it != gate_observers_.end()) {
        spdlog::debug("[PanelWidgetManager] Clearing {} gate observers for panel '{}'",
                      it->second.size(), panel_id);
        gate_observers_.erase(it);
    }
}

} // namespace helix
