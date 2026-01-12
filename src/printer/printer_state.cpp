// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file printer_state.cpp
 * @brief Reactive printer state model with LVGL subjects for all printer data
 *
 * @pattern Singleton with set_*() -> set_*_internal() for thread-safe updates
 * @threading Public setters called from WebSocket; internal setters run on main thread
 * @gotchas Static string buffers; init subjects before XML; temps in centidegrees
 *
 * @see moonraker_client.cpp, ui_update_queue.h
 */

#include "printer_state.h"

#include "ui_update_queue.h"

#include "async_helpers.h"
#include "capability_overrides.h"
#include "device_display_name.h"
#include "filament_sensor_manager.h"
#include "hardware_validator.h"
#include "lvgl.h"
#include "lvgl/src/display/lv_display_private.h" // For rendering_in_progress check
#include "lvgl_debug_invalidate.h"
#include "moonraker_client.h" // For ConnectionState enum
#include "runtime_config.h"
#include "unit_conversions.h"

#include <algorithm>
#include <cctype>
#include <cstring>

// ============================================================================
// PrintJobState Free Functions
// ============================================================================

PrintJobState parse_print_job_state(const char* state_str) {
    if (!state_str) {
        return PrintJobState::STANDBY;
    }

    if (std::strcmp(state_str, "standby") == 0) {
        return PrintJobState::STANDBY;
    } else if (std::strcmp(state_str, "printing") == 0) {
        return PrintJobState::PRINTING;
    } else if (std::strcmp(state_str, "paused") == 0) {
        return PrintJobState::PAUSED;
    } else if (std::strcmp(state_str, "complete") == 0) {
        return PrintJobState::COMPLETE;
    } else if (std::strcmp(state_str, "cancelled") == 0) {
        return PrintJobState::CANCELLED;
    } else if (std::strcmp(state_str, "error") == 0) {
        return PrintJobState::ERROR;
    }

    // Unknown state defaults to STANDBY
    spdlog::warn("[PrinterState] Unknown print state string: '{}', defaulting to STANDBY",
                 state_str);
    return PrintJobState::STANDBY;
}

const char* print_job_state_to_string(PrintJobState state) {
    switch (state) {
    case PrintJobState::STANDBY:
        return "Standby";
    case PrintJobState::PRINTING:
        return "Printing";
    case PrintJobState::PAUSED:
        return "Paused";
    case PrintJobState::COMPLETE:
        return "Complete";
    case PrintJobState::CANCELLED:
        return "Cancelled";
    case PrintJobState::ERROR:
        return "Error";
    default:
        return "Unknown";
    }
}

// ============================================================================
// PrinterState Implementation
// ============================================================================

PrinterState::PrinterState() {
    // Initialize string buffers
    // Note: homed_axes_buf_ is now in motion_state_ component
    // Note: print-related buffers are now in print_domain_ component
    std::memset(printer_connection_message_buf_, 0, sizeof(printer_connection_message_buf_));
    std::memset(klipper_version_buf_, 0, sizeof(klipper_version_buf_));
    std::memset(moonraker_version_buf_, 0, sizeof(moonraker_version_buf_));

    // Set default values
    std::strcpy(printer_connection_message_buf_, "Disconnected");
    std::strcpy(klipper_version_buf_, "—");
    std::strcpy(moonraker_version_buf_, "—");

    // Load user-configured capability overrides from helixconfig.json
    capability_overrides_.load_from_config();
}

PrinterState::~PrinterState() {}

void PrinterState::reset_for_testing() {
    if (!subjects_initialized_) {
        spdlog::debug(
            "[PrinterState] reset_for_testing: subjects not initialized, nothing to reset");
        return; // Nothing to reset
    }

    spdlog::info("[PrinterState] reset_for_testing: Deinitializing subjects to clear observers");

    // Reset temperature state component
    temperature_state_.reset_for_testing();

    // Reset motion state component
    motion_state_.reset_for_testing();

    // Reset LED state component
    led_state_component_.reset_for_testing();

    // Reset fan state component
    fan_state_.reset_for_testing();

    // Reset print state component
    print_domain_.reset_for_testing();

    // Use SubjectManager for automatic subject cleanup
    subjects_.deinit_all();

    // Reset printer type and capabilities to initial empty state
    printer_type_.clear();
    print_start_capabilities_ = PrintStartCapabilities{};

    subjects_initialized_ = false;
}

