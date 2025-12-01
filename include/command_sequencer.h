// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gcode_ops_detector.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "hv/json.hpp"

using json = nlohmann::json;

// Forward declarations
class MoonrakerClient;
class MoonrakerAPI;
class PrinterState;

namespace gcode {

/**
 * @brief Parameters for a sequenced operation
 */
struct OperationParams {
    std::string filename;                     ///< For START_PRINT
    double temperature = 0.0;                 ///< For preheat operations
    int duration_minutes = 0;                 ///< For chamber soak
    std::string profile_name;                 ///< For BED_MESH_PROFILE LOAD=...
    std::map<std::string, std::string> extra; ///< Additional macro parameters
};

/**
 * @brief A single operation in the command queue
 */
struct QueuedOperation {
    OperationType type;
    OperationParams params;
    std::string display_name;                  ///< Human-readable name for UI
    std::chrono::milliseconds timeout{300000}; ///< Default 5 minutes
};

/**
 * @brief State of the command sequencer
 */
enum class SequencerState {
    IDLE,       ///< No operations queued or running
    RUNNING,    ///< Executing operations
    WAITING,    ///< Waiting for operation completion (state change)
    CANCELLING, ///< Cancel requested, waiting for safe stop
    CANCELLED,  ///< Sequence was cancelled
    COMPLETED,  ///< All operations finished successfully
    FAILED,     ///< An operation failed or timed out
};

/**
 * @brief Completion detection strategy for an operation type
 *
 * Defines what Moonraker state to watch and what condition indicates completion.
 */
struct CompletionCondition {
    std::string object_name;                ///< Moonraker object to watch (e.g., "toolhead")
    std::string field_path;                 ///< JSON path within object (e.g., "homed_axes")
    std::function<bool(const json&)> check; ///< Returns true when complete
};

/**
 * @brief Manages sequential execution of printer operations with state-based completion.
 *
 * Executes G-code commands in sequence, waiting for each operation to complete
 * by monitoring Moonraker state changes. Provides progress callbacks and
 * 2-level cancellation support.
 *
 * Thread-safe. Destructor cancels any in-progress operations.
 *
 * @code
 * CommandSequencer seq(client, api, state);
 *
 * seq.add_operation(OperationType::HOMING, {}, "Homing");
 * seq.add_operation(OperationType::QGL, {}, "Leveling Gantry");
 * seq.add_operation(OperationType::BED_LEVELING, {}, "Probing Bed");
 *
 * seq.start(
 *     [](const std::string& op, int step, int total, float progress) {
 *         ui_update_progress(op, step, total, progress);
 *     },
 *     [](bool success, const std::string& error) {
 *         if (success) start_print();
 *         else show_error(error);
 *     });
 * @endcode
 */
class CommandSequencer {
  public:
    using ProgressCallback = std::function<void(const std::string& operation_name, int current_step,
                                                int total_steps, float estimated_progress)>;

    using CompletionCallback = std::function<void(bool success, const std::string& error_message)>;

    /**
     * @brief Construct sequencer with required dependencies
     *
     * @param client MoonrakerClient for sending commands
     * @param api MoonrakerAPI for high-level operations
     * @param state PrinterState for state subscriptions
     */
    CommandSequencer(MoonrakerClient& client, MoonrakerAPI& api, PrinterState& state);

    ~CommandSequencer();

    // Non-copyable, non-movable (owns subscription state)
    CommandSequencer(const CommandSequencer&) = delete;
    CommandSequencer& operator=(const CommandSequencer&) = delete;
    CommandSequencer(CommandSequencer&&) = delete;
    CommandSequencer& operator=(CommandSequencer&&) = delete;

    // ========================================================================
    // Queue Management
    // ========================================================================

    /**
     * @brief Add an operation to the queue
     *
     * Must be called before start(). Cannot add operations while running.
     *
     * @param type Operation type
     * @param params Operation parameters
     * @param display_name Human-readable name for progress UI
     * @param timeout Optional timeout override
     */
    void add_operation(OperationType type, const OperationParams& params,
                       const std::string& display_name,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds{300000});

    /**
     * @brief Clear all queued operations
     *
     * Cannot be called while running.
     */
    void clear();

    /**
     * @brief Get number of operations in queue
     */
    [[nodiscard]] size_t queue_size() const;

    // ========================================================================
    // Execution Control
    // ========================================================================

