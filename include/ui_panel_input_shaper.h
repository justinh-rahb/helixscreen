// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "calibration_types.h" // For InputShaperResult
#include "input_shaper_calibrator.h"
#include "overlay_base.h"
#include "subject_managed_panel.h"

#include <array>
#include <atomic>
#include <lvgl.h>
#include <memory>
#include <string>
#include <vector>

class MoonrakerClient;
class MoonrakerAPI;

/**
 * @file ui_panel_input_shaper.h
 * @brief Input Shaper calibration panel for resonance compensation tuning
 *
 * Interactive panel that guides users through SHAPER_CALIBRATE workflow.
 * Allows measuring resonance on X/Y axes, viewing recommendations, and
 * applying optimal shaper settings to reduce ringing/ghosting.
 *
 * ## State Machine:
 * - IDLE: Shows instructions and buttons to start calibration
 * - MEASURING: Calibration running, show spinner and cancel button
 * - RESULTS: Display recommendations, Apply/Dismiss buttons
 * - ERROR: Something went wrong, retry option
 *
 * ## Klipper Commands Used:
 * - MEASURE_AXES_NOISE: Check accelerometer noise level
 * - SHAPER_CALIBRATE AXIS=X/Y: Run resonance test
 * - SET_INPUT_SHAPER: Apply recommended settings
 * - SAVE_CONFIG: Save settings permanently (restarts Klipper)
 *
 * ## Usage:
 * ```cpp
 * InputShaperPanel& panel = get_global_input_shaper_panel();
 * panel.init_subjects();  // Once at startup
 * panel.create(screen);   // Lazy create
 * panel.show();           // Opens overlay
 * ```
 */
class InputShaperPanel : public OverlayBase {
  public:
    /**
     * @brief Panel state machine states
     */
    enum class State {
        IDLE,      ///< Ready to start, showing instructions
        MEASURING, ///< SHAPER_CALIBRATE or MEASURE_AXES_NOISE running
        RESULTS,   ///< Showing calibration recommendations
        ERROR      ///< Error occurred
    };

    /**
     * @brief Parsed shaper result for display
     */
    struct ShaperFit {
        std::string type;            ///< Shaper type (zv, mzv, ei, 2hump_ei, 3hump_ei)
        float frequency = 0.0f;      ///< Recommended frequency in Hz
        float vibrations = 0.0f;     ///< Remaining vibrations percentage
        float smoothing = 0.0f;      ///< Smoothing value (lower is better)
        bool is_recommended = false; ///< True if this is the recommended shaper
    };

    InputShaperPanel() = default;
    ~InputShaperPanel() override;

    //
    // === OverlayBase Interface ===
    //

    /**
     * @brief Initialize subjects for reactive XML bindings
     *
     * Must be called BEFORE XML creation (from register_callbacks).
     * Subject bindings are resolved at XML parse time.
     */
    void init_subjects() override;

    /**
     * @brief Create overlay UI from XML
     *
     * @param parent Parent screen widget to attach overlay to
     * @return Root object of overlay, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent) override;

    /**
     * @brief Get human-readable overlay name
     * @return "Input Shaper"
     */
    const char* get_name() const override {
        return "Input Shaper";
    }

    /**
     * @brief Called when overlay becomes visible
     *
     * Resets state to IDLE, clears previous results.
     */
    void on_activate() override;

    /**
     * @brief Called when overlay is being hidden
     *
     * Cancels calibration if in progress.
     */
    void on_deactivate() override;

    /**
     * @brief Clean up resources for async-safe destruction
     */
    void cleanup() override;

    //
    // === Public API ===
    //

    /**
     * @brief Show overlay panel
     *
     * Pushes overlay onto navigation stack and registers with NavigationManager.
     * on_activate() will be called automatically after animation completes.
     */
    void show();

    /**
     * @brief Set Moonraker client and API for G-code commands
     *
     * Creates InputShaperCalibrator instance with the API.
     *
     * @param client MoonrakerClient (kept for potential future use)
     * @param api MoonrakerAPI for G-code execution
     */
    void set_api(MoonrakerClient* client, MoonrakerAPI* api);

