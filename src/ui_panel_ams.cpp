// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_ams.h"

#include "ui_ams_slot.h"
#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_filament_path_canvas.h"
#include "ui_fonts.h"
#include "ui_hsv_picker.h"
#include "ui_icon.h"
#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_panel_common.h"
#include "ui_spool_canvas.h"
#include "ui_theme.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "ams_types.h"
#include "app_globals.h"
#include "color_utils.h"
#include "filament_database.h"
#include "moonraker_api.h"
#include "printer_state.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>

// Global instance pointer for XML callback access (atomic for safety during destruction)
static std::atomic<AmsPanel*> g_ams_panel_instance{nullptr};

// Subject for edit modal remaining weight mode (0=view, 1=edit)
// Registered globally for XML binding: <bind_flag_if_eq subject="edit_remaining_mode" .../>
static lv_subject_t s_edit_remaining_mode;
static bool s_edit_remaining_mode_initialized = false;

/**
 * @brief Map AMS system/type name to logo image path
 *
 * Maps both generic firmware names (Happy Hare, AFC) and specific hardware
 * names (ERCF, Box Turtle, etc.) to their logo assets.
 *
 * @param name System or type name (case-insensitive matching)
 * @return Logo path or empty string if no matching logo
 */
static const char* get_ams_logo_path(const std::string& name) {
    // Normalize to lowercase for matching
    std::string lower_name = name;
    for (auto& c : lower_name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Strip common suffixes like " (mock)", " (test)", etc.
    size_t paren_pos = lower_name.find(" (");
    if (paren_pos != std::string::npos) {
        lower_name = lower_name.substr(0, paren_pos);
    }

    // Map system names to logo paths
    // Note: All logos are 64x64 white-on-transparent PNGs
    static const std::unordered_map<std::string, const char*> logo_map = {
        // AFC/Box Turtle (AFC firmware only runs on Box Turtle hardware)
        {"afc", "A:assets/images/ams/box_turtle_64.png"},
        {"box turtle", "A:assets/images/ams/box_turtle_64.png"},
        {"box_turtle", "A:assets/images/ams/box_turtle_64.png"},
        {"boxturtle", "A:assets/images/ams/box_turtle_64.png"},

        // Happy Hare - generic firmware, defaults to ERCF logo
        // (most common hardware running Happy Hare)
        {"happy hare", "A:assets/images/ams/ercf_64.png"},
        {"happy_hare", "A:assets/images/ams/ercf_64.png"},
        {"happyhare", "A:assets/images/ams/ercf_64.png"},

        // Specific hardware types (when detected or configured)
        {"ercf", "A:assets/images/ams/ercf_64.png"},
        {"3ms", "A:assets/images/ams/3ms_64.png"},
        {"tradrack", "A:assets/images/ams/tradrack_64.png"},
        {"mmx", "A:assets/images/ams/mmx_64.png"},
        {"night owl", "A:assets/images/ams/night_owl_64.png"},
        {"night_owl", "A:assets/images/ams/night_owl_64.png"},
        {"nightowl", "A:assets/images/ams/night_owl_64.png"},
        {"quattro box", "A:assets/images/ams/quattro_box_64.png"},
        {"quattro_box", "A:assets/images/ams/quattro_box_64.png"},
        {"quattrobox", "A:assets/images/ams/quattro_box_64.png"},
        {"btt vivid", "A:assets/images/ams/btt_vivid_64.png"},
        {"btt_vivid", "A:assets/images/ams/btt_vivid_64.png"},
        {"bttvivid", "A:assets/images/ams/btt_vivid_64.png"},
        {"vivid", "A:assets/images/ams/btt_vivid_64.png"},
        {"kms", "A:assets/images/ams/kms_64.png"},
    };

    auto it = logo_map.find(lower_name);
    if (it != logo_map.end()) {
        return it->second;
    }
    return nullptr;
}

// Lazy registration flag - widgets and XML registered on first use
static bool s_ams_widgets_registered = false;

// Forward declarations for XML event callbacks (defined later in file)
static void on_unload_clicked_xml(lv_event_t* e);
static void on_reset_clicked_xml(lv_event_t* e);
static void on_bypass_clicked_xml(lv_event_t* e);
static void on_bypass_toggled_xml(lv_event_t* e);
static void on_dryer_open_modal_xml(lv_event_t* e);
static void on_dryer_modal_close_xml(lv_event_t* e);
static void on_dryer_preset_pla_xml(lv_event_t* e);
static void on_dryer_preset_petg_xml(lv_event_t* e);
static void on_dryer_preset_abs_xml(lv_event_t* e);
static void on_dryer_stop_xml(lv_event_t* e);
static void on_dryer_temp_minus_xml(lv_event_t* e);
static void on_dryer_temp_plus_xml(lv_event_t* e);
static void on_dryer_duration_minus_xml(lv_event_t* e);
static void on_dryer_duration_plus_xml(lv_event_t* e);
static void on_dryer_power_toggled_xml(lv_event_t* e);
static void on_context_backdrop_xml(lv_event_t* e);
static void on_context_load_xml(lv_event_t* e);
static void on_context_unload_xml(lv_event_t* e);
static void on_context_edit_xml(lv_event_t* e);
static void on_context_spoolman_xml(lv_event_t* e);
static void on_spoolman_picker_close_xml(lv_event_t* e);
static void on_spoolman_picker_unlink_xml(lv_event_t* e);
static void on_spoolman_spool_item_clicked_xml(lv_event_t* e);

// Edit modal callbacks (referenced in ams_edit_modal.xml)
static void on_edit_modal_close_xml(lv_event_t* e);
static void on_edit_vendor_changed_xml(lv_event_t* e);
static void on_edit_material_changed_xml(lv_event_t* e);
static void on_edit_color_clicked_xml(lv_event_t* e);
static void on_edit_remaining_edit_xml(lv_event_t* e);
static void on_edit_remaining_accept_xml(lv_event_t* e);
static void on_edit_remaining_cancel_xml(lv_event_t* e);
static void on_edit_remaining_changed_xml(lv_event_t* e);
static void on_edit_sync_spoolman_xml(lv_event_t* e);
static void on_edit_reset_xml(lv_event_t* e);
static void on_edit_save_xml(lv_event_t* e);

// Color picker callbacks (referenced in color_picker.xml)
static void on_color_picker_close_xml(lv_event_t* e);
static void on_color_swatch_clicked_xml(lv_event_t* e);
static void on_color_picker_cancel_xml(lv_event_t* e);
static void on_color_picker_select_xml(lv_event_t* e);

/**
 * @brief Register AMS widgets and XML component (lazy, called once on first use)
 *
 * Registers:
 * - spool_canvas: 3D filament spool visualization widget
 * - ams_slot: Individual slot widget with spool and status
 * - filament_path_canvas: Filament routing visualization
 * - ams_panel.xml: Main panel component
 * - ams_context_menu.xml: Slot context menu component
 */
static void ensure_ams_widgets_registered() {
    if (s_ams_widgets_registered) {
        return;
    }

    spdlog::info("[AMS Panel] Lazy-registering AMS widgets and XML components");

    // Register custom widgets (order matters - dependencies first)
    ui_spool_canvas_register();
    ui_ams_slot_register();
    ui_filament_path_canvas_register();

    // Initialize and register edit modal subjects BEFORE XML components
    // (subjects must exist when XML parser encounters <bind_flag_if_eq> elements)
    if (!s_edit_remaining_mode_initialized) {
        lv_subject_init_int(&s_edit_remaining_mode, 0); // 0 = view mode, 1 = edit mode
        lv_xml_register_subject(nullptr, "edit_remaining_mode", &s_edit_remaining_mode);
        s_edit_remaining_mode_initialized = true;
        spdlog::debug("[AMS Panel] Registered edit_remaining_mode subject");
    }

    // Register XML event callbacks BEFORE registering XML components
    // (callbacks must exist when XML parser encounters <event_cb> elements)
    lv_xml_register_event_cb(nullptr, "ams_unload_clicked_cb", on_unload_clicked_xml);
    lv_xml_register_event_cb(nullptr, "ams_reset_clicked_cb", on_reset_clicked_xml);
    lv_xml_register_event_cb(nullptr, "ams_bypass_clicked_cb", on_bypass_clicked_xml);
    lv_xml_register_event_cb(nullptr, "ams_bypass_toggled_cb", on_bypass_toggled_xml);

    // Dryer callbacks (referenced in ams_dryer_card.xml and dryer_presets_modal.xml)
    lv_xml_register_event_cb(nullptr, "dryer_open_modal_cb", on_dryer_open_modal_xml);
    lv_xml_register_event_cb(nullptr, "dryer_modal_close_cb", on_dryer_modal_close_xml);
    lv_xml_register_event_cb(nullptr, "dryer_preset_pla_cb", on_dryer_preset_pla_xml);
    lv_xml_register_event_cb(nullptr, "dryer_preset_petg_cb", on_dryer_preset_petg_xml);
    lv_xml_register_event_cb(nullptr, "dryer_preset_abs_cb", on_dryer_preset_abs_xml);
    lv_xml_register_event_cb(nullptr, "dryer_stop_clicked_cb", on_dryer_stop_xml);
    lv_xml_register_event_cb(nullptr, "dryer_temp_minus_cb", on_dryer_temp_minus_xml);
    lv_xml_register_event_cb(nullptr, "dryer_temp_plus_cb", on_dryer_temp_plus_xml);
    lv_xml_register_event_cb(nullptr, "dryer_duration_minus_cb", on_dryer_duration_minus_xml);
    lv_xml_register_event_cb(nullptr, "dryer_duration_plus_cb", on_dryer_duration_plus_xml);
    lv_xml_register_event_cb(nullptr, "dryer_power_toggled_cb", on_dryer_power_toggled_xml);

    // Context menu callbacks (referenced in ams_context_menu.xml)
    lv_xml_register_event_cb(nullptr, "ams_context_backdrop_cb", on_context_backdrop_xml);
    lv_xml_register_event_cb(nullptr, "ams_context_load_cb", on_context_load_xml);
    lv_xml_register_event_cb(nullptr, "ams_context_unload_cb", on_context_unload_xml);
    lv_xml_register_event_cb(nullptr, "ams_context_edit_cb", on_context_edit_xml);
    lv_xml_register_event_cb(nullptr, "ams_context_spoolman_cb", on_context_spoolman_xml);

    // Spoolman picker callbacks (referenced in spoolman_picker_modal.xml)
    lv_xml_register_event_cb(nullptr, "spoolman_picker_close_cb", on_spoolman_picker_close_xml);
    lv_xml_register_event_cb(nullptr, "spoolman_picker_unlink_cb", on_spoolman_picker_unlink_xml);
    lv_xml_register_event_cb(nullptr, "spoolman_spool_item_clicked_cb",
                             on_spoolman_spool_item_clicked_xml);

    // Edit modal callbacks (referenced in ams_edit_modal.xml)
    lv_xml_register_event_cb(nullptr, "ams_edit_modal_close_cb", on_edit_modal_close_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_vendor_changed_cb", on_edit_vendor_changed_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_material_changed_cb", on_edit_material_changed_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_color_clicked_cb", on_edit_color_clicked_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_remaining_edit_cb", on_edit_remaining_edit_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_remaining_accept_cb", on_edit_remaining_accept_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_remaining_cancel_cb", on_edit_remaining_cancel_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_remaining_changed_cb",
                             on_edit_remaining_changed_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_sync_spoolman_cb", on_edit_sync_spoolman_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_reset_cb", on_edit_reset_xml);
    lv_xml_register_event_cb(nullptr, "ams_edit_save_cb", on_edit_save_xml);

    // Color picker callbacks (referenced in color_picker.xml)
    lv_xml_register_event_cb(nullptr, "color_picker_close_cb", on_color_picker_close_xml);
    lv_xml_register_event_cb(nullptr, "color_swatch_clicked_cb", on_color_swatch_clicked_xml);
    lv_xml_register_event_cb(nullptr, "color_picker_cancel_cb", on_color_picker_cancel_xml);
    lv_xml_register_event_cb(nullptr, "color_picker_select_cb", on_color_picker_select_xml);

    // Register XML components (dryer card must be registered before ams_panel since it's used
    // there)
    lv_xml_register_component_from_file("A:ui_xml/ams_dryer_card.xml");
    lv_xml_register_component_from_file("A:ui_xml/dryer_presets_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/ams_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/ams_context_menu.xml");
    lv_xml_register_component_from_file("A:ui_xml/spoolman_spool_item.xml");
    lv_xml_register_component_from_file("A:ui_xml/spoolman_picker_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/ams_edit_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/color_picker.xml");

    s_ams_widgets_registered = true;
    spdlog::debug("[AMS Panel] Widget and XML registration complete");
}

// ============================================================================
// XML Event Callback Wrappers (for <event_cb> elements in XML)
// ============================================================================

static void on_unload_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_unload();
    }
}

