// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_moonraker_api_input_shaper.cpp
 * @brief Unit tests for MoonrakerAPI input shaper calibration methods
 *
 * Tests the InputShaperCollector pattern and API methods:
 * - start_resonance_test() - SHAPER_CALIBRATE command execution
 * - set_input_shaper() - SET_INPUT_SHAPER command execution
 * - Response parsing for calibration results
 * - Error handling for missing accelerometer
 *
 * Uses mock client to simulate G-code responses from Klipper.
 */

#include "../../include/calibration_types.h" // For InputShaperResult
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
struct LVGLInitializerInputShaper {
    LVGLInitializerInputShaper() {
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

static LVGLInitializerInputShaper lvgl_init;
} // namespace

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Test fixture for input shaper API testing with mock client
 */
class InputShaperTestFixture {
  public:
    InputShaperTestFixture() : mock_client_(MoonrakerClientMock::PrinterType::VORON_24) {
        state_.init_subjects(false); // Don't register XML bindings in tests
        api_ = std::make_unique<MoonrakerAPI>(mock_client_, state_);
        reset_callbacks();
    }

    ~InputShaperTestFixture() {
        api_.reset();
    }

    void reset_callbacks() {
        result_received_ = false;
        error_received_ = false;
        captured_result_ = InputShaperResult{};
        captured_error_.clear();
    }

    // Callback for successful calibration result
    void on_result(const InputShaperResult& result) {
        result_received_ = true;
        captured_result_ = result;
    }

    // Callback for errors
    void on_error(const MoonrakerError& err) {
        error_received_ = true;
        captured_error_ = err.message;
    }

  protected:
    MoonrakerClientMock mock_client_;
    PrinterState state_;
    std::unique_ptr<MoonrakerAPI> api_;

