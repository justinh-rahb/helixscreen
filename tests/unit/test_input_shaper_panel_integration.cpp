// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_input_shaper_panel_integration.cpp
 * @brief Integration tests for InputShaperPanel delegation to InputShaperCalibrator
 *
 * Test-first development: These tests document the expected behavior after
 * refactoring InputShaperPanel to delegate to InputShaperCalibrator.
 *
 * These tests verify that InputShaperPanel correctly:
 * 1. Creates an InputShaperCalibrator instance when set_api() is called
 * 2. Delegates all calibration operations through the calibrator
 * 3. Updates UI state based on calibrator callbacks
 *
 * NOTE: These tests focus on the delegation contract, not full UI rendering.
 * Full UI tests require LVGL initialization which is handled separately.
 */

#include "../../include/calibration_types.h"
#include "../../include/input_shaper_calibrator.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../catch_amalgamated.hpp"

using namespace helix::calibration;

// ============================================================================
// Mock InputShaperCalibrator for tracking delegation calls
// ============================================================================

/**
 * @brief Mock calibrator that tracks method calls for verification
 *
 * Does not perform any actual calibration - just records what was called
 * and allows tests to trigger callbacks to verify panel response.
 */
class MockInputShaperCalibrator {
  public:
    // Use the same State enum as the real calibrator
    enum class State { IDLE, CHECKING_ADXL, TESTING_X, TESTING_Y, READY };

    // Use the same CalibrationResults struct as the real calibrator
    struct CalibrationResults {
        InputShaperResult x_result;
        InputShaperResult y_result;
        float noise_level = 0.0f;

        [[nodiscard]] bool has_x() const {
            return x_result.is_valid();
        }
        [[nodiscard]] bool has_y() const {
            return y_result.is_valid();
        }
        [[nodiscard]] bool is_complete() const {
            return has_x() && has_y();
        }
    };

    MockInputShaperCalibrator() = default;

    // ========== Call tracking ==========

    struct CalibrationCall {
        char axis;
        bool has_progress_cb;
        bool has_result_cb;
        bool has_error_cb;
    };

    struct ApplyCall {
        ApplyConfig config;
        bool has_success_cb;
        bool has_error_cb;
    };

    // Track check_accelerometer calls
    bool check_accelerometer_called = false;
    AccelCheckCallback last_accel_complete_cb;
    ErrorCallback last_accel_error_cb;

    // Track run_calibration calls
    std::vector<CalibrationCall> calibration_calls;
    ProgressCallback last_progress_cb;
    ResultCallback last_result_cb;
    ErrorCallback last_calibration_error_cb;

    // Track apply_settings calls
    std::vector<ApplyCall> apply_calls;
    SuccessCallback last_apply_success_cb;
    ErrorCallback last_apply_error_cb;

    // Track save_to_config calls
    bool save_to_config_called = false;
    SuccessCallback last_save_success_cb;
    ErrorCallback last_save_error_cb;

    // Track cancel calls
    int cancel_call_count = 0;

    // State
    State state_ = State::IDLE;
    CalibrationResults results_;

    // ========== Mock interface matching InputShaperCalibrator ==========

    [[nodiscard]] State get_state() const {
        return state_;
    }

    [[nodiscard]] const CalibrationResults& get_results() const {
        return results_;
    }

    void check_accelerometer(AccelCheckCallback on_complete, ErrorCallback on_error) {
        check_accelerometer_called = true;
        last_accel_complete_cb = std::move(on_complete);
        last_accel_error_cb = std::move(on_error);
        state_ = State::CHECKING_ADXL;
    }

    void run_calibration(char axis, ProgressCallback on_progress, ResultCallback on_complete,
                         ErrorCallback on_error) {
        CalibrationCall call;
        call.axis = axis;
        call.has_progress_cb = (on_progress != nullptr);
        call.has_result_cb = (on_complete != nullptr);
        call.has_error_cb = (on_error != nullptr);
        calibration_calls.push_back(call);

        last_progress_cb = std::move(on_progress);
        last_result_cb = std::move(on_complete);
        last_calibration_error_cb = std::move(on_error);

        state_ = (axis == 'X') ? State::TESTING_X : State::TESTING_Y;
    }

