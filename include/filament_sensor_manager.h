// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 HelixScreen Authors

#pragma once

#include "filament_sensor_types.h"
#include "lvgl.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "hv/json.hpp"

using json = nlohmann::json;

namespace helix {

/**
 * @brief Central manager for filament sensor discovery, configuration, and state.
 *
 * Provides:
 * - Auto-discovery of sensors from Klipper objects list
 * - User configuration (role assignment, enable/disable)
 * - Real-time state tracking from Moonraker updates
 * - LVGL subjects for reactive UI binding
 * - Config persistence to helixconfig.json
 *
 * Thread-safe for state updates from Moonraker callbacks.
 *
 * @code
 * // Initialize after Moonraker connection
 * auto& mgr = FilamentSensorManager::instance();
 * mgr.init_subjects();
 * mgr.discover_sensors(capabilities.get_filament_sensor_names());
 * mgr.load_config();
 *
 * // Check sensor state
 * if (mgr.is_filament_detected(FilamentSensorRole::RUNOUT)) {
 *     // Filament present
 * }
 * @endcode
 */
class FilamentSensorManager {
  public:
    /// @brief Callback for sensor state change notifications
    using StateChangeCallback =
        std::function<void(const std::string& sensor_name, const FilamentSensorState& old_state,
                           const FilamentSensorState& new_state)>;

    /**
     * @brief Get singleton instance
     */
    static FilamentSensorManager& instance();

