// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_ams_overview.h"

#include "ui_ams_device_operations_overlay.h"
#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_panel_ams.h"
#include "ui_panel_common.h"
#include "ui_system_path_canvas.h"
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

/// Height of each mini slot bar
/// TODO: Replace with theme_manager_get_spacing("ams_bars_height") to use
/// the responsive value from globals.xml instead of this compile-time constant.
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
    LVGL_SAFE_EVENT_CB_BEGIN("[AMS Overview] on_settings_clicked");
    LV_UNUSED(e);

    spdlog::info("[AMS Overview] Opening AMS Device Operations overlay");

    auto& overlay = helix::ui::get_ams_device_operations_overlay();
    if (!overlay.are_subjects_initialized()) {
        overlay.init_subjects();
        overlay.register_callbacks();
    }

    auto* target = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    lv_obj_t* parent = lv_obj_get_screen(target);
    overlay.show(parent);

    LVGL_SAFE_EVENT_CB_END();
}

static void on_unload_clicked_xml(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AMS Overview] on_unload_clicked");
    LV_UNUSED(e);

    spdlog::info("[AMS Overview] Unload requested");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (backend) {
        AmsError error = backend->unload_filament();
        if (error.result != AmsResult::SUCCESS) {
            NOTIFY_ERROR("Unload failed: {}", error.user_msg);
        }
    } else {
        NOTIFY_WARNING("AMS not available");
    }

    LVGL_SAFE_EVENT_CB_END();
}

