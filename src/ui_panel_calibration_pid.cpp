// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_calibration_pid.h"

#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_theme.h"

#include "moonraker_client.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <memory>

// ============================================================================
// SETUP
// ============================================================================

void PIDCalibrationPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen, MoonrakerClient* client) {
    panel_ = panel;
    parent_screen_ = parent_screen;
    client_ = client;

    if (!panel_) {
        spdlog::error("[PIDCal] NULL panel");
        return;
    }

    // Find state views
    state_idle_ = lv_obj_find_by_name(panel_, "state_idle");
    state_calibrating_ = lv_obj_find_by_name(panel_, "state_calibrating");
    state_saving_ = lv_obj_find_by_name(panel_, "state_saving");
    state_complete_ = lv_obj_find_by_name(panel_, "state_complete");
    state_error_ = lv_obj_find_by_name(panel_, "state_error");

    // Find widgets in idle state
    btn_heater_extruder_ = lv_obj_find_by_name(panel_, "btn_heater_extruder");
    btn_heater_bed_ = lv_obj_find_by_name(panel_, "btn_heater_bed");
    temp_display_ = lv_obj_find_by_name(panel_, "temp_display");
    temp_hint_ = lv_obj_find_by_name(panel_, "temp_hint");

    // Find widgets in calibrating state
    calibrating_heater_ = lv_obj_find_by_name(panel_, "calibrating_heater");
    current_temp_display_ = lv_obj_find_by_name(panel_, "current_temp_display");

    // Find widgets in complete state
    pid_kp_ = lv_obj_find_by_name(panel_, "pid_kp");
    pid_ki_ = lv_obj_find_by_name(panel_, "pid_ki");
    pid_kd_ = lv_obj_find_by_name(panel_, "pid_kd");

    // Find error message label
    error_message_ = lv_obj_find_by_name(panel_, "error_message");

    // Wire up button handlers
    if (btn_heater_extruder_) {
        lv_obj_add_event_cb(btn_heater_extruder_, on_heater_extruder_clicked, LV_EVENT_CLICKED,
                            this);
    }
    if (btn_heater_bed_) {
        lv_obj_add_event_cb(btn_heater_bed_, on_heater_bed_clicked, LV_EVENT_CLICKED, this);
    }

    lv_obj_t* btn = nullptr;
    btn = lv_obj_find_by_name(panel_, "btn_temp_up");
    if (btn)
        lv_obj_add_event_cb(btn, on_temp_up, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_temp_down");
    if (btn)
        lv_obj_add_event_cb(btn, on_temp_down, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_start");
    if (btn)
        lv_obj_add_event_cb(btn, on_start_clicked, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_abort");
    if (btn)
        lv_obj_add_event_cb(btn, on_abort_clicked, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_done");
    if (btn)
        lv_obj_add_event_cb(btn, on_done_clicked, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_close_error");
    if (btn)
        lv_obj_add_event_cb(btn, on_done_clicked, LV_EVENT_CLICKED, this);
    btn = lv_obj_find_by_name(panel_, "btn_retry");
    if (btn)
        lv_obj_add_event_cb(btn, on_retry_clicked, LV_EVENT_CLICKED, this);

    // Set initial state
    set_state(State::IDLE);
    update_heater_selection();
    update_temp_display();
    update_temp_hint();

    spdlog::info("[PIDCal] Setup complete");
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void PIDCalibrationPanel::set_state(State new_state) {
    spdlog::debug("[PIDCal] State change: {} -> {}", static_cast<int>(state_),
                  static_cast<int>(new_state));
    state_ = new_state;
    show_state_view(new_state);
}

void PIDCalibrationPanel::show_state_view(State state) {
    // Hide all state views
    if (state_idle_)
        lv_obj_add_flag(state_idle_, LV_OBJ_FLAG_HIDDEN);
    if (state_calibrating_)
        lv_obj_add_flag(state_calibrating_, LV_OBJ_FLAG_HIDDEN);
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
    case State::CALIBRATING:
        view = state_calibrating_;
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
// UI UPDATES
// ============================================================================

void PIDCalibrationPanel::update_heater_selection() {
    if (!btn_heater_extruder_ || !btn_heater_bed_)
        return;

    // Use background color to indicate selection
    lv_color_t selected_color = ui_theme_get_color("primary_color");
    lv_color_t neutral_color = ui_theme_get_color("theme_grey");

    if (selected_heater_ == Heater::EXTRUDER) {
        lv_obj_set_style_bg_color(btn_heater_extruder_, selected_color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_heater_bed_, neutral_color, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(btn_heater_extruder_, neutral_color, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_heater_bed_, selected_color, LV_PART_MAIN);
    }
}

void PIDCalibrationPanel::update_temp_display() {
    if (!temp_display_)
        return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d°C", target_temp_);
    lv_label_set_text(temp_display_, buf);
}

void PIDCalibrationPanel::update_temp_hint() {
    if (!temp_hint_)
        return;

    if (selected_heater_ == Heater::EXTRUDER) {
        lv_label_set_text(temp_hint_, "Recommended: 200°C for extruder");
    } else {
        lv_label_set_text(temp_hint_, "Recommended: 60°C for heated bed");
    }
}

void PIDCalibrationPanel::update_temperature(float current, float target) {
    if (!current_temp_display_)
        return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f°C / %.0f°C", current, target);
    lv_label_set_text(current_temp_display_, buf);
}

// ============================================================================
// GCODE COMMANDS
// ============================================================================

void PIDCalibrationPanel::send_pid_calibrate() {
    if (!client_) {
        spdlog::error("[PIDCal] No Moonraker client");
        on_calibration_result(false, 0, 0, 0, "No printer connection");
        return;
    }

    const char* heater_name = (selected_heater_ == Heater::EXTRUDER) ? "extruder" : "heater_bed";

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "PID_CALIBRATE HEATER=%s TARGET=%d", heater_name, target_temp_);

    spdlog::info("[PIDCal] Sending: {}", cmd);
    int result = client_->gcode_script(cmd);
    if (result <= 0) {
        spdlog::error("[PIDCal] Failed to send PID_CALIBRATE");
        on_calibration_result(false, 0, 0, 0, "Failed to start calibration");
    }

    // Update calibrating state label
    if (calibrating_heater_) {
        const char* label = (selected_heater_ == Heater::EXTRUDER) ? "Extruder PID Tuning"
                                                                   : "Heated Bed PID Tuning";
        lv_label_set_text(calibrating_heater_, label);
    }

    // For demo purposes, simulate completion after a delay
    // In real implementation, this would be triggered by Moonraker events
    lv_timer_t* timer = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<PIDCalibrationPanel*>(lv_timer_get_user_data(t));
            if (self && self->get_state() == State::CALIBRATING) {
                // Simulate successful calibration with typical values
                self->on_calibration_result(true, 22.865f, 1.292f, 101.178f);
            }
            lv_timer_delete(t);
        },
        5000, this); // 5 second delay to simulate calibration
    lv_timer_set_repeat_count(timer, 1);
}

void PIDCalibrationPanel::send_save_config() {
    if (!client_)
        return;

    spdlog::info("[PIDCal] Sending SAVE_CONFIG");
    int result = client_->gcode_script("SAVE_CONFIG");
    if (result <= 0) {
        spdlog::error("[PIDCal] Failed to send SAVE_CONFIG");
        on_calibration_result(false, 0, 0, 0, "Failed to save configuration");
        return;
    }

    // Simulate save completing
    lv_timer_t* timer = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<PIDCalibrationPanel*>(lv_timer_get_user_data(t));
            if (self && self->get_state() == State::SAVING) {
                self->set_state(State::COMPLETE);
            }
            lv_timer_delete(t);
        },
        2000, this);
    lv_timer_set_repeat_count(timer, 1);
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void PIDCalibrationPanel::handle_heater_extruder_clicked() {
    if (state_ != State::IDLE)
        return;

    spdlog::debug("[PIDCal] Extruder selected");
    selected_heater_ = Heater::EXTRUDER;
    target_temp_ = EXTRUDER_DEFAULT_TEMP;
    update_heater_selection();
    update_temp_display();
    update_temp_hint();
}

void PIDCalibrationPanel::handle_heater_bed_clicked() {
    if (state_ != State::IDLE)
        return;

    spdlog::debug("[PIDCal] Heated bed selected");
    selected_heater_ = Heater::BED;
    target_temp_ = BED_DEFAULT_TEMP;
    update_heater_selection();
    update_temp_display();
    update_temp_hint();
}

void PIDCalibrationPanel::handle_temp_up() {
    if (state_ != State::IDLE)
        return;

    int max_temp = (selected_heater_ == Heater::EXTRUDER) ? EXTRUDER_MAX_TEMP : BED_MAX_TEMP;

    if (target_temp_ < max_temp) {
        target_temp_ += 5;
        update_temp_display();
    }
}

void PIDCalibrationPanel::handle_temp_down() {
    if (state_ != State::IDLE)
        return;

    int min_temp = (selected_heater_ == Heater::EXTRUDER) ? EXTRUDER_MIN_TEMP : BED_MIN_TEMP;

    if (target_temp_ > min_temp) {
        target_temp_ -= 5;
        update_temp_display();
    }
}

void PIDCalibrationPanel::handle_start_clicked() {
    spdlog::debug("[PIDCal] Start clicked");
    set_state(State::CALIBRATING);
    send_pid_calibrate();
}

void PIDCalibrationPanel::handle_abort_clicked() {
    spdlog::debug("[PIDCal] Abort clicked");
    // Send TURN_OFF_HEATERS to abort
    if (client_) {
        client_->gcode_script("TURN_OFF_HEATERS");
    }
    set_state(State::IDLE);
}

void PIDCalibrationPanel::handle_done_clicked() {
    spdlog::debug("[PIDCal] Done clicked");
    set_state(State::IDLE);
    ui_nav_go_back();
}

void PIDCalibrationPanel::handle_retry_clicked() {
    spdlog::debug("[PIDCal] Retry clicked");
    set_state(State::IDLE);
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

void PIDCalibrationPanel::on_calibration_result(bool success, float kp, float ki, float kd,
                                                const std::string& error_message) {
    if (success) {
        // Store results
        result_kp_ = kp;
        result_ki_ = ki;
        result_kd_ = kd;

        // Update display
        if (pid_kp_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.3f", kp);
            lv_label_set_text(pid_kp_, buf);
        }
        if (pid_ki_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.3f", ki);
            lv_label_set_text(pid_ki_, buf);
        }
        if (pid_kd_) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.3f", kd);
            lv_label_set_text(pid_kd_, buf);
        }

        // Save config (will transition to COMPLETE when done)
        set_state(State::SAVING);
        send_save_config();
    } else {
        if (error_message_) {
            lv_label_set_text(error_message_, error_message.c_str());
        }
        set_state(State::ERROR);
    }
}

// ============================================================================
// STATIC TRAMPOLINES
// ============================================================================

void PIDCalibrationPanel::on_heater_extruder_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_heater_extruder_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_heater_extruder_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_heater_bed_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_heater_bed_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_heater_bed_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_temp_up(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_temp_up");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_temp_up();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_temp_down(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_temp_down");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_temp_down();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_start_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_start_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_start_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_abort_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_abort_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_abort_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_done_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_done_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_done_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_retry_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_retry_clicked");
    auto* self = static_cast<PIDCalibrationPanel*>(lv_event_get_user_data(e));
    if (self)
        self->handle_retry_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

static std::unique_ptr<PIDCalibrationPanel> g_pid_cal_panel;

PIDCalibrationPanel& get_global_pid_cal_panel() {
    if (!g_pid_cal_panel) {
        g_pid_cal_panel = std::make_unique<PIDCalibrationPanel>();
    }
    return *g_pid_cal_panel;
}
