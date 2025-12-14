// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ams_error.h"
#include "ams_types.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @file ams_backend.h
 * @brief Abstract interface for AMS/MMU backend implementations
 *
 * Provides a platform-agnostic API for multi-filament operations.
 * Concrete implementations handle system-specific details:
 * - AmsBackendHappyHare: Happy Hare MMU via Moonraker
 * - AmsBackendAfc: AFC-Klipper-Add-On via Moonraker
 * - AmsBackendMock: Simulator mode with fake data
 *
 * Design principles:
 * - Hide all backend-specific commands/protocols from AmsManager
 * - Provide async operations with event-based completion
 * - Thread-safe operations where needed
 * - Clean error handling with user-friendly messages
 */
class AmsBackend {
  public:
    virtual ~AmsBackend() = default;

    // ========================================================================
    // Event Types
    // ========================================================================

    /**
     * @brief Standard AMS event types
     *
     * Events are delivered asynchronously via registered callbacks.
     * Event names are strings to allow backend-specific extensions.
     */
    static constexpr const char* EVENT_STATE_CHANGED = "STATE_CHANGED"; ///< System state updated
    static constexpr const char* EVENT_SLOT_CHANGED = "SLOT_CHANGED";   ///< Slot info updated
    static constexpr const char* EVENT_LOAD_COMPLETE = "LOAD_COMPLETE"; ///< Load operation finished
    static constexpr const char* EVENT_UNLOAD_COMPLETE =
        "UNLOAD_COMPLETE";                                            ///< Unload operation finished
    static constexpr const char* EVENT_TOOL_CHANGED = "TOOL_CHANGED"; ///< Tool change completed
    static constexpr const char* EVENT_ERROR = "ERROR";               ///< Error occurred
    static constexpr const char* EVENT_ATTENTION_REQUIRED =
        "ATTENTION"; ///< User intervention needed

    // ========================================================================
    // Lifecycle Management
    // ========================================================================

    /**
     * @brief Initialize and start the AMS backend
     *
     * Connects to the underlying AMS system and starts monitoring state.
     * For real backends, this initiates Moonraker subscriptions.
     * For mock backend, this sets up simulated state.
     *
     * @return AmsError with detailed status information
     */
    virtual AmsError start() = 0;

    /**
     * @brief Stop the AMS backend
     *
     * Cleanly shuts down monitoring and releases resources.
     * Safe to call even if not started.
     */
    virtual void stop() = 0;

    /**
     * @brief Check if backend is currently running/initialized
     * @return true if backend is active and ready for operations
     */
    [[nodiscard]] virtual bool is_running() const = 0;

    // ========================================================================
    // Event System
    // ========================================================================

    /**
     * @brief Callback type for AMS events
     *
     * @param event_name Event identifier (EVENT_* constants)
     * @param data Event-specific payload (JSON string or empty)
     */
    using EventCallback =
        std::function<void(const std::string& event_name, const std::string& data)>;

    /**
     * @brief Register callback for AMS events
     *
     * Events are delivered asynchronously and may arrive from background threads.
     * The callback should be thread-safe or post to main thread.
     *
     * @param callback Handler function for events
     */
    virtual void set_event_callback(EventCallback callback) = 0;

    // ========================================================================
    // State Queries
    // ========================================================================

    /**
     * @brief Get current AMS system information
     *
     * Returns a snapshot of the current system state including:
     * - System type and version
     * - Current tool/slot selection
     * - All unit and slot information
     * - Capability flags
     *
     * @return Current AmsSystemInfo (copy, safe for caller to hold)
     */
    [[nodiscard]] virtual AmsSystemInfo get_system_info() const = 0;

    /**
     * @brief Get the detected AMS type
     * @return AmsType enum value
     */
    [[nodiscard]] virtual AmsType get_type() const = 0;

    /**
     * @brief Get information about a specific slot
     * @param slot_index Slot index (0 to total_slots-1)
     * @return SlotInfo struct (copy, safe for caller to hold)
     */
    [[nodiscard]] virtual SlotInfo get_slot_info(int slot_index) const = 0;

    /**
     * @brief Get current action/operation status
     * @return Current AmsAction enum value
     */
    [[nodiscard]] virtual AmsAction get_current_action() const = 0;

