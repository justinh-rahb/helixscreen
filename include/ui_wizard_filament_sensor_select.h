// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filament_sensor_types.h"
#include "lvgl/lvgl.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file ui_wizard_filament_sensor_select.h
 * @brief Wizard filament sensor selection step - assigns roles to detected sensors
 *
 * Uses FilamentSensorManager to discover sensors and assign roles (RUNOUT, TOOLHEAD, ENTRY).
 *
 * ## Skip Logic:
 *
 * - 0 non-AMS sensors: Skip entirely
 * - 1 non-AMS sensor: Auto-assign to RUNOUT, skip step
 * - 2+ non-AMS sensors: Show wizard step for manual assignment
 *
 * AMS sensors (lane/slot sensors from AFC, etc.) are filtered out.
 *
 * ## Subject Bindings (3 total):
 *
 * - runout_sensor_selected (int) - Selected sensor index for runout role
 * - toolhead_sensor_selected (int) - Selected sensor index for toolhead role
 * - entry_sensor_selected (int) - Selected sensor index for entry role
 *
 * Index 0 = "None", 1+ = sensor index in sensor_items_
 */

/**
 * @class WizardFilamentSensorSelectStep
 * @brief Filament sensor configuration step for the first-run wizard
 */
class WizardFilamentSensorSelectStep {
  public:
    WizardFilamentSensorSelectStep();
    ~WizardFilamentSensorSelectStep();

    // Non-copyable
    WizardFilamentSensorSelectStep(const WizardFilamentSensorSelectStep&) = delete;
    WizardFilamentSensorSelectStep& operator=(const WizardFilamentSensorSelectStep&) = delete;

    // Movable
    WizardFilamentSensorSelectStep(WizardFilamentSensorSelectStep&& other) noexcept;
    WizardFilamentSensorSelectStep& operator=(WizardFilamentSensorSelectStep&& other) noexcept;

    /**
     * @brief Initialize reactive subjects
     */
    void init_subjects();

    /**
     * @brief Register event callbacks
     */
    void register_callbacks();

    /**
     * @brief Create the filament sensor selection UI from XML
     *
     * @param parent Parent container (wizard_content)
     * @return Root object of the step, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent);

    /**
     * @brief Cleanup resources and save role assignments to config
     */
    void cleanup();

    /**
     * @brief Check if step should be skipped
     *
     * Returns true if there are fewer than 2 non-AMS sensors.
     * If exactly 1 sensor, it will be auto-assigned to RUNOUT.
     *
     * @return true if step should be skipped
     */
    bool should_skip() const;

    /**
     * @brief Check if step is validated
     *
     * @return true (always validated for baseline)
     */
    bool is_validated() const;

    /**
     * @brief Get step name for logging
     */
    const char* get_name() const {
        return "Wizard Filament Sensor";
    }

    /**
     * @brief Get the number of standalone (non-AMS) sensors
     */
    size_t get_standalone_sensor_count() const {
        return standalone_sensors_.size();
    }

    /**
     * @brief Auto-configure a single sensor as RUNOUT
     *
     * Called when exactly 1 sensor is detected to auto-assign it.
     */
    void auto_configure_single_sensor();

    // Public access to subjects for helper functions
    lv_subject_t* get_runout_sensor_subject() {
        return &runout_sensor_selected_;
    }
    lv_subject_t* get_toolhead_sensor_subject() {
        return &toolhead_sensor_selected_;
    }
    lv_subject_t* get_entry_sensor_subject() {
        return &entry_sensor_selected_;
    }

    std::vector<std::string>& get_sensor_items() {
        return sensor_items_;
    }

  private:
    /**
     * @brief Check if a sensor name indicates it's managed by AMS
     */
    static bool is_ams_sensor(const std::string& name);

    /**
     * @brief Filter sensors to get only standalone (non-AMS) sensors
     */
    void filter_standalone_sensors();

    /**
     * @brief Populate dropdown options from discovered sensors
     */
    void populate_dropdowns();

    /**
     * @brief Get the klipper name for a dropdown selection index
     *
     * @param dropdown_index Index in dropdown (0 = None, 1+ = sensor)
     * @return Klipper name, or empty string if None selected
     */
    std::string get_klipper_name_for_index(int dropdown_index) const;

    // Screen instance
    lv_obj_t* screen_root_ = nullptr;

    // Subjects (dropdown selection indices)
    lv_subject_t runout_sensor_selected_;
    lv_subject_t toolhead_sensor_selected_;
    lv_subject_t entry_sensor_selected_;

    // Dynamic options storage
    std::vector<std::string> sensor_items_;                       // Klipper names for dropdown
    std::vector<helix::FilamentSensorConfig> standalone_sensors_; // Filtered sensors

    // Track initialization
    bool subjects_initialized_ = false;
};

// ============================================================================
// Global Instance Access
// ============================================================================

WizardFilamentSensorSelectStep* get_wizard_filament_sensor_select_step();
void destroy_wizard_filament_sensor_select_step();
