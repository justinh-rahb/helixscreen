// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_macros.h"

#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_panel_common.h"
#include "ui_subject_registry.h"

#include "app_globals.h"
#include "moonraker_client.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

/**
 * Dangerous macros that could cause issues if accidentally triggered
 */
const std::unordered_set<std::string> DANGEROUS_MACROS = {
    "SAVE_CONFIG",    "FIRMWARE_RESTART", "RESTART", "SHUTDOWN",
    "M112", // Emergency stop
    "EMERGENCY_STOP",
};

} // namespace

MacrosPanel::MacrosPanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    std::snprintf(status_buf_, sizeof(status_buf_), "Loading macros...");
}

void MacrosPanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    // Initialize status subject for reactive binding
    UI_SUBJECT_INIT_AND_REGISTER_STRING(status_subject_, status_buf_, status_buf_, "macros_status");

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized: macros_status", get_name());
}

void MacrosPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::info("[{}] Setting up event handlers...", get_name());

    // Register XML event callback (once)
    static bool callbacks_registered = false;
    if (!callbacks_registered) {
        lv_xml_register_event_cb(nullptr, "on_macro_card_clicked", on_macro_card_clicked);
        callbacks_registered = true;
    }

    // Use standard overlay panel setup (wires header, back button, handles responsive padding)
    ui_overlay_panel_setup_standard(panel_, parent_screen_, "overlay_header", "overlay_content");

    // Find widget references
    lv_obj_t* overlay_content = lv_obj_find_by_name(panel_, "overlay_content");
    if (overlay_content) {
        macro_list_container_ = lv_obj_find_by_name(overlay_content, "macro_list");
        empty_state_container_ = lv_obj_find_by_name(overlay_content, "empty_state");
        status_label_ = lv_obj_find_by_name(overlay_content, "status_message");
        system_toggle_ = lv_obj_find_by_name(overlay_content, "show_system_toggle");
    }

    if (!macro_list_container_) {
        spdlog::error("[{}] macro_list container not found!", get_name());
        return;
    }

    // Populate macros from capabilities
    populate_macro_list();

    spdlog::info("[{}] Setup complete!", get_name());
}

void MacrosPanel::clear_macro_list() {
    for (auto& entry : macro_entries_) {
        if (entry.card) {
            lv_obj_delete(entry.card);
        }
    }
    macro_entries_.clear();
}

