// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRINTER_STATE_H
#define PRINTER_STATE_H

#include "capability_overrides.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <mutex>
#include <string>

#include "hv/json.hpp" // libhv's nlohmann json (via cpputil/)

using json = nlohmann::json;

/**
 * @brief Network connection status states
 */
enum class NetworkStatus {
    DISCONNECTED, ///< No network connection
    CONNECTING,   ///< Connecting to network
    CONNECTED     ///< Connected to network
};

/**
 * @brief Printer connection status states
 */
enum class PrinterStatus {
    DISCONNECTED, ///< Printer not connected
    READY,        ///< Printer connected and ready
    PRINTING,     ///< Printer actively printing
    ERROR         ///< Printer in error state
};

/**
 * @brief Klipper firmware state (klippy_state from Moonraker)
 *
 * Represents the state of the Klipper firmware service, independent of
 * the Moonraker WebSocket connection. When klippy_state is not READY,
 * the printer cannot accept G-code commands even if Moonraker is connected.
 */
enum class KlippyState {
    READY = 0,   ///< Normal operation, printer ready for commands
    STARTUP = 1, ///< Klipper is starting up (during RESTART/FIRMWARE_RESTART)
    SHUTDOWN = 2, ///< Emergency shutdown (M112)
    ERROR = 3    ///< Klipper error state (check klippy.log)
};

/**
 * @brief Print job state (from Moonraker print_stats.state)
 *
 * Represents the state of the current print job as reported by Klipper/Moonraker.
 * This is the canonical enum for print job state throughout HelixScreen.
 *
 * @note Values are chosen to match the integer representation used internally
 *       by MoonrakerClientMock for backward compatibility.
 */
enum class PrintJobState {
    STANDBY = 0,   ///< No active print, printer idle (Moonraker: "standby")
    PRINTING = 1,  ///< Actively printing (Moonraker: "printing")
    PAUSED = 2,    ///< Print paused (Moonraker: "paused")
    COMPLETE = 3,  ///< Print finished successfully (Moonraker: "complete")
    CANCELLED = 4, ///< Print cancelled by user (Moonraker: "cancelled")
    ERROR = 5      ///< Print failed with error (Moonraker: "error")
};

/**
 * @brief Parse Moonraker print state string to PrintJobState enum
 *
 * Converts Moonraker's print_stats.state string to the corresponding enum.
 * Unknown strings default to STANDBY.
 *
 * @param state_str Moonraker state string (e.g., "printing", "paused")
 * @return Corresponding PrintJobState enum value
 */
PrintJobState parse_print_job_state(const char* state_str);

/**
 * @brief Convert PrintJobState enum to display string
 *
 * Returns a human-readable string for UI display.
 *
 * @param state PrintJobState enum value
 * @return Display string (e.g., "Printing", "Paused")
 */
const char* print_job_state_to_string(PrintJobState state);

/**
 * @brief Printer state manager with LVGL 9 reactive subjects
 *
 * Implements hybrid architecture:
 * - LVGL subjects for UI-bound data (automatic reactive updates)
 * - JSON cache for complex data (file lists, capabilities, metadata)
 *
 * All subjects are thread-safe and automatically update bound UI widgets.
 */
class PrinterState {
  public:
    /**
     * @brief Construct printer state manager
     *
     * Initializes internal data structures. Call init_subjects() before
     * creating XML components.
     */
    PrinterState();

    /**
     * @brief Destroy printer state manager
     *
     * Cleans up LVGL subjects and releases resources.
     */
    ~PrinterState();

    /**
     * @brief Initialize all LVGL subjects
     *
     * MUST be called BEFORE creating XML components that bind to these subjects.
     * Can be called multiple times safely - subsequent calls are ignored.
     *
     * @param register_xml If true, registers subjects with LVGL XML system (default).
     *                     Set to false in tests to avoid XML observer creation.
     */
    void init_subjects(bool register_xml = true);

