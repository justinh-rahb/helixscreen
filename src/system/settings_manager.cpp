// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_manager.h"

#include "ui_theme.h"

#include "config.h"
#include "display_manager.h"
#include "moonraker_api.h"
#include "moonraker_client.h"
#include "runtime_config.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>

// Display sleep option values (seconds)
// Index: 0=Never, 1=1min, 2=5min, 3=10min, 4=30min
static const int SLEEP_OPTIONS[] = {0, 60, 300, 600, 1800};
static const int SLEEP_OPTIONS_COUNT = sizeof(SLEEP_OPTIONS) / sizeof(SLEEP_OPTIONS[0]);
static const char* SLEEP_OPTIONS_TEXT = "Never\n1 minute\n5 minutes\n10 minutes\n30 minutes";

// Completion alert options (Off=0, Notification=1, Alert=2)
static const char* COMPLETION_ALERT_OPTIONS_TEXT = "Off\nNotification\nAlert";

// Bed mesh render mode options (Auto=0, 3D=1, 2D=2)
static const char* BED_MESH_RENDER_MODE_OPTIONS_TEXT = "Auto\n3D View\n2D Heatmap";
static const char* GCODE_RENDER_MODE_OPTIONS_TEXT = "Auto\n3D View\n2D Layers";

// Time format options (12H=0, 24H=1)
static const char* TIME_FORMAT_OPTIONS_TEXT = "12 Hour\n24 Hour";

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager() {
    spdlog::trace("[SettingsManager] Constructor");
}