void MacrosPanel::populate_macro_list() {
    clear_macro_list();

    // Get macros from capabilities
    MoonrakerClient* client = get_moonraker_client();
    if (!client) {
        spdlog::warn("[{}] No MoonrakerClient available", get_name());
        std::snprintf(status_buf_, sizeof(status_buf_), "Not connected to printer");
        lv_subject_copy_string(&status_subject_, status_buf_);
        return;
    }

    const auto& macros = client->capabilities().macros();

    // Sort macros alphabetically for consistent display
    std::vector<std::string> sorted_macros(macros.begin(), macros.end());
    std::sort(sorted_macros.begin(), sorted_macros.end());

    // Filter and create cards
    int visible_count = 0;
    for (const auto& macro_name : sorted_macros) {
        // Skip system macros if not showing them
        bool is_system = !macro_name.empty() && macro_name[0] == '_';
        if (is_system && !show_system_macros_) {
            continue;
        }

        create_macro_card(macro_name);
        visible_count++;
    }

    // Toggle visibility: show macro list OR empty state
    bool has_macros = visible_count > 0;

    if (macro_list_container_) {
        if (has_macros) {
            lv_obj_remove_flag(macro_list_container_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(macro_list_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (empty_state_container_) {
        if (has_macros) {
            lv_obj_add_flag(empty_state_container_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(empty_state_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update status
    if (has_macros) {
        std::snprintf(status_buf_, sizeof(status_buf_), "");
    } else {
        std::snprintf(status_buf_, sizeof(status_buf_), "No macros found");
    }
    lv_subject_copy_string(&status_subject_, status_buf_);

    spdlog::info("[{}] Displayed {} macros ({} total in capabilities)", get_name(), visible_count,
                 macros.size());
}

void MacrosPanel::create_macro_card(const std::string& macro_name) {
    if (!macro_list_container_) {
        return;
    }

    // Prettify the macro name for display
    std::string display_name = prettify_macro_name(macro_name);

    // Create card using XML component
    const char* attrs[] = {"macro_name", display_name.c_str(), nullptr, nullptr};
    lv_obj_t* card =
        static_cast<lv_obj_t*>(lv_xml_create(macro_list_container_, "macro_card", attrs));

    if (!card) {
        spdlog::error("[{}] Failed to create macro_card for '{}'", get_name(), macro_name);
        return;
    }

    // Check if dangerous macro - style differently
    bool is_dangerous = is_dangerous_macro(macro_name);
    if (is_dangerous) {
        // Add warning icon or change color
        lv_obj_t* icon = lv_obj_find_by_name(card, "macro_icon");
        if (icon) {
            // Could change icon to alert-circle or similar
            // For now, leave as-is - could enhance later
        }
    }

    // Store entry info
    MacroEntry entry;
    entry.card = card;
    entry.name = macro_name;
    entry.display_name = display_name;
    entry.is_system = !macro_name.empty() && macro_name[0] == '_';
    entry.is_dangerous = is_dangerous;
    macro_entries_.push_back(entry);

    // Store pointer to MacroEntry in card's user_data for callback lookup
    lv_obj_set_user_data(card, &macro_entries_.back());

    spdlog::debug("[{}] Created card for macro '{}' (dangerous: {})", get_name(), macro_name,
                  is_dangerous);
}

std::string MacrosPanel::prettify_macro_name(const std::string& name) {
    if (name.empty()) {
        return name;
    }

    std::string result;
    bool capitalize_next = true;

    // Skip leading underscore for display
    size_t start = (name[0] == '_') ? 1 : 0;

    // Also skip common prefixes for cleaner display
    if (name.rfind("HELIX_", 0) == 0) {
        start = 6;
    }

    for (size_t i = start; i < name.size(); ++i) {
        char c = name[i];

        if (c == '_') {
            result += ' ';
            capitalize_next = true;
        } else if (capitalize_next) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize_next = false;
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    return result.empty() ? name : result;
}

bool MacrosPanel::is_dangerous_macro(const std::string& name) {
    // Check against known dangerous macros
    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return DANGEROUS_MACROS.count(upper_name) > 0;
}

void MacrosPanel::execute_macro(const std::string& macro_name) {
    if (!api_) {
        spdlog::warn("[{}] No MoonrakerAPI available - cannot execute macro", get_name());
        return;
    }

    spdlog::info("[{}] Executing macro: {}", get_name(), macro_name);

    // Execute via G-code (macros are just G-code commands)
    api_->execute_gcode(
        macro_name,
        [this, macro_name]() {
            spdlog::info("[{}] Macro '{}' executed successfully", get_name(), macro_name);
            // Could show toast notification here
        },
        [this, macro_name](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to execute macro '{}': {}", get_name(), macro_name,
                          err.message);
            std::snprintf(status_buf_, sizeof(status_buf_), "Failed: %s", macro_name.c_str());
            lv_subject_copy_string(&status_subject_, status_buf_);
        });
}

void MacrosPanel::set_show_system_macros(bool show_system) {
    if (show_system_macros_ != show_system) {
        show_system_macros_ = show_system;
        populate_macro_list(); // Refresh list
    }
}

void MacrosPanel::on_macro_card_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[MacrosPanel] on_macro_card_clicked");

    // Get the global instance
    auto& self = get_global_macros_panel();

    lv_obj_t* card = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!card) {
        spdlog::warn("[MacrosPanel] No target in click event");
    } else {
        auto* entry = static_cast<MacroEntry*>(lv_obj_get_user_data(card));
        if (!entry) {
            spdlog::warn("[MacrosPanel] Card has no MacroEntry user_data");
        } else {
            // For dangerous macros, could show confirmation dialog
            // For now, execute directly
            if (entry->is_dangerous) {
                spdlog::warn("[MacrosPanel] Executing dangerous macro: {}", entry->name);
                // TODO: Add confirmation modal for dangerous macros
            }
            self.execute_macro(entry->name);
        }
    }

    LVGL_SAFE_EVENT_CB_END();
}

// Global instance accessor
static std::unique_ptr<MacrosPanel> g_macros_panel;

MacrosPanel& get_global_macros_panel() {
    if (!g_macros_panel) {
        g_macros_panel = std::make_unique<MacrosPanel>(get_printer_state(), get_moonraker_api());
    }
    return *g_macros_panel;
}
