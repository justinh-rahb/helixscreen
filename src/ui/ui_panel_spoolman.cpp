// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_spoolman.h"

#include "ui_global_panel_helper.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_panel_common.h"
#include "ui_spool_canvas.h"
#include "ui_subject_registry.h"
#include "ui_toast.h"
#include "ui_update_queue.h"

#include "ams_state.h"
#include "app_globals.h"
#include "format_utils.h"
#include "lvgl/src/others/translation/lv_translation.h"
#include "moonraker_api.h"
#include "printer_state.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

// ============================================================================
// Global Instance
// ============================================================================

DEFINE_GLOBAL_PANEL(SpoolmanPanel, g_spoolman_panel, get_global_spoolman_panel)

// ============================================================================
// Helpers
// ============================================================================

/**
 * Parse a hex color string (with or without '#' prefix) into an lv_color_t.
 * Returns fallback_color if the string is empty or unparseable.
 */
static lv_color_t parse_spool_color(const std::string& color_hex, lv_color_t fallback_color) {
    if (color_hex.empty()) {
        return fallback_color;
    }

    const char* hex = color_hex.c_str();
    if (hex[0] == '#') {
        hex++;
    }

    unsigned int color_val = 0;
    if (sscanf(hex, "%x", &color_val) == 1) {
        return lv_color_hex(color_val);
    }
    return fallback_color;
}

// ============================================================================
// Constructor
// ============================================================================

SpoolmanPanel::SpoolmanPanel() {
    spdlog::trace("[{}] Constructor", get_name());
    std::memset(header_title_buf_, 0, sizeof(header_title_buf_));
}

SpoolmanPanel::~SpoolmanPanel() {
    deinit_subjects();
}

// ============================================================================
// Subject Initialization
// ============================================================================

void SpoolmanPanel::init_subjects() {
    init_subjects_guarded([this]() {
        // Initialize panel state subject (starts in LOADING state)
        UI_MANAGED_SUBJECT_INT(panel_state_subject_,
                               static_cast<int32_t>(SpoolmanPanelState::LOADING),
                               "spoolman_panel_state", subjects_);

        UI_MANAGED_SUBJECT_STRING(header_title_subject_, header_title_buf_, "Spoolman",
                                  "spoolman_header_title", subjects_);
    });
}

void SpoolmanPanel::deinit_subjects() {
    if (!subjects_initialized_) {
        return;
    }
    subjects_.deinit_all();
    subjects_initialized_ = false;
    spdlog::debug("[SpoolmanPanel] Subjects deinitialized");
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
    register_callbacks();

    if (!create_overlay_from_xml(parent, "spoolman_panel")) {
        return nullptr;
    }

    // Find widget references
    lv_obj_t* content = lv_obj_find_by_name(overlay_root_, "overlay_content");
    if (content) {
        spool_list_ = lv_obj_find_by_name(content, "spool_list");
    }

    if (!spool_list_) {
        spdlog::error("[{}] spool_list not found!", get_name());
        return nullptr;
    }

    // Bind header title to subject for dynamic "Spoolman: XX Spools" text
    lv_obj_t* header = lv_obj_find_by_name(overlay_root_, "overlay_header");
    if (header) {
        lv_obj_t* title = lv_obj_find_by_name(header, "header_title");
        if (title) {
            lv_label_bind_text(title, &header_title_subject_, nullptr);
        }
    }

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

    // Start Spoolman polling for weight updates
    AmsState::instance().start_spoolman_polling();
}