void PrinterState::init_subjects(bool register_xml) {
    // Detect LVGL reinitialization - display pointer changes when lv_init() called again
    // This happens in test suites where each test reinitializes LVGL but the PrinterState
    // singleton persists. Without this check, subjects would point to freed memory.
    lv_display_t* current_display = lv_display_get_default();

    if (subjects_initialized_) {
        if (current_display != cached_display_) {
            // LVGL was reinitialized - our subjects are now invalid
            spdlog::warn("[PrinterState] LVGL reinitialized (display changed), resetting subjects");
            reset_for_testing();
        } else {
            spdlog::debug("[PrinterState] Subjects already initialized, skipping");
            return;
        }
    }

    cached_display_ = current_display;

    spdlog::debug("[PrinterState] Initializing subjects (register_xml={})", register_xml);

    // Initialize temperature state component (extruder and bed temperatures)
    temperature_state_.init_subjects(register_xml);

    // Initialize motion state component (position, speed/flow, z-offset)
    motion_state_.init_subjects(register_xml);

    // Initialize LED state component (RGBW channels, brightness, on/off state)
    led_state_component_.init_subjects(register_xml);

    // Initialize fan state component (fan speed, multi-fan tracking)
    fan_state_.init_subjects(register_xml);

    // Initialize print state component (progress, state, timing, layers, print start)
    print_domain_.init_subjects(register_xml);

    // Note: Print subjects are now initialized by print_domain_.init_subjects() above

    // Note: Motion subjects (position_x_, position_y_, position_z_, homed_axes_,
    // speed_factor_, flow_factor_, gcode_z_offset_, pending_z_offset_delta_)
    // are now initialized by motion_state_.init_subjects() above

    // Note: Fan subjects (fan_speed_, fans_version_) are now initialized by
    // fan_state_.init_subjects() above

    // Printer connection state subjects (Moonraker WebSocket)
    lv_subject_init_int(&printer_connection_state_, 0); // 0 = disconnected
    lv_subject_init_string(&printer_connection_message_, printer_connection_message_buf_, nullptr,
                           sizeof(printer_connection_message_buf_), "Disconnected");

    // Network connectivity subject (WiFi/Ethernet)
    // TODO: Get actual network status from EthernetManager/WiFiManager
    lv_subject_init_int(&network_status_,
                        2); // 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED (mock mode default)

    // Klipper firmware state subject (default to READY)
    lv_subject_init_int(&klippy_state_, static_cast<int>(KlippyState::READY));

    // Combined nav button enabled subject (connected AND klippy ready)
    // Starts disabled (0) - will be updated when connection/klippy state changes
    lv_subject_init_int(&nav_buttons_enabled_, 0);

    // Note: LED subjects are initialized by led_state_component_.init_subjects() above

    // Excluded objects version subject (incremented when excluded_objects_ changes)
    lv_subject_init_int(&excluded_objects_version_, 0);

    // Printer capability subjects (all default to 0=not available)
    lv_subject_init_int(&printer_has_qgl_, 0);
    lv_subject_init_int(&printer_has_z_tilt_, 0);
    lv_subject_init_int(&printer_has_bed_mesh_, 0);
    lv_subject_init_int(&printer_has_nozzle_clean_, 0);
    lv_subject_init_int(&printer_has_probe_, 0);
    lv_subject_init_int(&printer_has_heater_bed_, 0);
    lv_subject_init_int(&printer_has_led_, 0);
    lv_subject_init_int(&printer_has_accelerometer_, 0);
    lv_subject_init_int(&printer_has_spoolman_, 0);
    lv_subject_init_int(&printer_has_speaker_, 0);
    lv_subject_init_int(&printer_has_timelapse_, 0);
    lv_subject_init_int(&printer_has_purge_line_, 0);
    lv_subject_init_int(&helix_plugin_installed_, -1); // -1=unknown, 0=not installed, 1=installed
    lv_subject_init_int(&phase_tracking_enabled_, -1); // -1=unknown, 0=disabled, 1=enabled
    lv_subject_init_int(&printer_has_firmware_retraction_, 0);
    lv_subject_init_int(&printer_bed_moves_, 0); // 0=gantry moves, 1=bed moves (cartesian)

    // Composite subjects for G-code modification option visibility
    // These are derived from helix_plugin_installed AND printer_has_* subjects
    lv_subject_init_int(&can_show_bed_mesh_, 0);
    lv_subject_init_int(&can_show_qgl_, 0);
    lv_subject_init_int(&can_show_z_tilt_, 0);
    lv_subject_init_int(&can_show_nozzle_clean_, 0);
    lv_subject_init_int(&can_show_purge_line_, 0);

    // Hardware validation subjects (for Hardware Health section in Settings)
    lv_subject_init_int(&hardware_has_issues_, 0);
    lv_subject_init_int(&hardware_issue_count_, 0);
    lv_subject_init_int(&hardware_max_severity_, 0);
    lv_subject_init_int(&hardware_validation_version_, 0);
    lv_subject_init_int(&hardware_critical_count_, 0);
    lv_subject_init_int(&hardware_warning_count_, 0);
    lv_subject_init_int(&hardware_info_count_, 0);
    lv_subject_init_int(&hardware_session_count_, 0);
    lv_subject_init_string(&hardware_status_title_, hardware_status_title_buf_, nullptr,
                           sizeof(hardware_status_title_buf_), "Healthy");
    lv_subject_init_string(&hardware_status_detail_, hardware_status_detail_buf_, nullptr,
                           sizeof(hardware_status_detail_buf_), "");
    lv_subject_init_string(&hardware_issues_label_, hardware_issues_label_buf_, nullptr,
                           sizeof(hardware_issues_label_buf_), "No Hardware Issues");

    // Firmware retraction settings (defaults: disabled)
    lv_subject_init_int(&retract_length_, 0);         // 0 = disabled
    lv_subject_init_int(&retract_speed_, 20);         // 20 mm/s default
    lv_subject_init_int(&unretract_extra_length_, 0); // 0mm extra
    lv_subject_init_int(&unretract_speed_, 10);       // 10 mm/s default

    // Manual probe subjects (for Z-offset calibration)
    lv_subject_init_int(&manual_probe_active_, 0);     // 0=inactive, 1=active
    lv_subject_init_int(&manual_probe_z_position_, 0); // Z position in microns

    // Motor enabled state (from idle_timeout.state - defaults to enabled/Ready)
    lv_subject_init_int(&motors_enabled_, 1); // 1=enabled (Ready/Printing), 0=disabled (Idle)

    // Version subjects (for About section)
    lv_subject_init_string(&klipper_version_, klipper_version_buf_, nullptr,
                           sizeof(klipper_version_buf_), "—");
    lv_subject_init_string(&moonraker_version_, moonraker_version_buf_, nullptr,
                           sizeof(moonraker_version_buf_), "—");

    // Register all subjects with SubjectManager for automatic cleanup
    // Note: Temperature subjects are managed by temperature_state_ component
    // Note: Print subjects are managed by print_domain_ component
    // Note: Motion subjects are registered by motion_state_ component
    // Note: Fan subjects are registered by fan_state_ component
    // Printer connection subjects
    subjects_.register_subject(&printer_connection_state_);
    subjects_.register_subject(&printer_connection_message_);
    subjects_.register_subject(&network_status_);
    subjects_.register_subject(&klippy_state_);
    subjects_.register_subject(&nav_buttons_enabled_);
    // Note: LED subjects are registered by led_state_component_.init_subjects()
    // Excluded objects
    subjects_.register_subject(&excluded_objects_version_);
    // Printer capability subjects
    subjects_.register_subject(&printer_has_qgl_);
    subjects_.register_subject(&printer_has_z_tilt_);
    subjects_.register_subject(&printer_has_bed_mesh_);
    subjects_.register_subject(&printer_has_nozzle_clean_);
    subjects_.register_subject(&printer_has_probe_);
    subjects_.register_subject(&printer_has_heater_bed_);
    subjects_.register_subject(&printer_has_led_);
    subjects_.register_subject(&printer_has_accelerometer_);
    subjects_.register_subject(&printer_has_spoolman_);
    subjects_.register_subject(&printer_has_speaker_);
    subjects_.register_subject(&printer_has_timelapse_);
    subjects_.register_subject(&printer_has_purge_line_);
    subjects_.register_subject(&helix_plugin_installed_);
    subjects_.register_subject(&phase_tracking_enabled_);
    subjects_.register_subject(&printer_has_firmware_retraction_);
    subjects_.register_subject(&printer_bed_moves_);
    // Composite subjects for G-code modification visibility
    subjects_.register_subject(&can_show_bed_mesh_);
    subjects_.register_subject(&can_show_qgl_);
    subjects_.register_subject(&can_show_z_tilt_);
    subjects_.register_subject(&can_show_nozzle_clean_);
    subjects_.register_subject(&can_show_purge_line_);
    // Hardware validation subjects
    subjects_.register_subject(&hardware_has_issues_);
    subjects_.register_subject(&hardware_issue_count_);
    subjects_.register_subject(&hardware_max_severity_);
    subjects_.register_subject(&hardware_validation_version_);
    subjects_.register_subject(&hardware_critical_count_);
    subjects_.register_subject(&hardware_warning_count_);
    subjects_.register_subject(&hardware_info_count_);
    subjects_.register_subject(&hardware_session_count_);
    subjects_.register_subject(&hardware_status_title_);
    subjects_.register_subject(&hardware_status_detail_);
    subjects_.register_subject(&hardware_issues_label_);
    // Firmware retraction subjects
    subjects_.register_subject(&retract_length_);
    subjects_.register_subject(&retract_speed_);
    subjects_.register_subject(&unretract_extra_length_);
    subjects_.register_subject(&unretract_speed_);
    // Manual probe subjects
    subjects_.register_subject(&manual_probe_active_);
    subjects_.register_subject(&manual_probe_z_position_);
    // Motor enabled state
    subjects_.register_subject(&motors_enabled_);
    // Version subjects
    subjects_.register_subject(&klipper_version_);
    subjects_.register_subject(&moonraker_version_);

    spdlog::debug("[PrinterState] Registered {} subjects with SubjectManager", subjects_.count());

    // Register all subjects with LVGL XML system (CRITICAL for XML bindings)
    // Note: Temperature subjects are registered by temperature_state_ component
    // Note: Print subjects are registered by print_domain_ component
    // Note: Motion subjects are registered by motion_state_ component
    // Note: Fan subjects are registered by fan_state_ component
    if (register_xml) {
        spdlog::debug("[PrinterState] Registering subjects with XML system");
        lv_xml_register_subject(NULL, "printer_connection_state", &printer_connection_state_);
        lv_xml_register_subject(NULL, "printer_connection_message", &printer_connection_message_);
        lv_xml_register_subject(NULL, "network_status", &network_status_);
        lv_xml_register_subject(NULL, "klippy_state", &klippy_state_);
        lv_xml_register_subject(NULL, "nav_buttons_enabled", &nav_buttons_enabled_);
        // Note: LED subjects are registered by led_state_component_.init_subjects()
        lv_xml_register_subject(NULL, "excluded_objects_version", &excluded_objects_version_);
        lv_xml_register_subject(NULL, "printer_has_qgl", &printer_has_qgl_);
        lv_xml_register_subject(NULL, "printer_has_z_tilt", &printer_has_z_tilt_);
        lv_xml_register_subject(NULL, "printer_has_bed_mesh", &printer_has_bed_mesh_);
        lv_xml_register_subject(NULL, "printer_has_nozzle_clean", &printer_has_nozzle_clean_);
        lv_xml_register_subject(NULL, "printer_has_probe", &printer_has_probe_);
        lv_xml_register_subject(NULL, "printer_has_heater_bed", &printer_has_heater_bed_);
        lv_xml_register_subject(NULL, "printer_has_led", &printer_has_led_);
        lv_xml_register_subject(NULL, "printer_has_accelerometer", &printer_has_accelerometer_);
        lv_xml_register_subject(NULL, "printer_has_spoolman", &printer_has_spoolman_);
        lv_xml_register_subject(NULL, "printer_has_speaker", &printer_has_speaker_);
        lv_xml_register_subject(NULL, "printer_has_timelapse", &printer_has_timelapse_);
        lv_xml_register_subject(NULL, "helix_plugin_installed", &helix_plugin_installed_);
        lv_xml_register_subject(NULL, "phase_tracking_enabled", &phase_tracking_enabled_);
        lv_xml_register_subject(NULL, "printer_has_firmware_retraction",
                                &printer_has_firmware_retraction_);
        lv_xml_register_subject(NULL, "printer_bed_moves", &printer_bed_moves_);
        lv_xml_register_subject(NULL, "can_show_bed_mesh", &can_show_bed_mesh_);
        lv_xml_register_subject(NULL, "can_show_qgl", &can_show_qgl_);
        lv_xml_register_subject(NULL, "can_show_z_tilt", &can_show_z_tilt_);
        lv_xml_register_subject(NULL, "can_show_nozzle_clean", &can_show_nozzle_clean_);
        lv_xml_register_subject(NULL, "can_show_purge_line", &can_show_purge_line_);
        lv_xml_register_subject(NULL, "hardware_has_issues", &hardware_has_issues_);
        lv_xml_register_subject(NULL, "hardware_issue_count", &hardware_issue_count_);
        lv_xml_register_subject(NULL, "hardware_max_severity", &hardware_max_severity_);
        lv_xml_register_subject(NULL, "hardware_validation_version", &hardware_validation_version_);
        lv_xml_register_subject(NULL, "hardware_critical_count", &hardware_critical_count_);
        lv_xml_register_subject(NULL, "hardware_warning_count", &hardware_warning_count_);
        lv_xml_register_subject(NULL, "hardware_info_count", &hardware_info_count_);
        lv_xml_register_subject(NULL, "hardware_session_count", &hardware_session_count_);
        lv_xml_register_subject(NULL, "hardware_status_title", &hardware_status_title_);
        lv_xml_register_subject(NULL, "hardware_status_detail", &hardware_status_detail_);
        lv_xml_register_subject(NULL, "hardware_issues_label", &hardware_issues_label_);
        lv_xml_register_subject(NULL, "retract_length", &retract_length_);
        lv_xml_register_subject(NULL, "retract_speed", &retract_speed_);
        lv_xml_register_subject(NULL, "unretract_extra_length", &unretract_extra_length_);
        lv_xml_register_subject(NULL, "unretract_speed", &unretract_speed_);
        lv_xml_register_subject(NULL, "manual_probe_active", &manual_probe_active_);
        lv_xml_register_subject(NULL, "manual_probe_z_position", &manual_probe_z_position_);
        lv_xml_register_subject(NULL, "motors_enabled", &motors_enabled_);
        lv_xml_register_subject(NULL, "klipper_version", &klipper_version_);
        lv_xml_register_subject(NULL, "moonraker_version", &moonraker_version_);
    } else {
        spdlog::debug("[PrinterState] Skipping XML registration (tests mode)");
    }

    subjects_initialized_ = true;
    spdlog::debug("[PrinterState] Subjects initialized and registered successfully");
}