static void on_reset_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_reset();
    }
}

static void on_bypass_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_bypass_toggle();
    }
}

static void on_bypass_toggled_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_bypass_toggle();
    }
}

// Dryer preset callbacks - set modal values (reactive binding updates UI)
// If dryer is already running, also apply new settings
static void on_dryer_preset_pla_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().set_modal_preset(45, 240); // PLA: 45°C, 4h
    // If dryer is running, apply immediately
    AmsBackend* backend = AmsState::instance().get_backend();
    AmsPanel* panel = g_ams_panel_instance.load();
    if (backend && backend->get_dryer_info().active && panel) {
        panel->handle_dryer_preset(45.0f, 240, 50);
    }
}

static void on_dryer_preset_petg_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().set_modal_preset(55, 240); // PETG: 55°C, 4h
    AmsBackend* backend = AmsState::instance().get_backend();
    AmsPanel* panel = g_ams_panel_instance.load();
    if (backend && backend->get_dryer_info().active && panel) {
        panel->handle_dryer_preset(55.0f, 240, 50);
    }
}

static void on_dryer_preset_abs_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().set_modal_preset(65, 240); // ABS: 65°C, 4h
    AmsBackend* backend = AmsState::instance().get_backend();
    AmsPanel* panel = g_ams_panel_instance.load();
    if (backend && backend->get_dryer_info().active && panel) {
        panel->handle_dryer_preset(65.0f, 240, 50);
    }
}

static void on_dryer_stop_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_dryer_stop();
    }
}

static void on_dryer_open_modal_xml(lv_event_t* e) {
    LV_UNUSED(e);
    spdlog::debug("[AmsPanel] Dryer button clicked, showing modal");
    // Show the dryer presets modal by setting visibility subject
    lv_subject_set_int(AmsState::instance().get_dryer_modal_visible_subject(), 1);
}

static void on_dryer_modal_close_xml(lv_event_t* e) {
    LV_UNUSED(e);
    // Hide the dryer presets modal
    lv_subject_set_int(AmsState::instance().get_dryer_modal_visible_subject(), 0);
    spdlog::debug("[AmsPanel] Dryer modal closed");
}

// Dryer modal temperature +/- callbacks
static void on_dryer_temp_minus_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().adjust_modal_temp(-5);
}

static void on_dryer_temp_plus_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().adjust_modal_temp(+5);
}

// Dryer modal duration +/- callbacks
static void on_dryer_duration_minus_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().adjust_modal_duration(-30);
}

static void on_dryer_duration_plus_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsState::instance().adjust_modal_duration(+30);
}

// Dryer power toggle callback
static void on_dryer_power_toggled_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        // Check current state and toggle
        AmsBackend* backend = AmsState::instance().get_backend();
        if (!backend)
            return;

        DryerInfo dryer = backend->get_dryer_info();
        if (dryer.active) {
            // Currently on - stop it
            panel->handle_dryer_stop();
        } else {
            // Currently off - start with modal settings
            int temp = AmsState::instance().get_modal_target_temp();
            int duration = AmsState::instance().get_modal_duration_min();
            panel->handle_dryer_preset(static_cast<float>(temp), duration, 50);
        }
    }
}

// Context menu callbacks (triggered via XML <event_cb>)
static void on_context_backdrop_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->hide_context_menu();
    }
}

static void on_context_load_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_context_load();
    }
}

static void on_context_unload_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_context_unload();
    }
}

static void on_context_edit_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_context_edit();
    }
}

static void on_context_spoolman_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_context_spoolman();
    }
}

// Spoolman picker callbacks
static void on_spoolman_picker_close_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_picker_close();
    }
}

static void on_spoolman_picker_unlink_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_picker_unlink();
    }
}

static void on_spoolman_spool_item_clicked_xml(lv_event_t* e) {
    AmsPanel* panel = g_ams_panel_instance.load();
    if (!panel) {
        return;
    }
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    // spool_id stored in user_data
    auto spool_id = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    panel->handle_picker_spool_selected(spool_id);
}

// Edit modal callbacks
static void on_edit_modal_close_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_modal_close();
    }
}

static void on_edit_vendor_changed_xml(lv_event_t* e) {
    AmsPanel* panel = g_ams_panel_instance.load();
    if (!panel) {
        return;
    }
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int index = lv_dropdown_get_selected(dropdown);
    panel->handle_edit_vendor_changed(index);
}

static void on_edit_material_changed_xml(lv_event_t* e) {
    AmsPanel* panel = g_ams_panel_instance.load();
    if (!panel) {
        return;
    }
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int index = lv_dropdown_get_selected(dropdown);
    panel->handle_edit_material_changed(index);
}

static void on_edit_color_clicked_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_color_clicked();
    }
}

static void on_edit_remaining_changed_xml(lv_event_t* e) {
    AmsPanel* panel = g_ams_panel_instance.load();
    if (!panel) {
        return;
    }
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int value = lv_slider_get_value(slider);
    panel->handle_edit_remaining_changed(value);
}

static void on_edit_remaining_edit_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_remaining_edit();
    }
}

static void on_edit_remaining_accept_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_remaining_accept();
    }
}

static void on_edit_remaining_cancel_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_remaining_cancel();
    }
}

static void on_edit_sync_spoolman_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_sync_spoolman();
    }
}

static void on_edit_reset_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_reset();
    }
}

static void on_edit_save_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_edit_save();
    }
}

// Color picker callback wrappers
static void on_color_picker_close_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_color_picker_close();
    }
}

static void on_color_swatch_clicked_xml(lv_event_t* e) {
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        lv_obj_t* swatch = static_cast<lv_obj_t*>(lv_event_get_target(e));
        panel->handle_color_swatch_clicked(swatch);
    }
}

static void on_color_picker_cancel_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_color_picker_cancel();
    }
}

static void on_color_picker_select_xml(lv_event_t* e) {
    LV_UNUSED(e);
    AmsPanel* panel = g_ams_panel_instance.load();
    if (panel) {
        panel->handle_color_picker_select();
    }
}

// ============================================================================
// Construction
// ============================================================================

AmsPanel::AmsPanel(PrinterState& printer_state, MoonrakerAPI* api) : PanelBase(printer_state, api) {
    spdlog::debug("[AmsPanel] Constructed");
}

// ============================================================================
// PanelBase Interface
// ============================================================================

void AmsPanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    // AmsState handles all subject registration centrally
    // We just ensure it's initialized before panel creation
    AmsState::instance().init_subjects(true);

    // NOTE: Backend creation is handled by:
    // - main.cpp (mock mode at startup)
    // - AmsState::init_backend_from_capabilities() (real printer connection)
    // Panel should NOT create backends - it just observes the existing one.

    // Register observers for state changes
    slots_version_observer_ = ObserverGuard(AmsState::instance().get_slots_version_subject(),
                                            on_slots_version_changed, this);

    action_observer_ =
        ObserverGuard(AmsState::instance().get_ams_action_subject(), on_action_changed, this);

    current_slot_observer_ = ObserverGuard(AmsState::instance().get_current_slot_subject(),
                                           on_current_slot_changed, this);

    // Slot count observer for dynamic slot creation
    slot_count_observer_ =
        ObserverGuard(AmsState::instance().get_slot_count_subject(), on_slot_count_changed, this);

    // Path state observers for filament path visualization
    path_segment_observer_ = ObserverGuard(AmsState::instance().get_path_filament_segment_subject(),
                                           on_path_state_changed, this);
    path_topology_observer_ = ObserverGuard(AmsState::instance().get_path_topology_subject(),
                                            on_path_state_changed, this);

    // Initialize string subjects for edit modal text binding
    lv_subject_init_string(&edit_slot_indicator_subject_, edit_slot_indicator_buf_, nullptr,
                           sizeof(edit_slot_indicator_buf_), "--");
    lv_subject_init_string(&edit_color_name_subject_, edit_color_name_buf_, nullptr,
                           sizeof(edit_color_name_buf_), "");
    lv_subject_init_string(&edit_temp_nozzle_subject_, edit_temp_nozzle_buf_, nullptr,
                           sizeof(edit_temp_nozzle_buf_), "200-230°C");
    lv_subject_init_string(&edit_temp_bed_subject_, edit_temp_bed_buf_, nullptr,
                           sizeof(edit_temp_bed_buf_), "60°C");
    lv_subject_init_string(&edit_remaining_pct_subject_, edit_remaining_pct_buf_, nullptr,
                           sizeof(edit_remaining_pct_buf_), "75%");

    // Initialize string subjects for spoolman picker
    lv_subject_init_string(&picker_slot_indicator_subject_, picker_slot_indicator_buf_, nullptr,
                           sizeof(picker_slot_indicator_buf_), "Assigning to Slot 1");

    // Initialize string subjects for color picker
    lv_subject_init_string(&color_hex_subject_, color_hex_buf_, nullptr, sizeof(color_hex_buf_),
                           "#808080");
    lv_subject_init_string(&color_name_subject_, color_name_buf_, nullptr, sizeof(color_name_buf_),
                           "Gray");

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized via AmsState + observers registered", get_name());
}

void AmsPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::debug("[{}] Setting up...", get_name());

    // Use standard overlay panel setup (header bar, responsive padding)
    ui_overlay_panel_setup_standard(panel_, parent_screen_, "overlay_header", "overlay_content");

    // Setup UI components
    setup_system_header();
    setup_slots();
    setup_action_buttons();
    setup_status_display();
    setup_dryer_card();
    setup_path_canvas();

    // Initial UI sync from backend state
    refresh_slots();

    spdlog::debug("[{}] Setup complete!", get_name());
}

void AmsPanel::on_activate() {
    spdlog::debug("[{}] Activated - syncing from backend", get_name());

    // Sync state when panel becomes visible
    AmsState::instance().sync_from_backend();
    refresh_slots();

    // Sync Spoolman active spool with currently loaded slot
    sync_spoolman_active_spool();
}

void AmsPanel::sync_spoolman_active_spool() {
    if (!api_) {
        return;
    }

    int current_slot = lv_subject_get_int(AmsState::instance().get_current_slot_subject());
    if (current_slot < 0) {
        return; // No active slot
    }

    auto* backend = AmsState::instance().get_backend();
    if (!backend) {
        return;
    }

    SlotInfo slot_info = backend->get_slot_info(current_slot);
    if (slot_info.spoolman_id > 0) {
        spdlog::debug("[{}] Syncing Spoolman: slot {} → spool ID {}", get_name(), current_slot,
                      slot_info.spoolman_id);
        api_->set_active_spool(
            slot_info.spoolman_id, []() {},
            [](const MoonrakerError& err) {
                spdlog::warn("[AmsPanel] Failed to sync active spool: {}", err.message);
            });
    }
}

void AmsPanel::on_deactivate() {
    spdlog::debug("[{}] Deactivated", get_name());
    // Note: UI destruction is handled by NavigationManager close callback
    // registered in get_global_ams_panel()
}

