// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file printer_print_state.cpp
 * @brief Print state management extracted from PrinterState
 *
 * Manages print subjects including progress, state, timing, layers, and
 * print start phases. Extracted from PrinterState as part of god class decomposition.
 */

#include "printer_print_state.h"

#include "async_helpers.h"
#include "printer_state.h" // For enum definitions
#include "unit_conversions.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace helix {

PrinterPrintState::PrinterPrintState() {
    // Initialize string buffers
    std::memset(print_filename_buf_, 0, sizeof(print_filename_buf_));
    std::memset(print_display_filename_buf_, 0, sizeof(print_display_filename_buf_));
    std::memset(print_thumbnail_path_buf_, 0, sizeof(print_thumbnail_path_buf_));
    std::memset(print_state_buf_, 0, sizeof(print_state_buf_));
    std::memset(print_start_message_buf_, 0, sizeof(print_start_message_buf_));

    // Set default values
    std::strcpy(print_state_buf_, "standby");
}

void PrinterPrintState::init_subjects(bool register_xml) {
    if (subjects_initialized_) {
        spdlog::debug("[PrinterPrintState] Subjects already initialized, skipping");
        return;
    }

    spdlog::debug("[PrinterPrintState] Initializing subjects (register_xml={})", register_xml);

    // Print progress subjects
    lv_subject_init_int(&print_progress_, 0);
    lv_subject_init_string(&print_filename_, print_filename_buf_, nullptr,
                           sizeof(print_filename_buf_), "");
    lv_subject_init_string(&print_state_, print_state_buf_, nullptr, sizeof(print_state_buf_),
                           "standby");
    lv_subject_init_int(&print_state_enum_, static_cast<int>(PrintJobState::STANDBY));
    lv_subject_init_int(&print_outcome_, static_cast<int>(PrintOutcome::NONE));
    lv_subject_init_int(&print_active_, 0);
    lv_subject_init_int(&print_show_progress_, 0);
    lv_subject_init_string(&print_display_filename_, print_display_filename_buf_, nullptr,
                           sizeof(print_display_filename_buf_), "");
    lv_subject_init_string(&print_thumbnail_path_, print_thumbnail_path_buf_, nullptr,
                           sizeof(print_thumbnail_path_buf_), "");

    // Layer tracking subjects
    lv_subject_init_int(&print_layer_current_, 0);
    lv_subject_init_int(&print_layer_total_, 0);

    // Print time tracking subjects
    lv_subject_init_int(&print_duration_, 0);
    lv_subject_init_int(&print_time_left_, 0);

    // Print start progress subjects
    lv_subject_init_int(&print_start_phase_, static_cast<int>(PrintStartPhase::IDLE));
    lv_subject_init_string(&print_start_message_, print_start_message_buf_, nullptr,
                           sizeof(print_start_message_buf_), "");
    lv_subject_init_int(&print_start_progress_, 0);

    // Print workflow in-progress subject
    lv_subject_init_int(&print_in_progress_, 0);

    // Register with SubjectManager for automatic cleanup
    subjects_.register_subject(&print_progress_);
    subjects_.register_subject(&print_filename_);
    subjects_.register_subject(&print_state_);
    subjects_.register_subject(&print_state_enum_);
    subjects_.register_subject(&print_outcome_);
    subjects_.register_subject(&print_active_);
    subjects_.register_subject(&print_show_progress_);
    subjects_.register_subject(&print_display_filename_);
    subjects_.register_subject(&print_thumbnail_path_);
    subjects_.register_subject(&print_layer_current_);
    subjects_.register_subject(&print_layer_total_);
    subjects_.register_subject(&print_duration_);
    subjects_.register_subject(&print_time_left_);
    subjects_.register_subject(&print_start_phase_);
    subjects_.register_subject(&print_start_message_);
    subjects_.register_subject(&print_start_progress_);
    subjects_.register_subject(&print_in_progress_);

    // Register with LVGL XML system for XML bindings
    if (register_xml) {
        spdlog::debug("[PrinterPrintState] Registering subjects with XML system");
        lv_xml_register_subject(NULL, "print_progress", &print_progress_);
        lv_xml_register_subject(NULL, "print_filename", &print_filename_);
        lv_xml_register_subject(NULL, "print_state", &print_state_);
        lv_xml_register_subject(NULL, "print_state_enum", &print_state_enum_);
        lv_xml_register_subject(NULL, "print_outcome", &print_outcome_);
        lv_xml_register_subject(NULL, "print_active", &print_active_);
        lv_xml_register_subject(NULL, "print_show_progress", &print_show_progress_);
        lv_xml_register_subject(NULL, "print_display_filename", &print_display_filename_);
        lv_xml_register_subject(NULL, "print_thumbnail_path", &print_thumbnail_path_);
        lv_xml_register_subject(NULL, "print_layer_current", &print_layer_current_);
        lv_xml_register_subject(NULL, "print_layer_total", &print_layer_total_);
        lv_xml_register_subject(NULL, "print_duration", &print_duration_);
        lv_xml_register_subject(NULL, "print_time_left", &print_time_left_);
        lv_xml_register_subject(NULL, "print_start_phase", &print_start_phase_);
        lv_xml_register_subject(NULL, "print_start_message", &print_start_message_);
        lv_xml_register_subject(NULL, "print_start_progress", &print_start_progress_);
        lv_xml_register_subject(NULL, "print_in_progress", &print_in_progress_);
    } else {
        spdlog::debug("[PrinterPrintState] Skipping XML registration (tests mode)");
    }

    subjects_initialized_ = true;
    spdlog::debug("[PrinterPrintState] Subjects initialized successfully");
}