void PrinterState::update_from_notification(const json& notification) {
    // Moonraker notifications have structure:
    // {"method": "notify_status_update", "params": [{...printer state...}, eventtime]}

    if (!notification.contains("method") || !notification.contains("params")) {
        return;
    }

    std::string method = notification["method"].get<std::string>();
    if (method != "notify_status_update") {
        return;
    }

    // Extract printer state from params[0] and delegate to update_from_status
    // CRITICAL: Defer to main thread via helix::async::invoke to avoid LVGL assertion
    // when subject updates trigger lv_obj_invalidate() during rendering
    auto params = notification["params"];
    if (params.is_array() && !params.empty()) {
        helix::async::invoke([this, state_json = params[0]]() {
            // Debug check: log if we're somehow in render phase (should never happen)
            if (lvgl_is_rendering()) {
                spdlog::error("[PrinterState] async status update running during render phase!");
                spdlog::error("[PrinterState] This should not happen - lv_async_call should run "
                              "between frames");
            }
            update_from_status(state_json);
        });
    }
}

void PrinterState::update_from_status(const json& state) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // Debug: Check if we're in render phase (this should never be true)
    LV_DEBUG_RENDER_STATE();

    // Delegate temperature updates to temperature state component
    temperature_state_.update_from_status(state);

    // Delegate motion updates to motion state component
    motion_state_.update_from_status(state);

    // Delegate print updates to print state component
    print_domain_.update_from_status(state);

    // Note: Toolhead position, homed_axes, speed_factor, flow_factor, and gcode_z_offset
    // are now updated by motion_state_.update_from_status() above

    // Extract kinematics type (determines if bed moves on Z or gantry moves)
    // This is not part of motion_state_ as it affects printer_bed_moves_ subject
    if (state.contains("toolhead")) {
        const auto& toolhead = state["toolhead"];
        if (toolhead.contains("kinematics") && toolhead["kinematics"].is_string()) {
            std::string kin = toolhead["kinematics"].get<std::string>();
            set_kinematics(kin);
        }
    }

    // Delegate fan state updates to fan component
    fan_state_.update_from_status(state);

    // Delegate LED state updates to LED component
    led_state_component_.update_from_status(state);

    // Update exclude_object state (for mid-print object exclusion)
    if (state.contains("exclude_object")) {
        const auto& eo = state["exclude_object"];

        if (eo.contains("excluded_objects") && eo["excluded_objects"].is_array()) {
            std::unordered_set<std::string> excluded;
            for (const auto& obj : eo["excluded_objects"]) {
                if (obj.is_string()) {
                    excluded.insert(obj.get<std::string>());
                }
            }
            // set_excluded_objects handles change detection and notification
            // Note: We're inside state_mutex_ lock, but set_excluded_objects only modifies
            // its own data and calls lv_subject_set_int which is safe
            set_excluded_objects(excluded);
        }
    }

    // Update klippy state from webhooks (for restart simulation)
    if (state.contains("webhooks")) {
        const auto& webhooks = state["webhooks"];
        if (webhooks.contains("state") && webhooks["state"].is_string()) {
            std::string klippy_state_str = webhooks["state"].get<std::string>();
            KlippyState new_state = KlippyState::READY; // default

            if (klippy_state_str == "ready") {
                new_state = KlippyState::READY;
            } else if (klippy_state_str == "startup") {
                new_state = KlippyState::STARTUP;
            } else if (klippy_state_str == "shutdown") {
                new_state = KlippyState::SHUTDOWN;
            } else if (klippy_state_str == "error") {
                new_state = KlippyState::ERROR;
            }

            lv_subject_set_int(&klippy_state_, static_cast<int>(new_state));
            spdlog::debug("[PrinterState] Klippy state from webhooks: {}", klippy_state_str);
        }
    }

    // Update manual probe state (for Z-offset calibration)
    // Klipper's manual_probe object is active during PROBE_CALIBRATE and Z_ENDSTOP_CALIBRATE
    if (state.contains("manual_probe")) {
        const auto& mp = state["manual_probe"];

        if (mp.contains("is_active") && mp["is_active"].is_boolean()) {
            bool is_active = mp["is_active"].get<bool>();
            int old_active = lv_subject_get_int(&manual_probe_active_);
            int new_active = is_active ? 1 : 0;

            if (old_active != new_active) {
                lv_subject_set_int(&manual_probe_active_, new_active);
                spdlog::info("[PrinterState] Manual probe active: {} -> {}", old_active != 0,
                             is_active);
            }
        }

        if (mp.contains("z_position") && mp["z_position"].is_number()) {
            // Store as microns (multiply by 1000) for integer subject with 0.001mm resolution
            double z_mm = mp["z_position"].get<double>();
            int z_microns = static_cast<int>(z_mm * 1000.0);
            lv_subject_set_int(&manual_probe_z_position_, z_microns);
            spdlog::trace("[PrinterState] Manual probe Z: {:.3f}mm", z_mm);
        }
    }

    // Update motor enabled state from idle_timeout
    // idle_timeout.state: "Ready" or "Printing" = motors enabled, "Idle" = motors disabled
    if (state.contains("idle_timeout")) {
        const auto& it = state["idle_timeout"];

        if (it.contains("state") && it["state"].is_string()) {
            std::string timeout_state = it["state"].get<std::string>();
            // Motors are enabled when state is "Ready" or "Printing", disabled when "Idle"
            int new_enabled = (timeout_state == "Ready" || timeout_state == "Printing") ? 1 : 0;
            int old_enabled = lv_subject_get_int(&motors_enabled_);

            if (old_enabled != new_enabled) {
                lv_subject_set_int(&motors_enabled_, new_enabled);
                spdlog::info("[PrinterState] Motors {}: idle_timeout.state='{}'",
                             new_enabled ? "enabled" : "disabled", timeout_state);
            }
        }
    }

    // Parse firmware_retraction settings (G10/G11 retraction parameters)
    if (state.contains("firmware_retraction")) {
        const auto& fr = state["firmware_retraction"];

        if (fr.contains("retract_length") && fr["retract_length"].is_number()) {
            // Store as centimillimeters (x100) to preserve 0.01mm precision
            int centimm = helix::units::json_to_centimm(fr, "retract_length");
            lv_subject_set_int(&retract_length_, centimm);
            spdlog::trace("[PrinterState] Retract length: {:.2f}mm",
                          helix::units::from_centimm(centimm));
        }

        if (fr.contains("retract_speed") && fr["retract_speed"].is_number()) {
            int speed = static_cast<int>(fr["retract_speed"].get<double>());
            lv_subject_set_int(&retract_speed_, speed);
            spdlog::trace("[PrinterState] Retract speed: {}mm/s", speed);
        }

        if (fr.contains("unretract_extra_length") && fr["unretract_extra_length"].is_number()) {
            int centimm = helix::units::json_to_centimm(fr, "unretract_extra_length");
            lv_subject_set_int(&unretract_extra_length_, centimm);
            spdlog::trace("[PrinterState] Unretract extra: {:.2f}mm",
                          helix::units::from_centimm(centimm));
        }

        if (fr.contains("unretract_speed") && fr["unretract_speed"].is_number()) {
            int speed = static_cast<int>(fr["unretract_speed"].get<double>());
            lv_subject_set_int(&unretract_speed_, speed);
            spdlog::trace("[PrinterState] Unretract speed: {}mm/s", speed);
        }
    }

    // Forward filament sensor updates to FilamentSensorManager
    // The manager handles all sensor types: filament_switch_sensor and filament_motion_sensor
    helix::FilamentSensorManager::instance().update_from_status(state);

    // Cache full state for complex queries
    json_state_.merge_patch(state);
}