    void apply_settings(const ApplyConfig& config, SuccessCallback on_success,
                        ErrorCallback on_error) {
        ApplyCall call;
        call.config = config;
        call.has_success_cb = (on_success != nullptr);
        call.has_error_cb = (on_error != nullptr);
        apply_calls.push_back(call);

        last_apply_success_cb = std::move(on_success);
        last_apply_error_cb = std::move(on_error);
    }

    void save_to_config(SuccessCallback on_success, ErrorCallback on_error) {
        save_to_config_called = true;
        last_save_success_cb = std::move(on_success);
        last_save_error_cb = std::move(on_error);
    }

    void cancel() {
        cancel_call_count++;
        state_ = State::IDLE;
    }

    // ========== Test helpers for triggering callbacks ==========

    void trigger_accel_complete(float noise_level) {
        results_.noise_level = noise_level;
        state_ = State::IDLE;
        if (last_accel_complete_cb) {
            last_accel_complete_cb(noise_level);
        }
    }

    void trigger_accel_error(const std::string& message) {
        state_ = State::IDLE;
        if (last_accel_error_cb) {
            last_accel_error_cb(message);
        }
    }

    void trigger_calibration_progress(int percent) {
        if (last_progress_cb) {
            last_progress_cb(percent);
        }
    }

    void trigger_calibration_result(const InputShaperResult& result) {
        if (result.axis == 'X') {
            results_.x_result = result;
        } else {
            results_.y_result = result;
        }
        state_ = State::READY;
        if (last_result_cb) {
            last_result_cb(result);
        }
    }

    void trigger_calibration_error(const std::string& message) {
        state_ = State::IDLE;
        if (last_calibration_error_cb) {
            last_calibration_error_cb(message);
        }
    }

    void trigger_apply_success() {
        if (last_apply_success_cb) {
            last_apply_success_cb();
        }
    }

    void trigger_apply_error(const std::string& message) {
        if (last_apply_error_cb) {
            last_apply_error_cb(message);
        }
    }

    void trigger_save_success() {
        if (last_save_success_cb) {
            last_save_success_cb();
        }
    }

    void trigger_save_error(const std::string& message) {
        if (last_save_error_cb) {
            last_save_error_cb(message);
        }
    }

    // ========== Reset for multiple test sections ==========

    void reset() {
        check_accelerometer_called = false;
        calibration_calls.clear();
        apply_calls.clear();
        save_to_config_called = false;
        cancel_call_count = 0;
        state_ = State::IDLE;
        results_ = CalibrationResults{};

        last_accel_complete_cb = nullptr;
        last_accel_error_cb = nullptr;
        last_progress_cb = nullptr;
        last_result_cb = nullptr;
        last_calibration_error_cb = nullptr;
        last_apply_success_cb = nullptr;
        last_apply_error_cb = nullptr;
        last_save_success_cb = nullptr;
        last_save_error_cb = nullptr;
    }
};

// ============================================================================
// Helper to create valid test results
// ============================================================================

static InputShaperResult make_test_result(char axis) {
    InputShaperResult result;
    result.axis = axis;
    result.shaper_type = "mzv";
    result.shaper_freq = 36.8f;
    result.max_accel = 4500.0f;
    result.smoothing = 0.05f;
    result.vibrations = 3.2f;

    // Add some shaper alternatives
    ShaperOption opt1{"zv", 38.0f, 5.0f, 0.02f, 6000.0f};
    ShaperOption opt2{"mzv", 36.8f, 3.2f, 0.05f, 4500.0f};
    ShaperOption opt3{"ei", 35.0f, 2.5f, 0.08f, 3500.0f};
    result.all_shapers = {opt1, opt2, opt3};

    return result;
}

// ============================================================================
// Calibrator Unit Tests (these pass now with the real calibrator)
// ============================================================================

