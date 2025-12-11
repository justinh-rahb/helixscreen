// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_calibration_zoffset.h"

#include "ui_event_safety.h"
#include "ui_nav.h"

#include "app_globals.h"
#include "moonraker_client.h"
#include "printer_state.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <memory>

// ============================================================================
// LIFECYCLE
// ============================================================================

ZOffsetCalibrationPanel::~ZOffsetCalibrationPanel() {
    // Remove observers to prevent use-after-free if subjects outlive us
    if (manual_probe_active_observer_) {
        lv_observer_remove(manual_probe_active_observer_);
        manual_probe_active_observer_ = nullptr;
    }
    if (manual_probe_z_observer_) {
        lv_observer_remove(manual_probe_z_observer_);
        manual_probe_z_observer_ = nullptr;
    }
    spdlog::debug("[ZOffsetCal] Destroyed, observers cleaned up");
}

// ============================================================================
// SETUP
// ============================================================================

void ZOffsetCalibrationPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen,
                                    MoonrakerClient* client) {
    panel_ = panel;
    parent_screen_ = parent_screen;
    client_ = client;

    if (!panel_) {
        spdlog::error("[ZOffsetCal] NULL panel");
        return;
    }

    // Find state views
    state_idle_ = lv_obj_find_by_name(panel_, "state_idle");
    state_probing_ = lv_obj_find_by_name(panel_, "state_probing");
    state_adjusting_ = lv_obj_find_by_name(panel_, "state_adjusting");
    state_saving_ = lv_obj_find_by_name(panel_, "state_saving");
    state_complete_ = lv_obj_find_by_name(panel_, "state_complete");
    state_error_ = lv_obj_find_by_name(panel_, "state_error");

    // Find display elements
    z_position_display_ = lv_obj_find_by_name(panel_, "z_position_display");
    final_offset_label_ = lv_obj_find_by_name(panel_, "final_offset_label");
    error_message_ = lv_obj_find_by_name(panel_, "error_message");

    // Wire up button handlers
    // Start button
    lv_obj_t* btn_start = lv_obj_find_by_name(panel_, "btn_start");
    if (btn_start) {
        lv_obj_add_event_cb(btn_start, on_start_clicked, LV_EVENT_CLICKED, this);
    }

    // Abort buttons (in probing and adjusting states)
    lv_obj_t* btn_abort_probing = lv_obj_find_by_name(panel_, "btn_abort_probing");
    if (btn_abort_probing) {
        lv_obj_add_event_cb(btn_abort_probing, on_abort_clicked, LV_EVENT_CLICKED, this);
    }
    lv_obj_t* btn_abort = lv_obj_find_by_name(panel_, "btn_abort");
    if (btn_abort) {
        lv_obj_add_event_cb(btn_abort, on_abort_clicked, LV_EVENT_CLICKED, this);
    }

    // Accept button
    lv_obj_t* btn_accept = lv_obj_find_by_name(panel_, "btn_accept");
    if (btn_accept) {
        lv_obj_add_event_cb(btn_accept, on_accept_clicked, LV_EVENT_CLICKED, this);
    }

    // Done button
    lv_obj_t* btn_done = lv_obj_find_by_name(panel_, "btn_done");
    if (btn_done) {
        lv_obj_add_event_cb(btn_done, on_done_clicked, LV_EVENT_CLICKED, this);
    }

    // Error state buttons
    lv_obj_t* btn_close_error = lv_obj_find_by_name(panel_, "btn_close_error");
    if (btn_close_error) {
        lv_obj_add_event_cb(btn_close_error, on_done_clicked, LV_EVENT_CLICKED, this);
    }
    lv_obj_t* btn_retry = lv_obj_find_by_name(panel_, "btn_retry");
    if (btn_retry) {
        lv_obj_add_event_cb(btn_retry, on_retry_clicked, LV_EVENT_CLICKED, this);
    }

    // Z adjustment buttons
    lv_obj_t* btn = nullptr;
    btn = lv_obj_find_by_name(panel_, "btn_z_down_1");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_down_1, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_down_01");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_down_01, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_down_005");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_down_005, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_down_001");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_down_001, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_up_001");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_up_001, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_up_005");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_up_005, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_up_01");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_up_01, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_z_up_1");
    if (btn)
        lv_obj_add_event_cb(btn, on_z_up_1, LV_EVENT_CLICKED, this);

    // Set initial state
    set_state(State::IDLE);

    // Subscribe to manual_probe state changes from Klipper
    // This replaces the fake timer with real state tracking
    PrinterState& ps = get_printer_state();

    manual_probe_active_observer_ = lv_subject_add_observer(ps.get_manual_probe_active_subject(),
                                                            on_manual_probe_active_changed, this);

    manual_probe_z_observer_ = lv_subject_add_observer(ps.get_manual_probe_z_position_subject(),
                                                       on_manual_probe_z_changed, this);

    spdlog::info("[ZOffsetCal] Setup complete, observing manual_probe subjects");
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void ZOffsetCalibrationPanel::set_state(State new_state) {
    spdlog::debug("[ZOffsetCal] State change: {} -> {}", static_cast<int>(state_),
                  static_cast<int>(new_state));
    state_ = new_state;
    show_state_view(new_state);
}

