// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_ams_device_operations_overlay.cpp
 * @brief Implementation of AmsDeviceOperationsOverlay
 */

#include "ui_ams_device_operations_overlay.h"

#include "ui_event_safety.h"
#include "ui_nav_manager.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "ams_types.h"
#include "static_panel_registry.h"
#include "theme_manager.h"

#include <spdlog/spdlog.h>

#include <memory>

namespace helix::ui {

// ============================================================================
// SINGLETON ACCESSOR
// ============================================================================

static std::unique_ptr<AmsDeviceOperationsOverlay> g_ams_device_operations_overlay;

AmsDeviceOperationsOverlay& get_ams_device_operations_overlay() {
    if (!g_ams_device_operations_overlay) {
        g_ams_device_operations_overlay = std::make_unique<AmsDeviceOperationsOverlay>();
        StaticPanelRegistry::instance().register_destroy(
            "AmsDeviceOperationsOverlay", []() { g_ams_device_operations_overlay.reset(); });
    }
    return *g_ams_device_operations_overlay;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

AmsDeviceOperationsOverlay::AmsDeviceOperationsOverlay() {
    spdlog::debug("[{}] Created", get_name());
}

AmsDeviceOperationsOverlay::~AmsDeviceOperationsOverlay() {
    if (subjects_initialized_ && lv_is_initialized()) {
        lv_subject_deinit(&status_subject_);
        lv_subject_deinit(&supports_bypass_subject_);
        lv_subject_deinit(&bypass_active_subject_);
        lv_subject_deinit(&supports_auto_heat_subject_);
        lv_subject_deinit(&has_backend_subject_);
        lv_subject_deinit(&has_calibration_subject_);
        lv_subject_deinit(&has_speed_subject_);
    }
    spdlog::debug("[{}] Destroyed", get_name());
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void AmsDeviceOperationsOverlay::init_subjects() {
    if (subjects_initialized_) {
        return;
    }

    // Initialize status subject with default text
    snprintf(status_buf_, sizeof(status_buf_), "Idle");
    lv_subject_init_string(&status_subject_, status_buf_, nullptr, sizeof(status_buf_),
                           status_buf_);
    lv_xml_register_subject(nullptr, "ams_device_ops_status", &status_subject_);

    // Initialize bypass support subject (0=not supported, 1=supported)
    lv_subject_init_int(&supports_bypass_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_supports_bypass", &supports_bypass_subject_);

    // Initialize bypass active subject (0=inactive, 1=active)
    lv_subject_init_int(&bypass_active_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_bypass_active", &bypass_active_subject_);

    // Initialize auto-heat support subject (0=not supported, 1=supported)
    lv_subject_init_int(&supports_auto_heat_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_supports_auto_heat",
                            &supports_auto_heat_subject_);

    // Initialize backend presence subject (0=no backend, 1=has backend)
    lv_subject_init_int(&has_backend_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_has_backend", &has_backend_subject_);

    // Initialize calibration actions presence subject (0=none, 1=has actions)
    lv_subject_init_int(&has_calibration_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_has_calibration", &has_calibration_subject_);

    // Initialize speed actions presence subject (0=none, 1=has actions)
    lv_subject_init_int(&has_speed_subject_, 0);
    lv_xml_register_subject(nullptr, "ams_device_ops_has_speed", &has_speed_subject_);

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized", get_name());
}

void AmsDeviceOperationsOverlay::register_callbacks() {
    // Register quick action button callbacks
    lv_xml_register_event_cb(nullptr, "on_ams_device_ops_home", on_home_clicked);
    lv_xml_register_event_cb(nullptr, "on_ams_device_ops_recover", on_recover_clicked);
    lv_xml_register_event_cb(nullptr, "on_ams_device_ops_abort", on_abort_clicked);

    // Register bypass toggle callback
    lv_xml_register_event_cb(nullptr, "on_ams_device_ops_bypass_toggled", on_bypass_toggled);

    spdlog::debug("[{}] Callbacks registered", get_name());
}

// ============================================================================
// UI CREATION
// ============================================================================

lv_obj_t* AmsDeviceOperationsOverlay::create(lv_obj_t* parent) {
    if (overlay_) {
        spdlog::warn("[{}] create() called but overlay already exists", get_name());
        return overlay_;
    }

    spdlog::debug("[{}] Creating overlay...", get_name());

    // Create from XML component
    overlay_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "ams_device_operations", nullptr));
    if (!overlay_) {
        spdlog::error("[{}] Failed to create overlay from XML", get_name());
        return nullptr;
    }

    // Find dynamic action containers
    calibration_container_ = lv_obj_find_by_name(overlay_, "calibration_actions_container");
    speed_container_ = lv_obj_find_by_name(overlay_, "speed_actions_container");

    if (!calibration_container_) {
        spdlog::warn("[{}] calibration_actions_container not found in XML", get_name());
    }
    if (!speed_container_) {
        spdlog::warn("[{}] speed_actions_container not found in XML", get_name());
    }

    // Initially hidden until show() pushes it
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);

    spdlog::info("[{}] Overlay created", get_name());
    return overlay_;
}

void AmsDeviceOperationsOverlay::show(lv_obj_t* parent_screen) {
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

    // Update from backend
    refresh();

    // Register with NavigationManager for lifecycle callbacks
    NavigationManager::instance().register_overlay_instance(overlay_, this);

    // Push onto navigation stack
    ui_nav_push_overlay(overlay_);
}

void AmsDeviceOperationsOverlay::refresh() {
    if (!overlay_) {
        return;
    }

    spdlog::debug("[{}] Refreshing from backend", get_name());
    update_from_backend();
}

// ============================================================================
// BACKEND QUERIES
// ============================================================================

void AmsDeviceOperationsOverlay::update_from_backend() {
    AmsBackend* backend = AmsState::instance().get_backend();

    if (!backend) {
        spdlog::warn("[{}] No backend available", get_name());
        // Show no backend message
        lv_subject_set_int(&has_backend_subject_, 0);
        lv_subject_set_int(&supports_bypass_subject_, 0);
        lv_subject_set_int(&bypass_active_subject_, 0);
        lv_subject_set_int(&supports_auto_heat_subject_, 0);
        lv_subject_set_int(&has_calibration_subject_, 0);
        lv_subject_set_int(&has_speed_subject_, 0);
        snprintf(status_buf_, sizeof(status_buf_), "No AMS connected");
        lv_subject_copy_string(&status_subject_, status_buf_);

        // Clear dynamic action containers
        if (calibration_container_) {
            clear_actions(calibration_container_);
        }
        if (speed_container_) {
            clear_actions(speed_container_);
        }
        return;
    }

    // Has backend
    lv_subject_set_int(&has_backend_subject_, 1);

    // Query backend capabilities
    auto info = backend->get_system_info();
    bool supports_bypass = info.supports_bypass;
    bool bypass_active = backend->is_bypass_active();
    bool supports_auto_heat = backend->supports_auto_heat_on_load();

    spdlog::debug("[{}] Backend caps: bypass={}, bypass_active={}, auto_heat={}", get_name(),
                  supports_bypass, bypass_active, supports_auto_heat);

    // Update capability subjects
    lv_subject_set_int(&supports_bypass_subject_, supports_bypass ? 1 : 0);
    lv_subject_set_int(&bypass_active_subject_, bypass_active ? 1 : 0);
    lv_subject_set_int(&supports_auto_heat_subject_, supports_auto_heat ? 1 : 0);

    // Update status
    AmsAction action = backend->get_current_action();
    const char* status_str = action_to_string(static_cast<int>(action));
    snprintf(status_buf_, sizeof(status_buf_), "%s", status_str);
    lv_subject_copy_string(&status_subject_, status_buf_);

    // Get all device actions from backend
    cached_actions_ = backend->get_device_actions();
    spdlog::debug("[{}] Got {} device actions from backend", get_name(), cached_actions_.size());

    // Populate dynamic sections
    action_ids_.clear();

    int calibration_count = 0;
    int speed_count = 0;

    if (calibration_container_) {
        clear_actions(calibration_container_);
        calibration_count = populate_section_actions(calibration_container_, "calibration");
    }

    if (speed_container_) {
        clear_actions(speed_container_);
        speed_count = populate_section_actions(speed_container_, "speed");
    }

    // Update section visibility subjects
    lv_subject_set_int(&has_calibration_subject_, calibration_count > 0 ? 1 : 0);
    lv_subject_set_int(&has_speed_subject_, speed_count > 0 ? 1 : 0);

    spdlog::debug("[{}] Populated {} calibration, {} speed actions", get_name(), calibration_count,
                  speed_count);
}

int AmsDeviceOperationsOverlay::populate_section_actions(lv_obj_t* container,
                                                         const std::string& section_id) {
    int count = 0;
    for (const auto& action : cached_actions_) {
        if (action.section == section_id) {
            create_action_control(container, action);
            count++;
        }
    }
    return count;
}

void AmsDeviceOperationsOverlay::create_action_control(lv_obj_t* parent,
                                                       const helix::printer::DeviceAction& action) {
    spdlog::debug("[{}] Creating action control: {} (type={})", get_name(), action.label,
                  helix::printer::action_type_to_string(action.type));

    // Create row container for action
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, theme_manager_get_spacing("space_xs"), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    switch (action.type) {
    case helix::printer::ActionType::BUTTON: {
        // Create action button spanning full width
        lv_obj_t* btn = lv_button_create(row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, theme_manager_get_spacing("button_height_sm"));
        lv_obj_set_style_radius(btn, theme_manager_get_spacing("border_radius"), 0);

        // Button label
        lv_obj_t* btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, action.label.c_str());
        lv_obj_center(btn_label);

        // Store action ID in vector, pass index as user_data
        action_ids_.push_back(action.id);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(action_ids_.size() - 1));

        // Register click callback
        lv_obj_add_event_cb(btn, on_action_clicked, LV_EVENT_CLICKED, nullptr);

        // Handle disabled state
        if (!action.enabled) {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
            if (!action.disable_reason.empty()) {
                spdlog::debug("[{}] Action '{}' disabled: {}", get_name(), action.id,
                              action.disable_reason);
            }
        }
        break;
    }

    case helix::printer::ActionType::TOGGLE: {
        // Label on left
        lv_obj_t* label = lv_label_create(row);
        lv_label_set_text(label, action.label.c_str());
        lv_obj_set_style_text_color(label, theme_manager_get_color("text_primary"), 0);

        // Switch on right
        lv_obj_t* sw = lv_switch_create(row);

        // Try to get current value
        try {
            if (action.current_value.has_value()) {
                bool val = std::any_cast<bool>(action.current_value);
                if (val) {
                    lv_obj_add_state(sw, LV_STATE_CHECKED);
                }
            }
        } catch (const std::bad_any_cast&) {
            spdlog::warn("[{}] Failed to cast toggle value for {}", get_name(), action.id);
        }

        // Store action ID in vector, pass index as user_data
        action_ids_.push_back(action.id);
        lv_obj_set_user_data(sw, reinterpret_cast<void*>(action_ids_.size() - 1));

        // Register value changed callback
        lv_obj_add_event_cb(sw, on_toggle_changed, LV_EVENT_VALUE_CHANGED, nullptr);
        break;
    }

    case helix::printer::ActionType::INFO: {
        // Label on left
        lv_obj_t* label = lv_label_create(row);
        lv_label_set_text(label, action.label.c_str());
        lv_obj_set_style_text_color(label, theme_manager_get_color("text_primary"), 0);

        // Value on right
        lv_obj_t* value_label = lv_label_create(row);
        lv_obj_set_style_text_color(value_label, theme_manager_get_color("text_secondary"), 0);
        try {
            if (action.current_value.has_value()) {
                std::string val = std::any_cast<std::string>(action.current_value);
                if (!action.unit.empty()) {
                    val += " " + action.unit;
                }
                lv_label_set_text(value_label, val.c_str());
            } else {
                lv_label_set_text(value_label, "-");
            }
        } catch (const std::bad_any_cast&) {
            lv_label_set_text(value_label, "-");
        }
        break;
    }

    case helix::printer::ActionType::SLIDER:
    case helix::printer::ActionType::DROPDOWN:
        // Placeholder for future implementation
        {
            lv_obj_t* label = lv_label_create(row);
            std::string text = action.label + " (coming soon)";
            lv_label_set_text(label, text.c_str());
            lv_obj_set_style_text_color(label, theme_manager_get_color("text_secondary"), 0);
            spdlog::debug("[{}] {} control '{}' placeholder created", get_name(),
                          helix::printer::action_type_to_string(action.type), action.id);
        }
        break;
    }
}

void AmsDeviceOperationsOverlay::clear_actions(lv_obj_t* container) {
    if (!container) {
        return;
    }
    lv_obj_clean(container);
}

const char* AmsDeviceOperationsOverlay::action_to_string(int action) {
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

void AmsDeviceOperationsOverlay::on_home_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_home_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsDeviceOperationsOverlay] Home button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsDeviceOperationsOverlay] No backend available for home operation");
    } else {
        AmsError result = backend->reset();
        if (result.success()) {
            spdlog::info("[AmsDeviceOperationsOverlay] Home command sent successfully");
        } else {
            spdlog::error("[AmsDeviceOperationsOverlay] Home command failed: {}",
                          result.technical_msg);
        }
        get_ams_device_operations_overlay().refresh();
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsDeviceOperationsOverlay::on_recover_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_recover_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsDeviceOperationsOverlay] Recover button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsDeviceOperationsOverlay] No backend available for recover operation");
    } else {
        AmsError result = backend->recover();
        if (result.success()) {
            spdlog::info("[AmsDeviceOperationsOverlay] Recover command sent successfully");
        } else {
            spdlog::error("[AmsDeviceOperationsOverlay] Recover command failed: {}",
                          result.technical_msg);
        }
        get_ams_device_operations_overlay().refresh();
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsDeviceOperationsOverlay::on_abort_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_abort_clicked");
    LV_UNUSED(e);

    spdlog::info("[AmsDeviceOperationsOverlay] Abort button clicked");

    AmsBackend* backend = AmsState::instance().get_backend();
    if (!backend) {
        spdlog::warn("[AmsDeviceOperationsOverlay] No backend available for abort operation");
    } else {
        AmsError result = backend->cancel();
        if (result.success()) {
            spdlog::info("[AmsDeviceOperationsOverlay] Abort command sent successfully");
        } else {
            spdlog::error("[AmsDeviceOperationsOverlay] Abort command failed: {}",
                          result.technical_msg);
        }
        get_ams_device_operations_overlay().refresh();
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsDeviceOperationsOverlay::on_bypass_toggled(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_bypass_toggled");

    auto* toggle = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!toggle || !lv_obj_is_valid(toggle)) {
        spdlog::warn("[AmsDeviceOperationsOverlay] Stale callback - toggle no longer valid");
    } else {
        bool is_checked = lv_obj_has_state(toggle, LV_STATE_CHECKED);

        spdlog::info("[AmsDeviceOperationsOverlay] Bypass toggle: {}",
                     is_checked ? "enabled" : "disabled");

        AmsBackend* backend = AmsState::instance().get_backend();
        if (backend) {
            AmsError result;
            if (is_checked) {
                result = backend->enable_bypass();
            } else {
                result = backend->disable_bypass();
            }

            if (result.success()) {
                spdlog::info("[AmsDeviceOperationsOverlay] Bypass mode {}",
                             is_checked ? "enabled" : "disabled");
                // Update the active subject to reflect new state
                lv_subject_set_int(&get_ams_device_operations_overlay().bypass_active_subject_,
                                   is_checked ? 1 : 0);
            } else {
                spdlog::error("[AmsDeviceOperationsOverlay] Failed to {} bypass: {}",
                              is_checked ? "enable" : "disable", result.user_msg);
                // Revert the toggle state on failure
                if (is_checked) {
                    lv_obj_remove_state(toggle, LV_STATE_CHECKED);
                } else {
                    lv_obj_add_state(toggle, LV_STATE_CHECKED);
                }
            }
        } else {
            spdlog::error("[AmsDeviceOperationsOverlay] No backend available for bypass toggle");
            // Revert the toggle state
            if (is_checked) {
                lv_obj_remove_state(toggle, LV_STATE_CHECKED);
            } else {
                lv_obj_add_state(toggle, LV_STATE_CHECKED);
            }
        }
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsDeviceOperationsOverlay::on_action_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_action_clicked");

    auto* btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!btn || !lv_obj_is_valid(btn)) {
        spdlog::warn("[AmsDeviceOperationsOverlay] on_action_clicked: invalid target");
    } else {
        // Get action ID from vector using index stored in user_data
        auto& overlay = get_ams_device_operations_overlay();
        auto index = reinterpret_cast<size_t>(lv_obj_get_user_data(btn));
        if (index >= overlay.action_ids_.size()) {
            spdlog::warn("[AmsDeviceOperationsOverlay] Invalid action index: {}", index);
        } else {
            const std::string& action_id = overlay.action_ids_[index];
            spdlog::info("[AmsDeviceOperationsOverlay] Action clicked: {}", action_id);

            // Execute via backend
            AmsBackend* backend = AmsState::instance().get_backend();
            if (!backend) {
                spdlog::warn("[AmsDeviceOperationsOverlay] No backend available for action");
            } else {
                AmsError result = backend->execute_device_action(action_id);
                if (result.success()) {
                    spdlog::info("[AmsDeviceOperationsOverlay] Action '{}' executed successfully",
                                 action_id);
                    // Update status
                    snprintf(overlay.status_buf_, sizeof(overlay.status_buf_), "Executed: %s",
                             action_id.c_str());
                    lv_subject_copy_string(&overlay.status_subject_, overlay.status_buf_);
                } else {
                    spdlog::error("[AmsDeviceOperationsOverlay] Action '{}' failed: {}", action_id,
                                  result.technical_msg);
                    // Update status with error
                    snprintf(overlay.status_buf_, sizeof(overlay.status_buf_), "Failed: %s",
                             result.user_msg.c_str());
                    lv_subject_copy_string(&overlay.status_subject_, overlay.status_buf_);
                }
            }
        }
    }

    LVGL_SAFE_EVENT_CB_END();
}

void AmsDeviceOperationsOverlay::on_toggle_changed(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[AmsDeviceOperationsOverlay] on_toggle_changed");

    auto* sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!sw || !lv_obj_is_valid(sw)) {
        spdlog::warn("[AmsDeviceOperationsOverlay] on_toggle_changed: invalid target");
    } else {
        // Get action ID from vector using index stored in user_data
        auto& overlay = get_ams_device_operations_overlay();
        auto index = reinterpret_cast<size_t>(lv_obj_get_user_data(sw));
        if (index >= overlay.action_ids_.size()) {
            spdlog::warn("[AmsDeviceOperationsOverlay] Invalid toggle action index: {}", index);
        } else {
            const std::string& action_id = overlay.action_ids_[index];
            bool new_value = lv_obj_has_state(sw, LV_STATE_CHECKED);
            spdlog::info("[AmsDeviceOperationsOverlay] Toggle changed: {} = {}", action_id,
                         new_value);

            // Execute via backend with the new value
            AmsBackend* backend = AmsState::instance().get_backend();
            if (!backend) {
                spdlog::warn("[AmsDeviceOperationsOverlay] No backend available for toggle");
            } else {
                // For toggles, pass the new value as a parameter
                AmsError result = backend->execute_device_action(action_id, std::any(new_value));
                if (result.success()) {
                    spdlog::info("[AmsDeviceOperationsOverlay] Toggle '{}' set to {}", action_id,
                                 new_value);
                } else {
                    spdlog::error("[AmsDeviceOperationsOverlay] Toggle '{}' failed: {}", action_id,
                                  result.technical_msg);
                    // Revert the toggle state on failure
                    if (new_value) {
                        lv_obj_remove_state(sw, LV_STATE_CHECKED);
                    } else {
                        lv_obj_add_state(sw, LV_STATE_CHECKED);
                    }
                }
            }
        }
    }

    LVGL_SAFE_EVENT_CB_END();
}

} // namespace helix::ui
