// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_spoolman.h"

#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_panel_common.h"
#include "ui_spool_canvas.h"
#include "ui_subject_registry.h"
#include "ui_theme.h"
#include "ui_toast.h"

#include "app_globals.h"
#include "moonraker_api.h"
#include "printer_state.h"
#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<SpoolmanPanel> g_spoolman_panel;

SpoolmanPanel& get_global_spoolman_panel() {
    if (!g_spoolman_panel) {
        g_spoolman_panel = std::make_unique<SpoolmanPanel>();
        StaticPanelRegistry::instance().register_destroy("SpoolmanPanel",
                                                         []() { g_spoolman_panel.reset(); });
    }
    return *g_spoolman_panel;
}

// ============================================================================
// Constructor
// ============================================================================

SpoolmanPanel::SpoolmanPanel() {
    spdlog::trace("[{}] Constructor", get_name());

    // Initialize buffer
    std::memset(spool_count_buf_, 0, sizeof(spool_count_buf_));
}

// ============================================================================
// Subject Initialization
// ============================================================================

void SpoolmanPanel::init_subjects() {
    if (subjects_initialized_) {
        spdlog::debug("[{}] Subjects already initialized", get_name());
        return;
    }

    spdlog::debug("[{}] Initializing subjects", get_name());

    // Initialize panel state subject (starts in LOADING state)
    UI_SUBJECT_INIT_AND_REGISTER_INT(panel_state_subject_,
                                     static_cast<int32_t>(SpoolmanPanelState::LOADING),
                                     "spoolman_panel_state");

    // Initialize spool count subject
    UI_SUBJECT_INIT_AND_REGISTER_STRING(spool_count_subject_, spool_count_buf_, "",
                                        "spoolman_spool_count");

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized: spoolman_panel_state, spoolman_spool_count",
                  get_name());
}

// ============================================================================
// Callback Registration
// ============================================================================

void SpoolmanPanel::register_callbacks() {
    if (callbacks_registered_) {
        spdlog::debug("[{}] Callbacks already registered", get_name());
        return;
    }

    spdlog::debug("[{}] Registering event callbacks", get_name());

    // Register XML event callbacks
    lv_xml_register_event_cb(nullptr, "on_spoolman_spool_row_clicked", on_spool_row_clicked);
    lv_xml_register_event_cb(nullptr, "on_spoolman_refresh_clicked", on_refresh_clicked);

    callbacks_registered_ = true;
    spdlog::debug("[{}] Event callbacks registered", get_name());
}

// ============================================================================
// Create
// ============================================================================

lv_obj_t* SpoolmanPanel::create(lv_obj_t* parent) {
    if (!parent) {
        spdlog::error("[{}] Cannot create: null parent", get_name());
        return nullptr;
    }

    spdlog::debug("[{}] Creating overlay from XML", get_name());

    parent_screen_ = parent;

    // Reset cleanup flag when (re)creating
    cleanup_called_ = false;

    // Create overlay from XML
    overlay_root_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "spoolman_panel", nullptr));

    if (!overlay_root_) {
        spdlog::error("[{}] Failed to create from XML", get_name());
        return nullptr;
    }

    // Standard overlay setup (handles back button and responsive layout)
    ui_overlay_panel_setup_standard(overlay_root_, parent_screen_, "overlay_header",
                                    "overlay_content");

    // Find widget references
    lv_obj_t* content = lv_obj_find_by_name(overlay_root_, "overlay_content");
    if (content) {
        spool_list_ = lv_obj_find_by_name(content, "spool_list");
    }

    if (!spool_list_) {
        spdlog::error("[{}] spool_list not found!", get_name());
        return nullptr;
    }

    // Initially hidden
    lv_obj_add_flag(overlay_root_, LV_OBJ_FLAG_HIDDEN);

    spdlog::info("[{}] Overlay created successfully", get_name());
    return overlay_root_;
}

// ============================================================================
// Lifecycle Hooks
// ============================================================================

void SpoolmanPanel::on_activate() {
    // Call base class first
    OverlayBase::on_activate();

    spdlog::debug("[{}] on_activate()", get_name());

    // Refresh spool list when panel becomes visible
    refresh_spools();
}

void SpoolmanPanel::on_deactivate() {
    spdlog::debug("[{}] on_deactivate()", get_name());

    // Call base class
    OverlayBase::on_deactivate();
}