static void on_reset_clicked_xml(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AMS Overview] on_reset_clicked");
    LV_UNUSED(e);

    spdlog::info("[AMS Overview] Reset requested");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (backend) {
        AmsError error = backend->reset();
        if (error.result != AmsResult::SUCCESS) {
            NOTIFY_ERROR("Reset failed: {}", error.user_msg);
        }
    } else {
        NOTIFY_WARNING("AMS not available");
    }

    LVGL_SAFE_EVENT_CB_END();
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

    // Find system path area and create path canvas widget
    lv_obj_t* path_area = lv_obj_find_by_name(panel_, "system_path_area");
    if (path_area) {
        system_path_ = ui_system_path_canvas_create(path_area);
        if (system_path_) {
            lv_obj_set_size(system_path_, LV_PCT(100), LV_PCT(100));
            spdlog::debug("[{}] Created system path canvas", get_name());
        }
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

    // Update system path visualization
    refresh_system_path(info, current_slot);
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
        uc.logo_image = lv_obj_find_by_name(uc.card, "unit_logo");
        uc.name_label = lv_obj_find_by_name(uc.card, "unit_name");
        uc.bars_container = lv_obj_find_by_name(uc.card, "bars_container");
        uc.slot_count_label = lv_obj_find_by_name(uc.card, "slot_count");

        // Set logo image based on AMS system type
        if (uc.logo_image) {
            // Try unit name first (e.g., "Box Turtle 1", "Night Owl"),
            // fall back to system type name (e.g., "AFC", "Happy Hare")
            const char* logo_path = AmsState::get_logo_path(unit.name);
            if (!logo_path || !logo_path[0]) {
                logo_path = AmsState::get_logo_path(info.type_name);
            }
            if (logo_path && logo_path[0]) {
                lv_image_set_src(uc.logo_image, logo_path);
            } else {
                lv_obj_add_flag(uc.logo_image, LV_OBJ_FLAG_HIDDEN);
            }
        }

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

    spdlog::debug("[{}] Created {} unit cards from XML (bypass={})", get_name(),
                  static_cast<int>(unit_cards_.size()), info.supports_bypass);
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
// System Path
// ============================================================================

void AmsOverviewPanel::refresh_system_path(const AmsSystemInfo& info, int current_slot) {
    if (!system_path_)
        return;

    int unit_count = static_cast<int>(info.units.size());
    ui_system_path_canvas_set_unit_count(system_path_, unit_count);

    // Calculate and set X positions based on unit card positions
    // Force layout so we can get accurate card positions
    if (cards_row_) {
        lv_obj_update_layout(cards_row_);
    }

    for (int i = 0; i < unit_count && i < static_cast<int>(unit_cards_.size()); ++i) {
        if (unit_cards_[i].card) {
            // Get card center X relative to the system path widget's parent
            lv_obj_update_layout(unit_cards_[i].card);
            lv_area_t card_coords;
            lv_obj_get_coords(unit_cards_[i].card, &card_coords);

            // Get system path widget position for relative offset
            if (system_path_) {
                lv_area_t path_coords;
                lv_obj_get_coords(system_path_, &path_coords);
                int32_t card_center_x = (card_coords.x1 + card_coords.x2) / 2 - path_coords.x1;
                ui_system_path_canvas_set_unit_x(system_path_, i, card_center_x);
            }
        }
    }

    // Set active unit based on current slot
    int active_unit = info.get_active_unit_index();
    ui_system_path_canvas_set_active_unit(system_path_, active_unit);

    // Set filament color from active slot
    if (current_slot >= 0) {
        const SlotInfo* slot = info.get_slot_global(current_slot);
        if (slot) {
            ui_system_path_canvas_set_active_color(system_path_, slot->color_rgb);
        }
    }

    // Set whether filament is fully loaded
    ui_system_path_canvas_set_filament_loaded(system_path_, info.filament_loaded);

    // Set bypass path state (bypass is drawn inside the canvas, no card needed)
    if (info.supports_bypass) {
        bool bypass_active = (current_slot == -2);
        ui_system_path_canvas_set_bypass(system_path_, true, bypass_active, 0x888888);
    } else {
        ui_system_path_canvas_set_bypass(system_path_, false, false, 0x888888);
    }

    // Set per-unit hub sensor states
    for (int i = 0; i < unit_count && i < static_cast<int>(info.units.size()); ++i) {
        ui_system_path_canvas_set_unit_hub_sensor(system_path_, i, info.units[i].has_hub_sensor,
                                                  info.units[i].hub_sensor_triggered);
    }

    // Set toolhead sensor state
    {
        auto segment = static_cast<PathSegment>(
            lv_subject_get_int(AmsState::instance().get_path_filament_segment_subject()));
        bool toolhead_triggered = (segment >= PathSegment::TOOLHEAD);

        bool has_toolhead = false;
        for (const auto& unit : info.units) {
            if (unit.has_toolhead_sensor)
                has_toolhead = true;
        }

        ui_system_path_canvas_set_toolhead_sensor(system_path_, has_toolhead, toolhead_triggered);
    }

    // Update currently loaded swatch color (imperative — color subject is int, not CSS)
    if (panel_) {
        lv_obj_t* swatch = lv_obj_find_by_name(panel_, "overview_swatch");
        if (swatch) {
            lv_color_t color = lv_color_hex(static_cast<uint32_t>(
                lv_subject_get_int(AmsState::instance().get_current_color_subject())));
            lv_obj_set_style_bg_color(swatch, color, 0);
            lv_obj_set_style_border_color(swatch, color, 0);
        }
    }

    // Set status text from action detail subject (drawn to left of nozzle)
    lv_subject_t* action_subject = AmsState::instance().get_ams_action_detail_subject();
    if (action_subject) {
        const char* action_text = lv_subject_get_string(action_subject);
        ui_system_path_canvas_set_status_text(system_path_, action_text);
    }

    ui_system_path_canvas_refresh(system_path_);
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
    system_path_ = nullptr;
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
    lv_xml_register_event_cb(nullptr, "on_ams_overview_unload_clicked", on_unload_clicked_xml);
    lv_xml_register_event_cb(nullptr, "on_ams_overview_reset_clicked", on_reset_clicked_xml);

    // Register the system path canvas widget
    ui_system_path_canvas_register();

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