json& PrinterState::get_json_state() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return json_state_;
}

void PrinterState::reset_for_new_print() {
    print_domain_.reset_for_new_print();
}

// Note: Multi-fan tracking (init_fans, update_fan_speed, get_fan_speed_subject) is now
// delegated to fan_state_ component. See printer_fan_state.cpp.

void PrinterState::set_printer_connection_state(int state, const char* message) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    std::string msg = message ? message : "";
    helix::async::invoke(
        [this, state, msg]() { set_printer_connection_state_internal(state, msg.c_str()); });
}

void PrinterState::set_printer_connection_state_internal(int state, const char* message) {
    // Called from main thread via ui_async_call
    spdlog::info("[PrinterState] Printer connection state changed: {} - {}", state, message);

    // Track if we've ever successfully connected
    if (state == static_cast<int>(ConnectionState::CONNECTED) && !was_ever_connected_) {
        was_ever_connected_ = true;
        spdlog::debug("[PrinterState] First successful connection - was_ever_connected_ = true");
    }

    spdlog::trace("[PrinterState] Setting printer_connection_state_ subject (at {}) to value {}",
                  (void*)&printer_connection_state_, state);
    lv_subject_set_int(&printer_connection_state_, state);
    spdlog::trace("[PrinterState] Subject value now: {}",
                  lv_subject_get_int(&printer_connection_state_));
    lv_subject_copy_string(&printer_connection_message_, message);
    update_nav_buttons_enabled();
    spdlog::trace(
        "[PrinterState] Printer connection state update complete, observers should be notified");
}

