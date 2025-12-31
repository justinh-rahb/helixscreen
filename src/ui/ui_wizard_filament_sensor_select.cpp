// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_wizard_filament_sensor_select.h"

#include "ui_error_reporting.h"
#include "ui_notification.h"
#include "ui_wizard.h"
#include "ui_wizard_helpers.h"

#include "filament_sensor_manager.h"
#include "lvgl/lvgl.h"
#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<WizardFilamentSensorSelectStep> g_wizard_filament_sensor_select_step;

WizardFilamentSensorSelectStep* get_wizard_filament_sensor_select_step() {
    if (!g_wizard_filament_sensor_select_step) {
        g_wizard_filament_sensor_select_step = std::make_unique<WizardFilamentSensorSelectStep>();
        StaticPanelRegistry::instance().register_destroy("WizardFilamentSensorSelectStep", []() {
            g_wizard_filament_sensor_select_step.reset();
        });
    }
    return g_wizard_filament_sensor_select_step.get();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

WizardFilamentSensorSelectStep::WizardFilamentSensorSelectStep() {
    spdlog::debug("[{}] Instance created", get_name());
}

WizardFilamentSensorSelectStep::~WizardFilamentSensorSelectStep() {
    // NOTE: Do NOT call LVGL functions here - LVGL may be destroyed first
    // NOTE: Do NOT log here - spdlog may be destroyed first
    screen_root_ = nullptr;
}

// ============================================================================
// Move Semantics
// ============================================================================

WizardFilamentSensorSelectStep::WizardFilamentSensorSelectStep(
    WizardFilamentSensorSelectStep&& other) noexcept
    : screen_root_(other.screen_root_), runout_sensor_selected_(other.runout_sensor_selected_),
      toolhead_sensor_selected_(other.toolhead_sensor_selected_),
      entry_sensor_selected_(other.entry_sensor_selected_),
      sensor_items_(std::move(other.sensor_items_)),
      standalone_sensors_(std::move(other.standalone_sensors_)),
      subjects_initialized_(other.subjects_initialized_) {
    other.screen_root_ = nullptr;
    other.subjects_initialized_ = false;
}

WizardFilamentSensorSelectStep&
WizardFilamentSensorSelectStep::operator=(WizardFilamentSensorSelectStep&& other) noexcept {
    if (this != &other) {
        screen_root_ = other.screen_root_;
        runout_sensor_selected_ = other.runout_sensor_selected_;
        toolhead_sensor_selected_ = other.toolhead_sensor_selected_;
        entry_sensor_selected_ = other.entry_sensor_selected_;
        sensor_items_ = std::move(other.sensor_items_);
        standalone_sensors_ = std::move(other.standalone_sensors_);
        subjects_initialized_ = other.subjects_initialized_;

        other.screen_root_ = nullptr;
        other.subjects_initialized_ = false;
    }
    return *this;
}

// ============================================================================
// AMS Sensor Detection
// ============================================================================

bool WizardFilamentSensorSelectStep::is_ams_sensor(const std::string& name) {
    // Convert to lowercase for case-insensitive matching
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    // AFC (Armored Turtle Filament Changer) patterns
    if (lower_name.find("lane") != std::string::npos)
        return true;
    if (lower_name.find("afc") != std::string::npos)
        return true;
    if (lower_name.find("slot") != std::string::npos)
        return true;

    // ERCF (Enraged Rabbit Carrot Feeder) patterns
    if (lower_name.find("ercf") != std::string::npos)
        return true;
    if (lower_name.find("gate") != std::string::npos)
        return true;

    // MMU2/MMU3 (Prusa Multi-Material Unit) patterns
    if (lower_name.find("mmu") != std::string::npos)
        return true;

    // TradRack patterns
    if (lower_name.find("trad") != std::string::npos)
        return true;

    // BoxTurtle patterns
    if (lower_name.find("turtle") != std::string::npos)
        return true;
    if (lower_name.find("box") != std::string::npos &&
        lower_name.find("filament") != std::string::npos)
        return true;

    // Happy Hare patterns
    if (lower_name.find("happy") != std::string::npos)
        return true;
    if (lower_name.find("hare") != std::string::npos)
        return true;

    // Generic multi-material patterns (numbered sensors)
    // Match patterns like "filament_0", "filament_1", "unit_0", "channel_1"
    if (lower_name.find("unit") != std::string::npos)
        return true;
    if (lower_name.find("channel") != std::string::npos)
        return true;
    if (lower_name.find("buffer") != std::string::npos)
        return true;
    if (lower_name.find("hub") != std::string::npos)
        return true;

    // Check for numbered filament sensors (e.g., filament_0, fil_sensor_2)
    // These typically indicate multi-material setups
    for (char c = '0'; c <= '9'; c++) {
        std::string pattern = std::string("filament_") + c;
        if (lower_name.find(pattern) != std::string::npos)
            return true;
        pattern = std::string("fil_") + c;
        if (lower_name.find(pattern) != std::string::npos)
            return true;
    }

    return false;
}

void WizardFilamentSensorSelectStep::filter_standalone_sensors() {
    standalone_sensors_.clear();

    auto& sensor_mgr = helix::FilamentSensorManager::instance();
    auto all_sensors = sensor_mgr.get_sensors();

    for (const auto& sensor : all_sensors) {
        if (!is_ams_sensor(sensor.sensor_name)) {
            standalone_sensors_.push_back(sensor);
            spdlog::debug("[{}] Found standalone sensor: {}", get_name(), sensor.sensor_name);
        } else {
            spdlog::debug("[{}] Filtered out AMS sensor: {}", get_name(), sensor.sensor_name);
        }
    }

    spdlog::info("[{}] Found {} standalone sensors (filtered from {} total)", get_name(),
                 standalone_sensors_.size(), all_sensors.size());
}

// ============================================================================
// Subject Initialization
// ============================================================================

void WizardFilamentSensorSelectStep::init_subjects() {
    spdlog::debug("[{}] Initializing subjects", get_name());

    // Initialize subjects with default index 0 (None)
    helix::ui::wizard::init_int_subject(&runout_sensor_selected_, 0, "runout_sensor_selected");
    helix::ui::wizard::init_int_subject(&toolhead_sensor_selected_, 0, "toolhead_sensor_selected");
    helix::ui::wizard::init_int_subject(&entry_sensor_selected_, 0, "entry_sensor_selected");

    subjects_initialized_ = true;
    spdlog::debug("[{}] Subjects initialized", get_name());
}

// ============================================================================
// Static Callbacks (XML event_cb pattern)
// ============================================================================

static void on_runout_sensor_dropdown_changed(lv_event_t* e) {
    auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int index = static_cast<int>(lv_dropdown_get_selected(dropdown));
    auto* step = get_wizard_filament_sensor_select_step();
    if (step) {
        lv_subject_set_int(step->get_runout_sensor_subject(), index);
        spdlog::debug(
            "[WizardFilamentSensorSelectStep] Runout sensor selection changed to index {}", index);
    }
}

static void on_toolhead_sensor_dropdown_changed(lv_event_t* e) {
    auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int index = static_cast<int>(lv_dropdown_get_selected(dropdown));
    auto* step = get_wizard_filament_sensor_select_step();
    if (step) {
        lv_subject_set_int(step->get_toolhead_sensor_subject(), index);
        spdlog::debug(
            "[WizardFilamentSensorSelectStep] Toolhead sensor selection changed to index {}",
            index);
    }
}

static void on_entry_sensor_dropdown_changed(lv_event_t* e) {
    auto* dropdown = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    int index = static_cast<int>(lv_dropdown_get_selected(dropdown));
    auto* step = get_wizard_filament_sensor_select_step();
    if (step) {
        lv_subject_set_int(step->get_entry_sensor_subject(), index);
        spdlog::debug("[WizardFilamentSensorSelectStep] Entry sensor selection changed to index {}",
                      index);
    }
}

// ============================================================================
// Callback Registration
// ============================================================================

void WizardFilamentSensorSelectStep::register_callbacks() {
    lv_xml_register_event_cb(nullptr, "on_runout_sensor_dropdown_changed",
                             on_runout_sensor_dropdown_changed);
    lv_xml_register_event_cb(nullptr, "on_toolhead_sensor_dropdown_changed",
                             on_toolhead_sensor_dropdown_changed);
    lv_xml_register_event_cb(nullptr, "on_entry_sensor_dropdown_changed",
                             on_entry_sensor_dropdown_changed);
    spdlog::debug("[{}] Registered dropdown callbacks", get_name());
}

// ============================================================================
// Dropdown Population
// ============================================================================

void WizardFilamentSensorSelectStep::populate_dropdowns() {
    if (!screen_root_)
        return;

    // Build sensor items list: "None" + sensor names
    sensor_items_.clear();
    sensor_items_.push_back("None");
    for (const auto& sensor : standalone_sensors_) {
        sensor_items_.push_back(sensor.klipper_name);
    }

    // Build options string for dropdowns (newline-separated)
    std::string options;
    for (size_t i = 0; i < sensor_items_.size(); i++) {
        if (i > 0)
            options += "\n";
        // Use display name (sensor_name) for dropdown, but store klipper_name
        if (i == 0) {
            options += "None";
        } else {
            options += standalone_sensors_[i - 1].sensor_name;
        }
    }

    // Find and populate dropdowns
    lv_obj_t* runout_dropdown = lv_obj_find_by_name(screen_root_, "runout_sensor_dropdown");
    lv_obj_t* toolhead_dropdown = lv_obj_find_by_name(screen_root_, "toolhead_sensor_dropdown");
    lv_obj_t* entry_dropdown = lv_obj_find_by_name(screen_root_, "entry_sensor_dropdown");

    if (runout_dropdown) {
        lv_dropdown_set_options(runout_dropdown, options.c_str());
        lv_dropdown_set_selected(
            runout_dropdown, static_cast<uint32_t>(lv_subject_get_int(&runout_sensor_selected_)));
    }

    if (toolhead_dropdown) {
        lv_dropdown_set_options(toolhead_dropdown, options.c_str());
        lv_dropdown_set_selected(toolhead_dropdown, static_cast<uint32_t>(lv_subject_get_int(
                                                        &toolhead_sensor_selected_)));
    }

    if (entry_dropdown) {
        lv_dropdown_set_options(entry_dropdown, options.c_str());
        lv_dropdown_set_selected(
            entry_dropdown, static_cast<uint32_t>(lv_subject_get_int(&entry_sensor_selected_)));
    }

    spdlog::debug("[{}] Populated dropdowns with {} options", get_name(), sensor_items_.size());
}

std::string WizardFilamentSensorSelectStep::get_klipper_name_for_index(int dropdown_index) const {
    if (dropdown_index <= 0 || static_cast<size_t>(dropdown_index) >= sensor_items_.size()) {
        return ""; // "None" selected or invalid
    }
    return sensor_items_[static_cast<size_t>(dropdown_index)];
}

// ============================================================================
// Screen Creation
// ============================================================================

lv_obj_t* WizardFilamentSensorSelectStep::create(lv_obj_t* parent) {
    spdlog::debug("[{}] Creating filament sensor select screen", get_name());

    // Safety check
    if (screen_root_) {
        spdlog::warn("[{}] Screen pointer not null - cleanup may not have been called properly",
                     get_name());
        screen_root_ = nullptr;
    }

    // Filter sensors to get standalone (non-AMS) sensors
    filter_standalone_sensors();

    // Create screen from XML
    screen_root_ =
        static_cast<lv_obj_t*>(lv_xml_create(parent, "wizard_filament_sensor_select", nullptr));
    if (!screen_root_) {
        spdlog::error("[{}] Failed to create screen from XML", get_name());
        ui_notification_error(
            "Wizard Error",
            "Failed to load filament sensor configuration screen. Please restart the application.");
        return nullptr;
    }

    // Restore selections from existing FilamentSensorManager config
    for (size_t i = 0; i < standalone_sensors_.size(); i++) {
        const auto& sensor = standalone_sensors_[i];
        int dropdown_index = static_cast<int>(i + 1); // +1 because index 0 is "None"

        if (sensor.role == helix::FilamentSensorRole::RUNOUT) {
            lv_subject_set_int(&runout_sensor_selected_, dropdown_index);
        } else if (sensor.role == helix::FilamentSensorRole::TOOLHEAD) {
            lv_subject_set_int(&toolhead_sensor_selected_, dropdown_index);
        } else if (sensor.role == helix::FilamentSensorRole::ENTRY) {
            lv_subject_set_int(&entry_sensor_selected_, dropdown_index);
        }
    }

    // Populate dropdowns
    populate_dropdowns();

    spdlog::debug("[{}] Screen created successfully", get_name());
    return screen_root_;
}

// ============================================================================
// Skip Logic
// ============================================================================

bool WizardFilamentSensorSelectStep::should_skip() const {
    auto& sensor_mgr = helix::FilamentSensorManager::instance();
    auto all_sensors = sensor_mgr.get_sensors();

    // Count non-AMS sensors
    size_t standalone_count = 0;
    for (const auto& sensor : all_sensors) {
        if (!is_ams_sensor(sensor.sensor_name)) {
            standalone_count++;
        }
    }

    // Skip if fewer than 2 standalone sensors
    return standalone_count < 2;
}

void WizardFilamentSensorSelectStep::auto_configure_single_sensor() {
    auto& sensor_mgr = helix::FilamentSensorManager::instance();
    auto all_sensors = sensor_mgr.get_sensors();

    // Find the single non-AMS sensor
    for (const auto& sensor : all_sensors) {
        if (!is_ams_sensor(sensor.sensor_name)) {
            spdlog::info("[{}] Auto-configuring single sensor '{}' as RUNOUT", get_name(),
                         sensor.sensor_name);
            sensor_mgr.set_sensor_role(sensor.klipper_name, helix::FilamentSensorRole::RUNOUT);
            sensor_mgr.save_config();
            return;
        }
    }
}

// ============================================================================
// Cleanup
// ============================================================================

void WizardFilamentSensorSelectStep::cleanup() {
    spdlog::debug("[{}] Cleaning up resources", get_name());

    auto& sensor_mgr = helix::FilamentSensorManager::instance();

    // Clear all existing role assignments first
    for (const auto& sensor : standalone_sensors_) {
        sensor_mgr.set_sensor_role(sensor.klipper_name, helix::FilamentSensorRole::NONE);
    }

    // Apply new role assignments based on dropdown selections
    std::string runout_name =
        get_klipper_name_for_index(lv_subject_get_int(&runout_sensor_selected_));
    std::string toolhead_name =
        get_klipper_name_for_index(lv_subject_get_int(&toolhead_sensor_selected_));
    std::string entry_name =
        get_klipper_name_for_index(lv_subject_get_int(&entry_sensor_selected_));

    if (!runout_name.empty()) {
        sensor_mgr.set_sensor_role(runout_name, helix::FilamentSensorRole::RUNOUT);
        spdlog::info("[{}] Assigned RUNOUT role to: {}", get_name(), runout_name);
    }

    if (!toolhead_name.empty()) {
        sensor_mgr.set_sensor_role(toolhead_name, helix::FilamentSensorRole::TOOLHEAD);
        spdlog::info("[{}] Assigned TOOLHEAD role to: {}", get_name(), toolhead_name);
    }

    if (!entry_name.empty()) {
        sensor_mgr.set_sensor_role(entry_name, helix::FilamentSensorRole::ENTRY);
        spdlog::info("[{}] Assigned ENTRY role to: {}", get_name(), entry_name);
    }

    // Persist to disk
    sensor_mgr.save_config();

    // Reset UI references
    screen_root_ = nullptr;

    spdlog::debug("[{}] Cleanup complete", get_name());
}

// ============================================================================
// Validation
// ============================================================================

bool WizardFilamentSensorSelectStep::is_validated() const {
    // Always return true for baseline implementation
    return true;
}