    std::atomic<bool> result_received_{false};
    std::atomic<bool> error_received_{false};
    InputShaperResult captured_result_;
    std::string captured_error_;
};

// ============================================================================
// start_resonance_test() Tests
// ============================================================================

TEST_CASE_METHOD(InputShaperTestFixture, "start_resonance_test accepts X axis",
                 "[calibration][input_shaper]") {
    std::atomic<bool> complete_called{false};
    InputShaperResult captured_result;

    api_->start_resonance_test(
        'X', [](int) {}, // progress callback
        [&](const InputShaperResult& result) {
            captured_result = result;
            complete_called = true;
        },
        [&](const MoonrakerError&) { FAIL("Error callback should not be called"); });

    // Wait for async callback (mock dispatches synchronously)
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
    REQUIRE(captured_result.axis == 'X');
    REQUIRE(captured_result.is_valid());
    REQUIRE(captured_result.shaper_type == "mzv");
    REQUIRE(captured_result.shaper_freq == Catch::Approx(36.7f).margin(0.1f));
}

TEST_CASE_METHOD(InputShaperTestFixture, "start_resonance_test accepts Y axis",
                 "[calibration][input_shaper]") {
    std::atomic<bool> complete_called{false};
    InputShaperResult captured_result;

    api_->start_resonance_test(
        'Y', [](int) {}, // progress callback
        [&](const InputShaperResult& result) {
            captured_result = result;
            complete_called = true;
        },
        [&](const MoonrakerError&) { FAIL("Error callback should not be called"); });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
    REQUIRE(captured_result.axis == 'Y');
    REQUIRE(captured_result.is_valid());
}

TEST_CASE_METHOD(InputShaperTestFixture, "start_resonance_test sends correct G-code command for X",
                 "[calibration][input_shaper]") {
    std::atomic<bool> complete_called{false};

    api_->start_resonance_test(
        'X', [](int) {},
        [&](const InputShaperResult& result) {
            complete_called = true;
            // Verify parsed values from mock response
            REQUIRE(result.shaper_type == "mzv");
            REQUIRE(result.shaper_freq == Catch::Approx(36.7f).margin(0.1f));
        },
        [&](const MoonrakerError&) { FAIL("Error callback should not be called"); });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
}

// ============================================================================
// set_input_shaper() Tests
// ============================================================================

TEST_CASE_METHOD(InputShaperTestFixture, "set_input_shaper sends command for X axis with mzv",
                 "[calibration][input_shaper]") {
    std::atomic<bool> success_called{false};

    api_->set_input_shaper(
        'X', "mzv", 36.7, [&]() { success_called = true; },
        [&](const MoonrakerError&) { FAIL("Error callback should not be called"); });

    // Wait for async callback
    for (int i = 0; i < 100 && !success_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(success_called);
}

TEST_CASE_METHOD(InputShaperTestFixture, "set_input_shaper sends command for Y axis",
                 "[calibration][input_shaper]") {
    std::atomic<bool> success_called{false};

    api_->set_input_shaper(
        'Y', "ei", 47.6, [&]() { success_called = true; },
        [&](const MoonrakerError&) { FAIL("Error callback should not be called"); });

    // Wait for async callback
    for (int i = 0; i < 100 && !success_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(success_called);
}

TEST_CASE_METHOD(InputShaperTestFixture, "set_input_shaper accepts all valid shaper types",
                 "[calibration][input_shaper]") {
    std::vector<std::string> shaper_types = {"zv", "mzv", "zvd", "ei", "2hump_ei", "3hump_ei"};

    for (const auto& type : shaper_types) {
        INFO("Testing shaper type: " << type);
        std::atomic<bool> success_called{false};

        api_->set_input_shaper(
            'X', type, 35.0, [&]() { success_called = true; },
            [&](const MoonrakerError& err) {
                FAIL("Error callback should not be called for type: " << type << " - "
                                                                      << err.message);
            });

        // Wait for async callback
        for (int i = 0; i < 100 && !success_called; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        REQUIRE(success_called);
    }
}

// ============================================================================
// InputShaperResult Parsing Tests
// ============================================================================

TEST_CASE("InputShaperResult default construction", "[slow][calibration]") {
    InputShaperResult result;

    // Default axis is 'X' per struct definition
    REQUIRE(result.axis == 'X');
    REQUIRE(result.shaper_type.empty());
    REQUIRE(result.shaper_freq == 0.0f);
    REQUIRE(result.max_accel == 0.0f);
    REQUIRE(result.smoothing == 0.0f);
    REQUIRE(result.vibrations == 0.0f);
    REQUIRE(result.freq_response.empty());
}

TEST_CASE("InputShaperResult is_valid check", "[slow][calibration]") {
    InputShaperResult result;

    // Empty result is not valid
    REQUIRE_FALSE(result.is_valid());

    // Set valid values
    result.shaper_type = "mzv";
    result.shaper_freq = 36.7f;

    REQUIRE(result.is_valid());
}

// ============================================================================
// Response Parsing Simulation Tests
// ============================================================================

TEST_CASE("InputShaperResult can store calibration data", "[slow][calibration]") {
    // Simulate building a result from parsed G-code responses
    InputShaperResult result;
    result.axis = 'X';
    result.shaper_type = "mzv";
    result.shaper_freq = 36.7f;
    result.max_accel = 5000.0f;
    result.smoothing = 0.140f;
    result.vibrations = 7.2f;

    // Add frequency response data points
    result.freq_response.push_back({10.0f, 0.1f});
    result.freq_response.push_back({20.0f, 0.3f});
    result.freq_response.push_back({36.7f, 1.0f}); // Peak at resonance
    result.freq_response.push_back({50.0f, 0.2f});

    // Verify the result
    REQUIRE(result.axis == 'X');
    REQUIRE(result.is_valid());
    REQUIRE(result.shaper_type == "mzv");
    REQUIRE(result.shaper_freq == Catch::Approx(36.7f));
    REQUIRE(result.max_accel == Catch::Approx(5000.0f));
    REQUIRE(result.vibrations == Catch::Approx(7.2f));
    REQUIRE(result.freq_response.size() == 4);
}

TEST_CASE("InputShaperResult can represent incomplete state", "[slow][calibration]") {
    InputShaperResult result;
    result.axis = 'Y';
    // Leave shaper_type empty to simulate error/incomplete

    REQUIRE_FALSE(result.is_valid());
    REQUIRE(result.shaper_type.empty());
}

// ============================================================================
// Shaper Type Validation Tests
// ============================================================================

TEST_CASE("Valid shaper type strings", "[slow][calibration][validation]") {
    // These are the official Klipper input shaper types
    std::vector<std::string> valid_types = {
        "zv",       // Zero Vibration
        "mzv",      // Modified Zero Vibration
        "zvd",      // ZV + Derivative
        "ei",       // Extra Insensitive
        "2hump_ei", // 2-hump EI
        "3hump_ei"  // 3-hump EI
    };

    // Verify these are recognized as valid types
    for (const auto& type : valid_types) {
        INFO("Checking valid shaper type: " << type);
        // Just verify the strings are what we expect from Klipper
        REQUIRE(type.length() > 0);
        REQUIRE(type.length() <= 10);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE_METHOD(InputShaperTestFixture, "API handles null callbacks gracefully",
                 "[calibration][edge_case][input_shaper]") {
    // Test that calling start_resonance_test with nullptr callbacks doesn't crash
    // Note: The InputShaperCollector handles null callbacks internally
    REQUIRE_NOTHROW(api_->start_resonance_test('X', nullptr, nullptr, nullptr));

    // Give it time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // set_input_shaper requires valid callbacks (by design), so we test with valid ones
    std::atomic<bool> success_called{false};
    REQUIRE_NOTHROW(
        api_->set_input_shaper('X', "mzv", 36.7, [&]() { success_called = true; }, nullptr));

    // Wait for async callback
    for (int i = 0; i < 100 && !success_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(success_called);
}

// ============================================================================
// Phase 1: New API Methods and Enhanced Results
// ============================================================================
//
// These tests are written BEFORE implementation (test-first methodology).
// They will FAIL to compile or link until the corresponding types and methods
// are implemented in:
//   - include/calibration_types.h (ShaperOption, InputShaperConfig)
//   - include/moonraker_api.h (measure_axes_noise, get_input_shaper_config)
//   - src/api/moonraker_api_advanced.cpp (NoiseCheckCollector, enhanced collector)
//   - src/api/moonraker_client_mock.cpp (mock implementations)
// ============================================================================

// ----------------------------------------------------------------------------
// New Types Tests
// ----------------------------------------------------------------------------

TEST_CASE("ShaperOption struct", "[input_shaper][types]") {
    SECTION("default construction") {
        ShaperOption option;

        // Default values should be zeroed/empty
        CHECK(option.type.empty());
        CHECK(option.frequency == 0.0f);
        CHECK(option.vibrations == 0.0f);
        CHECK(option.smoothing == 0.0f);
        CHECK(option.max_accel == 0.0f);
    }

    SECTION("can store fitted shaper data") {
        ShaperOption option;
        option.type = "mzv";
        option.frequency = 36.7f;
        option.vibrations = 7.2f;
        option.smoothing = 0.140f;
        option.max_accel = 5000.0f;

        REQUIRE(option.type == "mzv");
        REQUIRE(option.frequency == Catch::Approx(36.7f));
        REQUIRE(option.vibrations == Catch::Approx(7.2f));
        REQUIRE(option.smoothing == Catch::Approx(0.140f));
        REQUIRE(option.max_accel == Catch::Approx(5000.0f));
    }
}

TEST_CASE("InputShaperConfig struct", "[input_shaper][types]") {
    SECTION("default construction") {
        InputShaperConfig config;

        // Default should indicate unconfigured state
        CHECK(config.shaper_type_x.empty());
        CHECK(config.shaper_freq_x == 0.0f);
        CHECK(config.shaper_type_y.empty());
        CHECK(config.shaper_freq_y == 0.0f);
        CHECK(config.damping_ratio_x == 0.0f);
        CHECK(config.damping_ratio_y == 0.0f);
        CHECK_FALSE(config.is_configured);
    }

    SECTION("can store configured shaper settings") {
        InputShaperConfig config;
        config.shaper_type_x = "mzv";
        config.shaper_freq_x = 36.7f;
        config.shaper_type_y = "ei";
        config.shaper_freq_y = 47.6f;
        config.damping_ratio_x = 0.1f;
        config.damping_ratio_y = 0.1f;
        config.is_configured = true;

        REQUIRE(config.is_configured);
        REQUIRE(config.shaper_type_x == "mzv");
        REQUIRE(config.shaper_freq_x == Catch::Approx(36.7f));
        REQUIRE(config.shaper_type_y == "ei");
        REQUIRE(config.shaper_freq_y == Catch::Approx(47.6f));
    }
}

// ----------------------------------------------------------------------------
// Enhanced InputShaperResult Tests (all_shapers vector)
// ----------------------------------------------------------------------------

TEST_CASE("InputShaperResult has all_shapers vector", "[input_shaper][types]") {
    InputShaperResult result;

    // The all_shapers vector should exist and be empty by default
    REQUIRE(result.all_shapers.empty());

    // Should be able to add shaper options
    ShaperOption zv;
    zv.type = "zv";
    zv.frequency = 35.8f;
    zv.vibrations = 22.7f;
    zv.smoothing = 0.100f;

    ShaperOption mzv;
    mzv.type = "mzv";
    mzv.frequency = 36.7f;
    mzv.vibrations = 7.2f;
    mzv.smoothing = 0.140f;

    result.all_shapers.push_back(zv);
    result.all_shapers.push_back(mzv);

    REQUIRE(result.all_shapers.size() == 2);
    REQUIRE(result.all_shapers[0].type == "zv");
    REQUIRE(result.all_shapers[1].type == "mzv");
}

TEST_CASE_METHOD(InputShaperTestFixture, "start_resonance_test returns all shaper alternatives",
                 "[calibration][input_shaper]") {
    // The mock dispatches 3 shaper options: zv, mzv, ei
    // After enhancement, the result should contain ALL fitted shapers in all_shapers
    std::atomic<bool> complete_called{false};
    InputShaperResult captured_result;

    api_->start_resonance_test(
        'X', [](int) {}, // progress callback
        [&](const InputShaperResult& result) {
            captured_result = result;
            complete_called = true;
        },
        [&](const MoonrakerError& err) {
            FAIL("Error callback should not be called: " << err.message);
        });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
    REQUIRE(captured_result.is_valid());

    // After enhancement, all_shapers should contain all 3 fitted options from mock
    REQUIRE(captured_result.all_shapers.size() == 3);

    // Verify the mock data matches expected values
    // Mock outputs: zv@35.8Hz, mzv@36.7Hz, ei@47.6Hz

    // Find each shaper type and verify its values
    auto find_shaper = [&](const std::string& type) -> const ShaperOption* {
        for (const auto& s : captured_result.all_shapers) {
            if (s.type == type)
                return &s;
        }
        return nullptr;
    };

    const ShaperOption* zv = find_shaper("zv");
    REQUIRE(zv != nullptr);
    CHECK(zv->frequency == Catch::Approx(35.8f).margin(0.1f));
    CHECK(zv->vibrations == Catch::Approx(22.7f).margin(0.1f));
    CHECK(zv->smoothing == Catch::Approx(0.100f).margin(0.01f));

    const ShaperOption* mzv = find_shaper("mzv");
    REQUIRE(mzv != nullptr);
    CHECK(mzv->frequency == Catch::Approx(36.7f).margin(0.1f));
    CHECK(mzv->vibrations == Catch::Approx(7.2f).margin(0.1f));
    CHECK(mzv->smoothing == Catch::Approx(0.140f).margin(0.01f));

    const ShaperOption* ei = find_shaper("ei");
    REQUIRE(ei != nullptr);
    CHECK(ei->frequency == Catch::Approx(47.6f).margin(0.1f));
    CHECK(ei->vibrations == Catch::Approx(5.9f).margin(0.1f));
    CHECK(ei->smoothing == Catch::Approx(0.096f).margin(0.01f));
}

// ----------------------------------------------------------------------------
// measure_axes_noise() Tests
// ----------------------------------------------------------------------------

TEST_CASE_METHOD(InputShaperTestFixture, "measure_axes_noise returns noise level",
                 "[calibration][input_shaper]") {
    // measure_axes_noise() runs MEASURE_AXES_NOISE G-code
    // Klipper output format: "axes_noise = 0.012345"
    // The mock should return a realistic noise value (~0.012)

    std::atomic<bool> complete_called{false};
    float captured_noise = -1.0f;

    api_->measure_axes_noise(
        [&](float noise_level) {
            captured_noise = noise_level;
            complete_called = true;
        },
        [&](const MoonrakerError& err) {
            FAIL("Error callback should not be called: " << err.message);
        });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
    // Noise level should be a small positive value (typical range 0.001 - 1.0)
    // Values under 100 are generally considered good
    CHECK(captured_noise >= 0.0f);
    CHECK(captured_noise < 1000.0f); // Upper bound sanity check
}

TEST_CASE_METHOD(InputShaperTestFixture, "measure_axes_noise handles no accelerometer error",
                 "[calibration][input_shaper]") {
    // When no ADXL is configured, MEASURE_AXES_NOISE should fail
    // This test requires the mock to be configured to simulate missing accelerometer
    // For now, we test the error callback path exists

    // Configure mock to simulate no accelerometer (implementation detail)
    mock_client_.set_accelerometer_available(false);

    std::atomic<bool> error_called{false};
    std::string captured_error;

    api_->measure_axes_noise(
        [&](float) { FAIL("Success callback should not be called when accelerometer missing"); },
        [&](const MoonrakerError& err) {
            error_called = true;
            captured_error = err.message;
        });

    // Wait for async callback
    for (int i = 0; i < 100 && !error_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(error_called);
    // Error message should mention accelerometer/ADXL
    CHECK((captured_error.find("accelerometer") != std::string::npos ||
           captured_error.find("ADXL") != std::string::npos ||
           captured_error.find("adxl") != std::string::npos));
}

// ----------------------------------------------------------------------------
// get_input_shaper_config() Tests
// ----------------------------------------------------------------------------

TEST_CASE_METHOD(InputShaperTestFixture, "get_input_shaper_config returns current settings",
                 "[calibration][input_shaper]") {
    // get_input_shaper_config() queries the current input shaper configuration
    // from printer state (via Moonraker's printer.objects.query)

    std::atomic<bool> complete_called{false};
    InputShaperConfig captured_config;

    api_->get_input_shaper_config(
        [&](const InputShaperConfig& config) {
            captured_config = config;
            complete_called = true;
        },
        [&](const MoonrakerError& err) {
            FAIL("Error callback should not be called: " << err.message);
        });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);

    // The mock should return a configured shaper state
    // Typical mock values: mzv@36.7Hz for X, ei@47.6Hz for Y
    if (captured_config.is_configured) {
        CHECK_FALSE(captured_config.shaper_type_x.empty());
        CHECK(captured_config.shaper_freq_x > 0.0f);
        CHECK_FALSE(captured_config.shaper_type_y.empty());
        CHECK(captured_config.shaper_freq_y > 0.0f);
    }
}

TEST_CASE_METHOD(InputShaperTestFixture, "get_input_shaper_config handles unconfigured shaper",
                 "[calibration][input_shaper]") {
    // When no input shaper is configured, is_configured should be false
    // This requires mock to simulate unconfigured state

    // Configure mock to simulate unconfigured input shaper
    mock_client_.set_input_shaper_configured(false);

    std::atomic<bool> complete_called{false};
    InputShaperConfig captured_config;

    api_->get_input_shaper_config(
        [&](const InputShaperConfig& config) {
            captured_config = config;
            complete_called = true;
        },
        [&](const MoonrakerError& err) {
            FAIL("Error callback should not be called: " << err.message);
        });

    // Wait for async callback
    for (int i = 0; i < 100 && !complete_called; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(complete_called);
    REQUIRE_FALSE(captured_config.is_configured);
}