void PrinterState::set_network_status(int status) {
    spdlog::debug("[PrinterState] Network status changed: {}", status);
    lv_subject_set_int(&network_status_, status);
}

void PrinterState::set_klippy_state(KlippyState state) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    helix::async::call_method(this, &PrinterState::set_klippy_state_internal, state);
}

void PrinterState::set_klippy_state_sync(KlippyState state) {
    // Direct call for main-thread use (testing, or when already on main thread)
    set_klippy_state_internal(state);
}

void PrinterState::set_klippy_state_internal(KlippyState state) {
    const char* state_names[] = {"READY", "STARTUP", "SHUTDOWN", "ERROR"};
    int state_int = static_cast<int>(state);
    spdlog::info("[PrinterState] Klippy state changed: {} ({})", state_names[state_int], state_int);
    lv_subject_set_int(&klippy_state_, state_int);
    update_nav_buttons_enabled();
}

void PrinterState::update_nav_buttons_enabled() {
    // Compute combined state: enabled when connected AND klippy ready
    int connection = lv_subject_get_int(&printer_connection_state_);
    int klippy = lv_subject_get_int(&klippy_state_);
    bool connected = (connection == static_cast<int>(ConnectionState::CONNECTED));
    bool klippy_ready = (klippy == static_cast<int>(KlippyState::READY));
    int enabled = (connected && klippy_ready) ? 1 : 0;

    // Only update if changed to avoid unnecessary observer notifications
    if (lv_subject_get_int(&nav_buttons_enabled_) != enabled) {
        spdlog::debug("[PrinterState] nav_buttons_enabled: {} (connected={}, klippy_ready={})",
                      enabled, connected, klippy_ready);
        lv_subject_set_int(&nav_buttons_enabled_, enabled);
    }
}