void SpoolmanPanel::on_deactivate() {
    AmsState::instance().stop_spoolman_polling();

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

            // Also get active spool ID before updating UI
            MoonrakerAPI* api_inner = get_moonraker_api();
            if (!api_inner) {
                spdlog::warn("[{}] API unavailable for status check", get_name());
                // Schedule UI update on main thread
                auto* data = new std::pair<SpoolmanPanel*, std::vector<SpoolInfo>>(this, spools);
                ui_async_call(
                    [](void* ud) {
                        auto* ctx =
                            static_cast<std::pair<SpoolmanPanel*, std::vector<SpoolInfo>>*>(ud);
                        ctx->first->cached_spools_ = std::move(ctx->second);
                        ctx->first->active_spool_id_ = -1;
                        ctx->first->populate_spool_list();
                        delete ctx;
                    },
                    data);
                return;
            }

            api_inner->get_spoolman_status(
                [this, spools](bool /*connected*/, int active_id) {
                    spdlog::debug("[{}] Active spool ID: {}", get_name(), active_id);
                    // Schedule UI update on main thread
                    auto* data = new std::tuple<SpoolmanPanel*, std::vector<SpoolInfo>, int>(
                        this, spools, active_id);
                    ui_async_call(
                        [](void* ud) {
                            auto* ctx = static_cast<
                                std::tuple<SpoolmanPanel*, std::vector<SpoolInfo>, int>*>(ud);
                            auto* self = std::get<0>(*ctx);
                            self->cached_spools_ = std::move(std::get<1>(*ctx));
                            self->active_spool_id_ = std::get<2>(*ctx);
                            self->populate_spool_list();
                            delete ctx;
                        },
                        data);
                },
                [this, spools](const MoonrakerError& err) {
                    spdlog::warn("[{}] Failed to get active spool: {}", get_name(), err.message);
                    auto* data =
                        new std::pair<SpoolmanPanel*, std::vector<SpoolInfo>>(this, spools);
                    ui_async_call(
                        [](void* ud) {
                            auto* ctx =
                                static_cast<std::pair<SpoolmanPanel*, std::vector<SpoolInfo>>*>(ud);
                            ctx->first->cached_spools_ = std::move(ctx->second);
                            ctx->first->active_spool_id_ = -1;
                            ctx->first->populate_spool_list();
                            delete ctx;
                        },
                        data);
                });
        },
        [this](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to fetch spools: {}", get_name(), err.message);
            ui_async_call(
                [](void* ud) {
                    auto* self = static_cast<SpoolmanPanel*>(ud);
                    self->cached_spools_.clear();
                    self->show_empty_state();
                    ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to load spools"), 3000);
                },
                this);
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
        lv_subject_copy_string(&header_title_subject_, "Spoolman");
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Spoolman: %zu Spool%s", cached_spools_.size(),
                 cached_spools_.size() == 1 ? "" : "s");
        lv_subject_copy_string(&header_title_subject_, buf);
    }
}

// ============================================================================
// Cache Lookup
// ============================================================================

