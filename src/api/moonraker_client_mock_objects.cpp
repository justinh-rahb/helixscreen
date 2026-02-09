// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "moonraker_client_mock_internal.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace mock_internal {

void register_object_handlers(std::unordered_map<std::string, MethodHandler>& registry) {
    // printer.objects.query - Query printer object state
    registry["printer.objects.query"] =
        [](MoonrakerClientMock* self, const json& params, std::function<void(json)> success_cb,
           std::function<void(const MoonrakerError&)> error_cb) -> bool {
        (void)error_cb;
        json status_obj = json::object();

        // Check what objects are being queried
        if (params.contains("objects")) {
            auto& objects = params["objects"];

            // webhooks state (for is_printer_ready)
            if (objects.contains("webhooks")) {
                auto klippy = self->get_klippy_state();
                std::string state_str = "ready";
                switch (klippy) {
                case MoonrakerClientMock::KlippyState::STARTUP:
                    state_str = "startup";
                    break;
                case MoonrakerClientMock::KlippyState::SHUTDOWN:
                    state_str = "shutdown";
                    break;
                case MoonrakerClientMock::KlippyState::ERROR:
                    state_str = "error";
                    break;
                default:
                    break;
                }
                status_obj["webhooks"] = {{"state", state_str}};
            }

            // print_stats (for get_print_state)
            if (objects.contains("print_stats")) {
                // Use get_print_phase to derive print state string
                auto phase = self->get_print_phase();
                std::string state_str = "standby";
                switch (phase) {
                case MoonrakerClientMock::MockPrintPhase::IDLE:
                    state_str = "standby";
                    break;
                case MoonrakerClientMock::MockPrintPhase::PREHEAT:
                case MoonrakerClientMock::MockPrintPhase::PRINTING:
                    state_str = "printing";
                    break;
                case MoonrakerClientMock::MockPrintPhase::PAUSED:
                    state_str = "paused";
                    break;
                case MoonrakerClientMock::MockPrintPhase::COMPLETE:
                    state_str = "complete";
                    break;
                case MoonrakerClientMock::MockPrintPhase::CANCELLED:
                    state_str = "cancelled";
                    break;
                case MoonrakerClientMock::MockPrintPhase::ERROR:
                    state_str = "error";
                    break;
                }
                status_obj["print_stats"] = {{"state", state_str}};
            }

            // configfile.settings (for update_safety_limits_from_printer)
            if (objects.contains("configfile")) {
                status_obj["configfile"] = {
                    {"settings",
                     {{"printer", {{"max_velocity", 500.0}, {"max_accel", 10000.0}}},
                      {"stepper_x",
                       {{"position_min", MOCK_BED_X_MIN}, {"position_max", MOCK_BED_X_MAX}}},
                      {"stepper_y",
                       {{"position_min", MOCK_BED_Y_MIN}, {"position_max", MOCK_BED_Y_MAX}}},
                      {"stepper_z",
                       {{"position_min", 0.0},
                        {"position_max", MOCK_BED_Z_MAX},
                        {"position_endstop", 235.0}}},
                      {"extruder",
                       {{"min_temp", 0.0}, {"max_temp", 300.0}, {"min_extrude_temp", 170.0}}},
                      {"heater_bed", {{"min_temp", 0.0}, {"max_temp", 120.0}}}}}};
            }

            // toolhead (for get_machine_limits)
            if (objects.contains("toolhead")) {
                status_obj["toolhead"] = {{"max_velocity", 500.0},
                                          {"max_accel", 10000.0},
                                          {"max_accel_to_decel", 5000.0},
                                          {"square_corner_velocity", 5.0},
                                          {"max_z_velocity", 40.0},
                                          {"max_z_accel", 1000.0},
                                          {"position", {0.0, 0.0, 0.0, 0.0}},
                                          {"homed_axes", "xyz"}};
            }

            // stepper_enable (for motors_enabled state - immediate response to M84)
            if (objects.contains("stepper_enable")) {
                bool enabled = self->are_motors_enabled();
                status_obj["stepper_enable"] = {{"steppers",
                                                 {{"stepper_x", enabled},
                                                  {"stepper_y", enabled},
                                                  {"stepper_z", enabled},
                                                  {"extruder", enabled}}}};
            }

            // idle_timeout (for printer activity state)
            if (objects.contains("idle_timeout")) {
                // Derive state from print phase and idle_timeout_triggered:
                // - "Printing" when active print in progress
                // - "Idle" when idle timeout has triggered
                // - "Ready" when active but not printing
                std::string idle_state;
                auto phase = self->get_print_phase();
                if (phase == MoonrakerClientMock::MockPrintPhase::PRINTING ||
                    phase == MoonrakerClientMock::MockPrintPhase::PREHEAT) {
                    idle_state = "Printing";
                } else if (self->is_idle_timeout_triggered()) {
                    idle_state = "Idle";
                } else {
                    idle_state = "Ready";
                }
                status_obj["idle_timeout"] = {{"state", idle_state}};
            }

            // MCU objects (for discovery - chip type and firmware version)
            for (const auto& [key, val] : objects.items()) {
                if (key == "mcu" || key.rfind("mcu ", 0) == 0) {
                    json mcu_obj = json::object();
                    // Provide mock mcu_constants and mcu_version
                    if (key == "mcu") {
                        mcu_obj["mcu_constants"] = {{"MCU", "stm32f446xx"}};
                        mcu_obj["mcu_version"] = "v0.12.0-155-g4cfa273e";
                    } else {
                        // Secondary MCU (e.g., "mcu EBBCan")
                        mcu_obj["mcu_constants"] = {{"MCU", "stm32g0b1xx"}};
                        mcu_obj["mcu_version"] = "v0.12.0-155-g4cfa273e";
                    }
                    status_obj[key] = mcu_obj;
                }
            }

            // input_shaper (for get_input_shaper_config)
            if (objects.contains("input_shaper")) {
                // Return mock input shaper configuration
                // Check if input shaper is configured (for testing unconfigured state)
                if (self->is_input_shaper_configured()) {
                    // Simulates a configured input shaper with typical values
                    status_obj["input_shaper"] = {
                        {"shaper_type_x", "mzv"}, {"shaper_freq_x", 36.7},
                        {"shaper_type_y", "ei"},  {"shaper_freq_y", 47.6},
                        {"damping_ratio_x", 0.1}, {"damping_ratio_y", 0.1}};
                } else {
                    // Unconfigured state - empty types and zero frequencies
                    status_obj["input_shaper"] = {
                        {"shaper_type_x", ""},  {"shaper_freq_x", 0.0},   {"shaper_type_y", ""},
                        {"shaper_freq_y", 0.0}, {"damping_ratio_x", 0.1}, {"damping_ratio_y", 0.1}};
                }
            }
        }

        if (success_cb) {
            json response = {{"result", {{"status", status_obj}}}};
            success_cb(response);
        }
        return true;
    };

    // printer.objects.subscribe - Subscribe to printer object updates
    // Returns initial state with eventtime (subsequent updates come via notify_status_update)
    registry["printer.objects.subscribe"] =
        [](MoonrakerClientMock* self, const json& params, std::function<void(json)> success_cb,
           std::function<void(const MoonrakerError&)> error_cb) -> bool {
        (void)error_cb;

        // Get current time as eventtime (seconds since epoch with microsecond precision)
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        double eventtime =
            std::chrono::duration_cast<std::chrono::microseconds>(epoch).count() / 1000000.0;

        json status_obj = json::object();

        // Check what objects are being subscribed to
        if (params.contains("objects")) {
            auto& objects = params["objects"];

            // webhooks state
            if (objects.contains("webhooks")) {
                auto klippy = self->get_klippy_state();
                std::string state_str = "ready";
                switch (klippy) {
                case MoonrakerClientMock::KlippyState::STARTUP:
                    state_str = "startup";
                    break;
                case MoonrakerClientMock::KlippyState::SHUTDOWN:
                    state_str = "shutdown";
                    break;
                case MoonrakerClientMock::KlippyState::ERROR:
                    state_str = "error";
                    break;
                default:
                    break;
                }
                status_obj["webhooks"] = {{"state", state_str}, {"state_message", ""}};
            }

            // print_stats
            if (objects.contains("print_stats")) {
                auto phase = self->get_print_phase();
                std::string state_str = "standby";
                switch (phase) {
                case MoonrakerClientMock::MockPrintPhase::IDLE:
                    state_str = "standby";
                    break;
                case MoonrakerClientMock::MockPrintPhase::PREHEAT:
                case MoonrakerClientMock::MockPrintPhase::PRINTING:
                    state_str = "printing";
                    break;
                case MoonrakerClientMock::MockPrintPhase::PAUSED:
                    state_str = "paused";
                    break;
                case MoonrakerClientMock::MockPrintPhase::COMPLETE:
                    state_str = "complete";
                    break;
                case MoonrakerClientMock::MockPrintPhase::CANCELLED:
                    state_str = "cancelled";
                    break;
                case MoonrakerClientMock::MockPrintPhase::ERROR:
                    state_str = "error";
                    break;
                }
                status_obj["print_stats"] = {
                    {"state", state_str},
                    {"filename", ""},
                    {"total_duration", 0.0},
                    {"print_duration", 0.0},
                    {"filament_used", 0.0},
                    {"message", ""},
                    {"info", {{"total_layer", nullptr}, {"current_layer", nullptr}}}};
            }

            // heater_bed
            if (objects.contains("heater_bed")) {
                status_obj["heater_bed"] = {{"temperature", 25.0}, {"target", 0.0}, {"power", 0.0}};
            }

            // extruder
            if (objects.contains("extruder")) {
                status_obj["extruder"] = {{"temperature", 25.0},
                                          {"target", 0.0},
                                          {"power", 0.0},
                                          {"pressure_advance", 0.04}};
            }

            // toolhead
            if (objects.contains("toolhead")) {
                status_obj["toolhead"] = {{"max_velocity", 500.0},
                                          {"max_accel", 10000.0},
                                          {"max_accel_to_decel", 5000.0},
                                          {"square_corner_velocity", 5.0},
                                          {"position", {0.0, 0.0, 0.0, 0.0}},
                                          {"homed_axes", "xyz"},
                                          {"print_time", 0.0},
                                          {"estimated_print_time", 0.0},
                                          {"extruder", "extruder"}};
            }

            // virtual_sdcard
            if (objects.contains("virtual_sdcard")) {
                status_obj["virtual_sdcard"] = {{"file_path", ""},
                                                {"progress", 0.0},
                                                {"is_active", false},
                                                {"file_position", 0}};
            }

            // fan (part cooling fan)
            if (objects.contains("fan")) {
                status_obj["fan"] = {{"speed", 0.0}, {"rpm", nullptr}};
            }

            // gcode_move
            if (objects.contains("gcode_move")) {
                status_obj["gcode_move"] = {{"speed_factor", 1.0},
                                            {"extrude_factor", 1.0},
                                            {"absolute_coordinates", true},
                                            {"absolute_extrude", true},
                                            {"homing_origin", {0.0, 0.0, 0.0, 0.0}},
                                            {"position", {0.0, 0.0, 0.0, 0.0}},
                                            {"gcode_position", {0.0, 0.0, 0.0, 0.0}}};
            }

            // stepper_enable (for motor state)
            if (objects.contains("stepper_enable")) {
                bool enabled = self->are_motors_enabled();
                status_obj["stepper_enable"] = {{"steppers",
                                                 {{"stepper_x", enabled},
                                                  {"stepper_y", enabled},
                                                  {"stepper_z", enabled},
                                                  {"extruder", enabled}}}};
            }

            // idle_timeout (for activity state)
            if (objects.contains("idle_timeout")) {
                std::string idle_state;
                auto phase = self->get_print_phase();
                if (phase == MoonrakerClientMock::MockPrintPhase::PRINTING ||
                    phase == MoonrakerClientMock::MockPrintPhase::PREHEAT) {
                    idle_state = "Printing";
                } else if (self->is_idle_timeout_triggered()) {
                    idle_state = "Idle";
                } else {
                    idle_state = "Ready";
                }
                status_obj["idle_timeout"] = {{"state", idle_state}, {"printing_time", 0.0}};
            }

            // motion_report (for live position updates)
            if (objects.contains("motion_report")) {
                status_obj["motion_report"] = {{"live_position", {0.0, 0.0, 0.0, 0.0}},
                                               {"live_velocity", 0.0},
                                               {"live_extruder_velocity", 0.0}};
            }

            // display_status (for progress/message display)
            if (objects.contains("display_status")) {
                status_obj["display_status"] = {{"progress", 0.0}, {"message", nullptr}};
            }

            // configfile (printer configuration)
            if (objects.contains("configfile")) {
                status_obj["configfile"] = {
                    {"settings",
                     {{"printer", {{"max_velocity", 500.0}, {"max_accel", 10000.0}}},
                      {"stepper_x",
                       {{"position_min", MOCK_BED_X_MIN}, {"position_max", MOCK_BED_X_MAX}}},
                      {"stepper_y",
                       {{"position_min", MOCK_BED_Y_MIN}, {"position_max", MOCK_BED_Y_MAX}}},
                      {"stepper_z",
                       {{"position_min", 0.0},
                        {"position_max", MOCK_BED_Z_MAX},
                        {"position_endstop", 235.0}}},
                      {"extruder",
                       {{"min_temp", 0.0}, {"max_temp", 300.0}, {"min_extrude_temp", 170.0}}},
                      {"heater_bed", {{"min_temp", 0.0}, {"max_temp", 120.0}}}}},
                    // config section contains raw Klipper config keys (used for sensor discovery)
                    {"config",
                     {{"adxl345", json::object()}, {"resonance_tester", json::object()}}}};
            }
        }

        if (success_cb) {
            json response = {{"result", {{"eventtime", eventtime}, {"status", status_obj}}}};
            success_cb(response);
        }
        return true;
    };
}

} // namespace mock_internal