    /**
     * @brief Reset initialization state for testing
     *
     * FOR TESTING ONLY. Clears the initialization flag so init_subjects()
     * can be called again after lv_init() creates a new LVGL context.
     */
    void reset_for_testing();

    /**
     * @brief Update state from Moonraker notification
     *
     * Extracts values from notify_status_update messages and updates subjects.
     * Also maintains JSON cache for complex data.
     *
     * @param notification Parsed JSON notification from Moonraker
     */
    void update_from_notification(const json& notification);

    /**
     * @brief Update state from raw status data
     *
     * Updates subjects from a printer status object. Can be called directly
     * with subscription response data or extracted from notifications.
     * This is the core update logic used by both initial state and notifications.
     *
     * @param status Printer status object (e.g., from result.status or params[0])
     */
    void update_from_status(const json& status);

    /**
     * @brief Get raw JSON state for complex queries
     *
     * Thread-safe access to cached printer state.
     *
     * @return Reference to JSON state object
     */
    json& get_json_state();

    //
    // Subject accessors for XML binding
    //

    // Temperature subjects (integer, degrees Celsius)
    lv_subject_t* get_extruder_temp_subject() {
        return &extruder_temp_;
    }
    lv_subject_t* get_extruder_target_subject() {
        return &extruder_target_;
    }
    lv_subject_t* get_bed_temp_subject() {
        return &bed_temp_;
    }
    lv_subject_t* get_bed_target_subject() {
        return &bed_target_;
    }

    // Print progress subjects
    lv_subject_t* get_print_progress_subject() {
        return &print_progress_;
    } // 0-100
    lv_subject_t* get_print_filename_subject() {
        return &print_filename_;
    }
    lv_subject_t* get_print_state_subject() {
        return &print_state_;
    } // "standby", "printing", "paused", "complete" (string for UI display)

    /**
     * @brief Get print job state enum subject
     *
     * Integer subject holding PrintJobState enum value for type-safe comparisons.
     * Use this for logic, use get_print_state_subject() for UI display binding.
     *
     * @return Pointer to integer subject (cast value to PrintJobState)
     */
    lv_subject_t* get_print_state_enum_subject() {
        return &print_state_enum_;
    }

    /**
     * @brief Get current print job state as enum
     *
     * Convenience method for direct enum access without subject lookup.
     *
     * @return Current PrintJobState
     */
    PrintJobState get_print_job_state() const;

    /**
     * @brief Check if a new print can be started
     *
     * Returns true if the printer is in a state that allows starting a new print.
     * A print can be started when the printer is idle (STANDBY), a previous print
     * finished (COMPLETE, CANCELLED), or the printer recovered from an error (ERROR).
     *
     * @return true if start_print() can be called safely
     */
    [[nodiscard]] bool can_start_new_print() const;

    // Layer tracking subjects (from print_stats.info.current_layer/total_layer)
    lv_subject_t* get_print_layer_current_subject() {
        return &print_layer_current_;
    }
    lv_subject_t* get_print_layer_total_subject() {
        return &print_layer_total_;
    }

    // Motion subjects
    lv_subject_t* get_position_x_subject() {
        return &position_x_;
    }
    lv_subject_t* get_position_y_subject() {
        return &position_y_;
    }
    lv_subject_t* get_position_z_subject() {
        return &position_z_;
    }
    lv_subject_t* get_homed_axes_subject() {
        return &homed_axes_;
    } // "xyz", "xy", etc.

    // Speed/Flow subjects (percentages, 0-100)
    lv_subject_t* get_speed_factor_subject() {
        return &speed_factor_;
    }
    lv_subject_t* get_flow_factor_subject() {
        return &flow_factor_;
    }
    lv_subject_t* get_fan_speed_subject() {
        return &fan_speed_;
    }

