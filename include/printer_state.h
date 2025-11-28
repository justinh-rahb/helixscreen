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
    } // "standby", "printing", "paused", "complete"

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

    // LED state subject (for home panel light control)
    lv_subject_t* get_led_state_subject() {
        return &led_state_;
    } // 0=off, 1=on (derived from LED color data)

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
     * @brief Set network connectivity status
     *
     * Updates network_status_ subject based on WiFi/Ethernet availability.
     * Called periodically from main.cpp to reflect actual network state.
     *
     * @param status 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED (NetworkStatus enum)
     */
    void set_network_status(int status);

  private:
    // Temperature subjects
    lv_subject_t extruder_temp_;
    lv_subject_t extruder_target_;
    lv_subject_t bed_temp_;
    lv_subject_t bed_target_;

    // Print progress subjects
    lv_subject_t print_progress_; // Integer 0-100
    lv_subject_t print_filename_; // String buffer
    lv_subject_t print_state_;    // String buffer

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

    // LED state subject
    lv_subject_t led_state_; // Integer: 0=off, 1=on

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
};

#endif // PRINTER_STATE_H