const SpoolInfo* SpoolmanPanel::find_cached_spool(int spool_id) const {
    auto it = std::find_if(cached_spools_.begin(), cached_spools_.end(),
                           [spool_id](const SpoolInfo& s) { return s.id == spool_id; });
    return it != cached_spools_.end() ? &(*it) : nullptr;
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
        lv_color_t color =
            parse_spool_color(spool.color_hex, theme_manager_get_color("text_muted"));
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
        const char* vendor = spool.vendor.empty() ? "Unknown" : spool.vendor.c_str();
        lv_label_set_text(vendor_label, vendor);
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
        helix::fmt::format_percent(static_cast<int>(spool.remaining_percent()), percent_buf,
                                   sizeof(percent_buf));
        lv_label_set_text(percent_label, percent_buf);
    }

    // Low stock warning
    lv_obj_t* low_stock_icon = lv_obj_find_by_name(row, "low_stock_indicator");
    if (low_stock_icon) {
        if (spool.is_low()) {
            lv_obj_remove_flag(low_stock_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(low_stock_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Active spool: show checkmark + highlight row with checked state
    bool is_active = (spool.id == active_spool_id_);
    lv_obj_t* active_icon = lv_obj_find_by_name(row, "active_indicator");
    if (active_icon) {
        if (is_active) {
            lv_obj_remove_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (is_active) {
        lv_obj_add_state(row, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(row, LV_STATE_CHECKED);
    }
}

void SpoolmanPanel::update_active_indicators() {
    if (!spool_list_)
        return;

    uint32_t count = lv_obj_get_child_count(spool_list_);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_get_child(spool_list_, static_cast<int32_t>(i));
        if (!row)
            continue;

        int row_spool_id = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row)));
        bool is_active = (row_spool_id == active_spool_id_);

        // Update checked state (drives the active_style in XML)
        if (is_active) {
            lv_obj_add_state(row, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(row, LV_STATE_CHECKED);
        }

        // Update active indicator icon visibility
        lv_obj_t* active_icon = lv_obj_find_by_name(row, "active_indicator");
        if (active_icon) {
            if (is_active) {
                lv_obj_remove_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(active_icon, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    spdlog::debug("[{}] Updated active indicators (active={})", get_name(), active_spool_id_);
}

// ============================================================================
// Spool Selection
// ============================================================================

void SpoolmanPanel::handle_spool_clicked(lv_obj_t* row, lv_point_t click_pt) {
    if (!row)
        return;

    // Get spool ID from user_data
    void* user_data = lv_obj_get_user_data(row);
    int spool_id = static_cast<int>(reinterpret_cast<intptr_t>(user_data));

    spdlog::info("[{}] Spool {} clicked", get_name(), spool_id);

    const SpoolInfo* spool = find_cached_spool(spool_id);
    if (!spool) {
        spdlog::warn("[{}] Spool {} not found in cache", get_name(), spool_id);
        return;
    }

    // Set up context menu action handler
    context_menu_.set_action_callback([this](helix::ui::SpoolmanContextMenu::MenuAction action,
                                             int id) { handle_context_action(action, id); });

    // Show context menu near the click point
    context_menu_.set_click_point(click_pt);
    context_menu_.show_for_spool(lv_screen_active(), *spool, row);
}

void SpoolmanPanel::handle_context_action(helix::ui::SpoolmanContextMenu::MenuAction action,
                                          int spool_id) {
    using MenuAction = helix::ui::SpoolmanContextMenu::MenuAction;

    switch (action) {
    case MenuAction::SET_ACTIVE:
        set_active_spool(spool_id);
        break;

    case MenuAction::EDIT:
        show_edit_modal(spool_id);
        break;

    case MenuAction::PRINT_LABEL:
        // TODO: Print label (Phase 4)
        spdlog::info("[{}] Print label for spool {} (not yet implemented)", get_name(), spool_id);
        ui_toast_show(ToastSeverity::INFO, "Label printing coming soon", 2000);
        break;

    case MenuAction::DELETE:
        delete_spool(spool_id);
        break;

    case MenuAction::CANCELLED:
        spdlog::debug("[{}] Context menu cancelled", get_name());
        break;
    }
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

            // Find the spool name for toast (safe on any thread -- reads cached data)
            const SpoolInfo* found = find_cached_spool(spool_id);
            std::string spool_name =
                found ? found->display_name() : "Spool " + std::to_string(spool_id);

            // Update UI on main thread
            ui_async_call(
                [](void* ud) {
                    auto* ctx =
                        static_cast<std::pair<SpoolmanPanel*, std::pair<int, std::string>>*>(ud);
                    auto* self = ctx->first;
                    int id = ctx->second.first;
                    std::string name = std::move(ctx->second.second);
                    delete ctx;

                    self->active_spool_id_ = id;
                    self->update_active_indicators();
                    ui_toast_show(ToastSeverity::SUCCESS, ("Active: " + name).c_str(), 2000);
                },
                new std::pair<SpoolmanPanel*, std::pair<int, std::string>>(
                    this, {spool_id, std::move(spool_name)}));
        },
        [this, spool_id](const MoonrakerError& err) {
            spdlog::error("[{}] Failed to set active spool {}: {}", get_name(), spool_id,
                          err.message);
            ui_async_call(
                [](void*) {
                    ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to set active spool"), 3000);
                },
                nullptr);
        });
}

// ============================================================================
// Edit Spool Modal
// ============================================================================

void SpoolmanPanel::show_edit_modal(int spool_id) {
    const SpoolInfo* spool = find_cached_spool(spool_id);
    if (!spool) {
        spdlog::warn("[{}] Cannot edit - spool {} not in cache", get_name(), spool_id);
        return;
    }

    MoonrakerAPI* api = get_moonraker_api();

    edit_modal_.set_completion_callback([this](bool saved) {
        if (saved) {
            // Refresh the spool list to show updated values
            refresh_spools();
        }
    });

    edit_modal_.show_for_spool(lv_screen_active(), *spool, api);
}

// ============================================================================
// Delete Spool
// ============================================================================

void SpoolmanPanel::delete_spool(int spool_id) {
    // Build confirmation message with spool info
    const SpoolInfo* spool = find_cached_spool(spool_id);
    std::string spool_desc;
    if (spool) {
        spool_desc = spool->display_name() + " (#" + std::to_string(spool_id) + ")";
    } else {
        spool_desc = "Spool #" + std::to_string(spool_id);
    }

    std::string message = spool_desc + "\nThis cannot be undone.";

    // Store spool_id for the confirmation callback via a static (only one delete at a time)
    static int s_pending_delete_id = 0;
    s_pending_delete_id = spool_id;

    ui_modal_show_confirmation(
        "Delete Spool?", message.c_str(), ModalSeverity::Warning, "Delete",
        [](lv_event_t* /*e*/) {
            // Close the confirmation dialog immediately
            lv_obj_t* top = Modal::get_top();
            if (top) {
                Modal::hide(top);
            }

            int id = s_pending_delete_id;
            spdlog::info("[Spoolman] Confirmed delete of spool {}", id);

            MoonrakerAPI* api = get_moonraker_api();
            if (!api) {
                ui_toast_show(ToastSeverity::ERROR, "API not available", 3000);
                return;
            }

            api->delete_spoolman_spool(
                id,
                [id]() {
                    spdlog::info("[Spoolman] Spool {} deleted successfully", id);
                    // Schedule UI work on LVGL thread (API callbacks run on background thread)
                    ui_async_call(
                        [](void*) {
                            ui_toast_show(ToastSeverity::SUCCESS, "Spool deleted", 2000);
                            get_global_spoolman_panel().refresh_spools();
                        },
                        nullptr);
                },
                [id](const MoonrakerError& err) {
                    spdlog::error("[Spoolman] Failed to delete spool {}: {}", id, err.message);
                    ui_async_call(
                        [](void*) {
                            ui_toast_show(ToastSeverity::ERROR, "Failed to delete spool", 3000);
                        },
                        nullptr);
                });
        },
        nullptr, // No cancel callback needed
        nullptr);
}

// ============================================================================
// Static Event Callbacks
// ============================================================================

void SpoolmanPanel::on_spool_row_clicked(lv_event_t* e) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Capture click point from the input device while event is still active
    lv_point_t click_pt = {0, 0};
    lv_indev_t* indev = lv_indev_active();
    if (indev) {
        lv_indev_get_point(indev, &click_pt);
    }

    // The target might be a child of the row, walk up to find the row
    lv_obj_t* row = target;
    while (row && lv_obj_get_user_data(row) == nullptr) {
        row = lv_obj_get_parent(row);
    }

    if (row) {
        get_global_spoolman_panel().handle_spool_clicked(row, click_pt);
    }
}

void SpoolmanPanel::on_refresh_clicked(lv_event_t* /*e*/) {
    spdlog::debug("[Spoolman] Refresh clicked");
    get_global_spoolman_panel().refresh_spools();
}