    // Printer connection state subjects (Moonraker WebSocket)
    lv_subject_t* get_printer_connection_state_subject() {
        return &printer_connection_state_;
    } // 0=disconnected, 1=connecting, 2=connected, 3=reconnecting, 4=failed
    lv_subject_t* get_printer_connection_message_subject() {
        return &printer_connection_message_;
    } // Status message

    // Network connectivity subject (WiFi/Ethernet)
    lv_subject_t* get_network_status_subject() {
        return &network_status_;
    } // 0=disconnected, 1=connecting, 2=connected (matches NetworkStatus enum)

    // Klipper firmware state subject
    lv_subject_t* get_klippy_state_subject() {
        return &klippy_state_;
    } // 0=ready, 1=startup, 2=shutdown, 3=error (matches KlippyState enum)

    // LED state subject (for home panel light control)
    lv_subject_t* get_led_state_subject() {
        return &led_state_;
    } // 0=off, 1=on (derived from LED color data)

    /**
     * @brief Get excluded objects version subject
     *
     * This subject is incremented whenever the excluded objects list changes.
     * Observers should watch this subject and call get_excluded_objects() to
     * get the updated list when notified.
     *
     * @return Subject pointer (integer, incremented on each change)
     */
    lv_subject_t* get_excluded_objects_version_subject() {
        return &excluded_objects_version_;
    }

    /**
     * @brief Get the current set of excluded objects
     *
     * Returns object names that have been excluded from printing via Klipper's
     * EXCLUDE_OBJECT feature. Updated from Moonraker notify_status_update.
     *
     * @return Reference to the set of excluded object names
     */
    const std::unordered_set<std::string>& get_excluded_objects() const {
        return excluded_objects_;
    }

    /**
     * @brief Update excluded objects from Moonraker status update
     *
     * Called by status update handler when exclude_object.excluded_objects changes.
     * Increments the version subject to notify observers.
     *
     * @param objects Set of object names that are currently excluded
     */
    void set_excluded_objects(const std::unordered_set<std::string>& objects);

    /**
     * @brief Set which LED to track for state updates
     *
     * Call this after loading config to tell PrinterState which LED object
     * to monitor from Moonraker notifications. The LED name should match
     * the Klipper config (e.g., "neopixel chamber_light", "led status_led").
     *
     * @param led_name Full LED name including type prefix, or empty to disable
     */
    void set_tracked_led(const std::string& led_name);

    /**
     * @brief Get the currently tracked LED name
     *
     * @return LED name being tracked, or empty string if none
     */
    const std::string& get_tracked_led() const {
        return tracked_led_name_;
    }

    /**
     * @brief Check if an LED is configured for tracking
     *
     * @return true if a LED name has been set
     */
    bool has_tracked_led() const {
        return !tracked_led_name_.empty();
    }

    /**
     * @brief Set printer connection state (Moonraker WebSocket)
     *
     * Updates both printer_connection_state and printer_connection_message subjects.
     * Called by main.cpp WebSocket callbacks.
     *
     * @param state 0=disconnected, 1=connecting, 2=connected, 3=reconnecting, 4=failed
     * @param message Status message ("Connecting...", "Ready", "Disconnected", etc.)
     */
    void set_printer_connection_state(int state, const char* message);

    /**
     * @brief Check if printer has ever connected this session
     *
     * Returns true if we've successfully connected to Moonraker at least once.
     * Used to distinguish "never connected" (gray icon) from "disconnected after
     * being connected" (yellow warning icon).
     */
    bool was_ever_connected() const {
        return was_ever_connected_;
    }

    /**
     * @brief Set Klipper firmware state
     *
     * Updates klippy_state subject. Called when Moonraker sends klippy state
     * notifications (notify_klippy_ready, notify_klippy_disconnected) or when
     * the mock simulates RESTART/FIRMWARE_RESTART.
     *
     * @param state KlippyState enum value
     */
    void set_klippy_state(KlippyState state);