void AmsPanel::clear_panel_reference() {
    // Invalidate async callbacks before any widget deletion
    picker_callback_guard_.reset();

    // Delete modals on top layer first (they won't auto-delete with panel_)
    if (dryer_modal_) {
        lv_obj_delete(dryer_modal_);
        dryer_modal_ = nullptr;
    }
    if (spoolman_picker_) {
        lv_obj_delete(spoolman_picker_);
        spoolman_picker_ = nullptr;
    }
    if (context_menu_) {
        lv_obj_delete(context_menu_);
        context_menu_ = nullptr;
    }
    if (edit_modal_) {
        lv_obj_delete(edit_modal_);
        edit_modal_ = nullptr;
    }
    if (color_picker_) {
        lv_obj_delete(color_picker_);
        color_picker_ = nullptr;
    }

    // Clear observer guards BEFORE clearing widget pointers (they reference widgets)
    slots_version_observer_.reset();
    action_observer_.reset();
    current_slot_observer_.reset();
    slot_count_observer_.reset();
    path_segment_observer_.reset();
    path_topology_observer_.reset();
    dryer_progress_observer_.reset();

    // Now clear all widget references
    panel_ = nullptr;
    parent_screen_ = nullptr;
    slot_grid_ = nullptr;
    labels_layer_ = nullptr;
    path_canvas_ = nullptr;
    dryer_progress_fill_ = nullptr;
    context_menu_slot_ = -1;
    picker_target_slot_ = -1;
    edit_slot_index_ = -1;
    current_slot_count_ = 0;

    for (int i = 0; i < MAX_VISIBLE_SLOTS; ++i) {
        slot_widgets_[i] = nullptr;
        label_widgets_[i] = nullptr;
    }

    // Reset subjects_initialized_ so observers are recreated on next access
    subjects_initialized_ = false;

    // Clear global instance pointer to prevent callbacks from using stale pointer
    g_ams_panel_instance.store(nullptr);

    spdlog::debug("[AMS Panel] Cleared all widget references");
}

// ============================================================================
// Setup Helpers
// ============================================================================

void AmsPanel::setup_system_header() {
    // Find the system logo image in the header
    lv_obj_t* system_logo = lv_obj_find_by_name(panel_, "system_logo");
    if (!system_logo) {
        spdlog::warn("[{}] system_logo not found in XML", get_name());
        return;
    }

    // Get AMS system info from backend
    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::debug("[{}] No backend, hiding logo", get_name());
        lv_obj_add_flag(system_logo, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Get system name for logo lookup
    const auto& info = backend->get_system_info();
    const char* logo_path = get_ams_logo_path(info.type_name);

    if (logo_path) {
        spdlog::info("[{}] Setting logo: '{}' -> {}", get_name(), info.type_name, logo_path);
        lv_image_set_src(system_logo, logo_path);
        lv_obj_remove_flag(system_logo, LV_OBJ_FLAG_HIDDEN);
        // Log image dimensions after setting source
        lv_coord_t w = lv_obj_get_width(system_logo);
        lv_coord_t h = lv_obj_get_height(system_logo);
        spdlog::info("[{}] Logo widget size: {}x{}, hidden={}", get_name(), w, h,
                     lv_obj_has_flag(system_logo, LV_OBJ_FLAG_HIDDEN));
    } else {
        // Hide logo for unknown systems
        lv_obj_add_flag(system_logo, LV_OBJ_FLAG_HIDDEN);
        spdlog::debug("[{}] No logo for system '{}'", get_name(), info.type_name);
    }
}

void AmsPanel::setup_slots() {
    slot_grid_ = lv_obj_find_by_name(panel_, "slot_grid");
    if (!slot_grid_) {
        spdlog::warn("[{}] slot_grid not found in XML", get_name());
        return;
    }

    // Find labels layer for z-order (labels render on top of all slots)
    labels_layer_ = lv_obj_find_by_name(panel_, "labels_layer");
    if (!labels_layer_) {
        spdlog::warn(
            "[{}] labels_layer not found in XML - labels may be obscured by overlapping slots",
            get_name());
    }

    // Get initial slot count and create slots
    int slot_count = lv_subject_get_int(AmsState::instance().get_slot_count_subject());
    spdlog::debug("[{}] setup_slots: slot_count={} from subject", get_name(), slot_count);
    create_slots(slot_count);
}

void AmsPanel::create_slots(int count) {
    if (!slot_grid_) {
        return;
    }

    // Clamp to reasonable range
    if (count < 0) {
        count = 0;
    }
    if (count > MAX_VISIBLE_SLOTS) {
        spdlog::warn("[{}] Clamping slot_count {} to max {}", get_name(), count, MAX_VISIBLE_SLOTS);
        count = MAX_VISIBLE_SLOTS;
    }

    // Skip if unchanged
    if (count == current_slot_count_) {
        return;
    }

    spdlog::debug("[{}] Creating {} slots (was {})", get_name(), count, current_slot_count_);

    // Delete existing slots
    for (int i = 0; i < current_slot_count_; ++i) {
        if (slot_widgets_[i]) {
            lv_obj_delete(slot_widgets_[i]);
            slot_widgets_[i] = nullptr;
        }
        // Note: label_widgets_ are no longer used - labels are inside slot widgets
        label_widgets_[i] = nullptr;
    }

    // Create new slots via XML system (widget handles its own sizing/appearance)
    for (int i = 0; i < count; ++i) {
        lv_obj_t* slot = static_cast<lv_obj_t*>(lv_xml_create(slot_grid_, "ams_slot", nullptr));
        if (!slot) {
            spdlog::error("[{}] Failed to create ams_slot for index {}", get_name(), i);
            continue;
        }

        // Configure slot index (triggers reactive binding setup)
        ui_ams_slot_set_index(slot, i);

        // Set layout info for staggered label positioning
        // Each slot positions its own label based on its index and total count
        ui_ams_slot_set_layout_info(slot, i, count);

        // Store reference and setup click handler
        slot_widgets_[i] = slot;
        lv_obj_set_user_data(slot, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(slot, on_slot_clicked, LV_EVENT_CLICKED, this);
    }

    current_slot_count_ = count;

    // Get available width from slot_area (parent of slot_grid)
    lv_obj_t* slot_area = lv_obj_get_parent(slot_grid_);
    lv_obj_update_layout(slot_area); // Ensure layout is current
    int32_t available_width = lv_obj_get_content_width(slot_area);

    // Calculate dynamic slot width and overlap to fill available space
    // Formula: available_width = N * slot_width - (N-1) * overlap
    // With overlap = 50% of slot_width for 5+ gates:
    //   slot_width = available_width / (N * 0.5 + 0.5) for N >= 5
    //   slot_width = available_width / N for N <= 4 (no overlap)
    int32_t slot_width = 0;
    int32_t overlap = 0;

    if (count > 4) {
        // Use 50% overlap ratio for many gates
        // slot_width = available_width / (N * (1 - overlap_ratio) + overlap_ratio)
        // With overlap_ratio = 0.5: slot_width = available_width / (0.5*N + 0.5)
        float overlap_ratio = 0.5f;
        slot_width = static_cast<int32_t>(available_width /
                                          (count * (1.0f - overlap_ratio) + overlap_ratio));
        overlap = static_cast<int32_t>(slot_width * overlap_ratio);

        // Apply negative column padding for overlap effect
        lv_obj_set_style_pad_column(slot_grid_, -overlap, LV_PART_MAIN);
        spdlog::debug(
            "[{}] Dynamic sizing: available={}px, slot_width={}px, overlap={}px for {} gates",
            get_name(), available_width, slot_width, overlap, count);
    } else {
        // No overlap for 4 or fewer gates - evenly distributed
        slot_width = available_width / count;
        lv_obj_set_style_pad_column(slot_grid_, 0, LV_PART_MAIN);
        spdlog::debug("[{}] Even distribution: slot_width={}px for {} gates", get_name(),
                      slot_width, count);
    }

    // Apply calculated width to each slot
    for (int i = 0; i < count; ++i) {
        if (slot_widgets_[i]) {
            lv_obj_set_width(slot_widgets_[i], slot_width);
        }
    }

    // Move labels to overlay layer so they render on top of overlapping slots
    // This must happen after layout_info is set and widths are applied
    if (labels_layer_ && count > 4) {
        // First clear any previous labels from the layer
        lv_obj_clean(labels_layer_);

        // Calculate slot spacing (same formula as path canvas)
        int32_t slot_spacing = slot_width - overlap;

        for (int i = 0; i < count; ++i) {
            if (slot_widgets_[i]) {
                // Slot center X in labels_layer coords (no card_padding offset - we're inside the
                // card)
                int32_t slot_center_x = slot_width / 2 + i * slot_spacing;
                ui_ams_slot_move_label_to_layer(slot_widgets_[i], labels_layer_, slot_center_x);
            }
        }
        spdlog::debug("[{}] Moved {} labels to overlay layer", get_name(), count);
    }

    // Update path canvas with slot_width and overlap so lane positions match
    if (path_canvas_) {
        ui_filament_path_canvas_set_slot_overlap(path_canvas_, overlap);
        ui_filament_path_canvas_set_slot_width(path_canvas_, slot_width);
    }

    spdlog::info("[{}] Created {} slot widgets with dynamic width={}px", get_name(), count,
                 slot_width);

    // Update the visual tray to 1/3 of slot height
    update_tray_size();
}

void AmsPanel::update_tray_size() {
    if (!panel_ || !slot_grid_) {
        return;
    }

    // Find the tray element
    lv_obj_t* tray = lv_obj_find_by_name(panel_, "slot_tray");
    if (!tray) {
        spdlog::debug("[{}] slot_tray not found - skipping tray sizing", get_name());
        return;
    }

    // Force layout update so slot_grid has its final size
    lv_obj_update_layout(slot_grid_);

    // Get slot grid height (includes material label + spool + padding)
    int32_t grid_height = lv_obj_get_height(slot_grid_);
    if (grid_height <= 0) {
        spdlog::debug("[{}] slot_grid height {} - skipping tray sizing", get_name(), grid_height);
        return;
    }

    // Tray is 1/3 of the slot area height
    int32_t tray_height = grid_height / 3;

    // Set tray size and ensure it stays at bottom
    lv_obj_set_height(tray, tray_height);
    lv_obj_align(tray, LV_ALIGN_BOTTOM_MID, 0, 0);

    spdlog::debug("[{}] Tray sized to {}px (1/3 of {}px grid)", get_name(), tray_height,
                  grid_height);
}

void AmsPanel::on_slot_count_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self || !self->panel_) {
        return;
    }

    int new_count = lv_subject_get_int(subject);
    spdlog::debug("[AmsPanel] Slot count changed to {}", new_count);
    self->create_slots(new_count);
}

void AmsPanel::setup_action_buttons() {
    // Store panel pointer for static callbacks to access
    // (Callbacks are registered earlier in ensure_ams_widgets_registered())
    g_ams_panel_instance.store(this);

    spdlog::debug("[{}] Action buttons ready (callbacks registered during widget init)",
                  get_name());
}

void AmsPanel::setup_status_display() {
    // Status display is handled reactively via bind_text in XML
    // Just verify the elements exist
    lv_obj_t* status_label = lv_obj_find_by_name(panel_, "status_label");
    if (status_label) {
        spdlog::debug("[{}] Status label found - bound to ams_action_detail", get_name());
    }
}

