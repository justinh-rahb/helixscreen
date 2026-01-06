// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_wizard_summary.h"

#include "ui_subject_registry.h"
#include "ui_wizard.h"

#include "ams_backend.h"
#include "ams_state.h"
#include "app_globals.h"
#include "config.h"
#include "filament_sensor_manager.h"
#include "lvgl/lvgl.h"
#include "static_panel_registry.h"
#include "wizard_config_paths.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <memory>
#include <sstream>
#include <string>

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<WizardSummaryStep> g_wizard_summary_step;

WizardSummaryStep* get_wizard_summary_step() {
    if (!g_wizard_summary_step) {
        g_wizard_summary_step = std::make_unique<WizardSummaryStep>();
        StaticPanelRegistry::instance().register_destroy("WizardSummaryStep",
                                                         []() { g_wizard_summary_step.reset(); });
    }
    return g_wizard_summary_step.get();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

WizardSummaryStep::WizardSummaryStep() {
    // Zero-initialize buffers
    std::memset(printer_name_buffer_, 0, sizeof(printer_name_buffer_));
    std::memset(printer_type_buffer_, 0, sizeof(printer_type_buffer_));
    std::memset(wifi_ssid_buffer_, 0, sizeof(wifi_ssid_buffer_));
    std::memset(moonraker_connection_buffer_, 0, sizeof(moonraker_connection_buffer_));
    std::memset(bed_buffer_, 0, sizeof(bed_buffer_));
    std::memset(hotend_buffer_, 0, sizeof(hotend_buffer_));
    std::memset(part_fan_buffer_, 0, sizeof(part_fan_buffer_));
    std::memset(hotend_fan_buffer_, 0, sizeof(hotend_fan_buffer_));
    std::memset(led_strip_buffer_, 0, sizeof(led_strip_buffer_));
    std::memset(filament_sensor_buffer_, 0, sizeof(filament_sensor_buffer_));
    std::memset(ams_type_buffer_, 0, sizeof(ams_type_buffer_));

    spdlog::debug("[{}] Instance created", get_name());
}

WizardSummaryStep::~WizardSummaryStep() {
    // NOTE: Do NOT call LVGL functions here - LVGL may be destroyed first
    // NOTE: Do NOT log here - spdlog may be destroyed first
    screen_root_ = nullptr;
}

// ============================================================================
// Move Semantics
// ============================================================================

WizardSummaryStep::WizardSummaryStep(WizardSummaryStep&& other) noexcept
    : screen_root_(other.screen_root_), printer_name_(other.printer_name_),
      printer_type_(other.printer_type_), wifi_ssid_(other.wifi_ssid_),
      moonraker_connection_(other.moonraker_connection_), bed_(other.bed_), hotend_(other.hotend_),
      part_fan_(other.part_fan_), part_fan_visible_(other.part_fan_visible_),
      hotend_fan_(other.hotend_fan_), hotend_fan_visible_(other.hotend_fan_visible_),
      led_strip_(other.led_strip_), led_strip_visible_(other.led_strip_visible_),
      filament_sensor_(other.filament_sensor_),
      filament_sensor_visible_(other.filament_sensor_visible_), ams_type_(other.ams_type_),
      ams_visible_(other.ams_visible_), subjects_initialized_(other.subjects_initialized_) {
    // Move buffers
    std::memcpy(printer_name_buffer_, other.printer_name_buffer_, sizeof(printer_name_buffer_));
    std::memcpy(printer_type_buffer_, other.printer_type_buffer_, sizeof(printer_type_buffer_));
    std::memcpy(wifi_ssid_buffer_, other.wifi_ssid_buffer_, sizeof(wifi_ssid_buffer_));
    std::memcpy(moonraker_connection_buffer_, other.moonraker_connection_buffer_,
                sizeof(moonraker_connection_buffer_));
    std::memcpy(bed_buffer_, other.bed_buffer_, sizeof(bed_buffer_));
    std::memcpy(hotend_buffer_, other.hotend_buffer_, sizeof(hotend_buffer_));
    std::memcpy(part_fan_buffer_, other.part_fan_buffer_, sizeof(part_fan_buffer_));
    std::memcpy(hotend_fan_buffer_, other.hotend_fan_buffer_, sizeof(hotend_fan_buffer_));
    std::memcpy(led_strip_buffer_, other.led_strip_buffer_, sizeof(led_strip_buffer_));
    std::memcpy(filament_sensor_buffer_, other.filament_sensor_buffer_,
                sizeof(filament_sensor_buffer_));
    std::memcpy(ams_type_buffer_, other.ams_type_buffer_, sizeof(ams_type_buffer_));

    // Null out other
    other.screen_root_ = nullptr;
    other.subjects_initialized_ = false;
}

WizardSummaryStep& WizardSummaryStep::operator=(WizardSummaryStep&& other) noexcept {
    if (this != &other) {
        screen_root_ = other.screen_root_;
        printer_name_ = other.printer_name_;
        printer_type_ = other.printer_type_;
        wifi_ssid_ = other.wifi_ssid_;
        moonraker_connection_ = other.moonraker_connection_;
        bed_ = other.bed_;
        hotend_ = other.hotend_;
        part_fan_ = other.part_fan_;
        part_fan_visible_ = other.part_fan_visible_;
        hotend_fan_ = other.hotend_fan_;
        hotend_fan_visible_ = other.hotend_fan_visible_;
        led_strip_ = other.led_strip_;
        led_strip_visible_ = other.led_strip_visible_;
        filament_sensor_ = other.filament_sensor_;
        filament_sensor_visible_ = other.filament_sensor_visible_;
        ams_type_ = other.ams_type_;
        ams_visible_ = other.ams_visible_;
        subjects_initialized_ = other.subjects_initialized_;

        // Move buffers
        std::memcpy(printer_name_buffer_, other.printer_name_buffer_, sizeof(printer_name_buffer_));
        std::memcpy(printer_type_buffer_, other.printer_type_buffer_, sizeof(printer_type_buffer_));
        std::memcpy(wifi_ssid_buffer_, other.wifi_ssid_buffer_, sizeof(wifi_ssid_buffer_));
        std::memcpy(moonraker_connection_buffer_, other.moonraker_connection_buffer_,
                    sizeof(moonraker_connection_buffer_));
        std::memcpy(bed_buffer_, other.bed_buffer_, sizeof(bed_buffer_));
        std::memcpy(hotend_buffer_, other.hotend_buffer_, sizeof(hotend_buffer_));
        std::memcpy(part_fan_buffer_, other.part_fan_buffer_, sizeof(part_fan_buffer_));
        std::memcpy(hotend_fan_buffer_, other.hotend_fan_buffer_, sizeof(hotend_fan_buffer_));
        std::memcpy(led_strip_buffer_, other.led_strip_buffer_, sizeof(led_strip_buffer_));
        std::memcpy(filament_sensor_buffer_, other.filament_sensor_buffer_,
                    sizeof(filament_sensor_buffer_));
        std::memcpy(ams_type_buffer_, other.ams_type_buffer_, sizeof(ams_type_buffer_));

        // Null out other
        other.screen_root_ = nullptr;
        other.subjects_initialized_ = false;
    }
    return *this;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string WizardSummaryStep::format_bed_summary() {
    Config* config = Config::get_instance();
    std::stringstream ss;

    std::string heater = config->get<std::string>(helix::wizard::BED_HEATER, "None");
    std::string sensor = config->get<std::string>(helix::wizard::BED_SENSOR, "None");

    ss << "Heater: " << (heater == "None" ? "None" : heater);
    ss << ", Sensor: " << (sensor == "None" ? "None" : sensor);

    return ss.str();
}

std::string WizardSummaryStep::format_hotend_summary() {
    Config* config = Config::get_instance();
    std::stringstream ss;

    std::string heater = config->get<std::string>(helix::wizard::HOTEND_HEATER, "None");
    std::string sensor = config->get<std::string>(helix::wizard::HOTEND_SENSOR, "None");

    ss << "Heater: " << (heater == "None" ? "None" : heater);
    ss << ", Sensor: " << (sensor == "None" ? "None" : sensor);

    return ss.str();
}

// ============================================================================
// Subject Initialization
// ============================================================================

void WizardSummaryStep::init_subjects() {
    spdlog::debug("[{}] Initializing subjects", get_name());

    // Load all values from config
    Config* config = Config::get_instance();

    // Printer name
    std::string printer_name =
        config ? config->get<std::string>(helix::wizard::PRINTER_NAME, "Unnamed Printer")
               : "Unnamed Printer";
    spdlog::debug("[{}] Printer name from config: '{}'", get_name(), printer_name);

    // Printer type
    std::string printer_type =
        config ? config->get<std::string>(helix::wizard::PRINTER_TYPE, "Unknown") : "Unknown";
    spdlog::debug("[{}] Printer type from config: '{}'", get_name(), printer_type);

    // WiFi SSID
    std::string wifi_ssid =
        config ? config->get<std::string>(helix::wizard::WIFI_SSID, "Not configured")
               : "Not configured";
    spdlog::debug("[{}] WiFi SSID from config: '{}'", get_name(), wifi_ssid);

    // Moonraker connection (host:port)
    std::string moonraker_host =
        config ? config->get<std::string>(helix::wizard::MOONRAKER_HOST, "Not configured")
               : "Not configured";
    int moonraker_port = config ? config->get<int>(helix::wizard::MOONRAKER_PORT, 7125) : 7125;
    spdlog::debug("[{}] Moonraker host from config: '{}', port: {}", get_name(), moonraker_host,
                  moonraker_port);
    std::string moonraker_connection;
    if (moonraker_host != "Not configured") {
        moonraker_connection = moonraker_host + ":" + std::to_string(moonraker_port);
    } else {
        moonraker_connection = "Not configured";
    }
    spdlog::debug("[{}] Moonraker connection: '{}'", get_name(), moonraker_connection);

    // Bed configuration
    std::string bed_summary = config ? format_bed_summary() : "Not configured";

    // Hotend configuration
    std::string hotend_summary = config ? format_hotend_summary() : "Not configured";

    // Part cooling fan
    std::string part_fan =
        config ? config->get<std::string>(helix::wizard::PART_FAN, "None") : "None";
    int part_fan_visible = (part_fan != "None") ? 1 : 0;

    // Hotend cooling fan
    std::string hotend_fan =
        config ? config->get<std::string>(helix::wizard::HOTEND_FAN, "None") : "None";
    int hotend_fan_visible = (hotend_fan != "None") ? 1 : 0;

    // LED strip
    std::string led_strip =
        config ? config->get<std::string>(helix::wizard::LED_STRIP, "None") : "None";
    int led_strip_visible = (led_strip != "None") ? 1 : 0;

    // Filament sensor - get from FilamentSensorManager
    std::string filament_sensor = "None";
    int filament_sensor_visible = 0;
    auto& sensor_mgr = helix::FilamentSensorManager::instance();
    auto sensors = sensor_mgr.get_sensors();
    for (const auto& sensor : sensors) {
        if (sensor.role == helix::FilamentSensorRole::RUNOUT) {
            filament_sensor = sensor.sensor_name + " (Runout)";
            filament_sensor_visible = 1;
            break;
        }
    }
    // If no runout, check for any assigned role
    if (filament_sensor_visible == 0) {
        for (const auto& sensor : sensors) {
            if (sensor.role != helix::FilamentSensorRole::NONE) {
                filament_sensor =
                    sensor.sensor_name + " (" + helix::role_to_display_string(sensor.role) + ")";
                filament_sensor_visible = 1;
                break;
            }
        }
    }

    // Initialize and register all subjects
    // NOTE: Pass std::string.c_str() as initial_value, NOT the buffer itself.
    // The macro copies initial_value to buffer - passing the same pointer for both
    // is undefined behavior (overlapping source/dest in snprintf).
    UI_SUBJECT_INIT_AND_REGISTER_STRING(printer_name_, printer_name_buffer_, printer_name.c_str(),
                                        "summary_printer_name");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(printer_type_, printer_type_buffer_, printer_type.c_str(),
                                        "summary_printer_type");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(wifi_ssid_, wifi_ssid_buffer_, wifi_ssid.c_str(),
                                        "summary_wifi_ssid");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(moonraker_connection_, moonraker_connection_buffer_,
                                        moonraker_connection.c_str(),
                                        "summary_moonraker_connection");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(bed_, bed_buffer_, bed_summary.c_str(), "summary_bed");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(hotend_, hotend_buffer_, hotend_summary.c_str(),
                                        "summary_hotend");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(part_fan_, part_fan_buffer_, part_fan.c_str(),
                                        "summary_part_fan");
    UI_SUBJECT_INIT_AND_REGISTER_INT(part_fan_visible_, part_fan_visible,
                                     "summary_part_fan_visible");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(hotend_fan_, hotend_fan_buffer_, hotend_fan.c_str(),
                                        "summary_hotend_fan");
    UI_SUBJECT_INIT_AND_REGISTER_INT(hotend_fan_visible_, hotend_fan_visible,
                                     "summary_hotend_fan_visible");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(led_strip_, led_strip_buffer_, led_strip.c_str(),
                                        "summary_led_strip");
    UI_SUBJECT_INIT_AND_REGISTER_INT(led_strip_visible_, led_strip_visible,
                                     "summary_led_strip_visible");
    UI_SUBJECT_INIT_AND_REGISTER_STRING(filament_sensor_, filament_sensor_buffer_,
                                        filament_sensor.c_str(), "summary_filament_sensor");
    UI_SUBJECT_INIT_AND_REGISTER_INT(filament_sensor_visible_, filament_sensor_visible,
                                     "summary_filament_sensor_visible");

    // AMS/Multi-Material System
    std::string ams_type_str = "None";
    int ams_visible = 0;

    auto& ams = AmsState::instance();
    AmsBackend* backend = ams.get_backend();
    if (backend && backend->get_type() != AmsType::NONE) {
        AmsSystemInfo info = backend->get_system_info();
        // Format: "AFC • 4 lanes" or "Happy Hare • 8 lanes"
        ams_type_str = info.type_name;
        if (info.total_slots > 0) {
            ams_type_str += " • " + std::to_string(info.total_slots) + " lanes";
        }
        ams_visible = 1;
    }

    UI_SUBJECT_INIT_AND_REGISTER_STRING(ams_type_, ams_type_buffer_, ams_type_str.c_str(),
                                        "summary_ams_type");
    UI_SUBJECT_INIT_AND_REGISTER_INT(ams_visible_, ams_visible, "summary_ams_visible");

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized with config values", get_name());
}

// ============================================================================
// Callback Registration
// ============================================================================

void WizardSummaryStep::register_callbacks() {
    spdlog::debug("[{}] No callbacks to register (read-only screen)", get_name());
    // No interactive callbacks for summary screen
}

// ============================================================================
// Screen Creation
// ============================================================================

lv_obj_t* WizardSummaryStep::create(lv_obj_t* parent) {
    spdlog::debug("[{}] Creating summary screen", get_name());

    // Safety check: cleanup should have been called by wizard navigation
    if (screen_root_) {
        spdlog::warn("[{}] Screen pointer not null - cleanup may not have been called properly",
                     get_name());
        screen_root_ = nullptr; // Reset pointer, wizard framework handles deletion
    }

    // Refresh subjects with latest config values before creating UI
    init_subjects();

    // Create screen from XML
    screen_root_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "wizard_summary", nullptr));
    if (!screen_root_) {
        spdlog::error("[{}] Failed to create screen from XML", get_name());
        return nullptr;
    }

    spdlog::debug("[{}] Screen created successfully", get_name());
    return screen_root_;
}

// ============================================================================
// Cleanup
// ============================================================================

void WizardSummaryStep::cleanup() {
    spdlog::debug("[{}] Cleaning up resources", get_name());

    // NOTE: Wizard framework handles object deletion - we only null the pointer
    // See HANDOFF.md Pattern #9: Wizard Screen Lifecycle
    screen_root_ = nullptr;
}

// ============================================================================
// Validation
// ============================================================================

bool WizardSummaryStep::is_validated() const {
    // Summary screen is always validated (no user input required)
    return true;
}
