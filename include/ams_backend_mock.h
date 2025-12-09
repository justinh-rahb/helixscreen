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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

/**
 * @file ams_backend_mock.h
 * @brief Mock AMS backend for development and testing
 *
 * Provides a simulated multi-filament system with configurable gates,
 * fake operation timing, and predictable state for UI development.
 *
 * Features:
 * - Configurable gate count (default 4)
 * - Simulated load/unload timing
 * - Pre-populated filament colors and materials
 * - Responds to all AmsBackend operations
 */
class AmsBackendMock : public AmsBackend {
  public:
    /**
     * @brief Construct mock backend with specified gate count
     * @param gate_count Number of simulated gates (1-16, default 4)
     */
    explicit AmsBackendMock(int gate_count = 4);

    ~AmsBackendMock() override;

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

    // ========================================================================
    // Mock-specific methods (for testing)
    // ========================================================================

    /**
     * @brief Simulate an error condition
     * @param error The error to trigger
     */
    void simulate_error(AmsResult error);

    /**
     * @brief Set operation delay for simulated timing
     * @param delay_ms Delay in milliseconds (0 for instant)
     */
    void set_operation_delay(int delay_ms);

    /**
     * @brief Force a specific gate status (for testing)
     * @param gate_index Gate to modify
     * @param status New status
     */
    void force_gate_status(int gate_index, GateStatus status);

  private:
    /**
     * @brief Initialize mock state with sample data
     */
    void init_mock_data();

    /**
     * @brief Emit event to registered callback
     * @param event Event name
     * @param data Event data (JSON or empty)
     */
    void emit_event(const std::string& event, const std::string& data = "");

    /**
     * @brief Simulate async operation completion
     * @param action Action being performed
     * @param complete_event Event to emit on completion
     * @param gate_index Gate involved (-1 if N/A)
     */
    void schedule_completion(AmsAction action, const std::string& complete_event,
                             int gate_index = -1);

    /**
     * @brief Wait for any active operation thread to complete
     */
    void wait_for_operation_thread();

    mutable std::mutex mutex_;         ///< Protects state access
    std::atomic<bool> running_{false}; ///< Backend running state
    EventCallback event_callback_;     ///< Registered event handler

    AmsSystemInfo system_info_;    ///< Simulated system state
    int operation_delay_ms_ = 500; ///< Simulated operation delay

    // Path visualization state
    PathTopology topology_ = PathTopology::HUB;        ///< Simulated topology (default hub for AFC)
    PathSegment filament_segment_ = PathSegment::NONE; ///< Current filament position
    PathSegment error_segment_ = PathSegment::NONE;    ///< Error location (if any)

    // Thread-safe shutdown support
    std::thread operation_thread_;                ///< Current operation thread (if any)
    std::atomic<bool> shutdown_requested_{false}; ///< Signal thread to exit
    std::condition_variable shutdown_cv_;         ///< For interruptible sleep
    mutable std::mutex shutdown_mutex_;           ///< Protects shutdown_cv_ wait
};