    /**
     * @brief Get current panel state
     * @return Current State
     */
    [[nodiscard]] State get_state() const {
        return state_;
    }

    //
    // === Event Handlers (public for XML event_cb callbacks) ===
    //

    void handle_calibrate_x_clicked();
    void handle_calibrate_y_clicked();
    void handle_measure_noise_clicked();
    void handle_cancel_clicked();
    void handle_apply_clicked();
    void handle_close_clicked();
    void handle_retry_clicked();
    void handle_save_config_clicked();
    void handle_print_test_pattern_clicked();
    void handle_help_clicked();

  private:
    // Subject manager for RAII cleanup
    SubjectManager subjects_;

    // State management
    State state_ = State::IDLE;
    void set_state(State new_state);

    // Calibration commands
    void start_calibration(char axis);
    void measure_noise();
    void cancel_calibration();
    void apply_recommendation();
    void save_configuration();

    // Result callbacks (from MoonrakerAPI)
    void on_calibration_result(const InputShaperResult& result);
    void on_calibration_error(const std::string& message);

    // UI update helpers
    void populate_results();
    void clear_results();
    void update_status_label(const std::string& text);

    // Widget/client references (overlay_root_ inherited from OverlayBase)
    lv_obj_t* parent_screen_ = nullptr;
    MoonrakerClient* client_ = nullptr;
    MoonrakerAPI* api_ = nullptr;

    // Private setup helper (called by create())
    void setup_widgets();

    // Display elements
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* error_message_ = nullptr;
    lv_obj_t* recommendation_label_ = nullptr;

    // Subjects for reactive bindings
    static constexpr size_t MAX_SHAPERS = 5;
    static constexpr size_t SHAPER_TYPE_BUF_SIZE = 16;
    static constexpr size_t SHAPER_VALUE_BUF_SIZE = 16;

    std::array<lv_subject_t, MAX_SHAPERS> shaper_visible_subjects_;
    std::array<lv_subject_t, MAX_SHAPERS> shaper_type_subjects_;
    std::array<lv_subject_t, MAX_SHAPERS> shaper_freq_subjects_;
    std::array<lv_subject_t, MAX_SHAPERS> shaper_vib_subjects_;

    // Fixed char arrays for string subjects
    char shaper_type_bufs_[MAX_SHAPERS][SHAPER_TYPE_BUF_SIZE] = {};
    char shaper_freq_bufs_[MAX_SHAPERS][SHAPER_VALUE_BUF_SIZE] = {};
    char shaper_vib_bufs_[MAX_SHAPERS][SHAPER_VALUE_BUF_SIZE] = {};

    // Results data
    char current_axis_ = 'X';
    char last_calibrated_axis_ = 'X'; ///< Axis most recently calibrated (for apply)
    std::vector<ShaperFit> shaper_results_;
    std::string recommended_type_;
    float recommended_freq_ = 0.0f;

    // Calibrator for delegating operations
    std::unique_ptr<helix::calibration::InputShaperCalibrator> calibrator_;

    // Destruction flag for async callback safety [L012]
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

  public:
    /**
     * @brief Get calibrator for testing
     * @return Pointer to calibrator, or nullptr if not created yet
     */
    [[nodiscard]] helix::calibration::InputShaperCalibrator* get_calibrator() const {
        return calibrator_.get();
    }
};

// Global instance accessor
InputShaperPanel& get_global_input_shaper_panel();

/**
 * @brief Register XML event callbacks for input shaper panel
 *
 * Call this once at startup before creating any input_shaper_panel XML.
 * Registers callbacks for all button events (calibrate_x, calibrate_y, etc.).
 */
void ui_panel_input_shaper_register_callbacks();

/**
 * @brief Initialize row click callback for opening from Advanced panel
 *
 * Must be called during app initialization before XML creation.
 * Registers "on_input_shaper_row_clicked" callback.
 */
void init_input_shaper_row_handler();