void ZOffsetCalibrationPanel::show_state_view(State state) {
    // Hide all state views
    if (state_idle_)
        lv_obj_add_flag(state_idle_, LV_OBJ_FLAG_HIDDEN);
    if (state_probing_)
        lv_obj_add_flag(state_probing_, LV_OBJ_FLAG_HIDDEN);
    if (state_adjusting_)
        lv_obj_add_flag(state_adjusting_, LV_OBJ_FLAG_HIDDEN);
    if (state_saving_)
        lv_obj_add_flag(state_saving_, LV_OBJ_FLAG_HIDDEN);
    if (state_complete_)
        lv_obj_add_flag(state_complete_, LV_OBJ_FLAG_HIDDEN);
    if (state_error_)
        lv_obj_add_flag(state_error_, LV_OBJ_FLAG_HIDDEN);

    // Show the appropriate view
    lv_obj_t* view = nullptr;
    switch (state) {
    case State::IDLE:
        view = state_idle_;
        break;
    case State::PROBING:
        view = state_probing_;
        break;
    case State::ADJUSTING:
        view = state_adjusting_;
        break;
    case State::SAVING:
        view = state_saving_;
        break;
    case State::COMPLETE:
        view = state_complete_;
        break;
    case State::ERROR:
        view = state_error_;
        break;
    }

    if (view) {
        lv_obj_remove_flag(view, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================================
// GCODE COMMANDS
// ============================================================================

void ZOffsetCalibrationPanel::send_probe_calibrate() {
    if (!client_) {
        spdlog::error("[ZOffsetCal] No Moonraker client");
        on_calibration_result(false, "No printer connection");
        return;
    }

    spdlog::info("[ZOffsetCal] Sending PROBE_CALIBRATE");
    int result = client_->gcode_script("PROBE_CALIBRATE");
    if (result <= 0) {
        spdlog::error("[ZOffsetCal] Failed to send PROBE_CALIBRATE");
        on_calibration_result(false, "Failed to start calibration");
    }
    // State transition to ADJUSTING is handled by on_manual_probe_active_changed
    // when Klipper reports manual_probe.is_active=true
}

void ZOffsetCalibrationPanel::send_testz(float delta) {
    if (!client_)
        return;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "TESTZ Z=%.3f", delta);
    spdlog::debug("[ZOffsetCal] Sending: {}", cmd);

    int result = client_->gcode_script(cmd);
    if (result <= 0) {
        spdlog::warn("[ZOffsetCal] Failed to send TESTZ");
    }

    // Z position display is updated by the manual_probe_z_position observer
    // when Klipper reports the new position after processing the TESTZ command
}

void ZOffsetCalibrationPanel::send_accept() {
    if (!client_)
        return;

    spdlog::info("[ZOffsetCal] Sending ACCEPT");
    int result = client_->gcode_script("ACCEPT");
    if (result <= 0) {
        spdlog::error("[ZOffsetCal] Failed to send ACCEPT");
        on_calibration_result(false, "Failed to accept calibration");
        return;
    }

    // Save the final offset
    final_offset_ = current_z_;

    // Now save config (this will restart Klipper)
    set_state(State::SAVING);

    spdlog::info("[ZOffsetCal] Sending SAVE_CONFIG");
    result = client_->gcode_script("SAVE_CONFIG");
    if (result <= 0) {
        spdlog::error("[ZOffsetCal] Failed to send SAVE_CONFIG");
        on_calibration_result(false, "Failed to save configuration");
    }
    // Note: SAVE_CONFIG restarts Klipper, we need to wait for reconnection
    // For now, just show complete state
    on_calibration_result(true, "");
}

void ZOffsetCalibrationPanel::send_abort() {
    if (!client_)
        return;

    spdlog::info("[ZOffsetCal] Sending ABORT");
    client_->gcode_script("ABORT");

    set_state(State::IDLE);
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void ZOffsetCalibrationPanel::handle_start_clicked() {
    spdlog::debug("[ZOffsetCal] Start clicked");
    set_state(State::PROBING);
    send_probe_calibrate();

    // State transition from PROBING -> ADJUSTING is now handled by the
    // manual_probe_active observer when Klipper reports is_active=true
}

void ZOffsetCalibrationPanel::handle_z_adjust(float delta) {
    if (state_ != State::ADJUSTING)
        return;
    send_testz(delta);
}

void ZOffsetCalibrationPanel::handle_accept_clicked() {
    spdlog::debug("[ZOffsetCal] Accept clicked");
    send_accept();
}

void ZOffsetCalibrationPanel::handle_abort_clicked() {
    spdlog::debug("[ZOffsetCal] Abort clicked");
    send_abort();
}

void ZOffsetCalibrationPanel::handle_done_clicked() {
    spdlog::debug("[ZOffsetCal] Done clicked");
    set_state(State::IDLE);
    ui_nav_go_back();
}

void ZOffsetCalibrationPanel::handle_retry_clicked() {
    spdlog::debug("[ZOffsetCal] Retry clicked");
    set_state(State::IDLE);
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void ZOffsetCalibrationPanel::update_z_position(float z_position) {
    current_z_ = z_position;
    if (z_position_display_) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Z: %.3f", z_position);
        lv_label_set_text(z_position_display_, buf);
    }
}

void ZOffsetCalibrationPanel::on_calibration_result(bool success, const std::string& message) {
    if (success) {
        // Update final offset display
        if (final_offset_label_) {
            char buf[64];
            snprintf(buf, sizeof(buf), "New Z-Offset: %.3f", final_offset_);
            lv_label_set_text(final_offset_label_, buf);
        }
        set_state(State::COMPLETE);
    } else {
        if (error_message_) {
            lv_label_set_text(error_message_, message.c_str());
        }
        set_state(State::ERROR);
    }
}

// ============================================================================
// STATIC TRAMPOLINES
// ============================================================================

void ZOffsetCalibrationPanel::on_start_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_start_clicked");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_start_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_down_1(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_down_1");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(-1.0f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_down_01(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_down_01");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(-0.1f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_down_005(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_down_005");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(-0.05f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_down_001(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_down_001");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(-0.01f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_up_001(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_up_001");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(0.01f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_up_005(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_up_005");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(0.05f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_up_01(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_up_01");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(0.1f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_z_up_1(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_z_up_1");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_z_adjust(1.0f);
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_accept_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_accept_clicked");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_accept_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_abort_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_abort_clicked");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_abort_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_done_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_done_clicked");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_done_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void ZOffsetCalibrationPanel::on_retry_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[ZOffsetCal] on_retry_clicked");
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_retry_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// OBSERVER CALLBACKS (for real Klipper manual_probe state)
// ============================================================================

void ZOffsetCalibrationPanel::on_manual_probe_active_changed(lv_observer_t* observer,
                                                             lv_subject_t* subject) {
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_observer_get_user_data(observer));
    if (!self)
        return;

    int is_active = lv_subject_get_int(subject);
    spdlog::debug("[ZOffsetCal] manual_probe_active changed: {}", is_active);

    if (is_active && self->state_ == State::PROBING) {
        // Klipper has entered manual probe mode - transition to ADJUSTING
        spdlog::info("[ZOffsetCal] PROBE_CALIBRATE complete, entering adjustment phase");
        self->set_state(State::ADJUSTING);
    } else if (!is_active && self->state_ == State::ADJUSTING) {
        // Manual probe mode ended externally (G28 from console, printer error, ABORT from macros)
        // The state should already have been changed by button handlers for user-initiated actions,
        // but this catches cases where Klipper ends the session externally
        spdlog::info("[ZOffsetCal] Manual probe ended externally, returning to IDLE");
        self->set_state(State::IDLE);
    }
}

void ZOffsetCalibrationPanel::on_manual_probe_z_changed(lv_observer_t* observer,
                                                        lv_subject_t* subject) {
    auto* self = static_cast<ZOffsetCalibrationPanel*>(lv_observer_get_user_data(observer));
    if (!self)
        return;

    // Only update Z display when in ADJUSTING state
    if (self->state_ != State::ADJUSTING)
        return;

    // Z position is stored in microns (multiply by 0.001 to get mm)
    int z_microns = lv_subject_get_int(subject);
    float z_mm = static_cast<float>(z_microns) * 0.001f;

    spdlog::trace("[ZOffsetCal] Z position from Klipper: {:.3f}mm", z_mm);
    self->update_z_position(z_mm);
}

// ============================================================================
// GLOBAL INSTANCE AND ROW CLICK HANDLER
// ============================================================================

static std::unique_ptr<ZOffsetCalibrationPanel> g_zoffset_cal_panel;
static lv_obj_t* g_zoffset_cal_panel_obj = nullptr;

// Forward declarations
static void on_zoffset_row_clicked(lv_event_t* e);
MoonrakerClient* get_moonraker_client();

ZOffsetCalibrationPanel& get_global_zoffset_cal_panel() {
    if (!g_zoffset_cal_panel) {
        g_zoffset_cal_panel = std::make_unique<ZOffsetCalibrationPanel>();
    }
    return *g_zoffset_cal_panel;
}

void init_zoffset_row_handler() {
    lv_xml_register_event_cb(nullptr, "on_zoffset_row_clicked", on_zoffset_row_clicked);
    spdlog::debug("[ZOffsetCal] Row click callback registered");
}

/**
 * @brief Row click handler for opening Z-Offset calibration from Advanced panel
 *
 * Registered via init_zoffset_row_handler().
 * Lazy-creates the calibration panel on first click.
 */
static void on_zoffset_row_clicked(lv_event_t* e) {
    (void)e;
    spdlog::debug("[ZOffsetCal] Z-Offset row clicked");

    // Lazy-create the Z-Offset calibration panel
    if (!g_zoffset_cal_panel_obj) {
        spdlog::debug("[ZOffsetCal] Creating calibration panel...");
        g_zoffset_cal_panel_obj = static_cast<lv_obj_t*>(lv_xml_create(
            lv_display_get_screen_active(NULL), "calibration_zoffset_panel", nullptr));

        if (g_zoffset_cal_panel_obj) {
            MoonrakerClient* client = get_moonraker_client();
            get_global_zoffset_cal_panel().setup(g_zoffset_cal_panel_obj,
                                                 lv_display_get_screen_active(NULL), client);
            lv_obj_add_flag(g_zoffset_cal_panel_obj, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("[ZOffsetCal] Panel created and setup complete");
        } else {
            spdlog::error("[ZOffsetCal] Failed to create calibration_zoffset_panel");
            return;
        }
    }

    // Show the overlay
    ui_nav_push_overlay(g_zoffset_cal_panel_obj);
}