TEST_CASE("InputShaperCalibrator state machine basics", "[calibrator][input_shaper]") {
    InputShaperCalibrator calibrator;

    SECTION("Initial state is IDLE") {
        CHECK(calibrator.get_state() == InputShaperCalibrator::State::IDLE);
    }

    SECTION("Results start empty") {
        const auto& results = calibrator.get_results();
        CHECK_FALSE(results.has_x());
        CHECK_FALSE(results.has_y());
        CHECK_FALSE(results.is_complete());
    }

    SECTION("Cancel returns to IDLE") {
        calibrator.cancel();
        CHECK(calibrator.get_state() == InputShaperCalibrator::State::IDLE);
    }
}

// ============================================================================
// Mock Calibrator Unit Tests (verify mock works correctly)
// ============================================================================

TEST_CASE("MockInputShaperCalibrator tracks calls correctly", "[mock][input_shaper]") {
    MockInputShaperCalibrator mock;

    SECTION("check_accelerometer is tracked") {
        bool callback_called = false;
        mock.check_accelerometer([&](float) { callback_called = true; }, nullptr);

        CHECK(mock.check_accelerometer_called);
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::CHECKING_ADXL);

        mock.trigger_accel_complete(0.05f);
        CHECK(callback_called);
        CHECK(mock.get_results().noise_level == Catch::Approx(0.05f));
    }

    SECTION("run_calibration X is tracked") {
        bool result_called = false;
        mock.run_calibration(
            'X', nullptr, [&](const InputShaperResult&) { result_called = true; }, nullptr);

        REQUIRE(mock.calibration_calls.size() == 1);
        CHECK(mock.calibration_calls[0].axis == 'X');
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::TESTING_X);

        auto result = make_test_result('X');
        mock.trigger_calibration_result(result);
        CHECK(result_called);
        CHECK(mock.get_results().has_x());
    }

    SECTION("run_calibration Y is tracked") {
        mock.run_calibration('Y', nullptr, nullptr, nullptr);

        REQUIRE(mock.calibration_calls.size() == 1);
        CHECK(mock.calibration_calls[0].axis == 'Y');
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::TESTING_Y);
    }

    SECTION("apply_settings is tracked") {
        ApplyConfig config;
        config.axis = 'X';
        config.shaper_type = "mzv";
        config.frequency = 36.8f;

        mock.apply_settings(config, nullptr, nullptr);

        REQUIRE(mock.apply_calls.size() == 1);
        CHECK(mock.apply_calls[0].config.axis == 'X');
        CHECK(mock.apply_calls[0].config.shaper_type == "mzv");
        CHECK(mock.apply_calls[0].config.frequency == Catch::Approx(36.8f));
    }

    SECTION("save_to_config is tracked") {
        mock.save_to_config(nullptr, nullptr);
        CHECK(mock.save_to_config_called);
    }

    SECTION("cancel is tracked") {
        mock.run_calibration('X', nullptr, nullptr, nullptr);
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::TESTING_X);

        mock.cancel();
        CHECK(mock.cancel_call_count == 1);
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::IDLE);
    }

    SECTION("reset clears all state") {
        mock.check_accelerometer(nullptr, nullptr);
        mock.run_calibration('X', nullptr, nullptr, nullptr);
        mock.apply_settings({}, nullptr, nullptr);
        mock.save_to_config(nullptr, nullptr);
        mock.cancel();

        mock.reset();

        CHECK_FALSE(mock.check_accelerometer_called);
        CHECK(mock.calibration_calls.empty());
        CHECK(mock.apply_calls.empty());
        CHECK_FALSE(mock.save_to_config_called);
        CHECK(mock.cancel_call_count == 0);
        CHECK(mock.get_state() == MockInputShaperCalibrator::State::IDLE);
    }
}

// ============================================================================
// Panel Integration Contract Tests (document expected behavior)
// ============================================================================