    /**
     * @brief Set network connectivity status
     *
     * Updates network_status_ subject based on WiFi/Ethernet availability.
     * Called periodically from main.cpp to reflect actual network state.
     *
     * @param status 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED (NetworkStatus enum)
     */
    void set_network_status(int status);

    /**
     * @brief Update printer capability subjects from PrinterCapabilities
     *
     * Updates subjects that control visibility of pre-print option checkboxes.
     * Applies user-configured overrides from helixconfig.json before updating subjects.
     * Called by main.cpp after MoonrakerClient::discover_printer() completes.
     *
     * @param caps PrinterCapabilities populated from printer.objects.list
     */
    void set_printer_capabilities(const PrinterCapabilities& caps);

    /**
     * @brief Get the capability overrides for external access
     *
     * Allows other components to check effective capability availability
     * with user overrides applied.
     *
     * @return Reference to the CapabilityOverrides instance
     */
    [[nodiscard]] const CapabilityOverrides& get_capability_overrides() const {
        return capability_overrides_;
    }

  private:
    // Temperature subjects
    lv_subject_t extruder_temp_;
    lv_subject_t extruder_target_;
    lv_subject_t bed_temp_;
    lv_subject_t bed_target_;

    // Print progress subjects
    lv_subject_t print_progress_;   // Integer 0-100
    lv_subject_t print_filename_;   // String buffer
    lv_subject_t print_state_;      // String buffer (for UI display binding)
    lv_subject_t print_state_enum_; // Integer: PrintJobState enum (for type-safe logic)

    // Layer tracking subjects (from Moonraker print_stats.info)
    lv_subject_t print_layer_current_; // Current layer (0-based)
    lv_subject_t print_layer_total_;   // Total layers from file metadata

    // Motion subjects
    lv_subject_t position_x_;
    lv_subject_t position_y_;
    lv_subject_t position_z_;
    lv_subject_t homed_axes_; // String buffer

    // Speed/Flow subjects
    lv_subject_t speed_factor_;
    lv_subject_t flow_factor_;
    lv_subject_t fan_speed_;

    // Printer connection state subjects (Moonraker WebSocket)
    lv_subject_t printer_connection_state_;   // Integer: uses PrinterStatus enum values
    lv_subject_t printer_connection_message_; // String buffer

    // Network connectivity subject (WiFi/Ethernet)
    lv_subject_t network_status_; // Integer: uses NetworkStatus enum values

    // Klipper firmware state subject
    lv_subject_t klippy_state_; // Integer: uses KlippyState enum values

    // LED state subject
    lv_subject_t led_state_; // Integer: 0=off, 1=on

    // Exclude object subjects
    lv_subject_t excluded_objects_version_;            // Integer: incremented on change
    std::unordered_set<std::string> excluded_objects_; // Set of excluded object names

    // Printer capability subjects (for pre-print options visibility)
    lv_subject_t printer_has_qgl_;          // Integer: 0=no, 1=yes
    lv_subject_t printer_has_z_tilt_;       // Integer: 0=no, 1=yes
    lv_subject_t printer_has_bed_mesh_;     // Integer: 0=no, 1=yes
    lv_subject_t printer_has_nozzle_clean_; // Integer: 0=no, 1=yes

    // Tracked LED name (e.g., "neopixel chamber_light")
    std::string tracked_led_name_;

    // String buffers for subject storage
    char print_filename_buf_[256];
    char print_state_buf_[32];
    char homed_axes_buf_[8];
    char printer_connection_message_buf_[128];

    // JSON cache for complex data
    json json_state_;
    std::mutex state_mutex_;

    // Initialization guard to prevent multiple subject initializations
    bool subjects_initialized_ = false;

    // Track if we've ever successfully connected (for UI display)
    bool was_ever_connected_ = false;

    // Capability override layer (user config overrides for auto-detected capabilities)
    CapabilityOverrides capability_overrides_;
};

#endif // PRINTER_STATE_H
