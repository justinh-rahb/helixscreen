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

#pragma once

#include "ams_backend.h"
#include "moonraker_client.h"

#include <atomic>
#include <mutex>

// Forward declaration
class MoonrakerAPI;

/**
 * @file ams_backend_happy_hare.h
 * @brief Happy Hare MMU backend implementation
 *
 * Implements the AmsBackend interface for Happy Hare MMU systems.
 * Communicates with Moonraker to control the MMU via G-code commands
 * and receives state updates via printer.mmu.* subscriptions.
 *
 * Happy Hare Moonraker Variables:
 * - printer.mmu.gate       (int): Current gate (-1=none, -2=bypass)
 * - printer.mmu.tool       (int): Current tool
 * - printer.mmu.filament   (string): "Loaded" or "Unloaded"
 * - printer.mmu.action     (string): "Idle", "Loading", etc.
 * - printer.mmu.gate_status (array[int]): -1=unknown, 0=empty, 1=available, 2=from_buffer
 * - printer.mmu.gate_color_rgb (array[int]): RGB values like 0xFF0000
 * - printer.mmu.gate_material (array[string]): "PLA", "PETG", etc.
 *
 * G-code Commands:
 * - MMU_LOAD GATE={n}   - Load filament from specified gate
 * - MMU_UNLOAD          - Unload current filament
 * - MMU_SELECT GATE={n} - Select gate without loading
 * - T{n}                - Tool change (unload + load)
 * - MMU_HOME            - Home the selector
 * - MMU_RECOVER         - Attempt error recovery
 */
class AmsBackendHappyHare : public AmsBackend {
  public:
    /**
     * @brief Construct Happy Hare backend
     *
     * @param api Pointer to MoonrakerAPI (for sending G-code commands)
     * @param client Pointer to MoonrakerClient (for subscribing to updates)
     *
     * @note Both pointers must remain valid for the lifetime of this backend.
     */
    AmsBackendHappyHare(MoonrakerAPI* api, MoonrakerClient* client);

    ~AmsBackendHappyHare() override;

    // Lifecycle
    AmsError start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;

    // Events
    void set_event_callback(EventCallback callback) override;

    // State queries
    [[nodiscard]] AmsSystemInfo get_system_info() const override;
    [[nodiscard]] AmsType get_type() const override;
    [[nodiscard]] GateInfo get_gate_info(int global_index) const override;
    [[nodiscard]] AmsAction get_current_action() const override;
    [[nodiscard]] int get_current_tool() const override;
    [[nodiscard]] int get_current_gate() const override;
    [[nodiscard]] bool is_filament_loaded() const override;

    // Path visualization
    [[nodiscard]] PathTopology get_topology() const override;
    [[nodiscard]] PathSegment get_filament_segment() const override;
    [[nodiscard]] PathSegment infer_error_segment() const override;

    // Operations
    AmsError load_filament(int gate_index) override;
    AmsError unload_filament() override;
    AmsError select_gate(int gate_index) override;
    AmsError change_tool(int tool_number) override;

    // Recovery
    AmsError recover() override;
    AmsError reset() override;
    AmsError cancel() override;

    // Configuration
    AmsError set_gate_info(int gate_index, const GateInfo& info) override;
    AmsError set_tool_mapping(int tool_number, int gate_index) override;

    // Bypass mode
    AmsError enable_bypass() override;
    AmsError disable_bypass() override;
    [[nodiscard]] bool is_bypass_active() const override;

  private:
    /**
     * @brief Handle status update notifications from Moonraker
     *
     * Called when printer.mmu.* values change via notify_status_update.
     * Parses the JSON and updates internal state.
     *
     * @param notification JSON notification from Moonraker
     */
    void handle_status_update(const nlohmann::json& notification);

    /**
     * @brief Parse MMU state from Moonraker JSON
     *
     * Extracts mmu object from notification and updates system_info_.
     *
     * @param mmu_data JSON object containing printer.mmu data
     */
    void parse_mmu_state(const nlohmann::json& mmu_data);

    /**
     * @brief Initialize gate structures based on gate_status array size
     *
     * Called when we first receive gate_status to create the correct
     * number of GateInfo entries.
     *
     * @param gate_count Number of gates detected
     */
    void initialize_gates(int gate_count);

    /**
     * @brief Emit event to registered callback
     * @param event Event name
     * @param data Event data (JSON or empty)
     */
    void emit_event(const std::string& event, const std::string& data = "");

    /**
     * @brief Execute a G-code command via MoonrakerAPI
     *
     * @param gcode The G-code command to execute
     * @return AmsError indicating success or failure to queue command
     */
    AmsError execute_gcode(const std::string& gcode);

    /**
     * @brief Check common preconditions before operations
     *
     * Validates:
     * - Backend is running
     * - System is not busy
     *
     * @return AmsError (SUCCESS if ok, appropriate error otherwise)
     */
    AmsError check_preconditions() const;

    /**
     * @brief Validate gate index is within range
     *
     * @param gate_index Gate index to validate
     * @return AmsError (SUCCESS if valid, INVALID_GATE otherwise)
     */
    AmsError validate_gate_index(int gate_index) const;

    // Dependencies
    MoonrakerAPI* api_;       ///< For sending G-code commands
    MoonrakerClient* client_; ///< For subscribing to updates

    // State
    mutable std::mutex mutex_;          ///< Protects state access
    std::atomic<bool> running_{false};  ///< Backend running state
    EventCallback event_callback_;      ///< Registered event handler
    SubscriptionId subscription_id_{0}; ///< Moonraker subscription ID

    // Cached MMU state
    AmsSystemInfo system_info_;     ///< Current system state
    bool gates_initialized_{false}; ///< Have we seen gate_status yet?

    // Path visualization state
    int filament_pos_{0};                          ///< Happy Hare filament_pos value
    PathSegment error_segment_{PathSegment::NONE}; ///< Inferred error location
};