    /**
     * @brief Get currently selected tool number
     * @return Tool number (-1 if none, -2 for bypass on Happy Hare)
     */
    [[nodiscard]] virtual int get_current_tool() const = 0;

    /**
     * @brief Get currently selected slot number
     * @return Slot number (-1 if none, -2 for bypass on Happy Hare)
     */
    [[nodiscard]] virtual int get_current_slot() const = 0;

    /**
     * @brief Check if filament is currently loaded in extruder
     * @return true if filament is loaded
     */
    [[nodiscard]] virtual bool is_filament_loaded() const = 0;

    // ========================================================================
    // Filament Path Visualization
    // ========================================================================

    /**
     * @brief Get the path topology for this AMS system
     *
     * Determines how the filament path is rendered:
     * - LINEAR: Selector picks from multiple gates (Happy Hare ERCF)
     * - HUB: Multiple lanes merge through a hub (AFC Box Turtle)
     *
     * @return PathTopology enum value
     */
    [[nodiscard]] virtual PathTopology get_topology() const = 0;

    /**
     * @brief Get current filament position in the path
     *
     * Returns which segment the filament is currently at/in.
     * Used for highlighting the active portion of the path visualization.
     *
     * @return PathSegment enum value (NONE if no filament in system)
     */
    [[nodiscard]] virtual PathSegment get_filament_segment() const = 0;

    /**
     * @brief Get filament position for a specific slot
     *
     * Returns how far filament from a specific slot extends into the path.
     * Used for visualizing all installed filaments, not just the active one.
     * For non-active slots, this typically shows filament up to the prep sensor.
     *
     * @param slot_index Slot index (0 to total_slots-1)
     * @return PathSegment enum value (NONE if no filament installed at slot)
     */
    [[nodiscard]] virtual PathSegment get_slot_filament_segment(int slot_index) const = 0;

    /**
     * @brief Infer which segment has an error
     *
     * When an error occurs, this determines which segment of the path
     * is most likely the problem area based on sensor states and
     * current operation. Used for visual error highlighting.
     *
     * @return PathSegment enum value (NONE if no error or can't determine)
     */
    [[nodiscard]] virtual PathSegment infer_error_segment() const = 0;

    // ========================================================================
    // Filament Operations
    // ========================================================================

    /**
     * @brief Load filament from specified slot (async)
     *
     * Initiates filament load from the specified slot to the extruder.
     * Results delivered via EVENT_LOAD_COMPLETE or EVENT_ERROR.
     *
     * Requires:
     * - System not busy with another operation
     * - Slot has filament available
     * - Extruder at appropriate temperature
     *
     * @param slot_index Slot to load from (0-based)
     * @return AmsError indicating if operation was started successfully
     */
    virtual AmsError load_filament(int slot_index) = 0;

    /**
     * @brief Unload current filament (async)
     *
     * Initiates filament unload from extruder back to current slot.
     * Results delivered via EVENT_UNLOAD_COMPLETE or EVENT_ERROR.
     *
     * Requires:
     * - Filament currently loaded
     * - System not busy with another operation
     * - Extruder at appropriate temperature
     *
     * @return AmsError indicating if operation was started successfully
     */
    virtual AmsError unload_filament() = 0;

    /**
     * @brief Select tool/slot without loading (async)
     *
     * Moves the selector to the specified slot without loading filament.
     * Used for preparation or manual operations.
     *
     * @param slot_index Slot to select (0-based)
     * @return AmsError indicating if operation was started successfully
     */
    virtual AmsError select_slot(int slot_index) = 0;

    /**
     * @brief Perform tool change (async)
     *
     * Complete tool change sequence: unload current, load new.
     * Equivalent to sending T{tool_number} command.
     * Results delivered via EVENT_TOOL_CHANGED or EVENT_ERROR.
     *
     * @param tool_number Tool to change to (0-based)
     * @return AmsError indicating if operation was started successfully
     */
    virtual AmsError change_tool(int tool_number) = 0;

    // ========================================================================
    // Recovery Operations
    // ========================================================================

    /**
     * @brief Attempt recovery from error state
     *
     * Initiates system recovery procedure appropriate to current error.
     * For Happy Hare, this typically invokes MMU_RECOVER.
     *
     * @return AmsError indicating if recovery was started
     */
    virtual AmsError recover() = 0;