void PrinterState::set_print_in_progress(bool in_progress) {
    print_domain_.set_print_in_progress(in_progress);
}

// Note: set_tracked_led() is now delegated to led_state_component_ in the header

void PrinterState::set_hardware(const helix::PrinterHardwareDiscovery& hardware) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    helix::async::call_method_ref(this, &PrinterState::set_hardware_internal, hardware);
}

void PrinterState::set_hardware_internal(const helix::PrinterHardwareDiscovery& hardware) {
    // Pass auto-detected hardware to the override layer
    capability_overrides_.set_hardware(hardware);

    // Update subjects using effective values (auto-detect + user overrides)
    // This allows users to force-enable features that weren't detected
    // (e.g., heat soak macro without chamber heater) or force-disable
    // features they don't want to see in the UI.
    lv_subject_set_int(&printer_has_qgl_, capability_overrides_.has_qgl() ? 1 : 0);
    lv_subject_set_int(&printer_has_z_tilt_, capability_overrides_.has_z_tilt() ? 1 : 0);
    lv_subject_set_int(&printer_has_bed_mesh_, capability_overrides_.has_bed_mesh() ? 1 : 0);
    lv_subject_set_int(&printer_has_nozzle_clean_,
                       capability_overrides_.has_nozzle_clean() ? 1 : 0);

    // Hardware capabilities (no user override support yet - set directly from detection)
    lv_subject_set_int(&printer_has_probe_, hardware.has_probe() ? 1 : 0);
    lv_subject_set_int(&printer_has_heater_bed_, hardware.has_heater_bed() ? 1 : 0);
    lv_subject_set_int(&printer_has_led_, hardware.has_led() ? 1 : 0);
    lv_subject_set_int(&printer_has_accelerometer_, hardware.has_accelerometer() ? 1 : 0);

    // Speaker capability (for M300 audio feedback)
    lv_subject_set_int(&printer_has_speaker_, hardware.has_speaker() ? 1 : 0);

    // Timelapse capability (Moonraker-Timelapse plugin)
    lv_subject_set_int(&printer_has_timelapse_, hardware.has_timelapse() ? 1 : 0);

    // Firmware retraction capability (for G10/G11 retraction settings)
    lv_subject_set_int(&printer_has_firmware_retraction_,
                       hardware.has_firmware_retraction() ? 1 : 0);

    // Spoolman requires async check - default to 0, updated separately

    spdlog::info("[PrinterState] Hardware set: probe={}, heater_bed={}, LED={}, "
                 "accelerometer={}, speaker={}, timelapse={}, fw_retraction={}",
                 hardware.has_probe(), hardware.has_heater_bed(), hardware.has_led(),
                 hardware.has_accelerometer(), hardware.has_speaker(), hardware.has_timelapse(),
                 hardware.has_firmware_retraction());
    spdlog::info("[PrinterState] Hardware set (with overrides): {}",
                 capability_overrides_.summary());

    // Update composite subjects for G-code modification options
    // (visibility depends on both plugin status and capability)
    update_gcode_modification_visibility();
}

void PrinterState::set_klipper_version(const std::string& version) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    helix::async::call_method_ref(this, &PrinterState::set_klipper_version_internal, version);
}

void PrinterState::set_klipper_version_internal(const std::string& version) {
    lv_subject_copy_string(&klipper_version_, version.c_str());
    spdlog::debug("[PrinterState] Klipper version set: {}", version);
}

void PrinterState::set_moonraker_version(const std::string& version) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    helix::async::call_method_ref(this, &PrinterState::set_moonraker_version_internal, version);
}

void PrinterState::set_moonraker_version_internal(const std::string& version) {
    lv_subject_copy_string(&moonraker_version_, version.c_str());
    spdlog::debug("[PrinterState] Moonraker version set: {}", version);
}

void PrinterState::set_spoolman_available(bool available) {
    // Thread-safe: Use helix::async::invoke to update LVGL subject from any thread
    helix::async::invoke([this, available]() {
        lv_subject_set_int(&printer_has_spoolman_, available ? 1 : 0);
        spdlog::info("[PrinterState] Spoolman availability set: {}", available);
    });
}

void PrinterState::set_helix_plugin_installed(bool installed) {
    // Thread-safe: Use helix::async::invoke to update LVGL subject from any thread
    helix::async::invoke([this, installed]() {
        lv_subject_set_int(&helix_plugin_installed_, installed ? 1 : 0);
        spdlog::info("[PrinterState] HelixPrint plugin installed: {}", installed);

        // Update composite subjects for G-code modification options
        update_gcode_modification_visibility();
    });
}

