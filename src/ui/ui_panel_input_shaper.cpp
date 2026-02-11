// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_input_shaper.h"

#include "ui_emergency_stop.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_nav_manager.h"
#include "ui_toast.h"

#include "format_utils.h"
#include "lvgl/src/others/translation/lv_translation.h"
#include "moonraker_api.h"
#include "moonraker_client.h"
#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

#include <cstdio>

// ============================================================================
// GLOBAL INSTANCE AND ROW CLICK HANDLER
// ============================================================================

static std::unique_ptr<InputShaperPanel> g_input_shaper_panel;

// State subject (0=IDLE, 1=MEASURING, 2=RESULTS, 3=ERROR)
static lv_subject_t s_input_shaper_state;

// Forward declarations
static void on_input_shaper_row_clicked(lv_event_t* e);
MoonrakerClient* get_moonraker_client();
MoonrakerAPI* get_moonraker_api();

InputShaperPanel& get_global_input_shaper_panel() {
    if (!g_input_shaper_panel) {
        g_input_shaper_panel = std::make_unique<InputShaperPanel>();
        StaticPanelRegistry::instance().register_destroy("InputShaperPanel",
                                                         []() { g_input_shaper_panel.reset(); });
    }
    return *g_input_shaper_panel;
}

InputShaperPanel::~InputShaperPanel() {
    // Applying [L011]: No mutex in destructors
    // Signal to async callbacks that this panel is being destroyed [L012]
    alive_->store(false);

    // Deinitialize subjects to disconnect observers before we're destroyed
    // This prevents use-after-free when lv_deinit() later deletes widgets
    if (subjects_initialized_) {
        subjects_.deinit_all();
        subjects_initialized_ = false;
    }

    // Clear widget pointers (owned by LVGL)
    overlay_root_ = nullptr;
    parent_screen_ = nullptr;

    // Guard against static destruction order fiasco (spdlog may be gone)
    if (!StaticPanelRegistry::is_destroyed()) {
        spdlog::trace("[InputShaper] Destroyed");
    }
}

void init_input_shaper_row_handler() {
    lv_xml_register_event_cb(nullptr, "on_input_shaper_row_clicked", on_input_shaper_row_clicked);
    spdlog::trace("[InputShaper] Row click callback registered");
}

/**
 * @brief Row click handler for opening input shaper from Advanced panel
 */
static void on_input_shaper_row_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("[InputShaper] Input Shaping row clicked");

    auto& panel = get_global_input_shaper_panel();

    // Lazy-create the input shaper panel
    if (!panel.get_root()) {
        spdlog::debug("[InputShaper] Creating input shaper panel...");

        // Set API references before create
        MoonrakerClient* client = get_moonraker_client();
        MoonrakerAPI* api = get_moonraker_api();
        panel.set_api(client, api);

        lv_obj_t* screen = lv_display_get_screen_active(nullptr);
        if (!panel.create(screen)) {
            spdlog::error("[InputShaper] Failed to create input_shaper_panel");
            return;
        }
        spdlog::info("[InputShaper] Panel created");
    }

    // Show the overlay (registers with NavigationManager and pushes)
    panel.show();
}

// ============================================================================
// XML EVENT CALLBACK REGISTRATION
// ============================================================================

void ui_panel_input_shaper_register_callbacks() {
    // Register event callbacks for XML
    lv_xml_register_event_cb(nullptr, "input_shaper_calibrate_all_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_calibrate_all_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_calibrate_x_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_calibrate_x_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_calibrate_y_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_calibrate_y_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_measure_noise_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_measure_noise_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_cancel_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_cancel_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_apply_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_apply_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_close_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_close_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_retry_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_retry_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_save_config_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_save_config_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_save_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_save_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_print_test_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_print_test_pattern_clicked();
    });

    lv_xml_register_event_cb(nullptr, "input_shaper_help_cb", [](lv_event_t* /*e*/) {
        get_global_input_shaper_panel().handle_help_clicked();
    });

    // Initialize subjects BEFORE XML creation
    auto& panel = get_global_input_shaper_panel();
    panel.init_subjects();

    spdlog::debug("[InputShaper] Registered XML event callbacks");
}

// ============================================================================
// SUBJECT INITIALIZATION
// ============================================================================