TEST_CASE("Panel-Calibrator integration contract", "[panel][input_shaper][integration][!mayfail]") {
    // These tests document the expected contract after refactoring
    // InputShaperPanel to delegate to InputShaperCalibrator

    SECTION("Panel should create calibrator on set_api()") {
        // After refactoring:
        // - Panel creates InputShaperCalibrator with the API
        // - Panel stores calibrator as member
        // - All button handlers delegate to calibrator methods
        WARN("Implement: Panel creates calibrator internally in set_api()");
    }

    SECTION("Calibrate X delegates to run_calibration('X')") {
        // Expected flow after refactoring:
        // 1. User clicks "Calibrate X"
        // 2. Panel calls calibrator_->run_calibration('X', progress_cb, result_cb, error_cb)
        // 3. Panel updates UI based on calibrator callbacks
        WARN(
            "Implement: handle_calibrate_x_clicked() calls calibrator_->run_calibration('X', ...)");
    }

    SECTION("Calibrate Y delegates to run_calibration('Y')") {
        WARN(
            "Implement: handle_calibrate_y_clicked() calls calibrator_->run_calibration('Y', ...)");
    }

    SECTION("Measure Noise delegates to check_accelerometer()") {
        WARN("Implement: handle_measure_noise_clicked() calls calibrator_->check_accelerometer()");
    }

    SECTION("Apply delegates to apply_settings()") {
        // Expected flow:
        // 1. User clicks "Apply"
        // 2. Panel constructs ApplyConfig from current results
        // 3. Panel calls calibrator_->apply_settings(config, success_cb, error_cb)
        WARN("Implement: handle_apply_clicked() calls calibrator_->apply_settings()");
    }

    SECTION("Save Config delegates to save_to_config()") {
        WARN("Implement: handle_save_config_clicked() calls calibrator_->save_to_config()");
    }

    SECTION("Cancel delegates to cancel()") {
        WARN("Implement: handle_cancel_clicked() calls calibrator_->cancel()");
    }
}

TEST_CASE("Panel state transitions from calibrator callbacks",
          "[panel][input_shaper][integration][!mayfail]") {
    SECTION("Successful calibration transitions to RESULTS state") {
        // When calibrator's result callback fires with valid result:
        // 1. Panel stores the result
        // 2. Panel transitions to RESULTS state
        // 3. Panel updates result display subjects
        WARN("Implement: Result callback updates panel state and UI");
    }

    SECTION("Calibration error transitions to ERROR state") {
        // When calibrator's error callback fires:
        // 1. Panel stores error message
        // 2. Panel transitions to ERROR state
        // 3. Panel shows error message and retry button
        WARN("Implement: Error callback updates panel to ERROR state");
    }

    SECTION("Progress callback updates UI progress indicator") {
        // When calibrator's progress callback fires with percent:
        // 1. Panel updates status label with "Testing... X%"
        // 2. Panel may update progress bar if one exists
        WARN("Implement: Progress callback updates status label");
    }
}

TEST_CASE("Panel lifecycle with calibrator", "[panel][input_shaper][integration][!mayfail]") {
    SECTION("Deactivate cancels in-progress calibration") {
        // When panel's on_deactivate() is called:
        // 1. Panel calls calibrator_->cancel()
        // 2. Panel resets to IDLE state
        WARN("Implement: on_deactivate() cancels calibration");
    }

    SECTION("Activate resets to IDLE state") {
        // When panel's on_activate() is called:
        // 1. Panel ensures calibrator is in IDLE state
        // 2. Panel resets UI to idle view
        WARN("Implement: on_activate() ensures clean state");
    }
}

// ============================================================================
// Expected API after refactoring
// ============================================================================

TEST_CASE("Expected InputShaperPanel API after refactoring",
          "[panel][input_shaper][api][!mayfail]") {
    // Document the expected panel interface after refactoring

    SECTION("Panel should expose get_calibrator() for testing") {
        // Optional: Allow injecting mock calibrator for unit tests
        // InputShaperPanel panel;
        // panel.set_calibrator(std::move(mock_calibrator));
        // OR
        // auto* calibrator = panel.get_calibrator();
        WARN("Consider: Expose calibrator for testing or use dependency injection");
    }

    SECTION("Panel should NOT directly call MoonrakerAPI after refactoring") {
        // All API calls should go through the calibrator:
        // - NO api_->start_resonance_test()
        // - NO api_->execute_gcode("MEASURE_AXES_NOISE")
        // - NO api_->set_input_shaper()
        // - NO api_->save_config()
        WARN("Verify: Remove direct MoonrakerAPI calls from panel");
    }
}