void SettingsManager::init_subjects() {
    if (subjects_initialized_) {
        spdlog::debug("[SettingsManager] Subjects already initialized, skipping");
        return;
    }

    spdlog::debug("[SettingsManager] Initializing subjects");

    // Get initial values from Config
    Config* config = Config::get_instance();

    // Dark mode (default: true = dark)
    bool dark_mode = config->get<bool>("/dark_mode", true);
    UI_MANAGED_SUBJECT_INT(dark_mode_subject_, dark_mode ? 1 : 0, "settings_dark_mode", subjects_);

    // Display sleep (default: 1800 seconds = 30 minutes)
    int sleep_sec = config->get<int>("/display/sleep_sec", 1800);
    UI_MANAGED_SUBJECT_INT(display_sleep_subject_, sleep_sec, "settings_display_sleep", subjects_);

    // Brightness: Read from config (DisplayManager handles hardware)
    int brightness = config->get<int>("/brightness", 50);
    brightness = std::clamp(brightness, 10, 100);
    UI_MANAGED_SUBJECT_INT(brightness_subject_, brightness, "settings_brightness", subjects_);
    spdlog::info("[SettingsManager] Brightness initialized to {}%", brightness);

    // Has backlight control subject (for UI visibility) - check DisplayManager
    bool has_backlight = false;
    if (auto* dm = DisplayManager::instance()) {
        has_backlight = dm->has_backlight_control();
    }
    UI_MANAGED_SUBJECT_INT(has_backlight_subject_, has_backlight ? 1 : 0, "settings_has_backlight",
                           subjects_);

    // LED state (ephemeral, not persisted - start as off)
    UI_MANAGED_SUBJECT_INT(led_enabled_subject_, 0, "settings_led_enabled", subjects_);

    // Sounds (default: true)
    bool sounds = config->get<bool>("/sounds_enabled", true);
    UI_MANAGED_SUBJECT_INT(sounds_enabled_subject_, sounds ? 1 : 0, "settings_sounds_enabled",
                           subjects_);

    // Completion alert mode (default: NOTIFICATION=1, handles old bool migration)
    int completion_mode = config->get<int>("/completion_alert", 1);
    completion_mode = std::max(0, std::min(2, completion_mode));
    UI_MANAGED_SUBJECT_INT(completion_alert_subject_, completion_mode, "settings_completion_alert",
                           subjects_);

    // E-Stop confirmation (default: false = immediate action)
    bool estop_confirm = config->get<bool>("/safety/estop_require_confirmation", false);
    UI_MANAGED_SUBJECT_INT(estop_require_confirmation_subject_, estop_confirm ? 1 : 0,
                           "settings_estop_confirm", subjects_);

    // Scroll throw (default: 25, range 5-50)
    int scroll_throw = config->get<int>("/input/scroll_throw", 25);
    scroll_throw = std::max(5, std::min(50, scroll_throw));
    UI_MANAGED_SUBJECT_INT(scroll_throw_subject_, scroll_throw, "settings_scroll_throw", subjects_);

    // Scroll limit (default: 5, range 1-20)
    int scroll_limit = config->get<int>("/input/scroll_limit", 5);
    scroll_limit = std::max(1, std::min(20, scroll_limit));
    UI_MANAGED_SUBJECT_INT(scroll_limit_subject_, scroll_limit, "settings_scroll_limit", subjects_);

    // Animations enabled (default: true)
    bool animations = config->get<bool>("/animations_enabled", true);
    UI_MANAGED_SUBJECT_INT(animations_enabled_subject_, animations ? 1 : 0,
                           "settings_animations_enabled", subjects_);

    // G-code 3D preview enabled (default: true)
    bool gcode_3d = config->get<bool>("/display/gcode_3d_enabled", true);
    UI_MANAGED_SUBJECT_INT(gcode_3d_enabled_subject_, gcode_3d ? 1 : 0, "settings_gcode_3d_enabled",
                           subjects_);

    // Bed mesh render mode (default: 0 = Auto)
    int bed_mesh_mode = config->get<int>("/display/bed_mesh_render_mode", 0);
    bed_mesh_mode = std::clamp(bed_mesh_mode, 0, 2);
    UI_MANAGED_SUBJECT_INT(bed_mesh_render_mode_subject_, bed_mesh_mode,
                           "settings_bed_mesh_render_mode", subjects_);

    // G-code render mode (default: 0 = Auto)
    int gcode_mode = config->get<int>("/display/gcode_render_mode", 0);
    gcode_mode = std::clamp(gcode_mode, 0, 2);
    UI_MANAGED_SUBJECT_INT(gcode_render_mode_subject_, gcode_mode, "settings_gcode_render_mode",
                           subjects_);

    // Time format (default: 0 = 12-hour)
    int time_format = config->get<int>("/time_format", 0);
    time_format = std::clamp(time_format, 0, 1);
    UI_MANAGED_SUBJECT_INT(time_format_subject_, time_format, "settings_time_format", subjects_);

    subjects_initialized_ = true;
    spdlog::info("[SettingsManager] Subjects initialized: dark_mode={}, sleep={}s, sounds={}, "
                 "completion_alert_mode={}, scroll_throw={}, scroll_limit={}, animations={}",
                 dark_mode, sleep_sec, sounds, completion_mode, scroll_throw, scroll_limit,
                 animations);
}

void SettingsManager::deinit_subjects() {
    if (!subjects_initialized_) {
        return;
    }

    spdlog::debug("[SettingsManager] Deinitializing subjects");

    // Use SubjectManager for RAII cleanup of all registered subjects
    subjects_.deinit_all();

    subjects_initialized_ = false;
    spdlog::debug("[SettingsManager] Subjects deinitialized");
}

void SettingsManager::set_moonraker_client(MoonrakerClient* client) {
    moonraker_client_ = client;
    spdlog::debug("[SettingsManager] Moonraker client set: {}", client ? "connected" : "nullptr");
}

void SettingsManager::set_moonraker_api(MoonrakerAPI* api) {
    moonraker_api_ = api;
    spdlog::debug("[SettingsManager] MoonrakerAPI set: {}", api ? "connected" : "nullptr");
}

void SettingsManager::set_configured_led(const std::string& led) {
    configured_led_ = led;
    spdlog::debug("[SettingsManager] Configured LED set: {}", led.empty() ? "(none)" : led);
}

