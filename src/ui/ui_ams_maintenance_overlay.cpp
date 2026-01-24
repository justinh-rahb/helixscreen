// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_ams_maintenance_overlay.cpp
 * @brief Implementation of AmsMaintenanceOverlay
 */

#include "ui_ams_maintenance_overlay.h"

#include "ui_event_safety.h"
#include "ui_nav_manager.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "ams_types.h"
#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

#include <memory>

namespace helix::ui {

// ============================================================================
// SINGLETON ACCESSOR
// ============================================================================

static std::unique_ptr<AmsMaintenanceOverlay> g_ams_maintenance_overlay;

AmsMaintenanceOverlay& get_ams_maintenance_overlay() {
    if (!g_ams_maintenance_overlay) {
        g_ams_maintenance_overlay = std::make_unique<AmsMaintenanceOverlay>();
        StaticPanelRegistry::instance().register_destroy(
            "AmsMaintenanceOverlay", []() { g_ams_maintenance_overlay.reset(); });
    }
    return *g_ams_maintenance_overlay;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

AmsMaintenanceOverlay::AmsMaintenanceOverlay() {
    spdlog::debug("[{}] Created", get_name());
}

AmsMaintenanceOverlay::~AmsMaintenanceOverlay() {
    if (subjects_initialized_ && lv_is_initialized()) {
        lv_subject_deinit(&status_subject_);
    }
    spdlog::debug("[{}] Destroyed", get_name());
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void AmsMaintenanceOverlay::init_subjects() {
    if (subjects_initialized_) {
        return;
    }

    // Initialize status subject with default text
    snprintf(status_buf_, sizeof(status_buf_), "Idle");
    lv_subject_init_string(&status_subject_, status_buf_, nullptr, sizeof(status_buf_),
                           status_buf_);
    lv_xml_register_subject(nullptr, "ams_maintenance_status", &status_subject_);

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized", get_name());
}

void AmsMaintenanceOverlay::register_callbacks() {
    // Register button callbacks
    lv_xml_register_event_cb(nullptr, "on_ams_home_clicked", on_home_clicked);
    lv_xml_register_event_cb(nullptr, "on_ams_recover_clicked", on_recover_clicked);
    lv_xml_register_event_cb(nullptr, "on_ams_abort_clicked", on_abort_clicked);

    spdlog::debug("[{}] Callbacks registered", get_name());
}

// ============================================================================
// UI CREATION
// ============================================================================

lv_obj_t* AmsMaintenanceOverlay::create(lv_obj_t* parent) {
    if (overlay_) {
        spdlog::warn("[{}] create() called but overlay already exists", get_name());
        return overlay_;
    }

    spdlog::debug("[{}] Creating overlay...", get_name());

    // Create from XML component
    overlay_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "ams_settings_maintenance", nullptr));
    if (!overlay_) {
        spdlog::error("[{}] Failed to create overlay from XML", get_name());
        return nullptr;
    }

    // Initially hidden until show() pushes it
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);

    spdlog::info("[{}] Overlay created", get_name());
    return overlay_;
}

void AmsMaintenanceOverlay::show(lv_obj_t* parent_screen) {
    spdlog::debug("[{}] show() called", get_name());

    parent_screen_ = parent_screen;

    // Ensure subjects and callbacks are initialized
    if (!subjects_initialized_) {
        init_subjects();
        register_callbacks();
    }

    // Lazy create overlay
    if (!overlay_ && parent_screen_) {
        create(parent_screen_);
    }

    if (!overlay_) {
        spdlog::error("[{}] Cannot show - overlay not created", get_name());
        return;
    }

    // Update status from backend
    update_status();

    // Register with NavigationManager for lifecycle callbacks
    NavigationManager::instance().register_overlay_instance(overlay_, this);

    // Push onto navigation stack
    ui_nav_push_overlay(overlay_);
}

void AmsMaintenanceOverlay::refresh() {
    if (!overlay_) {
        return;
    }

    update_status();
}

// ============================================================================
// STATUS HANDLING
// ============================================================================

void AmsMaintenanceOverlay::update_status() {
    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        snprintf(status_buf_, sizeof(status_buf_), "No AMS connected");
        lv_subject_copy_string(&status_subject_, status_buf_);
        return;
    }

    AmsAction action = backend->get_current_action();
    const char* status_str = action_to_string(static_cast<int>(action));
    snprintf(status_buf_, sizeof(status_buf_), "%s", status_str);
    lv_subject_copy_string(&status_subject_, status_buf_);

    spdlog::debug("[{}] Status updated: {}", get_name(), status_buf_);
}

const char* AmsMaintenanceOverlay::action_to_string(int action) {
    switch (static_cast<AmsAction>(action)) {
    case AmsAction::IDLE:
        return "Idle";
    case AmsAction::LOADING:
        return "Loading filament...";
    case AmsAction::UNLOADING:
        return "Unloading filament...";
    case AmsAction::SELECTING:
        return "Selecting slot...";
    case AmsAction::RESETTING:
        return "Resetting...";
    case AmsAction::FORMING_TIP:
        return "Forming tip...";
    case AmsAction::CUTTING:
        return "Cutting filament...";
    case AmsAction::HEATING:
        return "Heating...";
    case AmsAction::CHECKING:
        return "Checking slots...";
    case AmsAction::PAUSED:
        return "Paused (attention needed)";
    case AmsAction::ERROR:
        return "Error state";
    default:
        return "Unknown";
    }
}

// ============================================================================
// STATIC CALLBACKS
// ============================================================================

void AmsMaintenanceOverlay::on_home_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsMaintenanceOverlay] on_home_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsMaintenanceOverlay] Home button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsMaintenanceOverlay] No backend available for home operation");
    } else {
        AmsError result = backend->reset();
        if (result.success()) {
            spdlog::info("[AmsMaintenanceOverlay] Home command sent successfully");
        } else {
            spdlog::error("[AmsMaintenanceOverlay] Home command failed: {}", result.technical_msg);
        }
        get_ams_maintenance_overlay().update_status();
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsMaintenanceOverlay::on_recover_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsMaintenanceOverlay] on_recover_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsMaintenanceOverlay] Recover button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsMaintenanceOverlay] No backend available for recover operation");
    } else {
        AmsError result = backend->recover();
        if (result.success()) {
            spdlog::info("[AmsMaintenanceOverlay] Recover command sent successfully");
        } else {
            spdlog::error("[AmsMaintenanceOverlay] Recover command failed: {}",
                          result.technical_msg);
        }
        get_ams_maintenance_overlay().update_status();
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsMaintenanceOverlay::on_abort_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsMaintenanceOverlay] on_abort_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsMaintenanceOverlay] Abort button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsMaintenanceOverlay] No backend available for abort operation");
    } else {
        AmsError result = backend->cancel();
        if (result.success()) {
            spdlog::info("[AmsMaintenanceOverlay] Abort command sent successfully");
        } else {
            spdlog::error("[AmsMaintenanceOverlay] Abort command failed: {}", result.technical_msg);
        }
        get_ams_maintenance_overlay().update_status();
    }

    LVGL_SAFE_EVENT_CB_END();
}

} // namespace helix::ui