void PrinterPrintState::deinit_subjects() {
    if (!subjects_initialized_) {
        return;
    }

    spdlog::debug("[PrinterPrintState] Deinitializing subjects");
    subjects_.deinit_all();
    subjects_initialized_ = false;
}

void PrinterPrintState::reset_for_testing() {
    if (!subjects_initialized_) {
        spdlog::debug(
            "[PrinterPrintState] reset_for_testing: subjects not initialized, nothing to reset");
        return;
    }

    spdlog::info(
        "[PrinterPrintState] reset_for_testing: Deinitializing subjects to clear observers");

    // Use SubjectManager for automatic subject cleanup
    subjects_.deinit_all();
    subjects_initialized_ = false;
}

void PrinterPrintState::reset_for_new_print() {
    // Clear stale print PROGRESS data when starting a new print.
    // The preparing overlay covers the UI, so stale data isn't visible.
    // IMPORTANT: Do NOT clear print_filename_ or print_display_filename_ here!
    // Clearing filename triggers ActivePrintMediaManager to wipe the thumbnail we just set.
    // Filename is Moonraker's source of truth - it updates when the print actually starts.
    lv_subject_set_int(&print_progress_, 0);
    lv_subject_set_int(&print_layer_current_, 0);
    lv_subject_set_int(&print_duration_, 0);
    lv_subject_set_int(&print_time_left_, 0);
    spdlog::debug("[PrinterPrintState] Reset print progress for new print");
}