// =============================================================================
// APPEARANCE SETTINGS
// =============================================================================

bool SettingsManager::get_dark_mode() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&dark_mode_subject_)) != 0;
}

void SettingsManager::set_dark_mode(bool enabled) {
    spdlog::info("[SettingsManager] set_dark_mode({})", enabled);

    // 1. Update subject (UI reacts immediately via binding)
    lv_subject_set_int(&dark_mode_subject_, enabled ? 1 : 0);

    // 2. Persist to config (theme change requires restart to take effect)
    Config* config = Config::get_instance();
    config->set<bool>("/dark_mode", enabled);
    config->save();

    spdlog::debug("[SettingsManager] Dark mode {} saved (restart required)",
                  enabled ? "enabled" : "disabled");
}

int SettingsManager::get_display_sleep_sec() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&display_sleep_subject_));
}

void SettingsManager::set_display_sleep_sec(int seconds) {
    spdlog::info("[SettingsManager] set_display_sleep_sec({})", seconds);

    // 1. Update subject
    lv_subject_set_int(&display_sleep_subject_, seconds);

    // 2. Persist
    Config* config = Config::get_instance();
    config->set<int>("/display/sleep_sec", seconds);
    config->save();

    // Note: Actual display sleep is handled by the display driver reading this value
    spdlog::debug("[SettingsManager] Display sleep set to {}s", seconds);
}

int SettingsManager::get_brightness() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&brightness_subject_));
}

void SettingsManager::set_brightness(int percent) {
    // Clamp to valid range (10-100, minimum 10% to prevent black screen)
    int clamped = std::clamp(percent, 10, 100);
    spdlog::info("[SettingsManager] set_brightness({})", clamped);

    // 1. Update subject (UI reflects change immediately)
    lv_subject_set_int(&brightness_subject_, clamped);

    // 2. Apply to hardware via DisplayManager
    if (auto* dm = DisplayManager::instance()) {
        dm->set_backlight_brightness(clamped);
    }

    // 3. Persist to config
    Config* config = Config::get_instance();
    config->set<int>("/brightness", clamped);
    config->save();
}

bool SettingsManager::has_backlight_control() const {
    if (auto* dm = DisplayManager::instance()) {
        return dm->has_backlight_control();
    }
    return false;
}

bool SettingsManager::get_animations_enabled() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&animations_enabled_subject_)) != 0;
}

void SettingsManager::set_animations_enabled(bool enabled) {
    spdlog::info("[SettingsManager] set_animations_enabled({})", enabled);

    // 1. Update subject (UI reacts, animations check this dynamically)
    lv_subject_set_int(&animations_enabled_subject_, enabled ? 1 : 0);

    // 2. Persist to config
    Config* config = Config::get_instance();
    config->set<bool>("/animations_enabled", enabled);
    config->save();
}

bool SettingsManager::get_gcode_3d_enabled() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&gcode_3d_enabled_subject_)) != 0;
}

void SettingsManager::set_gcode_3d_enabled(bool enabled) {
    spdlog::info("[SettingsManager] set_gcode_3d_enabled({})", enabled);

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&gcode_3d_enabled_subject_, enabled ? 1 : 0);

    // 2. Persist to config
    Config* config = Config::get_instance();
    config->set<bool>("/display/gcode_3d_enabled", enabled);
    config->save();
}

int SettingsManager::get_bed_mesh_render_mode() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&bed_mesh_render_mode_subject_));
}

void SettingsManager::set_bed_mesh_render_mode(int mode) {
    // Clamp to valid range (0=Auto, 1=3D, 2=2D)
    int clamped = std::clamp(mode, 0, 2);
    spdlog::info("[SettingsManager] set_bed_mesh_render_mode({})", clamped);

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&bed_mesh_render_mode_subject_, clamped);

    // 2. Persist to config
    Config* config = Config::get_instance();
    config->set<int>("/display/bed_mesh_render_mode", clamped);
    config->save();

    spdlog::debug("[SettingsManager] Bed mesh render mode set to {} ({})", clamped,
                  clamped == 0 ? "Auto" : (clamped == 1 ? "3D" : "2D"));
}