bool PrinterState::service_has_helix_plugin() const {
    // Note: lv_subject_get_int is thread-safe (atomic read)
    // Tri-state: -1=unknown, 0=not installed, 1=installed
    return lv_subject_get_int(const_cast<lv_subject_t*>(&helix_plugin_installed_)) == 1;
}

void PrinterState::set_phase_tracking_enabled(bool enabled) {
    // Thread-safe: Use helix::async::invoke to update LVGL subject from any thread
    helix::async::invoke([this, enabled]() {
        lv_subject_set_int(&phase_tracking_enabled_, enabled ? 1 : 0);
        spdlog::info("[PrinterState] Phase tracking enabled: {}", enabled);
    });
}

bool PrinterState::is_phase_tracking_enabled() const {
    // Note: lv_subject_get_int is thread-safe (atomic read)
    // Tri-state: -1=unknown, 0=disabled, 1=enabled
    return lv_subject_get_int(const_cast<lv_subject_t*>(&phase_tracking_enabled_)) == 1;
}

void PrinterState::update_gcode_modification_visibility() {
    // Recalculate composite subjects: can_show_X = helix_plugin_installed && printer_has_X
    // These control visibility of pre-print G-code modification options in the UI
    // Tri-state: -1=unknown, 0=not installed, 1=installed (only 1 counts as "has plugin")
    bool plugin = lv_subject_get_int(&helix_plugin_installed_) == 1;

    auto update_if_changed = [](lv_subject_t* subject, int new_value) {
        if (lv_subject_get_int(subject) != new_value) {
            lv_subject_set_int(subject, new_value);
        }
    };

    update_if_changed(&can_show_bed_mesh_,
                      (plugin && lv_subject_get_int(&printer_has_bed_mesh_)) ? 1 : 0);
    update_if_changed(&can_show_qgl_, (plugin && lv_subject_get_int(&printer_has_qgl_)) ? 1 : 0);
    update_if_changed(&can_show_z_tilt_,
                      (plugin && lv_subject_get_int(&printer_has_z_tilt_)) ? 1 : 0);
    update_if_changed(&can_show_nozzle_clean_,
                      (plugin && lv_subject_get_int(&printer_has_nozzle_clean_)) ? 1 : 0);
    update_if_changed(&can_show_purge_line_,
                      (plugin && lv_subject_get_int(&printer_has_purge_line_)) ? 1 : 0);

    spdlog::debug("[PrinterState] G-code modification visibility updated: bed_mesh={}, qgl={}, "
                  "z_tilt={}, nozzle_clean={}, purge_line={} (plugin={})",
                  lv_subject_get_int(&can_show_bed_mesh_), lv_subject_get_int(&can_show_qgl_),
                  lv_subject_get_int(&can_show_z_tilt_),
                  lv_subject_get_int(&can_show_nozzle_clean_),
                  lv_subject_get_int(&can_show_purge_line_), plugin);
}

// Note: update_print_show_progress() is now in print_domain_ component

void PrinterState::set_excluded_objects(const std::unordered_set<std::string>& objects) {
    // Only update if the set actually changed
    if (excluded_objects_ != objects) {
        excluded_objects_ = objects;

        // Increment version to notify observers
        int version = lv_subject_get_int(&excluded_objects_version_);
        lv_subject_set_int(&excluded_objects_version_, version + 1);

        spdlog::debug("[PrinterState] Excluded objects updated: {} objects (version {})",
                      excluded_objects_.size(), version + 1);
    }
}

PrintJobState PrinterState::get_print_job_state() const {
    return print_domain_.get_print_job_state();
}

bool PrinterState::can_start_new_print() const {
    return print_domain_.can_start_new_print();
}

void PrinterState::set_kinematics(const std::string& kinematics) {
    // Determine if the bed moves on Z based on kinematics type:
    // - Cartesian: bed typically moves on Z axis (Ender 3, Prusa MK3, etc.)
    // - CoreXY/CoreXZ: gantry typically moves on Z axis (Voron 2.4, RatRig, etc.)
    // - Delta: effector moves on Z, bed is stationary
    //
    // Note: This is a heuristic. Some CoreXY printers (Voron Trident) have gantry-Z.
    // For perfect accuracy, we'd need to parse stepper_z configuration.
    bool bed_moves_z = (kinematics.find("cartesian") != std::string::npos);
    int new_value = bed_moves_z ? 1 : 0;

    // Only log when value actually changes (this gets called frequently from status updates)
    if (lv_subject_get_int(&printer_bed_moves_) != new_value) {
        lv_subject_set_int(&printer_bed_moves_, new_value);
        spdlog::info("[PrinterState] Kinematics: {} -> bed_moves_z={}", kinematics, bed_moves_z);
    }
}

// Note: Pending Z-offset delta methods are now delegated to motion_state_
// component in the header file.

// ============================================================================
// PRINT START PROGRESS TRACKING - Delegated to print_domain_
// ============================================================================

bool PrinterState::is_in_print_start() const {
    return print_domain_.is_in_print_start();
}

void PrinterState::set_print_start_state(PrintStartPhase phase, const char* message, int progress) {
    print_domain_.set_print_start_state(phase, message, progress);
}

void PrinterState::reset_print_start_state() {
    print_domain_.reset_print_start_state();
}

void PrinterState::set_print_thumbnail_path(const std::string& path) {
    print_domain_.set_print_thumbnail_path(path);
}

void PrinterState::set_print_display_filename(const std::string& name) {
    print_domain_.set_print_display_filename(name);
}

// ============================================================================
// HARDWARE VALIDATION
// ============================================================================