void InputShaperPanel::init_subjects() {
    if (subjects_initialized_) {
        return;
    }

    // Initialize state subject for state machine visibility
    UI_MANAGED_SUBJECT_INT(s_input_shaper_state, 0, "input_shaper_state", subjects_);

    // Per-axis comparison table subjects
    auto init_cmp_row = [this](ComparisonRow& row, const char* prefix, size_t idx) {
        char name[48];
        snprintf(name, sizeof(name), "is_%s_cmp_%zu_type", prefix, idx);
        UI_MANAGED_SUBJECT_STRING_N(row.type, row.type_buf, CMP_TYPE_BUF, "", name, subjects_);
        snprintf(name, sizeof(name), "is_%s_cmp_%zu_freq", prefix, idx);
        UI_MANAGED_SUBJECT_STRING_N(row.freq, row.freq_buf, CMP_VALUE_BUF, "", name, subjects_);
        snprintf(name, sizeof(name), "is_%s_cmp_%zu_vib", prefix, idx);
        UI_MANAGED_SUBJECT_STRING_N(row.vib, row.vib_buf, CMP_VALUE_BUF, "", name, subjects_);
        snprintf(name, sizeof(name), "is_%s_cmp_%zu_accel", prefix, idx);
        UI_MANAGED_SUBJECT_STRING_N(row.accel, row.accel_buf, CMP_VALUE_BUF, "", name, subjects_);
    };

    for (size_t i = 0; i < MAX_SHAPERS; i++) {
        init_cmp_row(x_cmp_[i], "x", i);
        init_cmp_row(y_cmp_[i], "y", i);
    }

    // Error message subject
    UI_MANAGED_SUBJECT_STRING(is_error_message_, is_error_message_buf_,
                              "An error occurred during calibration.", "is_error_message",
                              subjects_);

    // Current config display subjects
    UI_MANAGED_SUBJECT_INT(is_shaper_configured_, 0, "is_shaper_configured", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_current_x_type_, is_current_x_type_buf_, "", "is_current_x_type",
                              subjects_);
    UI_MANAGED_SUBJECT_STRING(is_current_x_freq_, is_current_x_freq_buf_, "", "is_current_x_freq",
                              subjects_);
    UI_MANAGED_SUBJECT_STRING(is_current_y_type_, is_current_y_type_buf_, "", "is_current_y_type",
                              subjects_);
    UI_MANAGED_SUBJECT_STRING(is_current_y_freq_, is_current_y_freq_buf_, "", "is_current_y_freq",
                              subjects_);
    UI_MANAGED_SUBJECT_STRING(is_current_max_accel_, is_current_max_accel_buf_, "",
                              "is_current_max_accel", subjects_);

    // Measuring state labels
    UI_MANAGED_SUBJECT_STRING(is_measuring_axis_label_, is_measuring_axis_label_buf_,
                              "Calibrating...", "is_measuring_axis_label", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_measuring_step_label_, is_measuring_step_label_buf_, "",
                              "is_measuring_step_label", subjects_);
    UI_MANAGED_SUBJECT_INT(is_measuring_progress_, 0, "is_measuring_progress", subjects_);

    // Per-axis result display subjects
    UI_MANAGED_SUBJECT_INT(is_results_has_x_, 0, "is_results_has_x", subjects_);
    UI_MANAGED_SUBJECT_INT(is_results_has_y_, 0, "is_results_has_y", subjects_);

    // Header button disabled state
    UI_MANAGED_SUBJECT_INT(is_calibrate_all_disabled_, 0, "is_calibrate_all_disabled", subjects_);

    // Recommended row index per axis (-1 = none highlighted)
    UI_MANAGED_SUBJECT_INT(is_x_recommended_row_, -1, "is_x_recommended_row", subjects_);
    UI_MANAGED_SUBJECT_INT(is_y_recommended_row_, -1, "is_y_recommended_row", subjects_);

    UI_MANAGED_SUBJECT_STRING(is_result_x_shaper_, is_result_x_shaper_buf_, "",
                              "is_result_x_shaper", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_x_explanation_, is_result_x_explanation_buf_, "",
                              "is_result_x_explanation", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_x_vibration_, is_result_x_vibration_buf_, "",
                              "is_result_x_vibration", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_x_max_accel_, is_result_x_max_accel_buf_, "",
                              "is_result_x_max_accel", subjects_);
    UI_MANAGED_SUBJECT_INT(is_result_x_quality_, 0, "is_result_x_quality", subjects_);

    UI_MANAGED_SUBJECT_STRING(is_result_y_shaper_, is_result_y_shaper_buf_, "",
                              "is_result_y_shaper", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_y_explanation_, is_result_y_explanation_buf_, "",
                              "is_result_y_explanation", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_y_vibration_, is_result_y_vibration_buf_, "",
                              "is_result_y_vibration", subjects_);
    UI_MANAGED_SUBJECT_STRING(is_result_y_max_accel_, is_result_y_max_accel_buf_, "",
                              "is_result_y_max_accel", subjects_);
    UI_MANAGED_SUBJECT_INT(is_result_y_quality_, 0, "is_result_y_quality", subjects_);

    subjects_initialized_ = true;
    spdlog::debug("[InputShaper] Subjects initialized and registered");
}

// ============================================================================
// CREATE
// ============================================================================

lv_obj_t* InputShaperPanel::create(lv_obj_t* parent) {
    if (overlay_root_) {
        spdlog::debug("[InputShaper] Panel already created");
        return overlay_root_;
    }

    parent_screen_ = parent;

    spdlog::debug("[InputShaper] Creating overlay from XML");
    overlay_root_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "input_shaper_panel", nullptr));

    if (!overlay_root_) {
        spdlog::error("[InputShaper] Failed to create overlay from XML");
        return nullptr;
    }

    // Start hidden (ui_nav_push_overlay will show it)
    lv_obj_add_flag(overlay_root_, LV_OBJ_FLAG_HIDDEN);

    setup_widgets();

    spdlog::info("[InputShaper] Overlay created successfully");
    return overlay_root_;
}

// ============================================================================
// SETUP WIDGETS (private helper)
// ============================================================================

void InputShaperPanel::setup_widgets() {
    if (!overlay_root_) {
        spdlog::error("[InputShaper] NULL overlay_root_");
        return;
    }

    // State visibility is handled via XML subject bindings
    // All display elements are now subject-bound in XML

    // Set initial state
    set_state(State::IDLE);

    spdlog::debug("[InputShaper] Widget setup complete");
}

// ============================================================================
// SHOW
// ============================================================================