void PrinterPrintState::update_from_status(const nlohmann::json& status) {
    // Update print progress
    if (status.contains("virtual_sdcard")) {
        const auto& sdcard = status["virtual_sdcard"];

        if (sdcard.contains("progress") && sdcard["progress"].is_number()) {
            int progress_pct = helix::units::json_to_percent(sdcard, "progress");

            // Guard: Don't reset progress to 0 in terminal print states (Complete/Cancelled/Error)
            // This preserves the 100% display when a print finishes successfully
            auto current_state = static_cast<PrintJobState>(lv_subject_get_int(&print_state_enum_));
            bool is_terminal_state = (current_state == PrintJobState::COMPLETE ||
                                      current_state == PrintJobState::CANCELLED ||
                                      current_state == PrintJobState::ERROR);

            // Allow updates except: progress going backward in terminal state
            int current_progress = lv_subject_get_int(&print_progress_);
            if (!is_terminal_state || progress_pct >= current_progress) {
                lv_subject_set_int(&print_progress_, progress_pct);
            }
        }
    }

    // Update print state
    if (status.contains("print_stats")) {
        const auto& stats = status["print_stats"];

        if (stats.contains("state")) {
            std::string state_str = stats["state"].get<std::string>();
            // Update string subject (for UI display binding)
            lv_subject_copy_string(&print_state_, state_str.c_str());
            // Update enum subject (for type-safe logic)
            PrintJobState new_state = parse_print_job_state(state_str.c_str());
            auto current_state = static_cast<PrintJobState>(lv_subject_get_int(&print_state_enum_));
            auto current_outcome = static_cast<PrintOutcome>(lv_subject_get_int(&print_outcome_));

            // Update print_outcome based on state transitions:
            // - Set outcome when print reaches a terminal state (COMPLETE/CANCELLED/ERROR)
            // - Clear outcome when a NEW print starts (PRINTING from non-PAUSED)
            if (new_state != current_state) {
                // Entering a terminal state: record the outcome
                if (new_state == PrintJobState::COMPLETE) {
                    spdlog::info("[PrinterPrintState] Print completed - setting outcome=COMPLETE");
                    lv_subject_set_int(&print_outcome_, static_cast<int>(PrintOutcome::COMPLETE));
                } else if (new_state == PrintJobState::CANCELLED) {
                    spdlog::info("[PrinterPrintState] Print cancelled - setting outcome=CANCELLED");
                    lv_subject_set_int(&print_outcome_, static_cast<int>(PrintOutcome::CANCELLED));
                } else if (new_state == PrintJobState::ERROR) {
                    spdlog::info("[PrinterPrintState] Print error - setting outcome=ERROR");
                    lv_subject_set_int(&print_outcome_, static_cast<int>(PrintOutcome::ERROR));
                }
                // Starting a NEW print: clear the previous outcome
                // (only when transitioning TO PRINTING from a non-PAUSED state)
                else if (new_state == PrintJobState::PRINTING &&
                         current_state != PrintJobState::PAUSED) {
                    if (current_outcome != PrintOutcome::NONE) {
                        spdlog::info("[PrinterPrintState] New print starting - clearing outcome");
                        lv_subject_set_int(&print_outcome_, static_cast<int>(PrintOutcome::NONE));
                    }
                }
            }

            // Always update print_state_enum to reflect true Moonraker state
            // (print_outcome handles UI persistence for terminal states)
            if (new_state != current_state) {
                spdlog::info("[PrinterPrintState] print_stats.state: '{}' -> enum {} (was {})",
                             state_str, static_cast<int>(new_state),
                             static_cast<int>(current_state));
                lv_subject_set_int(&print_state_enum_, static_cast<int>(new_state));
            }

            // Update print_active (1 when PRINTING/PAUSED, 0 otherwise)
            // This derived subject simplifies XML bindings for card visibility
            bool is_active =
                (new_state == PrintJobState::PRINTING || new_state == PrintJobState::PAUSED);
            int active_val = is_active ? 1 : 0;
            if (lv_subject_get_int(&print_active_) != active_val) {
                lv_subject_set_int(&print_active_, active_val);

                // Safety: When print becomes inactive, ensure print_start_phase is IDLE
                // This prevents "Preparing Print" from showing when print is finished
                if (!is_active) {
                    int phase = lv_subject_get_int(&print_start_phase_);
                    if (phase != static_cast<int>(PrintStartPhase::IDLE)) {
                        spdlog::warn(
                            "[PrinterPrintState] Safety reset: print inactive but phase={}, "
                            "resetting to IDLE",
                            phase);
                        lv_subject_set_int(&print_start_phase_,
                                           static_cast<int>(PrintStartPhase::IDLE));
                        lv_subject_copy_string(&print_start_message_, "");
                        lv_subject_set_int(&print_start_progress_, 0);
                    }
                }
            }

            // Update combined subject for home panel progress card visibility
            update_print_show_progress();
        }

        if (stats.contains("filename")) {
            std::string filename = stats["filename"].get<std::string>();
            lv_subject_copy_string(&print_filename_, filename.c_str());
        }

        // Update layer info from print_stats.info (sent by Moonraker/mock client)
        // Note: Moonraker can send null values for layer fields when not available
        if (stats.contains("info") && stats["info"].is_object()) {
            const auto& info = stats["info"];

            if (info.contains("current_layer") && info["current_layer"].is_number()) {
                int current_layer = info["current_layer"].get<int>();
                lv_subject_set_int(&print_layer_current_, current_layer);
            }

            if (info.contains("total_layer") && info["total_layer"].is_number()) {
                int total_layer = info["total_layer"].get<int>();
                lv_subject_set_int(&print_layer_total_, total_layer);
            }
        }

        // Update print time tracking (elapsed and remaining)
        if (stats.contains("print_duration") && stats["print_duration"].is_number()) {
            int elapsed_seconds = static_cast<int>(stats["print_duration"].get<double>());
            lv_subject_set_int(&print_duration_, elapsed_seconds);
        }

        if (stats.contains("total_duration") && stats["total_duration"].is_number()) {
            // total_duration is the estimated total time, calculate remaining
            int total_seconds = static_cast<int>(stats["total_duration"].get<double>());
            int elapsed_seconds = lv_subject_get_int(&print_duration_);
            int remaining_seconds = std::max(0, total_seconds - elapsed_seconds);
            lv_subject_set_int(&print_time_left_, remaining_seconds);
        }
    }
}