const char* SettingsManager::get_bed_mesh_render_mode_options() {
    return BED_MESH_RENDER_MODE_OPTIONS_TEXT;
}

int SettingsManager::get_gcode_render_mode() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&gcode_render_mode_subject_));
}

void SettingsManager::set_gcode_render_mode(int mode) {
    // Clamp to valid range (0=Auto, 1=3D, 2=2D)
    int clamped = std::clamp(mode, 0, 2);
    spdlog::info("[SettingsManager] set_gcode_render_mode({})", clamped);

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&gcode_render_mode_subject_, clamped);

    // 2. Persist to config
    Config* config = Config::get_instance();
    config->set<int>("/display/gcode_render_mode", clamped);
    config->save();

    spdlog::debug("[SettingsManager] G-code render mode set to {} ({})", clamped,
                  clamped == 0 ? "Auto" : (clamped == 1 ? "3D" : "2D"));
}

const char* SettingsManager::get_gcode_render_mode_options() {
    return GCODE_RENDER_MODE_OPTIONS_TEXT;
}

TimeFormat SettingsManager::get_time_format() const {
    int val = lv_subject_get_int(const_cast<lv_subject_t*>(&time_format_subject_));
    return static_cast<TimeFormat>(std::clamp(val, 0, 1));
}

void SettingsManager::set_time_format(TimeFormat format) {
    int val = static_cast<int>(format);
    spdlog::info("[SettingsManager] set_time_format({})", val == 0 ? "12H" : "24H");

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&time_format_subject_, val);

    // 2. Persist to config
    Config* config = Config::get_instance();
    config->set<int>("/time_format", val);
    config->save();
}

const char* SettingsManager::get_time_format_options() {
    return TIME_FORMAT_OPTIONS_TEXT;
}

// =============================================================================
// PRINTER SETTINGS
// =============================================================================

bool SettingsManager::get_led_enabled() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&led_enabled_subject_)) != 0;
}

void SettingsManager::set_led_enabled(bool enabled) {
    spdlog::info("[SettingsManager] set_led_enabled({})", enabled);

    // Guard: Don't update local subject if command can't be sent
    // This prevents UI/printer state divergence
    if (!moonraker_api_ || configured_led_.empty()) {
        spdlog::warn("[SettingsManager] LED toggle ignored - {}",
                     !moonraker_api_ ? "no API" : "no LED configured");
        return;
    }

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&led_enabled_subject_, enabled ? 1 : 0);

    // 2. Send command to printer
    send_led_command(enabled);

    // Note: LED state is NOT persisted - it's ephemeral
}

void SettingsManager::send_led_command(bool enabled) {
    if (!moonraker_api_) {
        spdlog::warn("[SettingsManager] Cannot send LED command - no MoonrakerAPI");
        return;
    }

    if (configured_led_.empty()) {
        spdlog::warn("[SettingsManager] Cannot send LED command - no LED configured");
        return;
    }

    auto on_success = []() { spdlog::debug("[SettingsManager] LED command sent successfully"); };

    auto on_error = [](const MoonrakerError& err) {
        spdlog::error("[SettingsManager] LED command failed: {}", err.message);
    };

    if (enabled) {
        moonraker_api_->set_led_on(configured_led_, on_success, on_error);
    } else {
        moonraker_api_->set_led_off(configured_led_, on_success, on_error);
    }
}

// =============================================================================
// NOTIFICATION SETTINGS
// =============================================================================

bool SettingsManager::get_sounds_enabled() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&sounds_enabled_subject_)) != 0;
}

void SettingsManager::set_sounds_enabled(bool enabled) {
    spdlog::info("[SettingsManager] set_sounds_enabled({})", enabled);

    lv_subject_set_int(&sounds_enabled_subject_, enabled ? 1 : 0);

    Config* config = Config::get_instance();
    config->set<bool>("/sounds_enabled", enabled);
    config->save();

    // Note: Actual sound playback is a placeholder - hardware TBD
}

