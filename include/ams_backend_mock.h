// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

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
 * Provides a simulated multi-filament system with configurable slots,
 * fake operation timing, and predictable state for UI development.
 *
 * Features:
 * - Configurable slot count (default 4)
 * - Simulated load/unload timing
 * - Pre-populated filament colors and materials
 * - Responds to all AmsBackend operations
 */
class AmsBackendMock : public AmsBackend {
  public:
    /**
     * @brief Construct mock backend with specified slot count
     * @param slot_count Number of simulated slots (1-16, default 4)
     */
    explicit AmsBackendMock(int slot_count = 4);

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
    [[nodiscard]] SlotInfo get_slot_info(int slot_index) const override;
    [[nodiscard]] AmsAction get_current_action() const override;
    [[nodiscard]] int get_current_tool() const override;
    [[nodiscard]] int get_current_slot() const override;
    [[nodiscard]] bool is_filament_loaded() const override;

    // Path visualization
    [[nodiscard]] PathTopology get_topology() const override;
    [[nodiscard]] PathSegment get_filament_segment() const override;
    [[nodiscard]] PathSegment get_slot_filament_segment(int slot_index) const override;
    [[nodiscard]] PathSegment infer_error_segment() const override;

    // Operations
    AmsError load_filament(int slot_index) override;
    AmsError unload_filament() override;
    AmsError select_slot(int slot_index) override;
    AmsError change_tool(int tool_number) override;

    // Recovery
    AmsError recover() override;
    AmsError reset() override;
    AmsError cancel() override;

    // Configuration
    AmsError set_slot_info(int slot_index, const SlotInfo& info) override;
    AmsError set_tool_mapping(int tool_number, int slot_index) override;

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
     * @brief Force a specific slot status (for testing)
     * @param slot_index Slot to modify
     * @param status New status
     */
    void force_slot_status(int slot_index, SlotStatus status);

    /**
     * @brief Set whether this mock simulates a hardware bypass sensor
     * @param has_sensor true=hardware sensor (auto-detect), false=virtual (manual toggle)
     *
     * When has_sensor is true:
     * - The bypass button should be disabled in the UI
     * - Bypass is controlled by the sensor, not user clicks
     */
    void set_has_hardware_bypass_sensor(bool has_sensor);

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
     * @param slot_index Slot involved (-1 if N/A)
     */
    void schedule_completion(AmsAction action, const std::string& complete_event,
                             int slot_index = -1);

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