// ============================================================================
// Data Loading
// ============================================================================

void SpoolmanPanel::refresh_spools() {
    MoonrakerAPI* api = get_moonraker_api();
    if (!api) {
        spdlog::warn("[{}] No API available, cannot refresh", get_name());
        show_empty_state();
        return;
    }

    show_loading_state();

    api->get_spoolman_spools(
        [this](const std::vector<SpoolInfo>& spools) {
            spdlog::info("[{}] Received {} spools from Spoolman", get_name(), spools.size());
            cached_spools_ = spools;

            // Also get active spool ID
            MoonrakerAPI* api_inner = get_moonraker_api();
            if (!api_inner) {
                spdlog::warn("[{}] API unavailable for status check", get_name());
                active_spool_id_ = -1;
                populate_spool_list();
                return;
            }

            api_inner->get_spoolman_status(
                [this](bool /*connected*/, int active_id) {
                    active_spool_id_ = active_id;
                    spdlog::debug("[{}] Active spool ID: {}", get_name(), active_spool_id_);
                    populate_spool_list();
                },
                [this](const MoonrakerError& err) {
                    spdlog::warn("[{}] Failed to get active spool: {}", get_name(), err.message);
                    active_spool_id_ = -1;
                    populate_spool_list();
                });
        },
        [this](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to fetch spools: {}", get_name(), err.message);
            cached_spools_.clear();
            show_empty_state();
            ui_toast_show(ToastSeverity::ERROR, "Failed to load spools", 3000);
        });
}

// ============================================================================
// UI State Management
// ============================================================================

void SpoolmanPanel::show_loading_state() {
    lv_subject_set_int(&panel_state_subject_, static_cast<int32_t>(SpoolmanPanelState::LOADING));
}

void SpoolmanPanel::show_empty_state() {
    lv_subject_set_int(&panel_state_subject_, static_cast<int32_t>(SpoolmanPanelState::EMPTY));
    update_spool_count();
}

void SpoolmanPanel::show_spool_list() {
    lv_subject_set_int(&panel_state_subject_, static_cast<int32_t>(SpoolmanPanelState::SPOOLS));
    update_spool_count();
}

void SpoolmanPanel::update_spool_count() {
    if (cached_spools_.empty()) {
        lv_subject_copy_string(&spool_count_subject_, "");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu spool%s", cached_spools_.size(),
                 cached_spools_.size() == 1 ? "" : "s");
        lv_subject_copy_string(&spool_count_subject_, buf);
    }
}

// ============================================================================
// Spool List Population
// ============================================================================

void SpoolmanPanel::populate_spool_list() {
    if (!spool_list_) {
        spdlog::error("[{}] spool_list_ is null", get_name());
        return;
    }

    // Clear existing rows
    lv_obj_clean(spool_list_);

    if (cached_spools_.empty()) {
        show_empty_state();
        return;
    }

    // Create a row for each spool
    for (const auto& spool : cached_spools_) {
        // Create row from XML component
        lv_obj_t* row =
            static_cast<lv_obj_t*>(lv_xml_create(spool_list_, "spoolman_spool_row", nullptr));

        if (!row) {
            spdlog::error("[{}] Failed to create row for spool {}", get_name(), spool.id);
            continue;
        }

        // Store spool ID in user_data for click handling
        lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(spool.id)));

        // Update row visuals
        update_row_visuals(row, spool);
    }

    show_spool_list();
    spdlog::debug("[{}] Populated {} spool rows", get_name(), cached_spools_.size());
}