CompletionAlertMode SettingsManager::get_completion_alert_mode() const {
    int val = lv_subject_get_int(const_cast<lv_subject_t*>(&completion_alert_subject_));
    return static_cast<CompletionAlertMode>(std::max(0, std::min(2, val)));
}

void SettingsManager::set_completion_alert_mode(CompletionAlertMode mode) {
    int val = static_cast<int>(mode);
    spdlog::info("[SettingsManager] set_completion_alert_mode({})", val);
    lv_subject_set_int(&completion_alert_subject_, val);
    Config* config = Config::get_instance();
    config->set<int>("/completion_alert", val);
    config->save();
}

const char* SettingsManager::get_completion_alert_options() {
    return COMPLETION_ALERT_OPTIONS_TEXT;
}

// =============================================================================
// DISPLAY SLEEP OPTIONS
// =============================================================================

const char* SettingsManager::get_display_sleep_options() {
    return SLEEP_OPTIONS_TEXT;
}

int SettingsManager::sleep_seconds_to_index(int seconds) {
    for (int i = 0; i < SLEEP_OPTIONS_COUNT; i++) {
        if (SLEEP_OPTIONS[i] == seconds) {
            return i;
        }
    }
    // Default to "10 minutes" if not found
    return 3;
}

int SettingsManager::index_to_sleep_seconds(int index) {
    if (index >= 0 && index < SLEEP_OPTIONS_COUNT) {
        return SLEEP_OPTIONS[index];
    }
    return 600; // Default 10 minutes
}

// =============================================================================
// INPUT SETTINGS (require restart)
// =============================================================================

int SettingsManager::get_scroll_throw() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&scroll_throw_subject_));
}

void SettingsManager::set_scroll_throw(int value) {
    // Clamp to valid range (5-50)
    int clamped = std::max(5, std::min(50, value));
    spdlog::info("[SettingsManager] set_scroll_throw({})", clamped);

    // 1. Update subject
    lv_subject_set_int(&scroll_throw_subject_, clamped);

    // 2. Persist
    Config* config = Config::get_instance();
    config->set<int>("/input/scroll_throw", clamped);
    config->save();

    // 3. Mark restart needed (this setting only takes effect on startup)
    restart_pending_ = true;
    spdlog::debug("[SettingsManager] Scroll throw set to {} (restart required)", clamped);
}

int SettingsManager::get_scroll_limit() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&scroll_limit_subject_));
}

void SettingsManager::set_scroll_limit(int value) {
    // Clamp to valid range (1-20)
    int clamped = std::max(1, std::min(20, value));
    spdlog::info("[SettingsManager] set_scroll_limit({})", clamped);

    // 1. Update subject
    lv_subject_set_int(&scroll_limit_subject_, clamped);

    // 2. Persist
    Config* config = Config::get_instance();
    config->set<int>("/input/scroll_limit", clamped);
    config->save();

    // 3. Mark restart needed (this setting only takes effect on startup)
    restart_pending_ = true;
    spdlog::debug("[SettingsManager] Scroll limit set to {} (restart required)", clamped);
}

// =============================================================================
// SAFETY SETTINGS
// =============================================================================

bool SettingsManager::get_estop_require_confirmation() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&estop_require_confirmation_subject_)) != 0;
}

void SettingsManager::set_estop_require_confirmation(bool require) {
    spdlog::info("[SettingsManager] set_estop_require_confirmation({})", require);

    // 1. Update subject (UI reacts)
    lv_subject_set_int(&estop_require_confirmation_subject_, require ? 1 : 0);

    // 2. Persist
    Config* config = Config::get_instance();
    config->set<bool>("/safety/estop_require_confirmation", require);
    config->save();

    spdlog::debug("[SettingsManager] E-Stop confirmation {} and saved",
                  require ? "enabled" : "disabled");
}