void AmsPanel::setup_dryer_card() {
    // Find dryer card (may not exist if XML component not registered)
    lv_obj_t* dryer_card = lv_obj_find_by_name(panel_, "dryer_card");
    if (!dryer_card) {
        spdlog::debug("[{}] dryer_card not found - dryer UI disabled", get_name());
        return;
    }

    // Find progress bar fill element
    lv_obj_t* progress_fill = lv_obj_find_by_name(dryer_card, "progress_fill");
    if (progress_fill) {
        // Store for progress updates
        dryer_progress_fill_ = progress_fill;

        // Set up observer to update width when progress changes
        dryer_progress_observer_ = ObserverGuard(
            AmsState::instance().get_dryer_progress_pct_subject(),
            [](lv_observer_t* observer, lv_subject_t* subject) {
                auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
                if (self && self->dryer_progress_fill_) {
                    int progress = lv_subject_get_int(subject);
                    // Set width as percentage of parent
                    lv_obj_set_width(self->dryer_progress_fill_,
                                     lv_pct(std::max(0, std::min(100, progress))));
                }
            },
            this);

        spdlog::debug("[{}] Dryer progress bar observer set up", get_name());
    }

    // Create the dryer presets modal on the TOP LAYER (above all overlays)
    // Must use lv_layer_top() since AMS panel is itself an overlay
    // The modal's visibility is controlled by the dryer_modal_visible subject
    lv_obj_t* top_layer = lv_layer_top();
    if (top_layer) {
        dryer_modal_ =
            static_cast<lv_obj_t*>(lv_xml_create(top_layer, "dryer_presets_modal", nullptr));
        if (dryer_modal_) {
            spdlog::debug("[{}] Dryer presets modal created on top layer", get_name());
        } else {
            spdlog::warn("[{}] Failed to create dryer presets modal", get_name());
        }
    } else {
        spdlog::warn("[{}] No top layer for dryer modal", get_name());
    }

    // Initial sync of dryer state
    AmsState::instance().sync_dryer_from_backend();
    spdlog::debug("[{}] Dryer card setup complete", get_name());
}

void AmsPanel::setup_path_canvas() {
    path_canvas_ = lv_obj_find_by_name(panel_, "path_canvas");
    if (!path_canvas_) {
        spdlog::warn("[{}] path_canvas not found in XML", get_name());
        return;
    }

    // Set slot click callback to trigger filament load
    ui_filament_path_canvas_set_slot_callback(path_canvas_, on_path_slot_clicked, this);

    // Set slot_width and overlap to match current slot configuration
    // This syncs with the dynamic sizing calculated in create_slots()
    if (slot_grid_) {
        lv_obj_t* slot_area = lv_obj_get_parent(slot_grid_);
        lv_obj_update_layout(slot_area);
        int32_t available_width = lv_obj_get_content_width(slot_area);
        int slot_count = lv_subject_get_int(AmsState::instance().get_slot_count_subject());

        int32_t slot_width = 0;
        int32_t overlap = 0;

        if (slot_count > 4) {
            float overlap_ratio = 0.5f;
            slot_width = static_cast<int32_t>(
                available_width / (slot_count * (1.0f - overlap_ratio) + overlap_ratio));
            overlap = static_cast<int32_t>(slot_width * overlap_ratio);
        } else if (slot_count > 0) {
            slot_width = available_width / slot_count;
        }

        ui_filament_path_canvas_set_slot_width(path_canvas_, slot_width);
        ui_filament_path_canvas_set_slot_overlap(path_canvas_, overlap);
    }

    // Initial configuration from backend
    update_path_canvas_from_backend();

    spdlog::debug("[{}] Path canvas setup complete", get_name());
}

void AmsPanel::update_path_canvas_from_backend() {
    if (!path_canvas_) {
        return;
    }

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        return;
    }

    // Get system info for slot count and topology
    AmsSystemInfo info = backend->get_system_info();

    // Set slot count from backend
    ui_filament_path_canvas_set_slot_count(path_canvas_, info.total_slots);

    // Set topology from backend
    PathTopology topology = backend->get_topology();
    ui_filament_path_canvas_set_topology(path_canvas_, static_cast<int>(topology));

    // Set active slot
    ui_filament_path_canvas_set_active_slot(path_canvas_, info.current_slot);

    // Set filament segment position
    PathSegment segment = backend->get_filament_segment();
    ui_filament_path_canvas_set_filament_segment(path_canvas_, static_cast<int>(segment));

    // Set error segment if any
    PathSegment error_seg = backend->infer_error_segment();
    ui_filament_path_canvas_set_error_segment(path_canvas_, static_cast<int>(error_seg));

    // Set filament color from current slot's filament
    if (info.current_slot >= 0) {
        SlotInfo slot_info = backend->get_slot_info(info.current_slot);
        ui_filament_path_canvas_set_filament_color(path_canvas_, slot_info.color_rgb);
    }

    // Set per-slot filament states for all slots with filament
    // This allows non-active slots to show their filament color/position
    ui_filament_path_canvas_clear_slot_filaments(path_canvas_);
    for (int i = 0; i < info.total_slots; ++i) {
        PathSegment slot_seg = backend->get_slot_filament_segment(i);
        if (slot_seg != PathSegment::NONE) {
            SlotInfo slot_info = backend->get_slot_info(i);
            ui_filament_path_canvas_set_slot_filament(path_canvas_, i, static_cast<int>(slot_seg),
                                                      slot_info.color_rgb);
        }
    }

    spdlog::trace("[{}] Path canvas updated: slots={}, topology={}, active={}, segment={}",
                  get_name(), info.total_slots, static_cast<int>(topology), info.current_slot,
                  static_cast<int>(segment));
}

// ============================================================================
// Public API
// ============================================================================

void AmsPanel::refresh_slots() {
    if (!panel_ || !subjects_initialized_) {
        return;
    }

    update_slot_colors();

    // Update current slot highlight
    int current_slot = lv_subject_get_int(AmsState::instance().get_current_slot_subject());
    update_current_slot_highlight(current_slot);
}

// ============================================================================
// UI Update Handlers
// ============================================================================

void AmsPanel::update_slot_colors() {
    int slot_count = lv_subject_get_int(AmsState::instance().get_slot_count_subject());
    AmsBackend* backend = AmsState::instance().get_backend();

    for (int i = 0; i < MAX_VISIBLE_SLOTS; ++i) {
        if (!slot_widgets_[i]) {
            continue;
        }

        if (i >= slot_count) {
            // Hide slots beyond configured count
            lv_obj_add_flag(slot_widgets_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_remove_flag(slot_widgets_[i], LV_OBJ_FLAG_HIDDEN);

        // Get slot color from AmsState subject
        lv_subject_t* color_subject = AmsState::instance().get_slot_color_subject(i);
        if (color_subject) {
            uint32_t rgb = static_cast<uint32_t>(lv_subject_get_int(color_subject));
            lv_color_t color = lv_color_hex(rgb);

            // Find color swatch within slot
            lv_obj_t* swatch = lv_obj_find_by_name(slot_widgets_[i], "color_swatch");
            if (swatch) {
                lv_obj_set_style_bg_color(swatch, color, 0);
            }
        }

        // Update material label and fill level from backend slot info
        if (backend) {
            SlotInfo slot_info = backend->get_slot_info(i);

            // Update slot-internal material label
            // Truncate long material names when many slots to prevent overlap
            lv_obj_t* material_label = lv_obj_find_by_name(slot_widgets_[i], "material_label");
            if (material_label) {
                if (!slot_info.material.empty()) {
                    std::string material = slot_info.material;
                    // Truncate to 4 chars when overlapping (5+ slots)
                    if (slot_count > 4 && material.length() > 4) {
                        material = material.substr(0, 4);
                    }
                    lv_label_set_text(material_label, material.c_str());
                } else {
                    lv_label_set_text(material_label, "---");
                }
            }

            // Set fill level from Spoolman weight data
            if (slot_info.total_weight_g > 0.0f) {
                float fill_level = slot_info.remaining_weight_g / slot_info.total_weight_g;
                ui_ams_slot_set_fill_level(slot_widgets_[i], fill_level);
            }
        }

        // Update status indicator
        update_slot_status(i);
    }
}

void AmsPanel::update_slot_status(int slot_index) {
    if (slot_index < 0 || slot_index >= MAX_VISIBLE_SLOTS || !slot_widgets_[slot_index]) {
        return;
    }

    lv_subject_t* status_subject = AmsState::instance().get_slot_status_subject(slot_index);
    if (!status_subject) {
        return;
    }

    auto status = static_cast<SlotStatus>(lv_subject_get_int(status_subject));

    // Find status indicator icon within slot
    lv_obj_t* status_icon = lv_obj_find_by_name(slot_widgets_[slot_index], "status_icon");
    if (!status_icon) {
        return;
    }

    // Update icon based on status
    switch (status) {
    case SlotStatus::EMPTY:
        // Show empty indicator
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_30, 0);
        break;

    case SlotStatus::AVAILABLE:
    case SlotStatus::FROM_BUFFER:
        // Show filament available
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case SlotStatus::LOADED:
        // Show loaded (highlighted)
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case SlotStatus::BLOCKED:
        // Show error state
        lv_obj_remove_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(status_icon, LV_OPA_100, 0);
        break;

    case SlotStatus::UNKNOWN:
    default:
        lv_obj_add_flag(status_icon, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

void AmsPanel::update_action_display(AmsAction action) {
    // Action display is handled via bind_text to ams_action_detail
    // This method can add visual feedback (progress indicators, etc.)

    lv_obj_t* progress = lv_obj_find_by_name(panel_, "action_progress");
    if (!progress) {
        return;
    }

    bool show_progress = (action == AmsAction::LOADING || action == AmsAction::UNLOADING ||
                          action == AmsAction::SELECTING || action == AmsAction::RESETTING);

    if (show_progress) {
        lv_obj_remove_flag(progress, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(progress, LV_OBJ_FLAG_HIDDEN);
    }
}

void AmsPanel::update_current_slot_highlight(int slot_index) {
    // Remove highlight from all slots (set border opacity to 0)
    for (int i = 0; i < MAX_VISIBLE_SLOTS; ++i) {
        if (slot_widgets_[i]) {
            lv_obj_remove_state(slot_widgets_[i], LV_STATE_CHECKED);
            lv_obj_set_style_border_opa(slot_widgets_[i], LV_OPA_0, 0);
        }
    }

    // Add highlight to current slot (show border)
    if (slot_index >= 0 && slot_index < MAX_VISIBLE_SLOTS && slot_widgets_[slot_index]) {
        lv_obj_add_state(slot_widgets_[slot_index], LV_STATE_CHECKED);
        lv_obj_set_style_border_opa(slot_widgets_[slot_index], LV_OPA_100, 0);
    }

    // Update the "Currently Loaded" card in the right column
    update_current_loaded_display(slot_index);
}

void AmsPanel::update_current_loaded_display(int slot_index) {
    if (!panel_) {
        return;
    }

    // Sync subjects for reactive UI binding
    // This updates ams_current_material_text, ams_current_slot_text, ams_current_weight_text,
    // ams_current_has_weight, and ams_current_color subjects which are bound to XML elements
    AmsState::instance().sync_current_loaded_from_backend();

    // Find the swatch element - color binding is not supported in XML, so we set it via C++
    lv_obj_t* current_swatch = lv_obj_find_by_name(panel_, "current_swatch");
    if (current_swatch) {
        // Get color from subject (set by sync_current_loaded_from_backend)
        uint32_t color_rgb = static_cast<uint32_t>(
            lv_subject_get_int(AmsState::instance().get_current_color_subject()));
        lv_color_t color = lv_color_hex(color_rgb);
        lv_obj_set_style_bg_color(current_swatch, color, 0);
        lv_obj_set_style_border_color(current_swatch, color, 0);
    }

    // Update bypass-related state for path canvas visualization
    AmsBackend* backend = AmsState::instance().get_backend();
    bool bypass_active = (slot_index == -2 && backend && backend->is_bypass_active());

    // Update path canvas bypass visualization
    if (path_canvas_) {
        ui_filament_path_canvas_set_bypass_active(path_canvas_, bypass_active);
    }
}

// ============================================================================
// Event Callbacks
// ============================================================================

void AmsPanel::on_path_slot_clicked(int slot_index, void* user_data) {
    auto* self = static_cast<AmsPanel*>(user_data);
    if (!self) {
        return;
    }

    spdlog::info("[AmsPanel] Path slot {} clicked - triggering load", slot_index);

    // Trigger filament load for the clicked slot
    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    // Check if backend is busy
    AmsSystemInfo info = backend->get_system_info();
    if (info.action != AmsAction::IDLE && info.action != AmsAction::ERROR) {
        NOTIFY_WARNING("AMS is busy: {}", ams_action_to_string(info.action));
        return;
    }

    AmsError error = backend->load_filament(slot_index);
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Load failed: {}", error.user_msg);
    }
}

void AmsPanel::on_slot_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_slot_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        // Use current_target (widget callback was registered on) not target (originally clicked
        // child)
        lv_obj_t* slot = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        auto slot_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(slot)));
        self->handle_slot_tap(slot_index);
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_unload_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_unload_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_unload();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_reset_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_reset_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_reset();
    }
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// Observer Callbacks
// ============================================================================

void AmsPanel::on_slots_version_changed(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    spdlog::debug("[AmsPanel] Gates version changed - refreshing slots");
    self->refresh_slots();
}

void AmsPanel::on_action_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    auto action = static_cast<AmsAction>(lv_subject_get_int(subject));
    spdlog::debug("[AmsPanel] Action changed: {}", ams_action_to_string(action));
    self->update_action_display(action);
}

