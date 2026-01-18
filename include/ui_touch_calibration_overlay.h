// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_touch_calibration_overlay.h
 * @brief Touch calibration overlay for 3-point calibration workflow
 *
 * Provides a fullscreen overlay for touch calibration with:
 * - Visual crosshair targets for touch point capture
 * - State-driven UI progression (idle -> points -> verify -> complete)
 * - Completion callback with success/skip status for wizard integration
 * - Optional skip button for setup wizard (allow_skip mode)
 *
 * ## States:
 *   IDLE -> POINT_1 -> POINT_2 -> POINT_3 -> VERIFY -> COMPLETE
 *
 * ## Completion Callback:
 * - (true, false)  = Accepted and saved
 * - (false, false) = Cancelled (back button)
 * - (false, true)  = Skipped (wizard only)
 *
 * ## Initialization Order:
 *   1. Register XML components (touch_calibration_overlay.xml)
 *   2. init_subjects()
 *   3. register_callbacks()
 *   4. create(parent_screen)
 *   5. show() when ready to display
 */

#pragma once

#include "overlay_base.h"
#include "subject_managed_panel.h"
#include "touch_calibration_panel.h"

#include <functional>
#include <memory>

// Forward declarations
struct TouchCalibration;

namespace helix::ui {

/**
 * @class TouchCalibrationOverlay
 * @brief Fullscreen overlay for 3-point touch calibration
 *
 * Manages the touch calibration UI workflow, displaying crosshair targets
 * and capturing touch points for calibration matrix computation. Integrates
 * with TouchCalibrationPanel for state machine logic.
 *
 * Inherits from OverlayBase for lifecycle management (on_activate/on_deactivate).
 */
class TouchCalibrationOverlay : public OverlayBase {
  public:
    /**
     * @brief Completion callback type
     *
     * @param success true if calibration was accepted and saved
     * @param skipped true if user chose to skip (wizard mode only)
     *
     * Callback interpretations:
     * - (true, false)  = Calibration accepted and saved
     * - (false, false) = Calibration cancelled (back button)
     * - (false, true)  = Calibration skipped (wizard only)
     */
    using CompletionCallback = std::function<void(bool success, bool skipped)>;

    TouchCalibrationOverlay();
    ~TouchCalibrationOverlay() override;

    // Non-copyable
    TouchCalibrationOverlay(const TouchCalibrationOverlay&) = delete;
    TouchCalibrationOverlay& operator=(const TouchCalibrationOverlay&) = delete;

    //
    // === OverlayBase Interface ===
    //

    /**
     * @brief Initialize reactive subjects for XML binding
     *
     * Creates and registers subjects:
     * - touch_cal_state (int): Current state 0-5
     * - touch_cal_instruction (string): Instruction text
     * - touch_cal_skip_visible (int): 1 if skip button shown
     *
     * MUST be called BEFORE create() to ensure bindings work.
     */
    void init_subjects() override;

    /**
     * @brief Register event callbacks with lv_xml system
     *
     * Registers callbacks:
     * - on_touch_cal_start_clicked
     * - on_touch_cal_accept_clicked
     * - on_touch_cal_retry_clicked
     * - on_touch_cal_skip_clicked
     * - on_touch_cal_screen_touched
     * - on_touch_cal_back_clicked
     */
    void register_callbacks() override;

    /**
     * @brief Create overlay UI from XML
     *
     * @param parent Parent widget to attach overlay to (usually screen)
     * @return Root object of overlay, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent) override;

    /**
     * @brief Get human-readable overlay name
     * @return "Touch Calibration"
     */
    const char* get_name() const override {
        return "Touch Calibration";
    }

    /**
     * @brief Called when overlay becomes visible
     *
     * Initializes crosshair position and prepares for calibration.
     */
    void on_activate() override;

    /**
     * @brief Called when overlay is being hidden
     *
     * Cancels any in-progress calibration.
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
     * @brief Show overlay and begin calibration workflow
     *
     * @param callback Optional callback invoked on completion/cancel/skip
     *
     * Pushes overlay onto navigation stack and shows initial UI state.
     */
    void show(CompletionCallback callback = nullptr);

