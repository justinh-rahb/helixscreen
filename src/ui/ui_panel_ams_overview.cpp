// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_ams_overview.h"

#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_panel_ams.h"
#include "ui_panel_common.h"
#include "ui_utils.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "ams_types.h"
#include "app_globals.h"
#include "lvgl/src/xml/lv_xml.h"
#include "observer_factory.h"
#include "static_panel_registry.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>

// ============================================================================
// Layout Constants
// ============================================================================

/// Minimum bar width for mini slot bars (prevents invisible bars)
static constexpr int32_t MINI_BAR_MIN_WIDTH_PX = 6;

/// Maximum bar width for mini slot bars
static constexpr int32_t MINI_BAR_MAX_WIDTH_PX = 14;

/// Height of each mini slot bar (matches ams_unit_card.xml #mini_bar_height)
static constexpr int32_t MINI_BAR_HEIGHT_PX = 40;

/// Border radius for bar corners
static constexpr int32_t MINI_BAR_RADIUS_PX = 4;

/// Height of status indicator line below each bar
static constexpr int32_t STATUS_LINE_HEIGHT_PX = 3;

/// Gap between bar and status line
static constexpr int32_t STATUS_LINE_GAP_PX = 2;

// Global instance pointer for XML callback access
static std::atomic<AmsOverviewPanel*> g_overview_panel_instance{nullptr};

// ============================================================================
// XML Event Callback Wrappers
// ============================================================================

static void on_settings_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    spdlog::debug("[AMS Overview] Settings button clicked");
    // Delegate to the AMS device operations overlay (same as AMS detail panel)
    // This will be wired up when the detail panel infrastructure is available
}

// ============================================================================
// Construction
// ============================================================================

AmsOverviewPanel::AmsOverviewPanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    spdlog::debug("[AMS Overview] Constructed");
}

// ============================================================================
// PanelBase Interface
// ============================================================================

void AmsOverviewPanel::init_subjects() {
    init_subjects_guarded([this]() {
        // AmsState handles all subject registration centrally.
        // Overview panel reuses existing AMS subjects (slots_version, etc.)
        AmsState::instance().init_subjects(true);

        // Observe slots_version to auto-refresh cards when slot data changes
        slots_version_observer_ = ObserverGuard(
            AmsState::instance().get_slots_version_subject(),
            [](lv_observer_t* observer, lv_subject_t* /*subject*/) {
                auto* self = static_cast<AmsOverviewPanel*>(lv_observer_get_user_data(observer));
                if (self && self->panel_) {
                    self->refresh_units();
                }
            },
            this);
    });
}

void AmsOverviewPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::debug("[{}] Setting up...", get_name());

    // Standard overlay panel setup (header bar, responsive padding)
    ui_overlay_panel_setup_standard(panel_, parent_screen_, "overlay_header", "overview_content");

    // Find the unit cards row container from XML
    cards_row_ = lv_obj_find_by_name(panel_, "unit_cards_row");
    if (!cards_row_) {
        spdlog::error("[{}] Could not find 'unit_cards_row' in XML", get_name());
        return;
    }

    // Store global instance for callback access
    g_overview_panel_instance.store(this);

    // Initial population from backend state
    refresh_units();

    spdlog::debug("[{}] Setup complete!", get_name());
}

void AmsOverviewPanel::on_activate() {
    spdlog::debug("[{}] Activated - syncing from backend", get_name());

    AmsState::instance().sync_from_backend();
    refresh_units();
}

void AmsOverviewPanel::on_deactivate() {
    spdlog::debug("[{}] Deactivated", get_name());
}

// ============================================================================
// Unit Card Management
// ============================================================================

void AmsOverviewPanel::refresh_units() {
    if (!cards_row_) {
        return;
    }

    auto* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::debug("[{}] No backend available", get_name());
        return;
    }

    AmsSystemInfo info = backend->get_system_info();
    int current_slot = lv_subject_get_int(AmsState::instance().get_current_slot_subject());

    int new_unit_count = static_cast<int>(info.units.size());
    int old_unit_count = static_cast<int>(unit_cards_.size());

    if (new_unit_count != old_unit_count) {
        // Unit count changed - rebuild all cards
        spdlog::debug("[{}] Unit count changed {} -> {}, rebuilding cards", get_name(),
                      old_unit_count, new_unit_count);
        create_unit_cards(info);
    } else {
        // Same number of units - update existing cards
        for (int i = 0; i < new_unit_count && i < static_cast<int>(unit_cards_.size()); ++i) {
            update_unit_card(unit_cards_[i], info.units[i], current_slot);
        }
    }
}