    /**
     * @brief Start executing queued operations
     *
     * @param on_progress Called when operation status changes
     * @param on_complete Called when sequence finishes (success or failure)
     * @return true if started, false if already running or queue empty
     */
    bool start(ProgressCallback on_progress, CompletionCallback on_complete);

    /**
     * @brief Request cancellation of current sequence
     *
     * 2-level escalation:
     * - First call: CANCEL_PRINT + M400 (graceful stop)
     * - Second call within 5 seconds: M112 (emergency stop)
     *
     * @return true if cancel initiated, false if not running
     */
    bool cancel();

    // ========================================================================
    // State Queries
    // ========================================================================

    /**
     * @brief Get current sequencer state
     */
    [[nodiscard]] SequencerState state() const {
        return sequencer_state_.load();
    }

    /**
     * @brief Get current step number (1-indexed)
     */
    [[nodiscard]] int current_step() const {
        return current_step_.load();
    }

    /**
     * @brief Get total number of steps
     */
    [[nodiscard]] int total_steps() const {
        return total_steps_.load();
    }

    /**
     * @brief Check if sequencer is currently executing
     */
    [[nodiscard]] bool is_running() const {
        auto s = sequencer_state_.load();
        return s == SequencerState::RUNNING || s == SequencerState::WAITING;
    }

    /**
     * @brief Get name of currently executing operation
     */
    [[nodiscard]] std::string current_operation_name() const;

    // ========================================================================
    // State Update Processing
    // ========================================================================

    /**
     * @brief Process Moonraker state update notification
     *
     * Should be called from the Moonraker notification handler.
     * Checks if current operation has completed.
     *
     * @param notification Full notify_status_update JSON
     */
    void process_state_update(const json& notification);

    // ========================================================================
    // Testing Support
    // ========================================================================

    /**
     * @brief Simulate state update for testing
     *
     * Directly triggers completion check without going through Moonraker.
     */
    void simulate_state_update(const json& status);

    /**
     * @brief Get the completion condition for an operation type
     *
     * Useful for testing to understand what state is expected.
     */
    [[nodiscard]] static CompletionCondition get_completion_condition(OperationType type);

    /**
     * @brief Force state for testing
     */
    void force_state(SequencerState new_state) {
        sequencer_state_.store(new_state);
    }

  private:
    /**
     * @brief Execute the next operation in the queue
     */
    void execute_next();

    /**
     * @brief Send G-code command for an operation
     */
    void send_operation_command(const QueuedOperation& op);

    /**
     * @brief Check if operation has completed based on current state
     */
    bool check_operation_complete(const QueuedOperation& op, const json& status);

    /**
     * @brief Handle operation timeout
     */
    void handle_timeout();

    /**
     * @brief Handle operation failure
     */
    void handle_failure(const std::string& error);

    /**
     * @brief Invoke progress callback safely
     */
    void notify_progress();

    /**
     * @brief Invoke completion callback safely
     */
    void notify_complete(bool success, const std::string& error);

    /**
     * @brief Generate G-code command for operation
     */
    [[nodiscard]] std::string generate_gcode(const QueuedOperation& op) const;

    // Dependencies (references - must remain valid)
    // client_ and printer_state_ reserved for future state-aware sequencing
    MoonrakerClient& client_;
    MoonrakerAPI& api_;
    PrinterState& printer_state_;

    // Queue and current operation
    std::queue<QueuedOperation> queue_;
    std::optional<QueuedOperation> current_op_;
    mutable std::mutex queue_mutex_;

    // Sequencer state machine
    std::atomic<SequencerState> sequencer_state_{SequencerState::IDLE};
    std::atomic<int> current_step_{0};
    std::atomic<int> total_steps_{0};

    // Callbacks (protected by mutex since they're not atomic)
    ProgressCallback on_progress_;
    CompletionCallback on_complete_;
    std::mutex callback_mutex_;

    // Timeout tracking
    std::chrono::steady_clock::time_point operation_start_time_;
    std::chrono::milliseconds current_timeout_{0};

    // Cancellation state
    std::atomic<bool> cancel_requested_{false};
    std::chrono::steady_clock::time_point last_cancel_time_;
    static constexpr auto ESCALATION_WINDOW = std::chrono::seconds{5};
};

/**
 * @brief Get human-readable name for sequencer state
 */
[[nodiscard]] std::string sequencer_state_name(SequencerState state);

} // namespace gcode