    // Prevent copying
    FilamentSensorManager(const FilamentSensorManager&) = delete;
    FilamentSensorManager& operator=(const FilamentSensorManager&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Initialize LVGL subjects for UI binding
     *
     * Must be called before creating any XML components that bind to sensor subjects.
     * Safe to call multiple times (idempotent).
     */
    void init_subjects();

    /**
     * @brief Deinitialize LVGL subjects
     *
     * Must be called before lv_deinit() to properly disconnect observers.
     * Called by StaticSubjectRegistry during application shutdown.
     */
    void deinit_subjects();

    /**
     * @brief Discover sensors from PrinterCapabilities
     *
     * Populates internal sensor list from Klipper objects.
     * Should be called after Moonraker connection established.
     *
     * @param klipper_sensor_names Full Klipper object names from PrinterCapabilities
     */
    void discover_sensors(const std::vector<std::string>& klipper_sensor_names);

    /**
     * @brief Check if any sensors have been discovered
     */
    [[nodiscard]] bool has_sensors() const;

    /**
     * @brief Get all discovered sensor configurations (thread-safe copy)
     */
    [[nodiscard]] std::vector<FilamentSensorConfig> get_sensors() const;

    /**
     * @brief Get sensor count
     */
    [[nodiscard]] size_t sensor_count() const;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Load configuration from helixconfig.json
     *
     * Merges saved config with discovered sensors. New sensors get default config,
     * removed sensors are preserved in config (in case they come back).
     */
    void load_config();

    /**
     * @brief Save current configuration to helixconfig.json
     */
    void save_config();

    /**
     * @brief Set role for a specific sensor
     *
     * @param klipper_name Full Klipper object name
     * @param role New role assignment
     */
    void set_sensor_role(const std::string& klipper_name, FilamentSensorRole role);

    /**
     * @brief Enable or disable a specific sensor
     *
     * @param klipper_name Full Klipper object name
     * @param enabled Whether sensor should be monitored
     */
    void set_sensor_enabled(const std::string& klipper_name, bool enabled);

    /**
     * @brief Set master enable switch
     *
     * When disabled, all sensor monitoring is bypassed.
     *
     * @param enabled Master enable state
     */
    void set_master_enabled(bool enabled);

    /**
     * @brief Check if master switch is enabled
     */
    [[nodiscard]] bool is_master_enabled() const;

    // ========================================================================
    // State Queries
    // ========================================================================

    /**
     * @brief Check if filament is detected for a given role
     *
     * Returns false if master disabled, sensor disabled, or no sensor assigned to role.
     *
     * @param role The sensor role to check
     * @return true if filament is detected
     */
    [[nodiscard]] bool is_filament_detected(FilamentSensorRole role) const;

    /**
     * @brief Check if a sensor is available (exists and enabled)
     *
     * @param role The sensor role to check
     * @return true if sensor exists, is enabled, and is available in Klipper
     */
    [[nodiscard]] bool is_sensor_available(FilamentSensorRole role) const;

    /**
     * @brief Get current state for a sensor by role (thread-safe copy)
     *
     * @param role The sensor role to query
     * @return State copy if sensor assigned to role, empty optional otherwise
     */
    [[nodiscard]] std::optional<FilamentSensorState>
    get_sensor_state(FilamentSensorRole role) const;

    /**
     * @brief Check if any sensor reports runout (no filament)
     *
     * Only checks enabled sensors with assigned roles.
     *
     * @return true if any monitored sensor shows no filament
     */
    [[nodiscard]] bool has_any_runout() const;

    /**
     * @brief Check if motion sensor encoder is active
     *
     * Only applicable for motion sensors during extrusion.
     *
     * @return true if motion sensor shows encoder activity
     */
    [[nodiscard]] bool is_motion_active() const;

    // ========================================================================
    // State Updates
    // ========================================================================

    /**
     * @brief Update sensor states from Moonraker notification
     *
     * Called by PrinterState when receiving notify_status_update.
     * Thread-safe.
     *
     * @param status JSON object containing sensor state updates
     */
    void update_from_status(const json& status);

    /**
     * @brief Register callback for state changes
     *
     * @param callback Function to call when any sensor state changes
     */
    void set_state_change_callback(StateChangeCallback callback);

    // ========================================================================
    // LVGL Subjects
    // ========================================================================

    /**
     * @brief Get subject for runout sensor detected state
     * @return Subject (int: 0=no filament, 1=detected, -1=no sensor)
     */
    [[nodiscard]] lv_subject_t* get_runout_detected_subject();

    /**
     * @brief Get subject for toolhead sensor detected state
     * @return Subject (int: 0=no filament, 1=detected, -1=no sensor)
     */
    [[nodiscard]] lv_subject_t* get_toolhead_detected_subject();

    /**
     * @brief Get subject for entry sensor detected state
     * @return Subject (int: 0=no filament, 1=detected, -1=no sensor)
     */
    [[nodiscard]] lv_subject_t* get_entry_detected_subject();

    /**
     * @brief Get subject for any runout active (any sensor shows no filament)
     * @return Subject (int: 0=all OK, 1=runout detected)
     */
    [[nodiscard]] lv_subject_t* get_any_runout_subject();

    /**
     * @brief Get subject for motion sensor activity
     * @return Subject (int: 0=idle, 1=motion detected)
     */
    [[nodiscard]] lv_subject_t* get_motion_active_subject();

    /**
     * @brief Get subject for master enable state
     * @return Subject (int: 0=disabled, 1=enabled)
     */
    [[nodiscard]] lv_subject_t* get_master_enabled_subject();

    /**
     * @brief Get subject for sensor count (for conditional UI visibility)
     * @return Subject (int: number of discovered sensors)
     */
    [[nodiscard]] lv_subject_t* get_sensor_count_subject();

    /**
     * @brief Check if still within startup grace period
     *
     * Used to suppress notifications and modals during initial startup
     * when sensor states are being synchronized.
     *
     * @return true if within grace period (first 10 seconds after init)
     */
    [[nodiscard]] bool is_in_startup_grace_period() const;

    /**
     * @brief Reset all state for testing.
     *
     * Clears all sensors, states, and resets flags.
     * Call this between tests to ensure isolation.
     *
     * @note This method is for testing purposes only.
     */
    void reset_for_testing();

    /**
     * @brief Enable synchronous mode for testing
     *
     * When enabled, update_from_status() calls update_subjects() synchronously
     * instead of using lv_async_call(). This avoids LVGL timer dependencies in unit tests.
     *
     * @param enabled true to enable synchronous updates
     */
    void set_sync_mode(bool enabled);

    /**
     * @brief Update subjects on main LVGL thread (called by async callback)
     *
     * This is public to allow the async callback in the anonymous namespace
     * to access it. Do not call directly - use update_from_status() instead.
     */
    void update_subjects_on_main_thread();

  private:
    FilamentSensorManager();
    ~FilamentSensorManager();

    /**
     * @brief Extract sensor name and type from Klipper object name
     *
     * @param klipper_name Full name like "filament_switch_sensor fsensor"
     * @param[out] sensor_name Extracted short name
     * @param[out] type Detected sensor type
     * @return true if successfully parsed
     */
    bool parse_klipper_name(const std::string& klipper_name, std::string& sensor_name,
                            FilamentSensorType& type) const;

    /**
     * @brief Find config by Klipper name
     * @return Pointer to config, or nullptr if not found
     */
    FilamentSensorConfig* find_config(const std::string& klipper_name);
    const FilamentSensorConfig* find_config(const std::string& klipper_name) const;

    /**
     * @brief Find config by assigned role
     * @return Pointer to config, or nullptr if no sensor has this role
     */
    const FilamentSensorConfig* find_config_by_role(FilamentSensorRole role) const;

    /**
     * @brief Update all LVGL subjects from current state
     */
    void update_subjects();

    // Recursive mutex for thread-safe state access
    // Recursive because update_subjects() calls has_any_runout()/is_motion_active()
    mutable std::recursive_mutex mutex_;

    // Configuration
    bool master_enabled_ = true;
    std::vector<FilamentSensorConfig> sensors_;

    // Runtime state (keyed by klipper_name)
    std::map<std::string, FilamentSensorState> states_;

    // State change callback
    StateChangeCallback state_change_callback_;

    // Test mode: when true, update_from_status() calls update_subjects() synchronously
    // instead of using lv_async_call(). This avoids LVGL timer dependencies in unit tests.
    bool sync_mode_ = false;

    // Startup time for suppressing initial state notifications
    // Similar to USB manager, we suppress toasts for 3 seconds after initialization
    std::chrono::steady_clock::time_point startup_time_;

    // LVGL subjects
    bool subjects_initialized_ = false;
    lv_subject_t runout_detected_;
    lv_subject_t toolhead_detected_;
    lv_subject_t entry_detected_;
    lv_subject_t any_runout_;
    lv_subject_t motion_active_;
    lv_subject_t master_enabled_subject_;
    lv_subject_t sensor_count_;
};

} // namespace helix