void SpoolmanPanel::update_row_visuals(lv_obj_t* row, const SpoolInfo& spool) {
    if (!row)
        return;

    // Update 3D spool canvas
    lv_obj_t* canvas = lv_obj_find_by_name(row, "spool_canvas");
    if (canvas) {
        // Parse color from hex string (e.g., "FF5722" or "#FF5722")
        lv_color_t color = ui_theme_get_color("text_secondary"); // Default gray
        if (!spool.color_hex.empty()) {
            std::string hex = spool.color_hex;
            if (!hex.empty() && hex[0] == '#') {
                hex = hex.substr(1);
            }
            unsigned int color_val = 0;
            if (sscanf(hex.c_str(), "%x", &color_val) == 1) {
                color = lv_color_hex(color_val);
            }
        }
        ui_spool_canvas_set_color(canvas, color);

        // Fill level: remaining_percent / 100
        float fill_level = static_cast<float>(spool.remaining_percent()) / 100.0f;
        ui_spool_canvas_set_fill_level(canvas, fill_level);
        ui_spool_canvas_redraw(canvas);
    }

    // Update spool name (Material - Color)
    lv_obj_t* name_label = lv_obj_find_by_name(row, "spool_name");
    if (name_label) {
        lv_label_set_text(name_label, spool.display_name().c_str());
    }

    // Update vendor
    lv_obj_t* vendor_label = lv_obj_find_by_name(row, "spool_vendor");
    if (vendor_label) {
        lv_label_set_text(vendor_label, spool.vendor.empty() ? "Unknown" : spool.vendor.c_str());
    }

    // Update weight
    lv_obj_t* weight_label = lv_obj_find_by_name(row, "weight_text");
    if (weight_label) {
        char weight_buf[32];
        snprintf(weight_buf, sizeof(weight_buf), "%.0fg", spool.remaining_weight_g);
        lv_label_set_text(weight_label, weight_buf);
    }

    // Update percentage
    lv_obj_t* percent_label = lv_obj_find_by_name(row, "percent_text");
    if (percent_label) {
        char percent_buf[16];
        snprintf(percent_buf, sizeof(percent_buf), "%d%%",
                 static_cast<int>(spool.remaining_percent()));
        lv_label_set_text(percent_label, percent_buf);
    }

    // Low stock warning (< 100g)
    lv_obj_t* low_stock_icon = lv_obj_find_by_name(row, "low_stock_indicator");
    if (low_stock_icon) {
        if (spool.remaining_weight_g < 100.0) {
            lv_obj_remove_flag(low_stock_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(low_stock_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Active indicator
    lv_obj_t* active_icon = lv_obj_find_by_name(row, "active_indicator");
    if (active_icon) {
        if (spool.id == active_spool_id_) {
            lv_obj_remove_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ============================================================================
// Spool Selection
// ============================================================================

void SpoolmanPanel::handle_spool_clicked(lv_obj_t* row) {
    if (!row)
        return;

    // Get spool ID from user_data
    void* user_data = lv_obj_get_user_data(row);
    int spool_id = static_cast<int>(reinterpret_cast<intptr_t>(user_data));

    spdlog::info("[{}] Spool {} clicked", get_name(), spool_id);

    // Set as active spool
    set_active_spool(spool_id);
}

void SpoolmanPanel::set_active_spool(int spool_id) {
    MoonrakerAPI* api = get_moonraker_api();
    if (!api) {
        spdlog::warn("[{}] No API, cannot set active spool", get_name());
        return;
    }

    api->set_active_spool(
        spool_id,
        [this, spool_id]() {
            spdlog::info("[{}] Set active spool to {}", get_name(), spool_id);
            active_spool_id_ = spool_id;

            // Find the spool name for toast
            std::string spool_name = "Spool " + std::to_string(spool_id);
            for (const auto& s : cached_spools_) {
                if (s.id == spool_id) {
                    spool_name = s.display_name();
                    break;
                }
            }

            ui_toast_show(ToastSeverity::SUCCESS, ("Active: " + spool_name).c_str(), 2000);

            // Refresh to update active indicators
            populate_spool_list();
        },
        [this, spool_id](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to set active spool {}: {}", get_name(), spool_id,
                          err.message);
            ui_toast_show(ToastSeverity::ERROR, "Failed to set active spool", 3000);
        });
}

// ============================================================================
// Static Event Callbacks
// ============================================================================

void SpoolmanPanel::on_spool_row_clicked(lv_event_t* e) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // The target might be a child of the row, walk up to find the row
    lv_obj_t* row = target;
    while (row && lv_obj_get_user_data(row) == nullptr) {
        row = lv_obj_get_parent(row);
    }

    if (row) {
        get_global_spoolman_panel().handle_spool_clicked(row);
    }
}

void SpoolmanPanel::on_refresh_clicked(lv_event_t* /*e*/) {
    spdlog::debug("[Spoolman] Refresh clicked");
    get_global_spoolman_panel().refresh_spools();
}