    /**
     * @brief Hide overlay and return to previous screen
     *
     * Pops overlay from navigation stack via ui_nav_go_back().
     */
    void hide();

    /**
     * @brief Enable or disable the skip button
     *
     * @param allow true to show skip button (wizard mode)
     *
     * When enabled, users can skip calibration during initial setup.
     */
    void set_allow_skip(bool allow);

    //
    // === Event Handlers (called by static trampolines) ===
    //

    /** @brief Handle start button click - begins calibration */
    void handle_start_clicked();

    /** @brief Handle accept button click - saves calibration */
    void handle_accept_clicked();

    /** @brief Handle retry button click - restarts calibration */
    void handle_retry_clicked();

    /** @brief Handle skip button click - skips without saving */
    void handle_skip_clicked();

    /**
     * @brief Handle screen touch event - captures calibration point
     * @param e LVGL event with touch coordinates
     */
    void handle_screen_touched(lv_event_t* e);

    /** @brief Handle back button click - cancels calibration */
    void handle_back_clicked();

    //
    // === Accessors ===
    //

    /**
     * @brief Check if overlay widget exists
     * @return true if overlay has been created
     */
    bool is_created() const {
        return overlay_root_ != nullptr;
    }

    /**
     * @brief Get the underlying calibration panel
     * @return Pointer to TouchCalibrationPanel, or nullptr if not created
     */
    helix::TouchCalibrationPanel* get_panel() {
        return panel_.get();
    }

  private:
    /** @brief Update state subject from panel state */
    void update_state_subject();

    /** @brief Update instruction text based on current state */
    void update_instruction_text();

    /** @brief Position crosshair at current calibration target */
    void update_crosshair_position();

    /**
     * @brief Handle calibration completion from panel
     * @param cal Calibration data if successful, nullptr if cancelled
     */
    void on_calibration_complete(const TouchCalibration* cal);

    //
    // === State Machine ===
    //

    std::unique_ptr<helix::TouchCalibrationPanel> panel_;

    //
    // === Subjects (managed by SubjectManager) ===
    //

    SubjectManager subjects_;
    lv_subject_t state_subject_;        ///< int: 0-5 for states
    lv_subject_t instruction_subject_;  ///< string: instruction text
    lv_subject_t skip_visible_subject_; ///< int: 1 if skip allowed
    char instruction_buffer_[128];

    //
    // === Callbacks ===
    //

    CompletionCallback completion_callback_;
    bool allow_skip_ = false;
    bool callback_invoked_ = false; ///< Guard against double-invoke

    //
    // === Widget References (for crosshair positioning) ===
    //

    lv_obj_t* crosshair_ = nullptr;
    lv_obj_t* verify_marker_ = nullptr;

    //
    // === State Constants ===
    //

    static constexpr int STATE_IDLE = 0;
    static constexpr int STATE_POINT_1 = 1;
    static constexpr int STATE_POINT_2 = 2;
    static constexpr int STATE_POINT_3 = 3;
    static constexpr int STATE_VERIFY = 4;
    static constexpr int STATE_COMPLETE = 5;

    static constexpr int CROSSHAIR_SIZE = 48;
    static constexpr int CROSSHAIR_HALF_SIZE = CROSSHAIR_SIZE / 2;
};

// ============================================================================
// Global Instance Access
// ============================================================================

/**
 * @brief Get the global TouchCalibrationOverlay instance
 *
 * Creates the instance on first call. Singleton pattern.
 *
 * @return Reference to the global TouchCalibrationOverlay
 */
TouchCalibrationOverlay& get_touch_calibration_overlay();

/**
 * @brief Register touch calibration overlay event callbacks
 *
 * Registers static callback trampolines with lv_xml_register_event_cb().
 * Call during application initialization before creating overlay.
 */
void register_touch_calibration_overlay_callbacks();

} // namespace helix::ui