    /**
     * @brief Reset the AMS system (async)
     *
     * Resets the system to a known good state.
     * - Happy Hare: Calls MMU_HOME to home the selector
     * - AFC: Calls AFC_RESET to reset the system
     *
     * @return AmsError indicating if operation was started
     */
    virtual AmsError reset() = 0;

    /**
     * @brief Cancel current operation
     *
     * Attempts to safely abort the current operation.
     * Not all operations can be cancelled.
     *
     * @return AmsError indicating if cancellation was accepted
     */
    virtual AmsError cancel() = 0;

    // ========================================================================
    // Configuration Operations
    // ========================================================================

    /**
     * @brief Update slot filament information
     *
     * Sets the color, material, and other filament info for a slot.
     * Changes are persisted via Moonraker/Spoolman as appropriate.
     *
     * @param slot_index Slot to update (0-based)
     * @param info New slot information (only filament fields used)
     * @return AmsError indicating if update succeeded
     */
    virtual AmsError set_slot_info(int slot_index, const SlotInfo& info) = 0;

    /**
     * @brief Set tool-to-slot mapping
     *
     * Configures which slot a tool number maps to.
     * Happy Hare specific - may not be supported on all backends.
     *
     * @param tool_number Tool number (0-based)
     * @param slot_index Slot to map to (0-based)
     * @return AmsError indicating if mapping was set
     */
    virtual AmsError set_tool_mapping(int tool_number, int slot_index) = 0;

    // ========================================================================
    // Bypass Mode Operations
    // ========================================================================

    /**
     * @brief Enable bypass mode
     *
     * Activates bypass mode where an external spool feeds directly to the
     * toolhead, bypassing the MMU/hub system. Sets current_slot to -2.
     *
     * Not all backends support bypass mode - check supports_bypass flag.
     *
     * @return AmsError indicating if bypass was enabled
     */
    virtual AmsError enable_bypass() = 0;

    /**
     * @brief Disable bypass mode
     *
     * Deactivates bypass mode. Filament should be unloaded from toolhead first.
     *
     * @return AmsError indicating if bypass was disabled
     */
    virtual AmsError disable_bypass() = 0;

    /**
     * @brief Check if bypass mode is currently active
     * @return true if bypass is active (current_slot == -2)
     */
    [[nodiscard]] virtual bool is_bypass_active() const = 0;

    // ========================================================================
    // Factory Method
    // ========================================================================

    /**
     * @brief Create appropriate backend for detected AMS type (mock only)
     *
     * Factory method that creates a mock backend for testing.
     * For real backends, use the overload that accepts MoonrakerAPI and MoonrakerClient.
     *
     * In mock mode (RuntimeConfig::should_mock_ams()), returns AmsBackendMock.
     *
     * @param detected_type The detected AMS type from printer discovery
     * @return Unique pointer to backend instance, or nullptr if type is NONE
     * @deprecated Use create(AmsType, MoonrakerAPI*, MoonrakerClient*) for real backends
     */
    static std::unique_ptr<AmsBackend> create(AmsType detected_type);

    /**
     * @brief Create appropriate backend for detected AMS type with dependencies
     *
     * Factory method that creates the correct backend implementation:
     * - HAPPY_HARE: AmsBackendHappyHare (requires api and client)
     * - AFC: AmsBackendAfc (requires api and client)
     * - NONE: nullptr (no AMS detected)
     *
     * In mock mode (RuntimeConfig::should_mock_ams()), returns AmsBackendMock.
     *
     * @param detected_type The detected AMS type from printer discovery
     * @param api Pointer to MoonrakerAPI for sending commands
     * @param client Pointer to MoonrakerClient for subscriptions
     * @return Unique pointer to backend instance, or nullptr if type is NONE
     */
    static std::unique_ptr<AmsBackend> create(AmsType detected_type, class MoonrakerAPI* api,
                                              class MoonrakerClient* client);

    /**
     * @brief Create mock backend for testing
     *
     * Creates a mock backend regardless of actual printer state.
     * Used when --test flag is passed or for development.
     *
     * @param slot_count Number of simulated slots (default 4)
     * @return Unique pointer to mock backend instance
     */
    static std::unique_ptr<AmsBackend> create_mock(int slot_count = 4);
};