void AmsPanel::on_current_slot_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    int slot = lv_subject_get_int(subject);
    spdlog::debug("[AmsPanel] Current slot changed: {}", slot);
    self->update_current_slot_highlight(slot);

    // Also update path canvas when current slot changes
    self->update_path_canvas_from_backend();

    // Auto-set active Spoolman spool when slot becomes active
    if (slot >= 0 && self->api_) {
        auto* backend = AmsState::instance().get_backend();
        if (backend) {
            SlotInfo slot_info = backend->get_slot_info(slot);
            if (slot_info.spoolman_id > 0) {
                spdlog::info("[AmsPanel] Slot {} has Spoolman ID {}, setting as active spool", slot,
                             slot_info.spoolman_id);
                self->api_->set_active_spool(
                    slot_info.spoolman_id,
                    []() { spdlog::debug("[AmsPanel] Active spool set successfully"); },
                    [](const MoonrakerError& err) {
                        spdlog::warn("[AmsPanel] Failed to set active spool: {}", err.message);
                    });
            }
        }
    }
}

void AmsPanel::on_path_state_changed(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<AmsPanel*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    if (!self->subjects_initialized_ || !self->panel_) {
        return; // Not yet ready
    }
    spdlog::debug("[AmsPanel] Path state changed - updating path canvas");
    self->update_path_canvas_from_backend();
}

// ============================================================================
// Action Handlers
// ============================================================================

void AmsPanel::handle_slot_tap(int slot_index) {
    spdlog::info("[{}] Slot {} tapped", get_name(), slot_index);

    // Validate slot index against configured slot count
    int slot_count = lv_subject_get_int(AmsState::instance().get_slot_count_subject());
    if (slot_index < 0 || slot_index >= slot_count) {
        spdlog::warn("[{}] Invalid slot index {} (slot_count={})", get_name(), slot_index,
                     slot_count);
        return;
    }

    // Show context menu near the tapped slot
    if (slot_index >= 0 && slot_index < MAX_VISIBLE_SLOTS && slot_widgets_[slot_index]) {
        show_context_menu(slot_index, slot_widgets_[slot_index]);
    }
}