void InputShaperPanel::set_api(MoonrakerClient* client, MoonrakerAPI* api) {
    client_ = client;
    api_ = api;

    // Create calibrator with API for delegated operations
    calibrator_ = std::make_unique<helix::calibration::InputShaperCalibrator>(api_);
    spdlog::debug("[InputShaper] Calibrator created");
}

void InputShaperPanel::show() {
    if (!overlay_root_) {
        spdlog::error("[InputShaper] Cannot show: overlay not created");
        return;
    }

    spdlog::debug("[InputShaper] Showing overlay");

    // Register with NavigationManager for lifecycle callbacks
    NavigationManager::instance().register_overlay_instance(overlay_root_, this);

    // Push onto navigation stack - on_activate() will be called by NavigationManager
    ui_nav_push_overlay(overlay_root_);

    spdlog::info("[InputShaper] Overlay shown");
}

// ============================================================================
// LIFECYCLE HOOKS
// ============================================================================

void InputShaperPanel::on_activate() {
    // Call base class first
    OverlayBase::on_activate();

    spdlog::debug("[InputShaper] on_activate()");

    // Reset to idle state
    set_state(State::IDLE);
    clear_results();
    calibrate_all_mode_ = false;

    // Query current input shaper configuration from printer
    if (api_) {
        auto alive = alive_;
        api_->get_input_shaper_config(
            [this, alive](const InputShaperConfig& config) {
                if (!alive->load())
                    return;
                populate_current_config(config);
            },
            [this, alive](const MoonrakerError& err) {
                if (!alive->load())
                    return;
                spdlog::debug("[InputShaper] Could not query config: {}", err.message);
                // Not an error - just means config not available
                InputShaperConfig empty;
                populate_current_config(empty);
            });
    }

    // Auto-start calibration for testing (env var)
    if (std::getenv("INPUT_SHAPER_AUTO_START")) {
        spdlog::info("[InputShaper] Auto-starting X calibration (INPUT_SHAPER_AUTO_START set)");
        start_with_preflight('X');
    }
}

void InputShaperPanel::on_deactivate() {
    spdlog::debug("[InputShaper] on_deactivate()");

    // Cancel any in-progress calibration
    if (state_ == State::MEASURING && calibrator_) {
        spdlog::info("[InputShaper] Cancelling calibration on deactivate");
        calibrator_->cancel();
        set_state(State::IDLE);
    }

    // Call base class
    OverlayBase::on_deactivate();
}