void PrinterPrintState::update_print_show_progress() {
    // Combined subject for home panel progress card visibility
    // Show progress card only when: print is active AND not in print start phase
    bool is_active = lv_subject_get_int(&print_active_) != 0;
    bool is_starting =
        lv_subject_get_int(&print_start_phase_) != static_cast<int>(PrintStartPhase::IDLE);
    int new_value = (is_active && !is_starting) ? 1 : 0;

    if (lv_subject_get_int(&print_show_progress_) != new_value) {
        lv_subject_set_int(&print_show_progress_, new_value);
        spdlog::debug(
            "[PrinterPrintState] print_show_progress updated: {} (active={}, starting={})",
            new_value, is_active, is_starting);
    }
}

// ============================================================================
// Setters
// ============================================================================

void PrinterPrintState::set_print_outcome(PrintOutcome outcome) {
    lv_subject_set_int(&print_outcome_, static_cast<int>(outcome));
    spdlog::info("[PrinterPrintState] Print outcome set to: {}", static_cast<int>(outcome));
}

void PrinterPrintState::set_print_thumbnail_path(const std::string& path) {
    // Thumbnail path is set from PrintStatusPanel's main-thread callback,
    // so we can safely update the subject directly without ui_async_call.
    if (path.empty()) {
        spdlog::debug("[PrinterPrintState] Clearing print thumbnail path");
    } else {
        spdlog::debug("[PrinterPrintState] Setting print thumbnail path: {}", path);
    }
    lv_subject_copy_string(&print_thumbnail_path_, path.c_str());
}

void PrinterPrintState::set_print_display_filename(const std::string& name) {
    // Display filename is set from PrintStatusPanel's main-thread callback.
    spdlog::debug("[PrinterPrintState] Setting print display filename: {}", name);
    lv_subject_copy_string(&print_display_filename_, name.c_str());
}

void PrinterPrintState::set_print_layer_total(int total) {
    lv_subject_set_int(&print_layer_total_, total);
}