void AmsPanel::handle_unload() {
    spdlog::info("[{}] Unload requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->unload_filament();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Unload failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_reset() {
    spdlog::info("[{}] Reset requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->reset();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Reset failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_bypass_toggle() {
    spdlog::info("[{}] Bypass toggle requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    // Check if hardware sensor controls bypass (button should be disabled, but check anyway)
    AmsSystemInfo info = backend->get_system_info();
    if (info.has_hardware_bypass_sensor) {
        NOTIFY_WARNING("Bypass controlled by sensor");
        spdlog::warn("[{}] Bypass toggle blocked - hardware sensor controls bypass", get_name());
        return;
    }

    // Check current bypass state and toggle
    bool currently_bypassed = backend->is_bypass_active();
    AmsError error;

    if (currently_bypassed) {
        error = backend->disable_bypass();
        if (error.result == AmsResult::SUCCESS) {
            NOTIFY_INFO("Bypass disabled");
        }
    } else {
        error = backend->enable_bypass();
        if (error.result == AmsResult::SUCCESS) {
            NOTIFY_INFO("Bypass enabled");
        }
    }

    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Bypass toggle failed: {}", error.user_msg);
    }
    // Switch state updates automatically via bypass_active subject binding
}

void AmsPanel::handle_dryer_preset(float temp_c, int duration_min, int fan_pct) {
    spdlog::info("[{}] Dryer preset: {}°C for {}min, fan {}%", get_name(), temp_c, duration_min,
                 fan_pct);

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    DryerInfo dryer = backend->get_dryer_info();
    if (!dryer.supported) {
        NOTIFY_WARNING("Dryer not available");
        return;
    }

    AmsError error = backend->start_drying(temp_c, duration_min, fan_pct);
    if (error.result == AmsResult::SUCCESS) {
        NOTIFY_INFO("Drying started: {}°C", static_cast<int>(temp_c));
        // Sync dryer state to update UI
        AmsState::instance().sync_dryer_from_backend();
        // Close the presets modal
        lv_subject_set_int(AmsState::instance().get_dryer_modal_visible_subject(), 0);
    } else {
        NOTIFY_ERROR("Failed to start drying: {}", error.user_msg);
    }
}

void AmsPanel::handle_dryer_stop() {
    spdlog::info("[{}] Dryer stop requested", get_name());

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->stop_drying();
    if (error.result == AmsResult::SUCCESS) {
        NOTIFY_INFO("Drying stopped");
        // Sync dryer state to update UI
        AmsState::instance().sync_dryer_from_backend();
    } else {
        NOTIFY_ERROR("Failed to stop drying: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_load() {
    if (context_menu_slot_ < 0) {
        return;
    }

    // Capture slot before hiding menu (hide_context_menu resets context_menu_slot_)
    int slot_to_load = context_menu_slot_;
    spdlog::info("[{}] Context menu: Load from slot {}", get_name(), slot_to_load);
    hide_context_menu();

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    // Check if backend is busy
    AmsSystemInfo info = backend->get_system_info();
    if (info.action != AmsAction::IDLE && info.action != AmsAction::ERROR) {
        NOTIFY_WARNING("AMS is busy: {}", ams_action_to_string(info.action));
        return;
    }

    AmsError error = backend->load_filament(slot_to_load);
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Load failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_unload() {
    if (context_menu_slot_ < 0) {
        return;
    }

    spdlog::info("[{}] Context menu: Unload slot {}", get_name(), context_menu_slot_);
    hide_context_menu();

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    AmsError error = backend->unload_filament();
    if (error.result != AmsResult::SUCCESS) {
        NOTIFY_ERROR("Unload failed: {}", error.user_msg);
    }
}

void AmsPanel::handle_context_edit() {
    if (context_menu_slot_ < 0) {
        return;
    }

    int slot_to_edit = context_menu_slot_;
    spdlog::info("[{}] Context menu: Edit slot {}", get_name(), slot_to_edit);
    hide_context_menu();

    // Open edit modal for this slot
    show_edit_modal(slot_to_edit);
}

void AmsPanel::handle_context_spoolman() {
    if (context_menu_slot_ < 0) {
        return;
    }

    int slot_to_assign = context_menu_slot_;
    spdlog::info("[{}] Context menu: Assign Spoolman spool to slot {}", get_name(), slot_to_assign);
    hide_context_menu();

    // Open Spoolman spool picker overlay
    show_spoolman_picker(slot_to_assign);
}

// ============================================================================
// Context Menu Management
// ============================================================================

void AmsPanel::show_context_menu(int slot_index, lv_obj_t* near_widget) {
    // Hide any existing context menu first
    hide_context_menu();

    if (!parent_screen_ || !near_widget) {
        return;
    }

    // Store which slot the menu is for
    context_menu_slot_ = slot_index;

    // Create context menu from XML component
    context_menu_ =
        static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "ams_context_menu", nullptr));
    if (!context_menu_) {
        spdlog::error("[{}] Failed to create context menu", get_name());
        return;
    }

    // Event callbacks are handled via XML <event_cb> elements
    // Find the menu card to position it
    lv_obj_t* menu_card = lv_obj_find_by_name(context_menu_, "context_menu");

    // Position the menu card near the tapped widget
    if (menu_card) {
        // Update layout to get accurate dimensions before positioning
        lv_obj_update_layout(menu_card);

        // Get the position of the slot widget in screen coordinates
        lv_point_t slot_pos;
        lv_obj_get_coords(near_widget, (lv_area_t*)&slot_pos);

        // Position menu to the right of the slot, or left if near edge
        int32_t screen_width = lv_obj_get_width(parent_screen_);
        int32_t menu_width = lv_obj_get_width(menu_card);
        int32_t slot_center_x = slot_pos.x + lv_obj_get_width(near_widget) / 2;
        int32_t slot_center_y = slot_pos.y + lv_obj_get_height(near_widget) / 2;

        int32_t menu_x = slot_center_x + 20; // Position to the right
        if (menu_x + menu_width > screen_width - 10) {
            menu_x = slot_center_x - menu_width - 20; // Position to the left instead
        }

        // Vertical: center on the slot
        int32_t menu_y = slot_center_y - lv_obj_get_height(menu_card) / 2;

        // Clamp to screen bounds
        int32_t screen_height = lv_obj_get_height(parent_screen_);
        if (menu_y < 10)
            menu_y = 10;
        if (menu_y + lv_obj_get_height(menu_card) > screen_height - 10) {
            menu_y = screen_height - lv_obj_get_height(menu_card) - 10;
        }

        lv_obj_set_pos(menu_card, menu_x, menu_y);
    }

    spdlog::debug("[{}] Context menu shown for slot {}", get_name(), slot_index);
}

void AmsPanel::hide_context_menu() {
    if (context_menu_) {
        lv_obj_delete(context_menu_);
        context_menu_ = nullptr;
        context_menu_slot_ = -1;
        spdlog::debug("[{}] Context menu hidden", get_name());
    }
}

// ============================================================================
// Context Menu Callbacks
// ============================================================================

void AmsPanel::on_context_backdrop_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_backdrop_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->hide_context_menu();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_load_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_load_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_load();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_unload_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_unload_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_unload();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void AmsPanel::on_context_edit_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsPanel] on_context_edit_clicked");
    auto* self = static_cast<AmsPanel*>(lv_event_get_user_data(e));
    if (self) {
        self->handle_context_edit();
    }
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// Spoolman Picker Management
// ============================================================================

void AmsPanel::show_spoolman_picker(int slot_index) {
    // Hide any existing picker first
    hide_spoolman_picker();

    if (!parent_screen_) {
        spdlog::warn("[{}] Cannot show picker - no parent screen", get_name());
        return;
    }

    picker_target_slot_ = slot_index;

    // Create picker modal from XML
    spoolman_picker_ =
        static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "spoolman_picker_backdrop", nullptr));
    if (!spoolman_picker_) {
        spdlog::error("[{}] Failed to create Spoolman picker modal", get_name());
        return;
    }

    // Update slot indicator text via subject
    snprintf(picker_slot_indicator_buf_, sizeof(picker_slot_indicator_buf_), "Assigning to Slot %d",
             slot_index + 1);
    lv_subject_copy_string(&picker_slot_indicator_subject_, picker_slot_indicator_buf_);

    // Bind slot indicator label to subject
    lv_obj_t* slot_indicator = lv_obj_find_by_name(spoolman_picker_, "slot_indicator");
    if (slot_indicator) {
        lv_label_bind_text(slot_indicator, &picker_slot_indicator_subject_, nullptr);
    }

    // Check if slot already has a Spoolman spool assigned - show unlink button
    AmsBackend* backend = AmsState::instance().get_backend();
    if (backend) {
        SlotInfo slot_info = backend->get_slot_info(slot_index);
        if (slot_info.spoolman_id > 0) {
            lv_obj_t* btn_unlink = lv_obj_find_by_name(spoolman_picker_, "btn_unlink");
            if (btn_unlink) {
                lv_obj_remove_flag(btn_unlink, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // Show loading state initially
    lv_obj_t* loading_container = lv_obj_find_by_name(spoolman_picker_, "loading_container");
    if (loading_container) {
        lv_obj_remove_flag(loading_container, LV_OBJ_FLAG_HIDDEN);
    }

    // Populate the picker with spools from Spoolman API
    populate_spoolman_picker();

    spdlog::info("[{}] Spoolman picker shown for slot {}", get_name(), slot_index);
}

void AmsPanel::hide_spoolman_picker() {
    // Invalidate async callbacks first (before deleting widget)
    picker_callback_guard_.reset();

    if (spoolman_picker_) {
        lv_obj_delete(spoolman_picker_);
        spoolman_picker_ = nullptr;
        picker_target_slot_ = -1;
        picker_spools_.clear(); // Clear cached spools
        spdlog::debug("[{}] Spoolman picker hidden", get_name());
    }
}

void AmsPanel::populate_spoolman_picker() {
    if (!spoolman_picker_ || !api_) {
        // No API - show empty state
        lv_obj_t* loading_container = lv_obj_find_by_name(spoolman_picker_, "loading_container");
        lv_obj_t* empty_container = lv_obj_find_by_name(spoolman_picker_, "empty_container");
        if (loading_container) {
            lv_obj_add_flag(loading_container, LV_OBJ_FLAG_HIDDEN);
        }
        if (empty_container) {
            lv_obj_remove_flag(empty_container, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    // Use weak_ptr pattern for async callback safety [L012]
    // Store shared_ptr as member so it outlives the async callback
    picker_callback_guard_ = std::make_shared<bool>(true);
    std::weak_ptr<bool> weak_guard = picker_callback_guard_;

    // Get the current spoolman_id for this slot to mark as selected
    int current_spool_id = 0;
    AmsBackend* backend = AmsState::instance().get_backend();
    if (backend && picker_target_slot_ >= 0) {
        SlotInfo slot_info = backend->get_slot_info(picker_target_slot_);
        current_spool_id = slot_info.spoolman_id;
    }

    api_->get_spoolman_spools(
        [this, weak_guard, current_spool_id](const std::vector<SpoolInfo>& spools) {
            if (weak_guard.expired() || !spoolman_picker_) {
                return; // Panel was destroyed
            }

            // Hide loading state
            lv_obj_t* loading_container =
                lv_obj_find_by_name(spoolman_picker_, "loading_container");
            if (loading_container) {
                lv_obj_add_flag(loading_container, LV_OBJ_FLAG_HIDDEN);
            }

            if (spools.empty()) {
                // Show empty state
                lv_obj_t* empty_container =
                    lv_obj_find_by_name(spoolman_picker_, "empty_container");
                if (empty_container) {
                    lv_obj_remove_flag(empty_container, LV_OBJ_FLAG_HIDDEN);
                }
                return;
            }

            // Cache spools for lookup when user selects one
            picker_spools_ = spools;

            // Find the spool list container
            lv_obj_t* spool_list = lv_obj_find_by_name(spoolman_picker_, "spool_list");
            if (!spool_list) {
                spdlog::error("[{}] spool_list not found in picker", get_name());
                return;
            }

            // Create a spool item for each spool
            for (const auto& spool : spools) {
                lv_obj_t* item =
                    static_cast<lv_obj_t*>(lv_xml_create(spool_list, "spool_item", nullptr));
                if (!item) {
                    continue;
                }

                // Store spool_id in user_data for click handler
                lv_obj_set_user_data(item,
                                     reinterpret_cast<void*>(static_cast<intptr_t>(spool.id)));

                // Update spool name (vendor + material)
                lv_obj_t* name_label = lv_obj_find_by_name(item, "spool_name");
                if (name_label) {
                    std::string name = spool.vendor.empty() ? spool.material
                                                            : (spool.vendor + " " + spool.material);
                    lv_label_set_text(name_label, name.c_str());
                }

                // Update color name
                lv_obj_t* color_label = lv_obj_find_by_name(item, "spool_color");
                if (color_label && !spool.color_name.empty()) {
                    lv_label_set_text(color_label, spool.color_name.c_str());
                }

                // Update weight
                lv_obj_t* weight_label = lv_obj_find_by_name(item, "spool_weight");
                if (weight_label && spool.remaining_weight_g > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.0fg", spool.remaining_weight_g);
                    lv_label_set_text(weight_label, buf);
                }

                // Update color swatch (parse hex string like "#1A1A2E")
                lv_obj_t* swatch = lv_obj_find_by_name(item, "spool_swatch");
                if (swatch && !spool.color_hex.empty()) {
                    lv_color_t color = ui_theme_parse_color(spool.color_hex.c_str());
                    lv_obj_set_style_bg_color(swatch, color, 0);
                    lv_obj_set_style_border_color(swatch, color, 0);
                }

                // Show checkmark if this is the currently assigned spool
                if (spool.id == current_spool_id) {
                    lv_obj_t* check_icon = lv_obj_find_by_name(item, "selected_icon");
                    if (check_icon) {
                        lv_obj_remove_flag(check_icon, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }

            spdlog::info("[{}] Populated picker with {} spools", get_name(), spools.size());
        },
        [this, weak_guard](const MoonrakerError& err) {
            if (weak_guard.expired() || !spoolman_picker_) {
                return;
            }

            spdlog::warn("[{}] Failed to fetch spools: {}", get_name(), err.message);

            // Hide loading, show empty state with error
            lv_obj_t* loading_container =
                lv_obj_find_by_name(spoolman_picker_, "loading_container");
            lv_obj_t* empty_container = lv_obj_find_by_name(spoolman_picker_, "empty_container");
            if (loading_container) {
                lv_obj_add_flag(loading_container, LV_OBJ_FLAG_HIDDEN);
            }
            if (empty_container) {
                lv_obj_remove_flag(empty_container, LV_OBJ_FLAG_HIDDEN);
            }
        });
}

void AmsPanel::handle_picker_close() {
    spdlog::debug("[{}] Picker close requested", get_name());
    hide_spoolman_picker();
}

void AmsPanel::handle_picker_unlink() {
    if (picker_target_slot_ < 0) {
        return;
    }

    spdlog::info("[{}] Unlinking Spoolman spool from slot {}", get_name(), picker_target_slot_);

    AmsBackend* backend = AmsState::instance().get_backend();
    if (backend) {
        // Clear the Spoolman assignment for this slot via set_slot_info
        SlotInfo slot_info = backend->get_slot_info(picker_target_slot_);
        slot_info.spoolman_id = 0;
        slot_info.spool_name.clear();
        slot_info.remaining_weight_g = -1;
        slot_info.total_weight_g = -1;
        backend->set_slot_info(picker_target_slot_, slot_info);

        // Update the slot display
        AmsState::instance().sync_from_backend();
        refresh_slots();

        NOTIFY_INFO("Slot {} assignment cleared", picker_target_slot_ + 1);
    }

    hide_spoolman_picker();
}

void AmsPanel::handle_picker_spool_selected(int spool_id) {
    if (picker_target_slot_ < 0 || spool_id <= 0) {
        return;
    }

    spdlog::info("[{}] Assigning spool {} to slot {}", get_name(), spool_id, picker_target_slot_);

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        hide_spoolman_picker();
        return;
    }

    // Look up spool details from cached list (populated when picker opened)
    const SpoolInfo* selected_spool = nullptr;
    for (const auto& spool : picker_spools_) {
        if (spool.id == spool_id) {
            selected_spool = &spool;
            break;
        }
    }

    // Get current slot info and enrich with Spoolman data
    SlotInfo slot_info = backend->get_slot_info(picker_target_slot_);
    slot_info.spoolman_id = spool_id;

    if (selected_spool) {
        // Enrich slot with full Spoolman details
        slot_info.color_name = selected_spool->color_name;
        slot_info.material = selected_spool->material;
        slot_info.brand = selected_spool->vendor;
        slot_info.spool_name = selected_spool->vendor + " " + selected_spool->material;
        slot_info.remaining_weight_g = static_cast<float>(selected_spool->remaining_weight_g);
        slot_info.total_weight_g = static_cast<float>(selected_spool->initial_weight_g);
        slot_info.nozzle_temp_min = selected_spool->nozzle_temp_min;
        slot_info.nozzle_temp_max = selected_spool->nozzle_temp_max;
        slot_info.bed_temp = selected_spool->bed_temp_recommended;

        // Parse color hex to RGB
        if (!selected_spool->color_hex.empty()) {
            std::string hex = selected_spool->color_hex;
            if (hex[0] == '#') {
                hex = hex.substr(1);
            }
            try {
                slot_info.color_rgb = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
            } catch (...) {
                spdlog::warn("[{}] Failed to parse color hex: {}", get_name(),
                             selected_spool->color_hex);
            }
        }

        spdlog::info("[{}] Enriched slot {} with Spoolman data: {} {} ({})", get_name(),
                     picker_target_slot_, selected_spool->vendor, selected_spool->material,
                     selected_spool->color_name);
    }

    backend->set_slot_info(picker_target_slot_, slot_info);

    // Update the slot display
    AmsState::instance().sync_from_backend();
    refresh_slots();

    NOTIFY_INFO("Spool assigned to Slot {}", picker_target_slot_ + 1);

    hide_spoolman_picker();
}

// ============================================================================
// Edit Modal Management
// ============================================================================

void AmsPanel::show_edit_modal(int slot_index) {
    // Hide any existing modal first
    hide_edit_modal();

    if (!parent_screen_) {
        spdlog::warn("[{}] Cannot show edit modal - no parent screen", get_name());
        return;
    }

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_WARNING("AMS not available");
        return;
    }

    edit_slot_index_ = slot_index;

    // Get current slot info and store original for reset
    edit_original_slot_info_ = backend->get_slot_info(slot_index);
    edit_slot_info_ = edit_original_slot_info_;

    // Reset remaining edit mode to view (before XML creation so bindings start correctly)
    lv_subject_set_int(&s_edit_remaining_mode, 0);

    // Create edit modal from XML (component name = filename without extension)
    edit_modal_ = static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "ams_edit_modal", nullptr));
    if (!edit_modal_) {
        spdlog::error("[{}] Failed to create edit modal", get_name());
        return;
    }

    // Bind labels to subjects for reactive text updates
    lv_obj_t* slot_indicator = lv_obj_find_by_name(edit_modal_, "slot_indicator");
    if (slot_indicator) {
        lv_label_bind_text(slot_indicator, &edit_slot_indicator_subject_, nullptr);
    }

    lv_obj_t* color_name_label = lv_obj_find_by_name(edit_modal_, "color_name_label");
    if (color_name_label) {
        lv_label_bind_text(color_name_label, &edit_color_name_subject_, nullptr);
    }

    lv_obj_t* temp_nozzle_label = lv_obj_find_by_name(edit_modal_, "temp_nozzle_label");
    if (temp_nozzle_label) {
        lv_label_bind_text(temp_nozzle_label, &edit_temp_nozzle_subject_, nullptr);
    }

    lv_obj_t* temp_bed_label = lv_obj_find_by_name(edit_modal_, "temp_bed_label");
    if (temp_bed_label) {
        lv_label_bind_text(temp_bed_label, &edit_temp_bed_subject_, nullptr);
    }

    lv_obj_t* remaining_pct_label = lv_obj_find_by_name(edit_modal_, "remaining_pct_label");
    if (remaining_pct_label) {
        lv_label_bind_text(remaining_pct_label, &edit_remaining_pct_subject_, nullptr);
    }

    // Update the modal UI with current slot data
    update_edit_modal_ui();

    // Set initial sync button state (disabled since nothing is dirty yet)
    update_sync_button_state();

    spdlog::info("[{}] Edit modal shown for slot {}", get_name(), slot_index);
}

void AmsPanel::hide_edit_modal() {
    if (edit_modal_) {
        // Reset edit mode subject before destroying modal
        lv_subject_set_int(&s_edit_remaining_mode, 0);

        lv_obj_delete(edit_modal_);
        edit_modal_ = nullptr;
        edit_slot_index_ = -1;
        spdlog::debug("[{}] Edit modal hidden", get_name());
    }
}

bool AmsPanel::is_edit_dirty() const {
    // Compare relevant fields that can be edited
    return edit_slot_info_.color_rgb != edit_original_slot_info_.color_rgb ||
           edit_slot_info_.material != edit_original_slot_info_.material ||
           edit_slot_info_.brand != edit_original_slot_info_.brand ||
           std::abs(edit_slot_info_.remaining_weight_g -
                    edit_original_slot_info_.remaining_weight_g) > 0.1f;
}

void AmsPanel::update_sync_button_state() {
    if (!edit_modal_) {
        return;
    }

    lv_obj_t* sync_btn = lv_obj_find_by_name(edit_modal_, "btn_sync_spoolman");
    if (!sync_btn) {
        return;
    }

    // Only enable if dirty and has Spoolman link
    bool should_enable = is_edit_dirty() && edit_slot_info_.spoolman_id > 0;

    if (should_enable) {
        lv_obj_remove_state(sync_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(sync_btn, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_add_state(sync_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(sync_btn, LV_OPA_50, LV_PART_MAIN);
    }
}

void AmsPanel::update_edit_modal_ui() {
    if (!edit_modal_) {
        return;
    }

    // Update slot indicator via subject
    snprintf(edit_slot_indicator_buf_, sizeof(edit_slot_indicator_buf_), "Slot %d",
             edit_slot_index_ + 1);
    lv_subject_copy_string(&edit_slot_indicator_subject_, edit_slot_indicator_buf_);

    // Set dropdown options (requires \n separators in C++)
    lv_obj_t* vendor_dropdown = lv_obj_find_by_name(edit_modal_, "vendor_dropdown");
    if (vendor_dropdown) {
        lv_dropdown_set_options(vendor_dropdown,
                                "Generic\nPolymaker\nBambu\neSUN\nOverture\nPrusa\nHatchbox");
    }

    lv_obj_t* material_dropdown = lv_obj_find_by_name(edit_modal_, "material_dropdown");
    if (material_dropdown) {
        lv_dropdown_set_options(material_dropdown, "PLA\nPETG\nABS\nASA\nTPU\nPA\nPC");
    }

    // Update color swatch
    lv_obj_t* color_swatch = lv_obj_find_by_name(edit_modal_, "color_swatch");
    if (color_swatch) {
        lv_obj_set_style_bg_color(color_swatch, lv_color_hex(edit_slot_info_.color_rgb), 0);
    }

    // Update color name label via subject
    if (!edit_slot_info_.color_name.empty()) {
        snprintf(edit_color_name_buf_, sizeof(edit_color_name_buf_), "%s",
                 edit_slot_info_.color_name.c_str());
    } else {
        edit_color_name_buf_[0] = '\0';
    }
    lv_subject_copy_string(&edit_color_name_subject_, edit_color_name_buf_);

    // Update remaining slider and label
    int remaining_pct = 75; // Default
    if (edit_slot_info_.total_weight_g > 0) {
        remaining_pct = static_cast<int>(100.0f * edit_slot_info_.remaining_weight_g /
                                         edit_slot_info_.total_weight_g);
        remaining_pct = std::max(0, std::min(100, remaining_pct));
    }

    lv_obj_t* remaining_slider = lv_obj_find_by_name(edit_modal_, "remaining_slider");
    if (remaining_slider) {
        lv_slider_set_value(remaining_slider, remaining_pct, LV_ANIM_OFF);
    }

    // Update remaining percentage label via subject
    snprintf(edit_remaining_pct_buf_, sizeof(edit_remaining_pct_buf_), "%d%%", remaining_pct);
    lv_subject_copy_string(&edit_remaining_pct_subject_, edit_remaining_pct_buf_);

    // Update progress bar fill width (shown in view mode)
    // Note: lv_obj_update_layout() required to get accurate container width
    lv_obj_t* progress_container = lv_obj_find_by_name(edit_modal_, "remaining_progress_container");
    lv_obj_t* progress_fill = lv_obj_find_by_name(edit_modal_, "remaining_progress_fill");
    if (progress_container && progress_fill) {
        lv_obj_update_layout(progress_container);
        int container_width = lv_obj_get_width(progress_container);
        int fill_width = container_width * remaining_pct / 100;
        lv_obj_set_width(progress_fill, fill_width);
    }

    // Update temperature display based on material
    update_edit_temp_display();

    // Show/hide Spoolman sync button based on whether slot has spoolman_id
    lv_obj_t* btn_sync = lv_obj_find_by_name(edit_modal_, "btn_sync_spoolman");
    if (btn_sync) {
        if (edit_slot_info_.spoolman_id > 0) {
            lv_obj_remove_flag(btn_sync, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_sync, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void AmsPanel::update_edit_temp_display() {
    if (!edit_modal_) {
        return;
    }

    // Get temperature range from slot info (populated from Spoolman or material defaults)
    int nozzle_min = edit_slot_info_.nozzle_temp_min;
    int nozzle_max = edit_slot_info_.nozzle_temp_max;
    int bed_temp = edit_slot_info_.bed_temp;

    // Fall back to material-based defaults from filament database if not set
    if (nozzle_min == 0 && nozzle_max == 0 && !edit_slot_info_.material.empty()) {
        auto mat_info = filament::find_material(edit_slot_info_.material);
        if (mat_info) {
            nozzle_min = mat_info->nozzle_min;
            nozzle_max = mat_info->nozzle_max;
            bed_temp = mat_info->bed_temp;
            spdlog::debug("[{}] Using filament database temps for {}: {}-{}°C nozzle, {}°C bed",
                          get_name(), edit_slot_info_.material, nozzle_min, nozzle_max, bed_temp);
        } else {
            // Generic defaults for unknown materials
            nozzle_min = 200;
            nozzle_max = 230;
            bed_temp = 60;
            spdlog::debug("[{}] Material '{}' not in database, using generic defaults", get_name(),
                          edit_slot_info_.material);
        }
    }

    // Update nozzle temp label via subject
    snprintf(edit_temp_nozzle_buf_, sizeof(edit_temp_nozzle_buf_), "%d-%d°C", nozzle_min,
             nozzle_max);
    lv_subject_copy_string(&edit_temp_nozzle_subject_, edit_temp_nozzle_buf_);

    // Update bed temp label via subject
    snprintf(edit_temp_bed_buf_, sizeof(edit_temp_bed_buf_), "%d°C", bed_temp);
    lv_subject_copy_string(&edit_temp_bed_subject_, edit_temp_bed_buf_);
}

// ============================================================================
// Edit Modal Handlers
// ============================================================================

void AmsPanel::handle_edit_modal_close() {
    spdlog::debug("[{}] Edit modal close requested", get_name());
    hide_edit_modal();
}

void AmsPanel::handle_edit_vendor_changed(int vendor_index) {
    // Vendor list from XML: Generic, Polymaker, Bambu, eSUN, Overture, Prusa, Hatchbox
    static const char* vendors[] = {"Generic",  "Polymaker", "Bambu",   "eSUN",
                                    "Overture", "Prusa",     "Hatchbox"};
    if (vendor_index >= 0 &&
        vendor_index < static_cast<int>(sizeof(vendors) / sizeof(vendors[0]))) {
        edit_slot_info_.brand = vendors[vendor_index];
        spdlog::debug("[{}] Vendor changed to: {}", get_name(), edit_slot_info_.brand);
        update_sync_button_state();
    }
}

void AmsPanel::handle_edit_material_changed(int material_index) {
    // Material list from XML: PLA, PETG, ABS, ASA, TPU, PA, PC
    static const char* materials[] = {"PLA", "PETG", "ABS", "ASA", "TPU", "PA", "PC"};
    if (material_index >= 0 &&
        material_index < static_cast<int>(sizeof(materials) / sizeof(materials[0]))) {
        edit_slot_info_.material = materials[material_index];
        spdlog::debug("[{}] Material changed to: {}", get_name(), edit_slot_info_.material);

        // Clear existing temp values so update_edit_temp_display uses material-based defaults
        // (Otherwise Spoolman-sourced temps would persist when user changes material)
        edit_slot_info_.nozzle_temp_min = 0;
        edit_slot_info_.nozzle_temp_max = 0;
        edit_slot_info_.bed_temp = 0;

        // Update temperature display based on new material
        update_edit_temp_display();
        update_sync_button_state();
    }
}

void AmsPanel::handle_edit_color_clicked() {
    spdlog::info("[{}] Opening color picker", get_name());
    show_color_picker();
}

void AmsPanel::handle_edit_remaining_changed(int percent) {
    if (!edit_modal_) {
        return;
    }

    // Update the percentage label via subject
    snprintf(edit_remaining_pct_buf_, sizeof(edit_remaining_pct_buf_), "%d%%", percent);
    lv_subject_copy_string(&edit_remaining_pct_subject_, edit_remaining_pct_buf_);

    // Update slot info remaining weight based on percentage
    if (edit_slot_info_.total_weight_g > 0) {
        edit_slot_info_.remaining_weight_g =
            edit_slot_info_.total_weight_g * static_cast<float>(percent) / 100.0f;
    }

    update_sync_button_state();
    spdlog::trace("[{}] Remaining changed to {}%", get_name(), percent);
}

void AmsPanel::handle_edit_remaining_edit() {
    if (!edit_modal_) {
        return;
    }

    // Store current remaining percentage before entering edit mode
    lv_obj_t* slider = lv_obj_find_by_name(edit_modal_, "remaining_slider");
    if (slider) {
        edit_remaining_pre_edit_pct_ = lv_slider_get_value(slider);
    }

    // Enter edit mode - subject binding will show slider/accept/cancel, hide progress/edit button
    lv_subject_set_int(&s_edit_remaining_mode, 1);
    spdlog::debug("[{}] Entered remaining edit mode (was {}%)", get_name(),
                  edit_remaining_pre_edit_pct_);
}

void AmsPanel::handle_edit_remaining_accept() {
    if (!edit_modal_) {
        return;
    }

    // Get the current slider value
    lv_obj_t* slider = lv_obj_find_by_name(edit_modal_, "remaining_slider");
    int new_pct = slider ? lv_slider_get_value(slider) : edit_remaining_pre_edit_pct_;

    // Update the progress bar fill to match
    lv_obj_t* progress_fill = lv_obj_find_by_name(edit_modal_, "remaining_progress_fill");
    lv_obj_t* progress_container = lv_obj_find_by_name(edit_modal_, "remaining_progress_container");
    if (progress_fill && progress_container) {
        int container_width = lv_obj_get_width(progress_container);
        int fill_width = container_width * new_pct / 100;
        lv_obj_set_width(progress_fill, fill_width);
    }

    // Exit edit mode - subject binding will show progress/edit button, hide slider/accept/cancel
    lv_subject_set_int(&s_edit_remaining_mode, 0);
    spdlog::debug("[{}] Accepted remaining edit: {}%", get_name(), new_pct);
}

void AmsPanel::handle_edit_remaining_cancel() {
    if (!edit_modal_) {
        return;
    }

    // Revert slider to pre-edit value
    lv_obj_t* slider = lv_obj_find_by_name(edit_modal_, "remaining_slider");
    if (slider) {
        lv_slider_set_value(slider, edit_remaining_pre_edit_pct_, LV_ANIM_OFF);
    }

    // Revert the percentage label via subject
    snprintf(edit_remaining_pct_buf_, sizeof(edit_remaining_pct_buf_), "%d%%",
             edit_remaining_pre_edit_pct_);
    lv_subject_copy_string(&edit_remaining_pct_subject_, edit_remaining_pct_buf_);

    // Revert the remaining weight in edit_slot_info_
    if (edit_slot_info_.total_weight_g > 0) {
        edit_slot_info_.remaining_weight_g = edit_slot_info_.total_weight_g *
                                             static_cast<float>(edit_remaining_pre_edit_pct_) /
                                             100.0f;
    }

    // Exit edit mode
    lv_subject_set_int(&s_edit_remaining_mode, 0);
    update_sync_button_state();
    spdlog::debug("[{}] Cancelled remaining edit (reverted to {}%)", get_name(),
                  edit_remaining_pre_edit_pct_);
}

void AmsPanel::handle_edit_sync_spoolman() {
    if (edit_slot_info_.spoolman_id <= 0) {
        NOTIFY_WARNING("Slot not linked to Spoolman");
        return;
    }

    spdlog::info("[{}] Sync to Spoolman requested for spool ID {}", get_name(),
                 edit_slot_info_.spoolman_id);

    // Check if weight changed - this is the primary sync we support
    bool weight_changed = std::abs(edit_slot_info_.remaining_weight_g -
                                   edit_original_slot_info_.remaining_weight_g) > 0.1f;

    if (!weight_changed) {
        NOTIFY_INFO("No changes to sync");
        return;
    }

    // Sync remaining weight to Spoolman
    int spool_id = edit_slot_info_.spoolman_id;
    double new_weight = static_cast<double>(edit_slot_info_.remaining_weight_g);

    spdlog::info("[{}] Syncing spool {} weight: {:.1f}g -> {:.1f}g", get_name(), spool_id,
                 edit_original_slot_info_.remaining_weight_g, new_weight);

    api_->update_spoolman_spool_weight(
        spool_id, new_weight,
        [this, spool_id]() {
            spdlog::info("[{}] Spoolman spool {} weight synced successfully", get_name(), spool_id);
            NOTIFY_SUCCESS("Synced to Spoolman");

            // Update original to match - no longer "dirty"
            edit_original_slot_info_.remaining_weight_g = edit_slot_info_.remaining_weight_g;
            update_sync_button_state();
        },
        [this](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to sync to Spoolman: {}", get_name(), err.message);
            NOTIFY_ERROR("Sync failed: " + err.message);
        });
}

void AmsPanel::handle_edit_reset() {
    spdlog::debug("[{}] Resetting edit modal to original values", get_name());

    // Restore original slot info
    edit_slot_info_ = edit_original_slot_info_;

    // Refresh the UI
    update_edit_modal_ui();
    update_sync_button_state();

    NOTIFY_INFO("Reset to original values");
}

void AmsPanel::handle_edit_save() {
    if (edit_slot_index_ < 0) {
        return;
    }

    spdlog::info("[{}] Saving edits for slot {}", get_name(), edit_slot_index_);

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        NOTIFY_ERROR("AMS not available");
        hide_edit_modal();
        return;
    }

    // Apply the edited slot info to the backend
    backend->set_slot_info(edit_slot_index_, edit_slot_info_);

    // Update the slot display
    AmsState::instance().sync_from_backend();
    refresh_slots();

    NOTIFY_INFO("Slot {} updated", edit_slot_index_ + 1);

    hide_edit_modal();
}

// ============================================================================
// Color Picker
// ============================================================================

/**
 * @brief Map hex color value to human-readable name
 *
 * Uses algorithmic color naming (HSL-based) with special names for
 * preset colors that have non-standard names (Gold, Bronze, Wood, etc.)
 *
 * Algorithm ported from Klipper DESCRIBE_COLOR macro on voronv2.local.
 */
static std::string get_color_name_from_hex(uint32_t rgb) {
    // Special preset names that don't follow standard color naming
    static const struct {
        uint32_t hex;
        const char* name;
    } special_names[] = {
        {0xD4AF37, "Gold"},  {0xCD7F32, "Bronze"}, {0x8B4513, "Wood"},
        {0xE8E8FF, "Clear"}, {0xC0C0C0, "Silver"}, {0xE0D5C7, "Marble"},
        {0xFF7043, "Coral"}, {0x1A237E, "Navy"},   {0xBCAAA4, "Taupe"},
    };

    // Check for special preset names first
    for (const auto& entry : special_names) {
        if (entry.hex == rgb) {
            return entry.name;
        }
    }

    // Use algorithmic color description
    return helix::describe_color(rgb);
}

void AmsPanel::show_color_picker() {
    hide_color_picker();

    if (!parent_screen_) {
        spdlog::warn("[{}] No parent screen for color picker", get_name());
        return;
    }

    // Initialize selected color from current edit_slot_info_
    picker_selected_color_ = edit_slot_info_.color_rgb;

    // Create the color picker modal on the parent screen (above edit modal)
    color_picker_ = static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "color_picker", nullptr));
    if (!color_picker_) {
        spdlog::error("[{}] Failed to create color picker", get_name());
        return;
    }

    // Bind hex and name labels to subjects
    lv_obj_t* hex_label = lv_obj_find_by_name(color_picker_, "selected_hex_label");
    if (hex_label) {
        lv_label_bind_text(hex_label, &color_hex_subject_, nullptr);
    }

    lv_obj_t* name_label = lv_obj_find_by_name(color_picker_, "selected_name_label");
    if (name_label) {
        lv_label_bind_text(name_label, &color_name_subject_, nullptr);
    }

    // Initialize preview with current color
    update_color_picker_selection(picker_selected_color_);

    // Initialize HSV picker with current color and set callback
    lv_obj_t* hsv_picker = lv_obj_find_by_name(color_picker_, "hsv_picker");
    if (hsv_picker) {
        ui_hsv_picker_set_color_rgb(hsv_picker, picker_selected_color_);
        ui_hsv_picker_set_callback(
            hsv_picker,
            [](uint32_t rgb, void* user_data) {
                auto* panel = static_cast<AmsPanel*>(user_data);
                panel->update_color_picker_selection(rgb, true); // from HSV picker
            },
            this);
        spdlog::debug("[{}] HSV picker initialized with color #{:06X}", get_name(),
                      picker_selected_color_);
    }

    spdlog::info("[{}] Color picker shown with initial color #{:06X}", get_name(),
                 picker_selected_color_);
}

void AmsPanel::hide_color_picker() {
    if (color_picker_) {
        lv_obj_delete(color_picker_);
        color_picker_ = nullptr;
        spdlog::debug("[{}] Color picker hidden", get_name());
    }
}

void AmsPanel::update_color_picker_selection(uint32_t color_rgb, bool from_hsv_picker) {
    if (!color_picker_) {
        return;
    }

    picker_selected_color_ = color_rgb;

    // Update the preview swatch
    lv_obj_t* preview = lv_obj_find_by_name(color_picker_, "selected_color_preview");
    if (preview) {
        lv_obj_set_style_bg_color(preview, lv_color_hex(color_rgb), 0);
    }

    // Update the hex label via subject
    snprintf(color_hex_buf_, sizeof(color_hex_buf_), "#%06X", color_rgb);
    lv_subject_copy_string(&color_hex_subject_, color_hex_buf_);

    // Update the color name label via subject
    snprintf(color_name_buf_, sizeof(color_name_buf_), "%s",
             get_color_name_from_hex(color_rgb).c_str());
    lv_subject_copy_string(&color_name_subject_, color_name_buf_);

    // Sync HSV picker if change came from preset swatch (not from HSV picker itself)
    if (!from_hsv_picker) {
        lv_obj_t* hsv_picker = lv_obj_find_by_name(color_picker_, "hsv_picker");
        if (hsv_picker) {
            ui_hsv_picker_set_color_rgb(hsv_picker, color_rgb);
        }
    }
}

void AmsPanel::handle_color_picker_close() {
    spdlog::debug("[{}] Color picker close requested", get_name());
    hide_color_picker();
}

void AmsPanel::handle_color_swatch_clicked(lv_obj_t* swatch) {
    if (!swatch || !color_picker_) {
        return;
    }

    // Get the background color from the clicked swatch
    lv_color_t color = lv_obj_get_style_bg_color(swatch, LV_PART_MAIN);
    uint32_t rgb = lv_color_to_u32(color) & 0xFFFFFF;

    update_color_picker_selection(rgb);
}

void AmsPanel::handle_color_picker_cancel() {
    spdlog::debug("[{}] Color picker cancelled", get_name());
    hide_color_picker();
}

void AmsPanel::handle_color_picker_select() {
    spdlog::info("[{}] Color selected: #{:06X}", get_name(), picker_selected_color_);

    // Update the edit slot info with selected color
    edit_slot_info_.color_rgb = picker_selected_color_;
    edit_slot_info_.color_name = get_color_name_from_hex(picker_selected_color_);

    // Close the color picker
    hide_color_picker();

    // Update the edit modal's color swatch to show new selection
    if (edit_modal_) {
        lv_obj_t* swatch = lv_obj_find_by_name(edit_modal_, "color_swatch");
        if (swatch) {
            lv_obj_set_style_bg_color(swatch, lv_color_hex(picker_selected_color_), 0);
        }

        // Update color name label via subject
        snprintf(edit_color_name_buf_, sizeof(edit_color_name_buf_), "%s",
                 edit_slot_info_.color_name.c_str());
        lv_subject_copy_string(&edit_color_name_subject_, edit_color_name_buf_);

        update_sync_button_state();
    }
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<AmsPanel> g_ams_panel;
static lv_obj_t* s_ams_panel_obj = nullptr;

void destroy_ams_panel_ui() {
    if (s_ams_panel_obj) {
        spdlog::info("[AMS Panel] Destroying panel UI to free memory");

        // Unregister close callback BEFORE deleting to prevent double-invocation
        // (e.g., if destroy called manually while panel is in overlay stack)
        NavigationManager::instance().unregister_overlay_close_callback(s_ams_panel_obj);

        // Clear the panel_ reference in AmsPanel before deleting
        if (g_ams_panel) {
            g_ams_panel->clear_panel_reference();
        }

        lv_obj_delete(s_ams_panel_obj);
        s_ams_panel_obj = nullptr;

        // Note: Widget registrations remain (LVGL doesn't support unregistration)
        // Note: g_ams_panel C++ object stays for state preservation
    }
}

AmsPanel& get_global_ams_panel() {
    if (!g_ams_panel) {
        g_ams_panel = std::make_unique<AmsPanel>(get_printer_state(), nullptr);
    }

    // Lazy create the panel UI if not yet created
    if (!s_ams_panel_obj && g_ams_panel) {
        // Ensure widgets and XML are registered
        ensure_ams_widgets_registered();

        // Create the panel on the active screen
        lv_obj_t* screen = lv_scr_act();
        s_ams_panel_obj = static_cast<lv_obj_t*>(lv_xml_create(screen, "ams_panel", nullptr));

        if (s_ams_panel_obj) {
            // Initialize subjects if needed
            if (!g_ams_panel->are_subjects_initialized()) {
                g_ams_panel->init_subjects();
            }

            // Setup the panel
            g_ams_panel->setup(s_ams_panel_obj, screen);
            lv_obj_add_flag(s_ams_panel_obj, LV_OBJ_FLAG_HIDDEN); // Hidden by default

            // Register close callback to destroy UI when overlay is closed
            NavigationManager::instance().register_overlay_close_callback(
                s_ams_panel_obj, []() { destroy_ams_panel_ui(); });

            spdlog::info("[AMS Panel] Lazy-created panel UI with close callback");
        } else {
            spdlog::error("[AMS Panel] Failed to create panel from XML");
        }
    }

    return *g_ams_panel;
}
