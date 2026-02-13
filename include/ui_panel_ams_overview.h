// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_observer_guard.h"
#include "ui_panel_base.h"

#include "ams_state.h"
#include "ams_types.h"

#include <vector>

/**
 * @file ui_panel_ams_overview.h
 * @brief Multi-unit AMS system overview panel
 *
 * Shows a zoomed-out view of all AMS units as compact cards.
 * Each card displays slot color bars (reusing ams_mini_status visual pattern).
 * Clicking a unit card opens the detail view scoped to that unit.
 *
 * Only shown for multi-unit setups (2+ units). Single-unit setups
 * skip this and go directly to the AMS detail panel.
 */
class AmsOverviewPanel : public PanelBase {
  public:
    AmsOverviewPanel(PrinterState& printer_state, MoonrakerAPI* api);
    ~AmsOverviewPanel() override = default;

    // === PanelBase Interface ===
    void init_subjects() override;
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    void on_activate() override;
    void on_deactivate() override;

    [[nodiscard]] const char* get_name() const override {
        return "AMS Overview";
    }
    [[nodiscard]] const char* get_xml_component_name() const override {
        return "ams_overview_panel";
    }

    [[nodiscard]] lv_obj_t* get_panel() const {
        return panel_;
    }

    /**
     * @brief Refresh unit cards from backend state
     */
    void refresh_units();

    /**
     * @brief Clear panel reference before UI destruction
     */
    void clear_panel_reference();

  private:
    // === Unit Card Management ===
    struct UnitCard {
        lv_obj_t* card = nullptr;             // Card container (clickable)
        lv_obj_t* logo_image = nullptr;       // AMS type logo
        lv_obj_t* name_label = nullptr;       // Unit name
        lv_obj_t* bars_container = nullptr;   // Mini status bars
        lv_obj_t* slot_count_label = nullptr; // "4 slots"
        int unit_index = -1;
    };

    std::vector<UnitCard> unit_cards_;
    lv_obj_t* cards_row_ = nullptr;
    lv_obj_t* system_path_ = nullptr;

    // === Observers ===
    ObserverGuard slots_version_observer_;

    // === Setup Helpers ===
    void create_unit_cards(const AmsSystemInfo& info);
    void update_unit_card(UnitCard& card, const AmsUnit& unit, int current_slot);
    void create_mini_bars(UnitCard& card, const AmsUnit& unit, int current_slot);
    void refresh_system_path(const AmsSystemInfo& info, int current_slot);

    // === Event Handling ===
    static void on_unit_card_clicked(lv_event_t* e);
};

/**
 * @brief Get global AMS overview panel singleton
 */
AmsOverviewPanel& get_global_ams_overview_panel();

/**
 * @brief Destroy the AMS overview panel UI
 */
void destroy_ams_overview_panel_ui();

/**
 * @brief Navigate to AMS panel with multi-unit awareness
 *
 * If multi-unit: push overview panel
 * If single-unit: push detail panel directly (unchanged behavior)
 */
void navigate_to_ams_panel();