void InputShaperPanel::cleanup() {
    spdlog::debug("[InputShaper] Cleaning up");

    // Signal to async callbacks that this panel is being destroyed [L012]
    alive_->store(false);

    // Unregister from NavigationManager before cleaning up
    if (overlay_root_) {
        NavigationManager::instance().unregister_overlay_instance(overlay_root_);
    }

    // Call base class to set cleanup_called_ flag
    OverlayBase::cleanup();

    // Clear references
    parent_screen_ = nullptr;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void InputShaperPanel::set_state(State new_state) {
    spdlog::debug("[InputShaper] State change: {} -> {}", static_cast<int>(state_),
                  static_cast<int>(new_state));
    state_ = new_state;

    // Update subject - XML bindings handle visibility automatically
    // State mapping: 0=IDLE, 1=MEASURING, 2=RESULTS, 3=ERROR
    lv_subject_set_int(&s_input_shaper_state, static_cast<int>(new_state));

    // Disable Calibrate All button when not idle
    lv_subject_set_int(&is_calibrate_all_disabled_, new_state != State::IDLE ? 1 : 0);
}

// ============================================================================
// CALIBRATION COMMANDS (using MoonrakerAPI)
// ============================================================================

void InputShaperPanel::start_with_preflight(char axis) {
    if (!calibrator_) {
        on_calibration_error("Internal error: calibrator not available");
        return;
    }

    current_axis_ = axis;
    last_calibrated_axis_ = axis;
    recommended_type_.clear();
    recommended_freq_ = 0.0f;

    // Show checking accelerometer status
    snprintf(is_measuring_axis_label_buf_, sizeof(is_measuring_axis_label_buf_),
             "Checking accelerometer...");
    lv_subject_copy_string(&is_measuring_axis_label_, is_measuring_axis_label_buf_);
    lv_subject_copy_string(&is_measuring_step_label_, "");
    lv_subject_set_int(&is_measuring_progress_, 0);

    set_state(State::MEASURING);
    spdlog::info("[InputShaper] Starting pre-flight noise check before {} axis calibration", axis);

    auto alive = alive_;
    calibrator_->check_accelerometer(
        [this, alive](float noise_level) {
            if (!alive->load())
                return;
            on_preflight_complete(noise_level);
        },
        [this, alive](const std::string& err) {
            if (!alive->load())
                return;
            on_preflight_error(err);
        });
}

void InputShaperPanel::on_preflight_complete(float noise_level) {
    if (state_ != State::MEASURING)
        return; // User cancelled

    spdlog::info("[InputShaper] Pre-flight passed, noise={:.4f}", noise_level);

    // Proceed to actual calibration
    start_calibration(current_axis_);
}

void InputShaperPanel::on_preflight_error(const std::string& message) {
    if (state_ != State::MEASURING)
        return;

    spdlog::error("[InputShaper] Pre-flight failed: {}", message);
    on_calibration_error("Accelerometer not responding. Check wiring and connection.");
}

void InputShaperPanel::calibrate_all() {
    calibrate_all_mode_ = true;
    x_result_ = InputShaperResult{}; // Clear stored X result
    start_with_preflight('X');
}

void InputShaperPanel::continue_calibrate_all_y() {
    spdlog::info("[InputShaper] Calibrate All: X complete, starting Y");
    // Don't reset calibrate_all_mode_ - still in multi-axis flow
    // Skip pre-flight for Y (accelerometer just verified for X)
    start_calibration('Y');
}

void InputShaperPanel::start_calibration(char axis) {
    if (!calibrator_) {
        spdlog::error("[InputShaper] No calibrator - cannot calibrate");
        on_calibration_error("Internal error: calibrator not available");
        return;
    }

    current_axis_ = axis;
    last_calibrated_axis_ = axis;

    // Only clear results for first axis in Calibrate All, or for single-axis
    if (!calibrate_all_mode_ || axis == 'X') {
        recommended_type_.clear();
        recommended_freq_ = 0.0f;
    }

    // Update measuring labels
    snprintf(is_measuring_axis_label_buf_, sizeof(is_measuring_axis_label_buf_),
             "Calibrating %c axis...", axis);
    lv_subject_copy_string(&is_measuring_axis_label_, is_measuring_axis_label_buf_);

    if (calibrate_all_mode_) {
        const char* step = (axis == 'X') ? "Step 1 of 2" : "Step 2 of 2";
        lv_subject_copy_string(&is_measuring_step_label_, step);
    } else {
        lv_subject_copy_string(&is_measuring_step_label_, "");
    }

    lv_subject_set_int(&is_measuring_progress_, 0);
    set_state(State::MEASURING);
    spdlog::info("[InputShaper] Starting calibration for axis {}", axis);

    // Capture alive flag for destruction detection [L012]
    auto alive = alive_;

    // Delegate to calibrator
    calibrator_->run_calibration(
        axis,
        [this, alive](int percent) {
            if (!alive->load())
                return;
            lv_subject_set_int(&is_measuring_progress_, percent);
            // Update step label with progress text
            if (percent < 55) {
                snprintf(is_measuring_step_label_buf_, sizeof(is_measuring_step_label_buf_),
                         "Measuring vibrations... %d%%", percent);
            } else if (percent < 100) {
                snprintf(is_measuring_step_label_buf_, sizeof(is_measuring_step_label_buf_),
                         "Analyzing data... %d%%", percent);
            } else {
                snprintf(is_measuring_step_label_buf_, sizeof(is_measuring_step_label_buf_),
                         "Complete");
            }
            lv_subject_copy_string(&is_measuring_step_label_, is_measuring_step_label_buf_);
        },
        [this, alive](const InputShaperResult& result) {
            if (!alive->load())
                return;
            on_calibration_result(result);
        },
        [this, alive](const std::string& err) {
            if (!alive->load())
                return;
            on_calibration_error(err);
        });
}

void InputShaperPanel::measure_noise() {
    if (!calibrator_) {
        spdlog::error("[InputShaper] No calibrator - cannot measure noise");
        on_calibration_error("Internal error: calibrator not available");
        return;
    }

    snprintf(is_measuring_axis_label_buf_, sizeof(is_measuring_axis_label_buf_),
             "Measuring accelerometer noise...");
    lv_subject_copy_string(&is_measuring_axis_label_, is_measuring_axis_label_buf_);
    set_state(State::MEASURING);
    spdlog::info("[InputShaper] Starting accelerometer check via calibrator");

    // Capture alive flag for destruction detection [L012]
    auto alive = alive_;

    calibrator_->check_accelerometer(
        [this, alive](float noise_level) {
            if (!alive->load())
                return;
            spdlog::debug("[InputShaper] Accelerometer check complete, noise={:.4f}", noise_level);
            char msg[64];
            snprintf(msg, sizeof(msg), "Noise level: %.4f", noise_level);
            ui_toast_show(ToastSeverity::INFO, msg, 3000);
            set_state(State::IDLE);
        },
        [this, alive](const std::string& err) {
            if (!alive->load())
                return;
            spdlog::error("[InputShaper] Failed to measure noise: {}", err);
            on_calibration_error(err);
        });
}

void InputShaperPanel::cancel_calibration() {
    spdlog::info("[InputShaper] Abort clicked, sending emergency stop + firmware restart");
    calibrate_all_mode_ = false;

    // Cancel calibrator state so we ignore any late results
    if (calibrator_) {
        calibrator_->cancel();
    }

    // Suppress shutdown/disconnect modals — E-stop + restart triggers expected reconnect
    EmergencyStopOverlay::instance().suppress_recovery_dialog(15000);
    if (api_) {
        api_->suppress_disconnect_modal(15000);
    }

    // M112 emergency stop halts immediately at MCU level (bypasses blocked gcode queue),
    // then firmware restart brings Klipper back online
    if (api_) {
        api_->emergency_stop(
            [this]() {
                spdlog::debug("[InputShaper] Emergency stop sent, sending firmware restart");
                if (api_) {
                    api_->restart_firmware(
                        []() { spdlog::debug("[InputShaper] Firmware restart initiated"); },
                        [](const MoonrakerError& err) {
                            spdlog::error("[InputShaper] Firmware restart failed: {}", err.message);
                        });
                }
            },
            [](const MoonrakerError& err) {
                spdlog::error("[InputShaper] Emergency stop failed: {}", err.message);
            });
    }

    set_state(State::IDLE);
}

void InputShaperPanel::apply_recommendation() {
    if (!calibrator_) {
        spdlog::error("[InputShaper] Cannot apply - no calibrator");
        return;
    }

    auto alive = alive_;

    // If we have stored X result from Calibrate All, apply X first then chain Y
    if (x_result_.is_valid()) {
        spdlog::info("[InputShaper] Applying X axis shaper: {} @ {:.1f} Hz", x_result_.shaper_type,
                     x_result_.shaper_freq);

        helix::calibration::ApplyConfig x_config;
        x_config.axis = 'X';
        x_config.shaper_type = x_result_.shaper_type;
        x_config.frequency = x_result_.shaper_freq;

        calibrator_->apply_settings(
            x_config,
            [this, alive]() {
                if (!alive->load())
                    return;
                spdlog::info("[InputShaper] X axis settings applied");
                // Chain Y apply if we have a recommendation
                if (!recommended_type_.empty() && recommended_freq_ > 0) {
                    apply_y_after_x();
                } else {
                    ui_toast_show(ToastSeverity::SUCCESS, lv_tr("Input shaper settings applied!"),
                                  2500);
                }
            },
            [alive](const std::string& err) {
                if (!alive->load())
                    return;
                spdlog::error("[InputShaper] Failed to apply X settings: {}", err);
                ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to apply settings"), 3000);
            });
    } else if (!recommended_type_.empty() && recommended_freq_ > 0) {
        // Single axis apply
        spdlog::info("[InputShaper] Applying {} axis shaper: {} @ {:.1f} Hz", last_calibrated_axis_,
                     recommended_type_, recommended_freq_);

        helix::calibration::ApplyConfig config;
        config.axis = last_calibrated_axis_;
        config.shaper_type = recommended_type_;
        config.frequency = recommended_freq_;

        calibrator_->apply_settings(
            config,
            [alive]() {
                if (!alive->load())
                    return;
                spdlog::info("[InputShaper] Settings applied successfully");
                ui_toast_show(ToastSeverity::SUCCESS, lv_tr("Input shaper settings applied!"),
                              2500);
            },
            [alive](const std::string& err) {
                if (!alive->load())
                    return;
                spdlog::error("[InputShaper] Failed to apply settings: {}", err);
                ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to apply settings"), 3000);
            });
    } else {
        spdlog::error("[InputShaper] Cannot apply - no valid recommendation");
    }
}

void InputShaperPanel::apply_y_after_x() {
    spdlog::info("[InputShaper] Applying Y axis shaper: {} @ {:.1f} Hz", recommended_type_,
                 recommended_freq_);

    helix::calibration::ApplyConfig y_config;
    y_config.axis = 'Y';
    y_config.shaper_type = recommended_type_;
    y_config.frequency = recommended_freq_;

    auto alive = alive_;
    calibrator_->apply_settings(
        y_config,
        [this, alive]() {
            if (!alive->load())
                return;
            spdlog::info("[InputShaper] Both axis settings applied");
            ui_toast_show(ToastSeverity::SUCCESS, lv_tr("Input shaper settings applied!"), 2500);
            // Refresh the current config display
            if (api_) {
                api_->get_input_shaper_config(
                    [this, alive](const InputShaperConfig& config) {
                        if (!alive->load())
                            return;
                        populate_current_config(config);
                    },
                    [](const MoonrakerError&) {});
            }
        },
        [alive](const std::string& err) {
            if (!alive->load())
                return;
            spdlog::error("[InputShaper] Failed to apply Y settings: {}", err);
            ui_toast_show(ToastSeverity::WARNING, lv_tr("X axis applied, but Y axis failed"), 4000);
        });
}

void InputShaperPanel::save_configuration() {
    if (!calibrator_) {
        return;
    }

    spdlog::info("[InputShaper] Saving configuration (SAVE_CONFIG)");
    ui_toast_show(ToastSeverity::WARNING, lv_tr("Saving config... Klipper will restart."), 3000);

    // Capture alive for async callback safety [L012]
    auto alive = alive_;

    calibrator_->save_to_config(
        [alive]() {
            if (!alive->load())
                return;
            spdlog::info("[InputShaper] SAVE_CONFIG sent - Klipper restarting");
        },
        [alive](const std::string& err) {
            if (!alive->load())
                return;
            spdlog::error("[InputShaper] SAVE_CONFIG failed: {}", err);
            ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to save configuration"), 3000);
        });
}

// ============================================================================
// RESULT CALLBACKS (from API)
// ============================================================================

void InputShaperPanel::on_calibration_result(const InputShaperResult& result) {
    // Ignore if we're not in measuring state (user may have cancelled)
    if (state_ != State::MEASURING) {
        spdlog::debug("[InputShaper] Ignoring result - not in measuring state");
        return;
    }

    spdlog::info("[InputShaper] Calibration complete: {} @ {:.1f} Hz (vib: {:.1f}%)",
                 result.shaper_type, result.shaper_freq, result.vibrations);

    // If Calibrate All and this was X, store result and continue to Y
    if (calibrate_all_mode_ && result.axis == 'X') {
        x_result_ = result;
        continue_calibrate_all_y();
        return;
    }

    // Store recommendation (from latest axis, or Y if Calibrate All)
    recommended_type_ = result.shaper_type;
    recommended_freq_ = result.shaper_freq;

    // Reset calibrate_all_mode (save before clearing for populate_axis_result)
    bool was_calibrate_all = calibrate_all_mode_;
    calibrate_all_mode_ = false;

    // Clear per-axis results
    lv_subject_set_int(&is_results_has_x_, 0);
    lv_subject_set_int(&is_results_has_y_, 0);

    // Populate per-axis result cards
    if (was_calibrate_all && x_result_.is_valid()) {
        populate_axis_result('X', x_result_);
    }
    populate_axis_result(result.axis, result);

    set_state(State::RESULTS);
}

void InputShaperPanel::on_calibration_error(const std::string& message) {
    // Ignore if we're not in measuring state
    if (state_ != State::MEASURING) {
        spdlog::debug("[InputShaper] Ignoring error - not in measuring state");
        return;
    }

    spdlog::error("[InputShaper] Calibration error: {}", message);

    // Reset Calibrate All mode on error to prevent stale state on retry
    calibrate_all_mode_ = false;

    snprintf(is_error_message_buf_, sizeof(is_error_message_buf_), "%s", message.c_str());
    lv_subject_copy_string(&is_error_message_, is_error_message_buf_);
    set_state(State::ERROR);
}

// ============================================================================
// UI UPDATE HELPERS
// ============================================================================

void InputShaperPanel::populate_current_config(const InputShaperConfig& config) {
    lv_subject_set_int(&is_shaper_configured_, config.is_configured ? 1 : 0);

    if (config.is_configured) {
        // Uppercase X type
        std::string x_upper = config.shaper_type_x;
        for (auto& c : x_upper)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        snprintf(is_current_x_type_buf_, sizeof(is_current_x_type_buf_), "%s", x_upper.c_str());
        lv_subject_copy_string(&is_current_x_type_, is_current_x_type_buf_);

        // X frequency
        helix::fmt::format_frequency_hz(config.shaper_freq_x, is_current_x_freq_buf_,
                                        sizeof(is_current_x_freq_buf_));
        lv_subject_copy_string(&is_current_x_freq_, is_current_x_freq_buf_);

        // Uppercase Y type
        std::string y_upper = config.shaper_type_y;
        for (auto& c : y_upper)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        snprintf(is_current_y_type_buf_, sizeof(is_current_y_type_buf_), "%s", y_upper.c_str());
        lv_subject_copy_string(&is_current_y_type_, is_current_y_type_buf_);

        // Y frequency
        helix::fmt::format_frequency_hz(config.shaper_freq_y, is_current_y_freq_buf_,
                                        sizeof(is_current_y_freq_buf_));
        lv_subject_copy_string(&is_current_y_freq_, is_current_y_freq_buf_);

        // Max accel - leave empty for now (populated from results in Chunk 3)
        lv_subject_copy_string(&is_current_max_accel_, "");

        spdlog::debug("[InputShaper] Config: X={} @ {}, Y={} @ {}", is_current_x_type_buf_,
                      is_current_x_freq_buf_, is_current_y_type_buf_, is_current_y_freq_buf_);
    } else {
        lv_subject_copy_string(&is_current_x_type_, "");
        lv_subject_copy_string(&is_current_x_freq_, "");
        lv_subject_copy_string(&is_current_y_type_, "");
        lv_subject_copy_string(&is_current_y_freq_, "");
        lv_subject_copy_string(&is_current_max_accel_, "");
        spdlog::debug("[InputShaper] No shaper configured");
    }
}

void InputShaperPanel::clear_results() {
    // Clear per-axis result cards
    lv_subject_set_int(&is_results_has_x_, 0);
    lv_subject_set_int(&is_results_has_y_, 0);
    lv_subject_set_int(&is_x_recommended_row_, -1);
    lv_subject_set_int(&is_y_recommended_row_, -1);

    // Clear comparison table subjects
    for (size_t i = 0; i < MAX_SHAPERS; i++) {
        lv_subject_copy_string(&x_cmp_[i].type, "");
        lv_subject_copy_string(&x_cmp_[i].freq, "");
        lv_subject_copy_string(&x_cmp_[i].vib, "");
        lv_subject_copy_string(&x_cmp_[i].accel, "");
        lv_subject_copy_string(&y_cmp_[i].type, "");
        lv_subject_copy_string(&y_cmp_[i].freq, "");
        lv_subject_copy_string(&y_cmp_[i].vib, "");
        lv_subject_copy_string(&y_cmp_[i].accel, "");
    }
}

// ============================================================================
// PER-AXIS RESULT HELPERS
// ============================================================================

const char* InputShaperPanel::get_shaper_explanation(const std::string& type) {
    if (type == "zv")
        return "Fast but minimal smoothing — best for well-built printers";
    if (type == "mzv")
        return "Good balance of speed and vibration reduction";
    if (type == "ei")
        return "Strong vibration reduction with moderate speed impact";
    if (type == "2hump_ei")
        return "Heavy smoothing — significant vibration issues detected";
    if (type == "3hump_ei")
        return "Maximum smoothing — consider checking mechanical issues";
    return "Vibration compensation active";
}

int InputShaperPanel::get_vibration_quality(float vibrations) {
    if (vibrations < 5.0f)
        return 0; // excellent (green)
    if (vibrations < 15.0f)
        return 1; // good (yellow)
    if (vibrations < 25.0f)
        return 2; // fair (orange)
    return 3;     // poor (red)
}

const char* InputShaperPanel::get_quality_description(float vibrations) {
    if (vibrations < 5.0f)
        return "Excellent — minimal residual vibration";
    if (vibrations < 15.0f)
        return "Good — acceptable vibration level";
    if (vibrations < 25.0f)
        return "Fair — mechanical improvements could help";
    return "Poor — check for mechanical issues";
}

void InputShaperPanel::populate_axis_result(char axis, const InputShaperResult& result) {
    // Uppercase the shaper type for display
    std::string type_upper = result.shaper_type;
    for (auto& c : type_upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    // Format frequency
    char freq_buf[16];
    helix::fmt::format_frequency_hz(result.shaper_freq, freq_buf, sizeof(freq_buf));

    if (axis == 'X') {
        lv_subject_set_int(&is_results_has_x_, 1);

        snprintf(is_result_x_shaper_buf_, sizeof(is_result_x_shaper_buf_), "%s @ %s",
                 type_upper.c_str(), freq_buf);
        lv_subject_copy_string(&is_result_x_shaper_, is_result_x_shaper_buf_);

        snprintf(is_result_x_explanation_buf_, sizeof(is_result_x_explanation_buf_), "* %s",
                 get_shaper_explanation(result.shaper_type));
        lv_subject_copy_string(&is_result_x_explanation_, is_result_x_explanation_buf_);

        snprintf(is_result_x_vibration_buf_, sizeof(is_result_x_vibration_buf_), "%.1f%%",
                 result.vibrations);
        lv_subject_copy_string(&is_result_x_vibration_, is_result_x_vibration_buf_);

        snprintf(is_result_x_max_accel_buf_, sizeof(is_result_x_max_accel_buf_),
                 "%.0f mm/s\xC2\xB2", result.max_accel);
        lv_subject_copy_string(&is_result_x_max_accel_, is_result_x_max_accel_buf_);

        lv_subject_set_int(&is_result_x_quality_, get_vibration_quality(result.vibrations));
    } else {
        lv_subject_set_int(&is_results_has_y_, 1);

        snprintf(is_result_y_shaper_buf_, sizeof(is_result_y_shaper_buf_), "%s @ %s",
                 type_upper.c_str(), freq_buf);
        lv_subject_copy_string(&is_result_y_shaper_, is_result_y_shaper_buf_);

        snprintf(is_result_y_explanation_buf_, sizeof(is_result_y_explanation_buf_), "* %s",
                 get_shaper_explanation(result.shaper_type));
        lv_subject_copy_string(&is_result_y_explanation_, is_result_y_explanation_buf_);

        snprintf(is_result_y_vibration_buf_, sizeof(is_result_y_vibration_buf_), "%.1f%%",
                 result.vibrations);
        lv_subject_copy_string(&is_result_y_vibration_, is_result_y_vibration_buf_);

        snprintf(is_result_y_max_accel_buf_, sizeof(is_result_y_max_accel_buf_),
                 "%.0f mm/s\xC2\xB2", result.max_accel);
        lv_subject_copy_string(&is_result_y_max_accel_, is_result_y_max_accel_buf_);

        lv_subject_set_int(&is_result_y_quality_, get_vibration_quality(result.vibrations));
    }

    // Populate comparison table subjects
    auto& cmp = (axis == 'X') ? x_cmp_ : y_cmp_;
    auto& recommended_row = (axis == 'X') ? is_x_recommended_row_ : is_y_recommended_row_;
    lv_subject_set_int(&recommended_row, -1); // Reset

    for (size_t i = 0; i < MAX_SHAPERS; i++) {
        if (i < result.all_shapers.size()) {
            const auto& opt = result.all_shapers[i];

            // Type with * marker for recommended
            std::string type_upper = opt.type;
            for (auto& c : type_upper)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (opt.type == result.shaper_type) {
                type_upper += " *";
                lv_subject_set_int(&recommended_row, static_cast<int>(i));
            }
            snprintf(cmp[i].type_buf, CMP_TYPE_BUF, "%s", type_upper.c_str());
            lv_subject_copy_string(&cmp[i].type, cmp[i].type_buf);

            // Frequency
            helix::fmt::format_frequency_hz(opt.frequency, cmp[i].freq_buf, CMP_VALUE_BUF);
            lv_subject_copy_string(&cmp[i].freq, cmp[i].freq_buf);

            // Vibration with quality description
            const char* quality = get_quality_description(opt.vibrations);
            // Truncate quality to first word only for compact display
            std::string quality_word;
            if (quality) {
                const char* dash = strstr(quality, " \xe2\x80\x94");
                if (dash) {
                    quality_word = std::string(quality, static_cast<size_t>(dash - quality));
                } else {
                    quality_word = quality;
                }
            }
            snprintf(cmp[i].vib_buf, CMP_VALUE_BUF, "%.1f%% %s", opt.vibrations,
                     quality_word.c_str());
            lv_subject_copy_string(&cmp[i].vib, cmp[i].vib_buf);

            // Max accel
            snprintf(cmp[i].accel_buf, CMP_VALUE_BUF, "%.0f", opt.max_accel);
            lv_subject_copy_string(&cmp[i].accel, cmp[i].accel_buf);
        } else {
            // Clear unused rows
            lv_subject_copy_string(&cmp[i].type, "");
            lv_subject_copy_string(&cmp[i].freq, "");
            lv_subject_copy_string(&cmp[i].vib, "");
            lv_subject_copy_string(&cmp[i].accel, "");
        }
    }

    spdlog::debug("[InputShaper] Populated {} axis comparison table with {} shapers", axis,
                  result.all_shapers.size());
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void InputShaperPanel::handle_calibrate_all_clicked() {
    if (state_ != State::IDLE)
        return;
    spdlog::debug("[InputShaper] Calibrate All clicked");
    calibrate_all();
}

void InputShaperPanel::handle_calibrate_x_clicked() {
    if (state_ != State::IDLE)
        return;
    spdlog::debug("[InputShaper] Calibrate X clicked");
    calibrate_all_mode_ = false;
    start_with_preflight('X');
}

void InputShaperPanel::handle_calibrate_y_clicked() {
    if (state_ != State::IDLE)
        return;
    spdlog::debug("[InputShaper] Calibrate Y clicked");
    calibrate_all_mode_ = false;
    start_with_preflight('Y');
}

void InputShaperPanel::handle_measure_noise_clicked() {
    if (state_ != State::IDLE) {
        return;
    }
    spdlog::debug("[InputShaper] Measure Noise clicked");
    measure_noise();
}

void InputShaperPanel::handle_cancel_clicked() {
    spdlog::debug("[InputShaper] Cancel clicked");
    cancel_calibration();
}

void InputShaperPanel::handle_apply_clicked() {
    spdlog::debug("[InputShaper] Apply clicked");
    apply_recommendation();
}

void InputShaperPanel::handle_close_clicked() {
    spdlog::debug("[InputShaper] Close clicked");
    clear_results();
    set_state(State::IDLE);
    ui_nav_go_back();
}

void InputShaperPanel::handle_retry_clicked() {
    spdlog::debug("[InputShaper] Retry clicked");
    calibrate_all_mode_ = false;
    start_with_preflight(current_axis_);
}

void InputShaperPanel::handle_save_config_clicked() {
    spdlog::debug("[InputShaper] Save Config clicked");
    save_configuration();
}

void InputShaperPanel::handle_save_clicked() {
    spdlog::debug("[InputShaper] Save clicked — applying and saving to config");
    apply_recommendation();
    save_configuration();
}

void InputShaperPanel::handle_print_test_pattern_clicked() {
    if (!api_) {
        spdlog::warn("[InputShaper] Cannot print test: API not set");
        return;
    }

    // TUNING_TOWER enables acceleration ramping during print
    // This allows user to visually compare ringing at different accelerations
    const std::string tuning_tower_cmd =
        "TUNING_TOWER COMMAND=SET_VELOCITY_LIMIT PARAMETER=ACCEL START=1500 FACTOR=500 BAND=5";

    spdlog::info("[InputShaper] Enabling tuning tower for test print");

    std::weak_ptr<std::atomic<bool>> alive_weak = alive_;

    api_->execute_gcode(
        tuning_tower_cmd,
        [alive_weak]() {
            if (auto alive = alive_weak.lock()) {
                if (*alive) {
                    spdlog::info(
                        "[InputShaper] Tuning tower enabled - start a print to test calibration");
                    ui_toast_show(ToastSeverity::INFO,
                                  lv_tr("Tuning tower enabled - start a print to test"), 3000);
                }
            }
        },
        [alive_weak](const MoonrakerError& err) {
            if (auto alive = alive_weak.lock()) {
                if (*alive) {
                    spdlog::error("[InputShaper] Failed to enable tuning tower: {}", err.message);
                    ui_toast_show(ToastSeverity::ERROR, lv_tr("Failed to enable tuning tower"),
                                  3000);
                }
            }
        });
}

void InputShaperPanel::handle_help_clicked() {
    spdlog::debug("[InputShaper] Help clicked - showing help modal");

    // Detailed help text explaining Input Shaper calibration
    static const char* help_message =
        "Input Shaper reduces ringing and ghosting artifacts caused by "
        "printer vibrations during fast movements.\n\n"

        "REQUIREMENTS:\n"
        "• ADXL345 accelerometer connected to your toolhead\n"
        "• [resonance_tester] section configured in printer.cfg\n"
        "• [input_shaper] section in printer.cfg (can be empty initially)\n\n"

        "HOW TO USE:\n"
        "1. Tap 'Measure Noise' first to verify accelerometer is working\n"
        "2. Tap 'Calibrate X' to measure X-axis resonance (~1-2 min)\n"
        "3. Tap 'Calibrate Y' to measure Y-axis resonance (~1-2 min)\n"
        "4. Review results and tap 'Apply' to use recommended settings\n"
        "5. Optionally 'Save Config' to make permanent (restarts Klipper)\n\n"

        "SHAPER TYPES:\n"
        "• ZV - Lowest smoothing, good for low vibration printers\n"
        "• MZV - Balanced choice, recommended for most printers\n"
        "• EI - More aggressive, better vibration reduction\n"
        "• 2HUMP_EI / 3HUMP_EI - Maximum reduction, more smoothing\n\n"

        "Lower vibration % is better. Lower smoothing preserves detail.";

    const char* attrs[] = {"title", "Input Shaper Help", "message", help_message, nullptr};

    ui_modal_configure(ModalSeverity::Info, false, "Got It", nullptr);
    lv_obj_t* help_dialog = ui_modal_show("modal_dialog", attrs);

    if (!help_dialog) {
        spdlog::error("[InputShaper] Failed to show help modal");
        return;
    }

    // Wire up Ok button to close
    lv_obj_t* ok_btn = lv_obj_find_by_name(help_dialog, "btn_primary");
    if (ok_btn) {
        lv_obj_set_user_data(ok_btn, help_dialog);
        lv_obj_add_event_cb(
            ok_btn,
            [](lv_event_t* e) {
                auto* btn = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
                auto* dialog = static_cast<lv_obj_t*>(lv_obj_get_user_data(btn));
                if (dialog) {
                    ui_modal_hide(dialog);
                }
            },
            LV_EVENT_CLICKED, nullptr);
    }
}
