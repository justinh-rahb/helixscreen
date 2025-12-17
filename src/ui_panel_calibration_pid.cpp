// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_calibration_pid.h"

#include "ui_event_safety.h"
#include "ui_nav.h"
#include "ui_theme.h"

#include "moonraker_client.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <lvgl.h>
#include <memory>

// ============================================================================
// STATIC SUBJECT
// ============================================================================

// State subject (0=IDLE, 1=CALIBRATING, 2=SAVING, 3=COMPLETE, 4=ERROR)
static lv_subject_t s_pid_cal_state;

// ============================================================================
// SUBJECT REGISTRATION
// ============================================================================

void PIDCalibrationPanel::init_subjects() {
    auto& panel = get_global_pid_cal_panel();

    // Register state subject
    lv_subject_init_int(&s_pid_cal_state, 0);
    lv_xml_register_subject(nullptr, "pid_cal_state", &s_pid_cal_state);

    // Initialize string subjects with initial values
    lv_subject_init_string(&panel.subj_temp_display_, panel.buf_temp_display_, nullptr,
                           sizeof(panel.buf_temp_display_), "200°C");
    lv_xml_register_subject(nullptr, "pid_temp_display", &panel.subj_temp_display_);

    lv_subject_init_string(&panel.subj_temp_hint_, panel.buf_temp_hint_, nullptr,
                           sizeof(panel.buf_temp_hint_), "Recommended: 200°C for extruder");
    lv_xml_register_subject(nullptr, "pid_temp_hint", &panel.subj_temp_hint_);

    lv_subject_init_string(&panel.subj_current_temp_display_, panel.buf_current_temp_display_,
                           nullptr, sizeof(panel.buf_current_temp_display_), "0.0°C / 0°C");
    lv_xml_register_subject(nullptr, "pid_current_temp", &panel.subj_current_temp_display_);

    lv_subject_init_string(&panel.subj_calibrating_heater_, panel.buf_calibrating_heater_, nullptr,
                           sizeof(panel.buf_calibrating_heater_), "Extruder PID Tuning");
    lv_xml_register_subject(nullptr, "pid_calibrating_heater", &panel.subj_calibrating_heater_);

    lv_subject_init_string(&panel.subj_pid_kp_, panel.buf_pid_kp_, nullptr,
                           sizeof(panel.buf_pid_kp_), "0.000");
    lv_xml_register_subject(nullptr, "pid_kp", &panel.subj_pid_kp_);

    lv_subject_init_string(&panel.subj_pid_ki_, panel.buf_pid_ki_, nullptr,
                           sizeof(panel.buf_pid_ki_), "0.000");
    lv_xml_register_subject(nullptr, "pid_ki", &panel.subj_pid_ki_);

    lv_subject_init_string(&panel.subj_pid_kd_, panel.buf_pid_kd_, nullptr,
                           sizeof(panel.buf_pid_kd_), "0.000");
    lv_xml_register_subject(nullptr, "pid_kd", &panel.subj_pid_kd_);

    lv_subject_init_string(&panel.subj_error_message_, panel.buf_error_message_, nullptr,
                           sizeof(panel.buf_error_message_),
                           "An error occurred during calibration.");
    lv_xml_register_subject(nullptr, "pid_error_message", &panel.subj_error_message_);

    // Register XML event callbacks using global accessor
    lv_xml_register_event_cb(nullptr, "on_pid_heater_extruder", on_heater_extruder_clicked);
    lv_xml_register_event_cb(nullptr, "on_pid_heater_bed", on_heater_bed_clicked);
    lv_xml_register_event_cb(nullptr, "on_pid_temp_up", on_temp_up);
    lv_xml_register_event_cb(nullptr, "on_pid_temp_down", on_temp_down);
    lv_xml_register_event_cb(nullptr, "on_pid_start", on_start_clicked);
    lv_xml_register_event_cb(nullptr, "on_pid_abort", on_abort_clicked);
    lv_xml_register_event_cb(nullptr, "on_pid_done", on_done_clicked);
    lv_xml_register_event_cb(nullptr, "on_pid_retry", on_retry_clicked);

    spdlog::debug("[PIDCal] Subjects and callbacks registered");
}

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

    // Find widgets in idle state (for heater selection styling)
    btn_heater_extruder_ = lv_obj_find_by_name(panel_, "btn_heater_extruder");
    btn_heater_bed_ = lv_obj_find_by_name(panel_, "btn_heater_bed");

    // Event callbacks are registered via XML <event_cb> elements
    // State visibility is controlled via subject binding in XML

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

    // Update subject - XML bindings handle visibility automatically
    // State mapping: 0=IDLE, 1=CALIBRATING, 2=SAVING, 3=COMPLETE, 4=ERROR
    lv_subject_set_int(&s_pid_cal_state, static_cast<int>(new_state));
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
    char buf[16];
    snprintf(buf, sizeof(buf), "%d°C", target_temp_);
    lv_subject_copy_string(&subj_temp_display_, buf);
}

void PIDCalibrationPanel::update_temp_hint() {
    const char* hint = (selected_heater_ == Heater::EXTRUDER) ? "Recommended: 200°C for extruder"
                                                              : "Recommended: 60°C for heated bed";
    lv_subject_copy_string(&subj_temp_hint_, hint);
}

void PIDCalibrationPanel::update_temperature(float current, float target) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f°C / %.0f°C", current, target);
    lv_subject_copy_string(&subj_current_temp_display_, buf);
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
    const char* label =
        (selected_heater_ == Heater::EXTRUDER) ? "Extruder PID Tuning" : "Heated Bed PID Tuning";
    lv_subject_copy_string(&subj_calibrating_heater_, label);

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

        // Update display using subjects
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", kp);
        lv_subject_copy_string(&subj_pid_kp_, buf);

        snprintf(buf, sizeof(buf), "%.3f", ki);
        lv_subject_copy_string(&subj_pid_ki_, buf);

        snprintf(buf, sizeof(buf), "%.3f", kd);
        lv_subject_copy_string(&subj_pid_kd_, buf);

        // Save config (will transition to COMPLETE when done)
        set_state(State::SAVING);
        send_save_config();
    } else {
        lv_subject_copy_string(&subj_error_message_, error_message.c_str());
        set_state(State::ERROR);
    }
}

// ============================================================================
// STATIC TRAMPOLINES (for XML event_cb)
// ============================================================================

void PIDCalibrationPanel::on_heater_extruder_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_heater_extruder_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_heater_extruder_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_heater_bed_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_heater_bed_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_heater_bed_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_temp_up(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_temp_up");
    (void)e;
    get_global_pid_cal_panel().handle_temp_up();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_temp_down(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_temp_down");
    (void)e;
    get_global_pid_cal_panel().handle_temp_down();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_start_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_start_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_start_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_abort_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_abort_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_abort_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_done_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_done_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_done_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void PIDCalibrationPanel::on_retry_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[PIDCal] on_retry_clicked");
    (void)e;
    get_global_pid_cal_panel().handle_retry_clicked();
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