void AmsOverviewPanel::create_unit_cards(const AmsSystemInfo& info) {
    if (!cards_row_) {
        return;
    }

    // Remove old card widgets
    lv_obj_clean(cards_row_);
    unit_cards_.clear();

    int current_slot = lv_subject_get_int(AmsState::instance().get_current_slot_subject());

    for (int i = 0; i < static_cast<int>(info.units.size()); ++i) {
        const AmsUnit& unit = info.units[i];
        UnitCard uc;
        uc.unit_index = i;

        // Create card from XML component — all static styling is declarative
        uc.card = static_cast<lv_obj_t*>(lv_xml_create(cards_row_, "ams_unit_card", nullptr));
        if (!uc.card) {
            spdlog::error("[{}] Failed to create ams_unit_card XML for unit {}", get_name(), i);
            continue;
        }

        // Flex grow so cards share available width equally
        lv_obj_set_flex_grow(uc.card, 1);

        // Store unit index for click handler
        lv_obj_set_user_data(uc.card, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(uc.card, on_unit_card_clicked, LV_EVENT_CLICKED, this);

        // Find child widgets declared in XML
        uc.name_label = lv_obj_find_by_name(uc.card, "unit_name");
        uc.bars_container = lv_obj_find_by_name(uc.card, "bars_container");
        uc.slot_count_label = lv_obj_find_by_name(uc.card, "slot_count");

        // Set dynamic content only — unit name and slot count vary per unit
        if (uc.name_label) {
            std::string display_name =
                unit.name.empty() ? ("Unit " + std::to_string(i + 1)) : unit.name;
            lv_label_set_text(uc.name_label, display_name.c_str());
        }

        if (uc.slot_count_label) {
            char count_buf[16];
            snprintf(count_buf, sizeof(count_buf), "%d slots", unit.slot_count);
            lv_label_set_text(uc.slot_count_label, count_buf);
        }

        // Create the mini bars for this unit (dynamic — slot count varies)
        create_mini_bars(uc, unit, current_slot);

        unit_cards_.push_back(uc);
    }

    spdlog::debug("[{}] Created {} unit cards from XML", get_name(),
                  static_cast<int>(unit_cards_.size()));
}

void AmsOverviewPanel::update_unit_card(UnitCard& card, const AmsUnit& unit, int current_slot) {
    if (!card.card) {
        return;
    }

    // Update name label
    if (card.name_label) {
        std::string display_name =
            unit.name.empty() ? ("Unit " + std::to_string(card.unit_index + 1)) : unit.name;
        lv_label_set_text(card.name_label, display_name.c_str());
    }

    // Rebuild mini bars (slot colors/status may have changed)
    if (card.bars_container) {
        lv_obj_clean(card.bars_container);
        create_mini_bars(card, unit, current_slot);
    }

    // Update slot count
    if (card.slot_count_label) {
        char count_buf[16];
        snprintf(count_buf, sizeof(count_buf), "%d slots", unit.slot_count);
        lv_label_set_text(card.slot_count_label, count_buf);
    }
}

void AmsOverviewPanel::create_mini_bars(UnitCard& card, const AmsUnit& unit, int current_slot) {
    if (!card.bars_container) {
        return;
    }

    int slot_count = static_cast<int>(unit.slots.size());
    if (slot_count <= 0) {
        return;
    }

    // Calculate bar width to fit within bars_container
    // Force layout to get actual container width, then divide among slots
    lv_obj_update_layout(card.bars_container);
    int32_t container_width = lv_obj_get_content_width(card.bars_container);
    if (container_width <= 0) {
        container_width = 80; // Fallback if layout not yet calculated
    }
    int32_t gap = theme_manager_get_spacing("space_xxs");
    int32_t total_gaps = (slot_count > 1) ? (slot_count - 1) * gap : 0;
    int32_t bar_width = (container_width - total_gaps) / std::max(1, slot_count);
    bar_width = std::clamp(bar_width, MINI_BAR_MIN_WIDTH_PX, MINI_BAR_MAX_WIDTH_PX);

    for (int s = 0; s < slot_count; ++s) {
        const SlotInfo& slot = unit.slots[s];
        int global_idx = unit.first_slot_global_index + s;
        bool is_loaded = (global_idx == current_slot);
        bool is_present =
            (slot.status == SlotStatus::AVAILABLE || slot.status == SlotStatus::LOADED ||
             slot.status == SlotStatus::FROM_BUFFER);
        bool has_error = (slot.status == SlotStatus::BLOCKED);

        // Slot column container (bar + status line)
        lv_obj_t* slot_col = lv_obj_create(card.bars_container);
        lv_obj_remove_flag(slot_col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(slot_col, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(slot_col, bar_width,
                        MINI_BAR_HEIGHT_PX + STATUS_LINE_HEIGHT_PX + STATUS_LINE_GAP_PX);
        lv_obj_set_flex_flow(slot_col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(slot_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(slot_col, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_row(slot_col, STATUS_LINE_GAP_PX, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(slot_col, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(slot_col, 0, LV_PART_MAIN);

        // Bar background (outline container)
        lv_obj_t* bar_bg = lv_obj_create(slot_col);
        lv_obj_remove_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(bar_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(bar_bg, bar_width, MINI_BAR_HEIGHT_PX);
        lv_obj_set_style_radius(bar_bg, MINI_BAR_RADIUS_PX, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar_bg, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar_bg, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar_bg, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(bar_bg, theme_manager_get_color("text_muted"), LV_PART_MAIN);
        lv_obj_set_style_border_opa(bar_bg, is_present ? LV_OPA_50 : LV_OPA_20, LV_PART_MAIN);

        // Fill portion (colored, anchored to bottom)
        if (is_present) {
            lv_obj_t* bar_fill = lv_obj_create(bar_bg);
            lv_obj_remove_flag(bar_fill, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(bar_fill, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_set_width(bar_fill, LV_PCT(100));
            lv_obj_set_style_border_width(bar_fill, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(bar_fill, 0, LV_PART_MAIN);
            lv_obj_set_style_radius(bar_fill, MINI_BAR_RADIUS_PX, LV_PART_MAIN);

            // Color gradient (lighter at top, darker at bottom)
            lv_color_t base_color = lv_color_hex(slot.color_rgb);
            lv_color_t light_color = lv_color_make(std::min(255, base_color.red + 50),
                                                   std::min(255, base_color.green + 50),
                                                   std::min(255, base_color.blue + 50));
            lv_obj_set_style_bg_color(bar_fill, light_color, LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(bar_fill, base_color, LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(bar_fill, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, LV_PART_MAIN);

            // Fill height based on weight percentage (default 100% if unknown)
            float pct = slot.get_remaining_percent();
            int fill_pct = (pct >= 0) ? static_cast<int>(pct) : 100;
            fill_pct = std::clamp(fill_pct, 5, 100); // Minimum 5% so bar is visible
            lv_obj_set_height(bar_fill, LV_PCT(fill_pct));
            lv_obj_align(bar_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        }

        // Status line below bar (green=loaded, red=error)
        lv_obj_t* status_line = lv_obj_create(slot_col);
        lv_obj_remove_flag(status_line, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(status_line, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(status_line, bar_width, STATUS_LINE_HEIGHT_PX);
        lv_obj_set_style_border_width(status_line, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(status_line, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(status_line, MINI_BAR_RADIUS_PX / 2, LV_PART_MAIN);

        if (has_error) {
            lv_obj_set_style_bg_color(status_line, theme_manager_get_color("danger"), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(status_line, LV_OPA_COVER, LV_PART_MAIN);
        } else if (is_loaded) {
            lv_obj_set_style_bg_color(status_line, theme_manager_get_color("success"),
                                      LV_PART_MAIN);
            lv_obj_set_style_bg_opa(status_line, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_add_flag(status_line, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ============================================================================
// Event Handling
// ============================================================================

void AmsOverviewPanel::on_unit_card_clicked(lv_event_t* e) {
    auto* self = static_cast<AmsOverviewPanel*>(lv_event_get_user_data(e));
    if (!self) {
        spdlog::warn("[AMS Overview] Card clicked but panel instance is null");
        return;
    }

    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int unit_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));

    spdlog::info("[AMS Overview] Unit {} clicked", unit_index);

    // Milestone 4: navigate to scoped detail view for this unit
}

// ============================================================================
// Cleanup
// ============================================================================

void AmsOverviewPanel::clear_panel_reference() {
    // Clear observer guards before clearing widget pointers
    slots_version_observer_.reset();

    // Clear global instance pointer
    g_overview_panel_instance.store(nullptr);

    // Clear widget references
    panel_ = nullptr;
    parent_screen_ = nullptr;
    cards_row_ = nullptr;
    unit_cards_.clear();

    // Reset subjects_initialized_ so observers are recreated on next access
    subjects_initialized_ = false;
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<AmsOverviewPanel> g_ams_overview_panel;
static lv_obj_t* s_ams_overview_panel_obj = nullptr;

// Lazy registration flag for XML component
static bool s_overview_registered = false;

static void ensure_overview_registered() {
    if (s_overview_registered) {
        return;
    }

    spdlog::info("[AMS Overview] Lazy-registering XML component");

    // Register XML event callbacks before component registration
    lv_xml_register_event_cb(nullptr, "on_ams_overview_settings_clicked", on_settings_clicked_xml);

    // Register the XML components (unit card must be registered before overview panel)
    lv_xml_register_component_from_file("A:ui_xml/ams_unit_card.xml");
    lv_xml_register_component_from_file("A:ui_xml/ams_overview_panel.xml");

    s_overview_registered = true;
    spdlog::debug("[AMS Overview] XML registration complete");
}

void destroy_ams_overview_panel_ui() {
    if (s_ams_overview_panel_obj) {
        spdlog::info("[AMS Overview] Destroying panel UI to free memory");

        NavigationManager::instance().unregister_overlay_close_callback(s_ams_overview_panel_obj);

        if (g_ams_overview_panel) {
            g_ams_overview_panel->clear_panel_reference();
        }

        lv_obj_safe_delete(s_ams_overview_panel_obj);
    }
}

AmsOverviewPanel& get_global_ams_overview_panel() {
    if (!g_ams_overview_panel) {
        g_ams_overview_panel =
            std::make_unique<AmsOverviewPanel>(get_printer_state(), get_moonraker_api());
        StaticPanelRegistry::instance().register_destroy("AmsOverviewPanel",
                                                         []() { g_ams_overview_panel.reset(); });
    }

    // Lazy create the panel UI if not yet created
    if (!s_ams_overview_panel_obj && g_ams_overview_panel) {
        ensure_overview_registered();

        // Initialize AmsState subjects BEFORE XML creation so bindings work
        AmsState::instance().init_subjects(true);

        // Create the panel on the active screen
        lv_obj_t* screen = lv_scr_act();
        s_ams_overview_panel_obj =
            static_cast<lv_obj_t*>(lv_xml_create(screen, "ams_overview_panel", nullptr));

        if (s_ams_overview_panel_obj) {
            // Initialize panel observers
            if (!g_ams_overview_panel->are_subjects_initialized()) {
                g_ams_overview_panel->init_subjects();
            }

            // Setup the panel
            g_ams_overview_panel->setup(s_ams_overview_panel_obj, screen);
            lv_obj_add_flag(s_ams_overview_panel_obj, LV_OBJ_FLAG_HIDDEN);

            // Register overlay instance for lifecycle management
            NavigationManager::instance().register_overlay_instance(s_ams_overview_panel_obj,
                                                                    g_ams_overview_panel.get());

            // Register close callback to destroy UI when overlay is closed
            NavigationManager::instance().register_overlay_close_callback(
                s_ams_overview_panel_obj, []() { destroy_ams_overview_panel_ui(); });

            spdlog::info("[AMS Overview] Lazy-created panel UI with close callback");
        } else {
            spdlog::error("[AMS Overview] Failed to create panel from XML");
        }
    }

    return *g_ams_overview_panel;
}

// ============================================================================
// Multi-unit Navigation
// ============================================================================

void navigate_to_ams_panel() {
    auto* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AMS] navigate_to_ams_panel called with no backend");
        return;
    }

    AmsSystemInfo info = backend->get_system_info();

    if (info.is_multi_unit()) {
        // Multi-unit: show overview panel
        spdlog::info("[AMS] Multi-unit setup ({} units) - showing overview", info.unit_count());
        auto& overview = get_global_ams_overview_panel();
        lv_obj_t* panel = overview.get_panel();
        if (panel) {
            NavigationManager::instance().push_overlay(panel);
        }
    } else {
        // Single-unit (or no units): go directly to detail panel
        spdlog::info("[AMS] Single-unit setup - showing detail panel directly");
        auto& detail = get_global_ams_panel();
        lv_obj_t* panel = detail.get_panel();
        if (panel) {
            NavigationManager::instance().push_overlay(panel);
        }
    }
}
