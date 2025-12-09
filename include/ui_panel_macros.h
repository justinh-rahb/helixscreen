// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_panel_base.h"

#include "lvgl.h"
#include "moonraker_api.h"
#include "printer_state.h"

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @file ui_panel_macros.h
 * @brief Klipper macro execution panel
 *
 * Displays all available Klipper macros and allows single-tap execution.
 * Macros are fetched from PrinterCapabilities after discovery.
 *
 * ## Features
 * - Lists all detected gcode_macro entries from Klipper
 * - Filters system macros (_* prefix) by default
 * - Executes macros via MoonrakerAPI::execute_gcode()
 * - Empty state when no macros available
 *
 * ## Usage
 * Panel is accessed via navigation from controls or settings panel.
 * Uses `macro_card.xml` component for each macro entry.
 */
class MacrosPanel : public PanelBase {
  public:
    /**
     * @brief Construct MacrosPanel
     * @param printer_state Reference to global printer state
     * @param api Pointer to MoonrakerAPI (may be nullptr in test mode)
     */
    MacrosPanel(PrinterState& printer_state, MoonrakerAPI* api);

    // PanelBase interface
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    [[nodiscard]] const char* get_name() const override {
        return "Macros";
    }
    [[nodiscard]] const char* get_xml_component_name() const override {
        return "macro_panel";
    }
    void init_subjects() override;

    /**
     * @brief Static callback for macro card clicks
     *
     * Registered globally via lv_xml_register_event_cb().
     * Routes to instance method via global accessor.
     */
    static void on_macro_card_clicked(lv_event_t* e);

  private:
    /**
     * @brief Information about a displayed macro
     */
    struct MacroEntry {
        lv_obj_t* card = nullptr;  ///< The macro_card widget
        std::string name;          ///< Macro name (uppercase)
        std::string display_name;  ///< Display name (prettified)
        bool is_system = false;    ///< True if _* prefix
        bool is_dangerous = false; ///< True if potentially destructive
    };

    /**
     * @brief Populate the macro list from capabilities
     */
    void populate_macro_list();

    /**
     * @brief Create a macro card widget
     * @param macro_name The macro name to display
     */
    void create_macro_card(const std::string& macro_name);

    /**
     * @brief Clear all macro cards
     */
    void clear_macro_list();

    /**
     * @brief Execute a macro by name
     * @param macro_name The macro to execute (e.g., "CLEAN_NOZZLE")
     */
    void execute_macro(const std::string& macro_name);

    /**
     * @brief Prettify a macro name for display
     *
     * Converts "CLEAN_NOZZLE" to "Clean Nozzle", handles prefixes.
     *
     * @param name Raw macro name
     * @return Prettified display name
     */
    static std::string prettify_macro_name(const std::string& name);

    /**
     * @brief Check if macro is potentially dangerous
     * @param name Macro name
     * @return true if macro could cause issues (SAVE_CONFIG, FIRMWARE_RESTART, etc.)
     */
    static bool is_dangerous_macro(const std::string& name);

    /**
     * @brief Toggle system macro visibility
     * @param show_system If true, show _* macros
     */
    void set_show_system_macros(bool show_system);

    // Widget references
    lv_obj_t* macro_list_container_ = nullptr;  ///< Scrollable container for macro cards
    lv_obj_t* empty_state_container_ = nullptr; ///< Shown when no macros
    lv_obj_t* status_label_ = nullptr;          ///< Status message label
    lv_obj_t* system_toggle_ = nullptr;         ///< Toggle for showing system macros

    // Data
    std::vector<MacroEntry> macro_entries_; ///< All displayed macro cards
    bool show_system_macros_ = false;       ///< Whether to show _* macros

    // Subjects
    bool subjects_initialized_ = false;
    char status_buf_[64] = {};
    lv_subject_t status_subject_{};
};

/**
 * @brief Get the global MacrosPanel instance
 *
 * Creates the instance on first call. Used by static callbacks.
 *
 * @return Reference to singleton MacrosPanel
 */
MacrosPanel& get_global_macros_panel();