void PrinterPrintState::set_print_start_state(PrintStartPhase phase, const char* message,
                                              int progress) {
    spdlog::debug("[PrinterPrintState] Print start: phase={}, message='{}', progress={}%",
                  static_cast<int>(phase), message ? message : "", progress);

    // CRITICAL: Defer to main thread via helix::async::invoke to avoid LVGL assertion
    // when subject updates trigger lv_obj_invalidate() during rendering.
    // This is called from WebSocket callbacks (background thread).
    std::string msg = message ? message : "";
    int clamped_progress = std::clamp(progress, 0, 100);
    helix::async::invoke([this, phase, msg, clamped_progress]() {
        // Reset print progress when transitioning from IDLE to a preparing phase
        // IMPORTANT: Read old_phase inside lambda for thread safety - avoids race
        // condition where another callback could modify print_start_phase_ between
        // the read and this lambda's execution.
        int old_phase = lv_subject_get_int(&print_start_phase_);
        if (old_phase == static_cast<int>(PrintStartPhase::IDLE) &&
            phase != PrintStartPhase::IDLE) {
            reset_for_new_print();
        }
        lv_subject_set_int(&print_start_phase_, static_cast<int>(phase));
        if (!msg.empty()) {
            lv_subject_copy_string(&print_start_message_, msg.c_str());
        }
        lv_subject_set_int(&print_start_progress_, clamped_progress);
        update_print_show_progress();
    });
}

void PrinterPrintState::reset_print_start_state() {
    // CRITICAL: Defer to main thread via helix::async::invoke
    helix::async::invoke([this]() {
        int phase = lv_subject_get_int(&print_start_phase_);
        if (phase != static_cast<int>(PrintStartPhase::IDLE)) {
            spdlog::info("[PrinterPrintState] Resetting print start state to IDLE");
            lv_subject_set_int(&print_start_phase_, static_cast<int>(PrintStartPhase::IDLE));
            lv_subject_copy_string(&print_start_message_, "");
            lv_subject_set_int(&print_start_progress_, 0);
            update_print_show_progress();
        }
    });
}

void PrinterPrintState::set_print_in_progress(bool in_progress) {
    // Thread-safe wrapper: defer LVGL subject updates to main thread
    helix::async::invoke([this, in_progress]() { set_print_in_progress_internal(in_progress); });
}

void PrinterPrintState::set_print_in_progress_internal(bool in_progress) {
    int new_value = in_progress ? 1 : 0;
    if (lv_subject_get_int(&print_in_progress_) != new_value) {
        spdlog::debug("[PrinterPrintState] Print in progress: {}", in_progress);
        lv_subject_set_int(&print_in_progress_, new_value);
    }
}

// ============================================================================
// State queries
// ============================================================================

PrintJobState PrinterPrintState::get_print_job_state() const {
    // Note: lv_subject_get_int is thread-safe (atomic read)
    return static_cast<PrintJobState>(
        lv_subject_get_int(const_cast<lv_subject_t*>(&print_state_enum_)));
}

bool PrinterPrintState::can_start_new_print() const {
    // Check if a print workflow is already in progress (UI state)
    // This prevents double-tap issues during long G-code modification workflows
    if (is_print_in_progress()) {
        return false;
    }

    // Check printer's physical state
    PrintJobState state = get_print_job_state();
    // A new print can be started when printer is idle or previous print finished
    switch (state) {
    case PrintJobState::STANDBY:
    case PrintJobState::COMPLETE:
    case PrintJobState::CANCELLED:
    case PrintJobState::ERROR:
        return true;
    case PrintJobState::PRINTING:
    case PrintJobState::PAUSED:
        return false;
    default:
        // Unknown state - be conservative
        return false;
    }
}

bool PrinterPrintState::is_print_in_progress() const {
    return lv_subject_get_int(const_cast<lv_subject_t*>(&print_in_progress_)) != 0;
}

bool PrinterPrintState::is_in_print_start() const {
    int phase = lv_subject_get_int(const_cast<lv_subject_t*>(&print_start_phase_));
    return phase != static_cast<int>(PrintStartPhase::IDLE);
}

} // namespace helix
