// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_pid_calibrate_collector.cpp
 * @brief Unit tests for PIDCalibrateCollector and MoonrakerAPI::start_pid_calibrate()
 *
 * Tests the PIDCalibrateCollector pattern and API method:
 * - PID result parsing from gcode responses
 * - Error handling for unknown commands and Klipper errors
 * - Bed heater calibration
 *
 * Uses mock client to simulate G-code responses from Klipper.
 */

#include "../../include/moonraker_api.h"
#include "../../include/moonraker_client_mock.h"
#include "../../include/printer_state.h"
#include "../../lvgl/lvgl.h"
#include "../ui_test_utils.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Global LVGL Initialization (called once)
// ============================================================================

namespace {
struct LVGLInitializerPIDCal {
    LVGLInitializerPIDCal() {
        static bool initialized = false;
        if (!initialized) {
            lv_init_safe();
            lv_display_t* disp = lv_display_create(800, 480);
            alignas(64) static lv_color_t buf[800 * 10];
            lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
            initialized = true;
        }
    }
};

static LVGLInitializerPIDCal lvgl_init;
} // namespace

// ============================================================================
// Test Fixture
// ============================================================================

class PIDCalibrateTestFixture {
  public:
    PIDCalibrateTestFixture() : mock_client_(MoonrakerClientMock::PrinterType::VORON_24) {
        state_.init_subjects(false);
        api_ = std::make_unique<MoonrakerAPI>(mock_client_, state_);
    }
    ~PIDCalibrateTestFixture() {
        api_.reset();
    }

    MoonrakerClientMock mock_client_;
    PrinterState state_;
    std::unique_ptr<MoonrakerAPI> api_;

    std::atomic<bool> result_received_{false};
    std::atomic<bool> error_received_{false};
    float captured_kp_ = 0, captured_ki_ = 0, captured_kd_ = 0;
    std::string captured_error_;
};

// ============================================================================
// Tests
// ============================================================================

TEST_CASE_METHOD(PIDCalibrateTestFixture, "PID calibrate collector parses results",
                 "[pid_calibrate]") {
    api_->start_pid_calibrate(
        "extruder", 200,
        [this](float kp, float ki, float kd) {
            captured_kp_ = kp;
            captured_ki_ = ki;
            captured_kd_ = kd;
            result_received_.store(true);
        },
        [this](const MoonrakerError& err) {
            captured_error_ = err.message;
            error_received_.store(true);
        });

    // Simulate Klipper PID output
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mock_client_.dispatch_gcode_response(
        "PID parameters: pid_Kp=22.865 pid_Ki=1.292 pid_Kd=101.178");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(result_received_.load());
    REQUIRE_FALSE(error_received_.load());
    REQUIRE(captured_kp_ == Catch::Approx(22.865f).margin(0.001f));
    REQUIRE(captured_ki_ == Catch::Approx(1.292f).margin(0.001f));
    REQUIRE(captured_kd_ == Catch::Approx(101.178f).margin(0.001f));
}

TEST_CASE_METHOD(PIDCalibrateTestFixture, "PID calibrate collector handles errors",
                 "[pid_calibrate]") {
    api_->start_pid_calibrate(
        "extruder", 200, [this](float, float, float) { result_received_.store(true); },
        [this](const MoonrakerError& err) {
            captured_error_ = err.message;
            error_received_.store(true);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mock_client_.dispatch_gcode_response("!! Error: heater extruder not heating at expected rate");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(error_received_.load());
    REQUIRE_FALSE(result_received_.load());
    REQUIRE(captured_error_.find("Error") != std::string::npos);
}

TEST_CASE_METHOD(PIDCalibrateTestFixture, "PID calibrate handles unknown command",
                 "[pid_calibrate]") {
    api_->start_pid_calibrate(
        "extruder", 200, [this](float, float, float) { result_received_.store(true); },
        [this](const MoonrakerError& err) {
            captured_error_ = err.message;
            error_received_.store(true);
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mock_client_.dispatch_gcode_response("Unknown command: \"PID_CALIBRATE\"");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(error_received_.load());
    REQUIRE_FALSE(result_received_.load());
}

TEST_CASE_METHOD(PIDCalibrateTestFixture, "PID calibrate bed heater", "[pid_calibrate]") {
    api_->start_pid_calibrate(
        "heater_bed", 60,
        [this](float kp, float ki, float kd) {
            captured_kp_ = kp;
            captured_ki_ = ki;
            captured_kd_ = kd;
            result_received_.store(true);
        },
        [this](const MoonrakerError& err) { error_received_.store(true); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mock_client_.dispatch_gcode_response(
        "PID parameters: pid_Kp=73.517 pid_Ki=1.132 pid_Kd=1194.093");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    REQUIRE(result_received_.load());
    REQUIRE(captured_kp_ == Catch::Approx(73.517f).margin(0.001f));
}