void PrinterState::set_hardware_validation_result(const HardwareValidationResult& result) {
    // Store the full result for UI access
    hardware_validation_result_ = result;

    // Update summary subjects
    lv_subject_set_int(&hardware_has_issues_, result.has_issues() ? 1 : 0);
    lv_subject_set_int(&hardware_issue_count_, static_cast<int>(result.total_issue_count()));
    lv_subject_set_int(&hardware_max_severity_, static_cast<int>(result.max_severity()));

    // Update category counts
    lv_subject_set_int(&hardware_critical_count_, static_cast<int>(result.critical_missing.size()));
    lv_subject_set_int(&hardware_warning_count_, static_cast<int>(result.expected_missing.size()));
    lv_subject_set_int(&hardware_info_count_, static_cast<int>(result.newly_discovered.size()));
    lv_subject_set_int(&hardware_session_count_,
                       static_cast<int>(result.changed_from_last_session.size()));

    // Update status text
    if (!result.has_issues()) {
        snprintf(hardware_status_title_buf_, sizeof(hardware_status_title_buf_), "All Healthy");
        snprintf(hardware_status_detail_buf_, sizeof(hardware_status_detail_buf_),
                 "All configured hardware detected");
    } else {
        size_t total = result.total_issue_count();
        snprintf(hardware_status_title_buf_, sizeof(hardware_status_title_buf_),
                 "%zu Issue%s Detected", total, total == 1 ? "" : "s");

        // Build detail string
        std::string detail;
        if (!result.critical_missing.empty()) {
            detail += std::to_string(result.critical_missing.size()) + " critical";
        }
        if (!result.expected_missing.empty()) {
            if (!detail.empty())
                detail += ", ";
            detail += std::to_string(result.expected_missing.size()) + " missing";
        }
        if (!result.newly_discovered.empty()) {
            if (!detail.empty())
                detail += ", ";
            detail += std::to_string(result.newly_discovered.size()) + " new";
        }
        if (!result.changed_from_last_session.empty()) {
            if (!detail.empty())
                detail += ", ";
            detail += std::to_string(result.changed_from_last_session.size()) + " changed";
        }
        snprintf(hardware_status_detail_buf_, sizeof(hardware_status_detail_buf_), "%s",
                 detail.c_str());
    }
    lv_subject_copy_string(&hardware_status_title_, hardware_status_title_buf_);
    lv_subject_copy_string(&hardware_status_detail_, hardware_status_detail_buf_);

    // Update issues label for settings panel ("1 Hardware Issue" / "5 Hardware Issues")
    size_t total = result.total_issue_count();
    if (total == 0) {
        snprintf(hardware_issues_label_buf_, sizeof(hardware_issues_label_buf_),
                 "No Hardware Issues");
    } else if (total == 1) {
        snprintf(hardware_issues_label_buf_, sizeof(hardware_issues_label_buf_),
                 "1 Hardware Issue");
    } else {
        snprintf(hardware_issues_label_buf_, sizeof(hardware_issues_label_buf_),
                 "%zu Hardware Issues", total);
    }
    lv_subject_copy_string(&hardware_issues_label_, hardware_issues_label_buf_);

    // Increment version to notify UI observers
    int version = lv_subject_get_int(&hardware_validation_version_);
    lv_subject_set_int(&hardware_validation_version_, version + 1);

    spdlog::debug("[PrinterState] Hardware validation updated: {} issues, max_severity={}",
                  result.total_issue_count(), static_cast<int>(result.max_severity()));
}

const HardwareValidationResult& PrinterState::get_hardware_validation_result() const {
    return hardware_validation_result_;
}

void PrinterState::remove_hardware_issue(const std::string& hardware_name) {
    // Helper lambda to remove an issue from a vector by hardware_name
    auto remove_by_name = [&hardware_name](std::vector<HardwareIssue>& issues) {
        issues.erase(std::remove_if(issues.begin(), issues.end(),
                                    [&hardware_name](const HardwareIssue& issue) {
                                        return issue.hardware_name == hardware_name;
                                    }),
                     issues.end());
    };

    // Remove from all issue lists
    remove_by_name(hardware_validation_result_.critical_missing);
    remove_by_name(hardware_validation_result_.expected_missing);
    remove_by_name(hardware_validation_result_.newly_discovered);
    remove_by_name(hardware_validation_result_.changed_from_last_session);

    // Re-apply the updated result to refresh all subjects
    set_hardware_validation_result(hardware_validation_result_);

    spdlog::debug("[PrinterState] Removed hardware issue: {}", hardware_name);
}

void PrinterState::set_print_outcome(PrintOutcome outcome) {
    print_domain_.set_print_outcome(outcome);
}

// ============================================================================
// PRINTER TYPE AND PRINT START CAPABILITIES
// ============================================================================

void PrinterState::set_printer_type(const std::string& type) {
    // Thread-safe wrapper: defer updates to main thread
    helix::async::call_method_ref(this, &PrinterState::set_printer_type_internal, type);
}

void PrinterState::set_printer_type_sync(const std::string& type) {
    // Direct call for main-thread use (testing, or when already on main thread)
    set_printer_type_internal(type);
}

void PrinterState::set_printer_type_internal(const std::string& type) {
    printer_type_ = type;
    print_start_capabilities_ = PrinterDetector::get_print_start_capabilities(type);

    // Update printer_has_purge_line_ based on capabilities database
    // "priming" is the capability key for purge/prime line in the database
    bool has_priming = print_start_capabilities_.get_capability("priming") != nullptr;
    lv_subject_set_int(&printer_has_purge_line_, has_priming ? 1 : 0);

    // Recalculate composite visibility subjects
    update_gcode_modification_visibility();

    spdlog::info("[PrinterState] Printer type set to: '{}' (capabilities: {}, priming={})", type,
                 print_start_capabilities_.empty() ? "none" : print_start_capabilities_.macro_name,
                 has_priming);
}

const std::string& PrinterState::get_printer_type() const {
    return printer_type_;
}

const PrintStartCapabilities& PrinterState::get_print_start_capabilities() const {
    return print_start_capabilities_;
}
