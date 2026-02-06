// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "filament_sensor_types.h"
#include "lvgl/lvgl.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @file ui_wizard_probe_sensor_select.h
 * @brief Wizard probe sensor selection step - assigns Z_PROBE role to switch sensor
 *
 * Uses FilamentSensorManager to discover unassigned switch sensors and assign
 * the Z_PROBE role to the selected one.
 *
 * ## Skip Logic:
 *
 * - 0 unassigned switch sensors: Skip entirely
 * - 1+ unassigned switch sensors: Show wizard step for manual assignment
 *
 * Note: Unlike filament sensor step, we do NOT auto-assign probe sensors.
 * Many printers have dedicated probes (BLTouch, Klicky, etc.) and assigning
 * a random switch sensor as probe could cause issues.
 *
 * ## Subject Bindings (1 total):
 *
 * - probe_sensor_selected (int) - Selected sensor index for Z_PROBE role
 *
 * Index 0 = "None", 1+ = sensor index in available_sensors_
 */

/**
 * @class WizardProbeSensorSelectStep
 * @brief Probe sensor configuration step for the first-run wizard
 */
class WizardProbeSensorSelectStep {
  public:
    WizardProbeSensorSelectStep();
    ~WizardProbeSensorSelectStep();

    // Non-copyable, non-movable (lv_subject_t cannot be safely moved)
    WizardProbeSensorSelectStep(const WizardProbeSensorSelectStep&) = delete;
    WizardProbeSensorSelectStep& operator=(const WizardProbeSensorSelectStep&) = delete;
    WizardProbeSensorSelectStep(WizardProbeSensorSelectStep&&) = delete;
    WizardProbeSensorSelectStep& operator=(WizardProbeSensorSelectStep&&) = delete;

    /**
     * @brief Initialize reactive subjects
     */
    void init_subjects();

    /**
     * @brief Register event callbacks
     */
    void register_callbacks();

    /**
     * @brief Create the probe sensor selection UI from XML
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
     * @brief Refresh sensor list and dropdown
     *
     * Call this when sensors may have been discovered after initial create().
     * Re-filters sensors to show only unassigned switch sensors.
     */
    void refresh();

    /**
     * @brief Check if step should be skipped
     *
     * Returns true if there are no unassigned switch sensors.
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
        return "Wizard Probe Sensor";
    }

    /**
     * @brief Get the number of available (unassigned switch) sensors
     *
     * Queries FilamentSensorManager directly as the single source of truth.
     * This works even when the step is skipped and create() was never called.
     */
    size_t get_available_sensor_count() const;

    // Public access to subjects for helper functions
    lv_subject_t* get_probe_sensor_subject() {
        return &probe_sensor_selected_;
    }

    std::vector<std::string>& get_sensor_items() {
        return sensor_items_;
    }

    // Pending refresh timer (set by wizard orchestrator, cancelled on cleanup)
    lv_timer_t* refresh_timer_ = nullptr;

  private:
    /**
     * @brief Filter sensors to get only unassigned switch sensors
     *
     * Gets switch sensors where role == NONE (not already assigned to
     * RUNOUT, TOOLHEAD, ENTRY, or Z_PROBE).
     */
    void filter_available_sensors();

    /**
     * @brief Populate dropdown options from available sensors
     */
    void populate_dropdown();

    /**
     * @brief Get the klipper name for a dropdown selection index
     *
     * @param dropdown_index Index in dropdown (0 = None, 1+ = sensor)
     * @return Klipper name, or empty string if None selected
     */
    std::string get_klipper_name_for_index(int dropdown_index) const;

    // Screen instance
    lv_obj_t* screen_root_ = nullptr;

    // Subject (dropdown selection index)
    lv_subject_t probe_sensor_selected_;

    // Dynamic options storage
    std::vector<std::string> sensor_items_;                      // Klipper names for dropdown
    std::vector<helix::FilamentSensorConfig> available_sensors_; // Filtered sensors

    // Track initialization
    bool subjects_initialized_ = false;
};

// ============================================================================
// Global Instance Access
// ============================================================================

WizardProbeSensorSelectStep* get_wizard_probe_sensor_select_step();
void destroy_wizard_probe_sensor_select_step();
