// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_printer_print_char.cpp
 * @brief Characterization tests for PrinterState print domain
 *
 * These tests capture the CURRENT behavior of print-related subjects
 * in PrinterState before extraction to a dedicated PrinterPrintState class.
 *
 * Print subjects (17 total):
 *
 * Core State (4):
 * - print_state_ (string): "standby", "printing", "paused", "complete", "cancelled", "error"
 * - print_state_enum_ (int): PrintJobState enum (0-5)
 * - print_active_ (int): 1 when PRINTING or PAUSED, 0 otherwise
 * - print_outcome_ (int): PrintOutcome enum - persists through STANDBY transition
 *
 * File Info (3):
 * - print_filename_ (string): from print_stats.filename
 * - print_display_filename_ (string): set via API, cleaned name
 * - print_thumbnail_path_ (string): set via API, LVGL path
 *
 * Progress (2):
 * - print_progress_ (int 0-100): from virtual_sdcard.progress (float 0.0-1.0)
 * - print_show_progress_ (int): derived = print_active && print_start_phase == IDLE
 *
 * Layer Tracking (2):
 * - print_layer_current_ (int): from print_stats.info.current_layer
 * - print_layer_total_ (int): from print_stats.info.total_layer OR set via API
 *
 * Time Tracking (2):
 * - print_duration_ (int seconds): from print_stats.print_duration
 * - print_time_left_ (int seconds): calculated as total_duration - print_duration
 *
 * Print Start Phases (3):
 * - print_start_phase_ (int): PrintStartPhase enum (0-10)
 * - print_start_message_ (string): human-readable phase description
 * - print_start_progress_ (int 0-100): progress through PRINT_START
 *
 * Workflow (2):
 * - print_in_progress_ (int): 1 when G-code workflow running
 * - can_start_new_print(): returns false when print_in_progress_ == 1 OR print_active_ == 1
 */

#include "ui_update_queue.h"

#include "../ui_test_utils.h"
#include "app_globals.h"
#include "printer_state.h"

#include "../catch_amalgamated.hpp"

using json = nlohmann::json;

// ============================================================================
// Subject Accessor Tests - Verify get_*_subject() returns valid pointers
// ============================================================================

TEST_CASE("Print characterization: get_*_subject() returns valid pointers",
          "[characterization][print]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false); // Skip XML registration

    SECTION("print_state_subject is not null") {
        lv_subject_t* subject = state.get_print_state_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_state_enum_subject is not null") {
        lv_subject_t* subject = state.get_print_state_enum_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_active_subject is not null") {
        lv_subject_t* subject = state.get_print_active_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_outcome_subject is not null") {
        lv_subject_t* subject = state.get_print_outcome_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_filename_subject is not null") {
        lv_subject_t* subject = state.get_print_filename_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_display_filename_subject is not null") {
        lv_subject_t* subject = state.get_print_display_filename_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_thumbnail_path_subject is not null") {
        lv_subject_t* subject = state.get_print_thumbnail_path_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_progress_subject is not null") {
        lv_subject_t* subject = state.get_print_progress_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_show_progress_subject is not null") {
        lv_subject_t* subject = state.get_print_show_progress_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_layer_current_subject is not null") {
        lv_subject_t* subject = state.get_print_layer_current_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_layer_total_subject is not null") {
        lv_subject_t* subject = state.get_print_layer_total_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_duration_subject is not null") {
        lv_subject_t* subject = state.get_print_duration_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_time_left_subject is not null") {
        lv_subject_t* subject = state.get_print_time_left_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_start_phase_subject is not null") {
        lv_subject_t* subject = state.get_print_start_phase_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_start_message_subject is not null") {
        lv_subject_t* subject = state.get_print_start_message_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_start_progress_subject is not null") {
        lv_subject_t* subject = state.get_print_start_progress_subject();
        REQUIRE(subject != nullptr);
    }

    SECTION("print_in_progress_subject is not null") {
        lv_subject_t* subject = state.get_print_in_progress_subject();
        REQUIRE(subject != nullptr);
    }
}

TEST_CASE("Print characterization: all subject pointers are distinct",
          "[characterization][print]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    std::vector<lv_subject_t*> subjects = {
        state.get_print_state_subject(),          state.get_print_state_enum_subject(),
        state.get_print_active_subject(),         state.get_print_outcome_subject(),
        state.get_print_filename_subject(),       state.get_print_display_filename_subject(),
        state.get_print_thumbnail_path_subject(), state.get_print_progress_subject(),
        state.get_print_show_progress_subject(),  state.get_print_layer_current_subject(),
        state.get_print_layer_total_subject(),    state.get_print_duration_subject(),
        state.get_print_time_left_subject(),      state.get_print_start_phase_subject(),
        state.get_print_start_message_subject(),  state.get_print_start_progress_subject(),
        state.get_print_in_progress_subject(),
    };

    // All 17 subjects must be distinct pointers
    for (size_t i = 0; i < subjects.size(); ++i) {
        for (size_t j = i + 1; j < subjects.size(); ++j) {
            REQUIRE(subjects[i] != subjects[j]);
        }
    }
}

// ============================================================================
// Initial State Tests - Document default initialization behavior
// ============================================================================

TEST_CASE("Print characterization: initial values after init", "[characterization][print][init]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("print_state initializes to 'standby'") {
        const char* val = lv_subject_get_string(state.get_print_state_subject());
        REQUIRE(std::string(val) == "standby");
    }

    SECTION("print_state_enum initializes to STANDBY (0)") {
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::STANDBY));
    }

    SECTION("print_active initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 0);
    }

    SECTION("print_outcome initializes to NONE (0)") {
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::NONE));
    }

    SECTION("print_filename initializes to empty string") {
        const char* val = lv_subject_get_string(state.get_print_filename_subject());
        REQUIRE(std::string(val) == "");
    }

    SECTION("print_display_filename initializes to empty string") {
        const char* val = lv_subject_get_string(state.get_print_display_filename_subject());
        REQUIRE(std::string(val) == "");
    }

    SECTION("print_thumbnail_path initializes to empty string") {
        const char* val = lv_subject_get_string(state.get_print_thumbnail_path_subject());
        REQUIRE(std::string(val) == "");
    }

    SECTION("print_progress initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    }

    SECTION("print_show_progress initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 0);
    }

    SECTION("print_layer_current initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 0);
    }

    SECTION("print_layer_total initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_layer_total_subject()) == 0);
    }

    SECTION("print_duration initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_duration_subject()) == 0);
    }

    SECTION("print_time_left initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_time_left_subject()) == 0);
    }

    SECTION("print_start_phase initializes to IDLE (0)") {
        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::IDLE));
    }

    SECTION("print_start_message initializes to empty string") {
        const char* val = lv_subject_get_string(state.get_print_start_message_subject());
        REQUIRE(std::string(val) == "");
    }

    SECTION("print_start_progress initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_start_progress_subject()) == 0);
    }

    SECTION("print_in_progress initializes to 0") {
        REQUIRE(lv_subject_get_int(state.get_print_in_progress_subject()) == 0);
    }
}

// ============================================================================
// Core State JSON Update Tests
// ============================================================================

TEST_CASE("Print characterization: core state from JSON", "[characterization][print][update]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("standby state updates correctly") {
        json status = {{"print_stats", {{"state", "standby"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "standby");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::STANDBY));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 0);
    }

    SECTION("printing state updates correctly") {
        json status = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "printing");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::PRINTING));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 1);
    }

    SECTION("paused state updates correctly") {
        json status = {{"print_stats", {{"state", "paused"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "paused");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::PAUSED));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 1);
    }

    SECTION("complete state updates correctly") {
        json status = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "complete");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::COMPLETE));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 0);
    }

    SECTION("cancelled state updates correctly") {
        json status = {{"print_stats", {{"state", "cancelled"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "cancelled");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::CANCELLED));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 0);
    }

    SECTION("error state updates correctly") {
        json status = {{"print_stats", {{"state", "error"}}}};
        state.update_from_status(status);

        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "error");
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::ERROR));
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 0);
    }

    SECTION("unknown state defaults to standby") {
        json status = {{"print_stats", {{"state", "unknown_state"}}}};
        state.update_from_status(status);

        // String subject gets the raw value
        REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) ==
                "unknown_state");
        // Enum defaults to STANDBY for unknown strings
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::STANDBY));
    }
}

// ============================================================================
// Terminal State Persistence Tests (print_outcome)
// ============================================================================

TEST_CASE("Print characterization: terminal state persistence",
          "[characterization][print][outcome]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("COMPLETE sets outcome to COMPLETE") {
        // Start with printing
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::NONE));

        // Complete the print
        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::COMPLETE));
    }

    SECTION("CANCELLED sets outcome to CANCELLED") {
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        json cancelled = {{"print_stats", {{"state", "cancelled"}}}};
        state.update_from_status(cancelled);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::CANCELLED));
    }

    SECTION("ERROR sets outcome to ERROR") {
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        json error = {{"print_stats", {{"state", "error"}}}};
        state.update_from_status(error);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::ERROR));
    }

    SECTION("outcome persists through transition to STANDBY") {
        // Complete a print
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::COMPLETE));

        // Transition to standby (Moonraker does this after print completion)
        json standby = {{"print_stats", {{"state", "standby"}}}};
        state.update_from_status(standby);

        // Outcome should PERSIST (not reset to NONE)
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::COMPLETE));
    }

    SECTION("outcome clears when NEW print starts (PRINTING from non-PAUSED)") {
        // Complete a print first
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::COMPLETE));

        // Go to standby
        json standby = {{"print_stats", {{"state", "standby"}}}};
        state.update_from_status(standby);

        // Start a NEW print (STANDBY -> PRINTING)
        json new_print = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(new_print);

        // Outcome should be cleared
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::NONE));
    }

    SECTION("resume from PAUSED keeps outcome (does not clear)") {
        // Start printing
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        // Pause
        json paused = {{"print_stats", {{"state", "paused"}}}};
        state.update_from_status(paused);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::NONE));

        // Resume (PAUSED -> PRINTING)
        state.update_from_status(printing);

        // Outcome should remain NONE (not cleared, just not set)
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::NONE));
    }

    SECTION("set_print_outcome API works") {
        state.set_print_outcome(PrintOutcome::CANCELLED);
        REQUIRE(lv_subject_get_int(state.get_print_outcome_subject()) ==
                static_cast<int>(PrintOutcome::CANCELLED));
    }
}

// ============================================================================
// File Info Tests
// ============================================================================

TEST_CASE("Print characterization: file info from JSON", "[characterization][print][file]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("filename updates from print_stats.filename") {
        json status = {{"print_stats", {{"filename", "test_model.gcode"}}}};
        state.update_from_status(status);

        const char* val = lv_subject_get_string(state.get_print_filename_subject());
        REQUIRE(std::string(val) == "test_model.gcode");
    }

    SECTION("filename with path") {
        json status = {{"print_stats", {{"filename", "folder/subfolder/model.gcode"}}}};
        state.update_from_status(status);

        const char* val = lv_subject_get_string(state.get_print_filename_subject());
        REQUIRE(std::string(val) == "folder/subfolder/model.gcode");
    }

    SECTION("empty filename") {
        json status = {{"print_stats", {{"filename", ""}}}};
        state.update_from_status(status);

        const char* val = lv_subject_get_string(state.get_print_filename_subject());
        REQUIRE(std::string(val) == "");
    }
}

TEST_CASE("Print characterization: file info API methods", "[characterization][print][file][api]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("set_print_display_filename updates subject") {
        state.set_print_display_filename("Clean Model Name");

        const char* val = lv_subject_get_string(state.get_print_display_filename_subject());
        REQUIRE(std::string(val) == "Clean Model Name");
    }

    SECTION("set_print_thumbnail_path updates subject") {
        state.set_print_thumbnail_path("A:/tmp/thumbnail_abc123.bin");

        const char* val = lv_subject_get_string(state.get_print_thumbnail_path_subject());
        REQUIRE(std::string(val) == "A:/tmp/thumbnail_abc123.bin");
    }

    SECTION("empty thumbnail path clears subject") {
        state.set_print_thumbnail_path("A:/tmp/thumbnail.bin");
        state.set_print_thumbnail_path("");

        const char* val = lv_subject_get_string(state.get_print_thumbnail_path_subject());
        REQUIRE(std::string(val) == "");
    }
}

// ============================================================================
// Progress Tests
// ============================================================================

TEST_CASE("Print characterization: progress from JSON", "[characterization][print][progress]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("progress converts 0.0-1.0 float to 0-100 percentage") {
        json status = {{"virtual_sdcard", {{"progress", 0.5}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 50);
    }

    SECTION("progress 0.0 becomes 0%") {
        json status = {{"virtual_sdcard", {{"progress", 0.0}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    }

    SECTION("progress 1.0 becomes 100%") {
        json status = {{"virtual_sdcard", {{"progress", 1.0}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 100);
    }

    SECTION("progress 0.753 becomes 75%") {
        json status = {{"virtual_sdcard", {{"progress", 0.753}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 75);
    }
}

TEST_CASE("Print characterization: terminal state progress guard",
          "[characterization][print][progress][guard]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("progress cannot go backward in COMPLETE state") {
        // Set up complete state with 100% progress
        json printing = {{"print_stats", {{"state", "printing"}}},
                         {"virtual_sdcard", {{"progress", 1.0}}}};
        state.update_from_status(printing);
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 100);

        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);

        // Try to set progress to 0 (Moonraker does this sometimes)
        json reset = {{"virtual_sdcard", {{"progress", 0.0}}}};
        state.update_from_status(reset);

        // Progress should stay at 100 (guarded)
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 100);
    }

    SECTION("progress cannot go backward in CANCELLED state") {
        json printing = {{"print_stats", {{"state", "printing"}}},
                         {"virtual_sdcard", {{"progress", 0.75}}}};
        state.update_from_status(printing);
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 75);

        json cancelled = {{"print_stats", {{"state", "cancelled"}}}};
        state.update_from_status(cancelled);

        json reset = {{"virtual_sdcard", {{"progress", 0.0}}}};
        state.update_from_status(reset);

        // Progress should stay at 75 (guarded)
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 75);
    }

    SECTION("progress cannot go backward in ERROR state") {
        json printing = {{"print_stats", {{"state", "printing"}}},
                         {"virtual_sdcard", {{"progress", 0.5}}}};
        state.update_from_status(printing);

        json error = {{"print_stats", {{"state", "error"}}}};
        state.update_from_status(error);

        json reset = {{"virtual_sdcard", {{"progress", 0.0}}}};
        state.update_from_status(reset);

        // Progress should stay at 50 (guarded)
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 50);
    }

    SECTION("progress CAN go forward in terminal state") {
        json printing = {{"print_stats", {{"state", "printing"}}},
                         {"virtual_sdcard", {{"progress", 0.95}}}};
        state.update_from_status(printing);

        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);

        // Can still update to 100%
        json full = {{"virtual_sdcard", {{"progress", 1.0}}}};
        state.update_from_status(full);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 100);
    }

    SECTION("progress can reset in non-terminal states") {
        json printing = {{"print_stats", {{"state", "printing"}}},
                         {"virtual_sdcard", {{"progress", 0.5}}}};
        state.update_from_status(printing);

        json standby = {{"print_stats", {{"state", "standby"}}}};
        state.update_from_status(standby);

        // In standby, progress CAN go backward
        json reset = {{"virtual_sdcard", {{"progress", 0.0}}}};
        state.update_from_status(reset);

        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    }
}

// ============================================================================
// Layer Tracking Tests
// ============================================================================

TEST_CASE("Print characterization: layer tracking from JSON", "[characterization][print][layer]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("current_layer updates from print_stats.info") {
        json status = {{"print_stats", {{"info", {{"current_layer", 42}}}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 42);
    }

    SECTION("total_layer updates from print_stats.info") {
        json status = {{"print_stats", {{"info", {{"total_layer", 150}}}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_layer_total_subject()) == 150);
    }

    SECTION("both layers update together") {
        json status = {{"print_stats", {{"info", {{"current_layer", 25}, {"total_layer", 100}}}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 25);
        REQUIRE(lv_subject_get_int(state.get_print_layer_total_subject()) == 100);
    }

    SECTION("set_print_layer_total API updates subject") {
        state.set_print_layer_total(200);

        REQUIRE(lv_subject_get_int(state.get_print_layer_total_subject()) == 200);
    }

    SECTION("null layer values are ignored") {
        // Set initial value
        state.set_print_layer_total(100);

        // Moonraker sometimes sends null for layer info
        json status = {{"print_stats", {{"info", {{"current_layer", nullptr}}}}}};
        state.update_from_status(status);

        // Values should remain unchanged (null is not a number)
        REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 0);
        REQUIRE(lv_subject_get_int(state.get_print_layer_total_subject()) == 100);
    }
}

// ============================================================================
// Time Tracking Tests
// ============================================================================

TEST_CASE("Print characterization: time tracking from JSON", "[characterization][print][time]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("print_duration updates from print_stats.print_duration") {
        json status = {{"print_stats", {{"print_duration", 3600.0}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_duration_subject()) == 3600);
    }

    SECTION("time_left calculated from total_duration - print_duration") {
        // First set print_duration
        json status1 = {{"print_stats", {{"print_duration", 3600.0}}}};
        state.update_from_status(status1);

        // Then set total_duration (time_left is calculated)
        json status2 = {{"print_stats", {{"total_duration", 7200.0}}}};
        state.update_from_status(status2);

        // time_left = 7200 - 3600 = 3600 seconds
        REQUIRE(lv_subject_get_int(state.get_print_time_left_subject()) == 3600);
    }

    SECTION("time_left cannot go negative") {
        // Set duration greater than total (edge case)
        json status1 = {{"print_stats", {{"print_duration", 5000.0}}}};
        state.update_from_status(status1);

        json status2 = {{"print_stats", {{"total_duration", 3000.0}}}};
        state.update_from_status(status2);

        // time_left should be clamped to 0
        REQUIRE(lv_subject_get_int(state.get_print_time_left_subject()) == 0);
    }

    SECTION("both duration and total in same update") {
        json status = {{"print_stats", {{"print_duration", 1800.0}, {"total_duration", 5400.0}}}};
        state.update_from_status(status);

        REQUIRE(lv_subject_get_int(state.get_print_duration_subject()) == 1800);
        REQUIRE(lv_subject_get_int(state.get_print_time_left_subject()) == 3600);
    }
}

// ============================================================================
// Print Start Phase Tests
// ============================================================================

TEST_CASE("Print characterization: print start phases", "[characterization][print][phase]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("set_print_start_state updates all three subjects") {
        state.set_print_start_state(PrintStartPhase::HEATING_BED, "Heating bed...", 30);

        // Drain the async queue to apply the updates
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::HEATING_BED));
        REQUIRE(std::string(lv_subject_get_string(state.get_print_start_message_subject())) ==
                "Heating bed...");
        REQUIRE(lv_subject_get_int(state.get_print_start_progress_subject()) == 30);
    }

    SECTION("is_in_print_start returns true when phase is not IDLE") {
        REQUIRE(state.is_in_print_start() == false);

        state.set_print_start_state(PrintStartPhase::HOMING, "Homing...", 10);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(state.is_in_print_start() == true);
    }

    SECTION("reset_print_start_state sets phase to IDLE") {
        state.set_print_start_state(PrintStartPhase::QGL, "QGL...", 50);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        state.reset_print_start_state();
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::IDLE));
        REQUIRE(std::string(lv_subject_get_string(state.get_print_start_message_subject())) == "");
        REQUIRE(lv_subject_get_int(state.get_print_start_progress_subject()) == 0);
    }

    SECTION("progress is clamped to 0-100") {
        state.set_print_start_state(PrintStartPhase::INITIALIZING, "Starting...", 150);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_start_progress_subject()) == 100);

        state.set_print_start_state(PrintStartPhase::INITIALIZING, "Starting...", -10);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_start_progress_subject()) == 0);
    }

    SECTION("all PrintStartPhase enum values are valid") {
        // Test that all enum values can be set
        std::vector<PrintStartPhase> phases = {
            PrintStartPhase::IDLE,           PrintStartPhase::INITIALIZING,
            PrintStartPhase::HOMING,         PrintStartPhase::HEATING_BED,
            PrintStartPhase::HEATING_NOZZLE, PrintStartPhase::QGL,
            PrintStartPhase::Z_TILT,         PrintStartPhase::BED_MESH,
            PrintStartPhase::CLEANING,       PrintStartPhase::PURGING,
            PrintStartPhase::COMPLETE};

        for (auto phase : phases) {
            state.set_print_start_state(phase, "Test", 50);
            helix::ui::UpdateQueue::instance().drain_queue_for_testing();

            REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                    static_cast<int>(phase));
        }
    }
}

TEST_CASE("Print characterization: print start phase safety reset",
          "[characterization][print][phase][safety]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("phase resets to IDLE when print_active becomes 0") {
        // Start a print and enter a phase
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        state.set_print_start_state(PrintStartPhase::HEATING_NOZZLE, "Heating...", 40);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::HEATING_NOZZLE));

        // Print ends (goes to complete)
        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);

        // Phase should be reset to IDLE (safety mechanism)
        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::IDLE));
    }
}

// ============================================================================
// print_show_progress Derived Subject Tests
// ============================================================================

TEST_CASE("Print characterization: print_show_progress derived subject",
          "[characterization][print][derived]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("print_show_progress is 0 when not printing") {
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 0);
    }

    SECTION("print_show_progress is 0 during print start phase") {
        // Start printing but in a start phase
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        state.set_print_start_state(PrintStartPhase::HEATING_BED, "Heating...", 30);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        // Active but in start phase = don't show progress yet
        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 1);
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 0);
    }

    SECTION("print_show_progress is 1 when printing and phase is IDLE") {
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        // Phase should be IDLE by default
        REQUIRE(lv_subject_get_int(state.get_print_start_phase_subject()) ==
                static_cast<int>(PrintStartPhase::IDLE));
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 1);
    }

    SECTION("print_show_progress is 1 when paused and phase is IDLE") {
        json paused = {{"print_stats", {{"state", "paused"}}}};
        state.update_from_status(paused);

        REQUIRE(lv_subject_get_int(state.get_print_active_subject()) == 1);
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 1);
    }

    SECTION("print_show_progress becomes 1 when phase transitions to IDLE") {
        // Start printing in a phase
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        state.set_print_start_state(PrintStartPhase::COMPLETE, "Done", 100);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();
        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 0);

        // Phase goes to IDLE
        state.reset_print_start_state();
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_show_progress_subject()) == 1);
    }
}

// ============================================================================
// Workflow Tests (print_in_progress and can_start_new_print)
// ============================================================================

TEST_CASE("Print characterization: workflow state", "[characterization][print][workflow]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("set_print_in_progress updates subject") {
        REQUIRE(lv_subject_get_int(state.get_print_in_progress_subject()) == 0);

        state.set_print_in_progress(true);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_in_progress_subject()) == 1);

        state.set_print_in_progress(false);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(lv_subject_get_int(state.get_print_in_progress_subject()) == 0);
    }

    SECTION("is_print_in_progress returns correct value") {
        REQUIRE(state.is_print_in_progress() == false);

        state.set_print_in_progress(true);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(state.is_print_in_progress() == true);
    }

    SECTION("can_start_new_print returns true when idle and not in progress") {
        // Default state: standby, not in progress
        REQUIRE(state.can_start_new_print() == true);
    }

    SECTION("can_start_new_print returns false when print_in_progress is true") {
        state.set_print_in_progress(true);
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();

        REQUIRE(state.can_start_new_print() == false);
    }

    SECTION("can_start_new_print returns false when PRINTING") {
        json printing = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(printing);

        REQUIRE(state.can_start_new_print() == false);
    }

    SECTION("can_start_new_print returns false when PAUSED") {
        json paused = {{"print_stats", {{"state", "paused"}}}};
        state.update_from_status(paused);

        REQUIRE(state.can_start_new_print() == false);
    }

    SECTION("can_start_new_print returns true when COMPLETE") {
        json complete = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(complete);

        REQUIRE(state.can_start_new_print() == true);
    }

    SECTION("can_start_new_print returns true when CANCELLED") {
        json cancelled = {{"print_stats", {{"state", "cancelled"}}}};
        state.update_from_status(cancelled);

        REQUIRE(state.can_start_new_print() == true);
    }

    SECTION("can_start_new_print returns true when ERROR") {
        json error = {{"print_stats", {{"state", "error"}}}};
        state.update_from_status(error);

        REQUIRE(state.can_start_new_print() == true);
    }

    SECTION("can_start_new_print returns true when STANDBY") {
        json standby = {{"print_stats", {{"state", "standby"}}}};
        state.update_from_status(standby);

        REQUIRE(state.can_start_new_print() == true);
    }
}

// ============================================================================
// reset_for_new_print Tests
// ============================================================================

TEST_CASE("Print characterization: reset_for_new_print clears progress subjects",
          "[characterization][print][reset]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    // Set various print values
    json status = {{"print_stats",
                    {{"filename", "test.gcode"},
                     {"print_duration", 3600.0},
                     {"info", {{"current_layer", 50}, {"total_layer", 100}}}}},
                   {"virtual_sdcard", {{"progress", 0.5}}}};
    state.update_from_status(status);

    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 50);
    REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 50);
    REQUIRE(lv_subject_get_int(state.get_print_duration_subject()) == 3600);

    // Reset for new print
    state.reset_for_new_print();

    // These should be cleared
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_print_layer_current_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_print_duration_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_print_time_left_subject()) == 0);

    // Filename should NOT be cleared (it's Moonraker's source of truth)
    const char* filename = lv_subject_get_string(state.get_print_filename_subject());
    REQUIRE(std::string(filename) == "test.gcode");
}

// ============================================================================
// Observer Notification Tests
// ============================================================================

TEST_CASE("Print characterization: observer fires when print_state_enum changes",
          "[characterization][print][observer]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    auto observer_cb = [](lv_observer_t* observer, lv_subject_t* subject) {
        int* count_ptr = static_cast<int*>(lv_observer_get_user_data(observer));
        int* value_ptr = count_ptr + 1;

        (*count_ptr)++;
        *value_ptr = lv_subject_get_int(subject);
    };

    int user_data[2] = {0, -1}; // [callback_count, last_value]

    lv_observer_t* observer =
        lv_subject_add_observer(state.get_print_state_enum_subject(), observer_cb, user_data);

    // LVGL auto-notifies observers when first added
    REQUIRE(user_data[0] == 1);
    REQUIRE(user_data[1] == static_cast<int>(PrintJobState::STANDBY));

    // Change to printing
    json status = {{"print_stats", {{"state", "printing"}}}};
    state.update_from_status(status);

    REQUIRE(user_data[0] >= 2);
    REQUIRE(user_data[1] == static_cast<int>(PrintJobState::PRINTING));

    lv_observer_remove(observer);
}

TEST_CASE("Print characterization: observer fires when print_progress changes",
          "[characterization][print][observer]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    auto observer_cb = [](lv_observer_t* observer, lv_subject_t* subject) {
        int* count_ptr = static_cast<int*>(lv_observer_get_user_data(observer));
        int* value_ptr = count_ptr + 1;

        (*count_ptr)++;
        *value_ptr = lv_subject_get_int(subject);
    };

    int user_data[2] = {0, -1};

    lv_observer_t* observer =
        lv_subject_add_observer(state.get_print_progress_subject(), observer_cb, user_data);

    // Initial notification
    REQUIRE(user_data[0] == 1);
    REQUIRE(user_data[1] == 0);

    // Update progress
    json status = {{"virtual_sdcard", {{"progress", 0.75}}}};
    state.update_from_status(status);

    REQUIRE(user_data[0] >= 2);
    REQUIRE(user_data[1] == 75);

    lv_observer_remove(observer);
}

// ============================================================================
// Reset Cycle Tests
// ============================================================================

TEST_CASE("Print characterization: subjects survive reset_for_testing cycle",
          "[characterization][print][reset]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    // Set some values
    json status = {{"print_stats", {{"state", "printing"}, {"filename", "test.gcode"}}},
                   {"virtual_sdcard", {{"progress", 0.5}}}};
    state.update_from_status(status);

    // Verify values were set
    REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
            static_cast<int>(PrintJobState::PRINTING));
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 50);

    // Reset and reinitialize
    state.reset_for_testing();
    state.init_subjects(false);

    // After reset, values should be back to defaults
    REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
            static_cast<int>(PrintJobState::STANDBY));
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    REQUIRE(std::string(lv_subject_get_string(state.get_print_filename_subject())) == "");

    // Subjects should still be functional after reset
    json new_status = {{"print_stats", {{"state", "paused"}}}};
    state.update_from_status(new_status);

    REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
            static_cast<int>(PrintJobState::PAUSED));
}

TEST_CASE("Print characterization: subject pointers remain valid after reset",
          "[characterization][print][reset]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    // Capture subject pointers
    lv_subject_t* state_enum_before = state.get_print_state_enum_subject();
    lv_subject_t* progress_before = state.get_print_progress_subject();
    lv_subject_t* outcome_before = state.get_print_outcome_subject();

    // Reset and reinitialize
    state.reset_for_testing();
    state.init_subjects(false);

    // Pointers should be the same (singleton subjects are reused)
    lv_subject_t* state_enum_after = state.get_print_state_enum_subject();
    lv_subject_t* progress_after = state.get_print_progress_subject();
    lv_subject_t* outcome_after = state.get_print_outcome_subject();

    REQUIRE(state_enum_before == state_enum_after);
    REQUIRE(progress_before == progress_after);
    REQUIRE(outcome_before == outcome_after);
}

// ============================================================================
// Independence Tests
// ============================================================================

TEST_CASE("Print characterization: print update does not affect non-print subjects",
          "[characterization][print][independence]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    // Set some non-print values first
    json initial = {{"toolhead", {{"position", {100.0, 200.0, 30.0}}}}};
    state.update_from_status(initial);

    // Positions stored as centimillimeters (×100) for 0.01mm precision
    REQUIRE(lv_subject_get_int(state.get_position_x_subject()) == 10000);

    // Now update print state
    json print_update = {{"print_stats", {{"state", "printing"}, {"filename", "test.gcode"}}},
                         {"virtual_sdcard", {{"progress", 0.5}}}};
    state.update_from_status(print_update);

    // Print values should be updated
    REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
            static_cast<int>(PrintJobState::PRINTING));
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 50);

    // Position should be unchanged (still centimillimeters)
    REQUIRE(lv_subject_get_int(state.get_position_x_subject()) == 10000);
}

TEST_CASE("Print characterization: non-print update does not affect print subjects",
          "[characterization][print][independence]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    // Set print values first
    json print_status = {{"print_stats", {{"state", "printing"}, {"filename", "test.gcode"}}},
                         {"virtual_sdcard", {{"progress", 0.75}}}};
    state.update_from_status(print_status);

    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 75);

    // Now update position (non-print)
    json position_update = {{"toolhead", {{"position", {50.0, 75.0, 10.0}}}}};
    state.update_from_status(position_update);

    // Print values should be unchanged
    REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
            static_cast<int>(PrintJobState::PRINTING));
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 75);
    REQUIRE(std::string(lv_subject_get_string(state.get_print_filename_subject())) == "test.gcode");
}

// ============================================================================
// get_print_job_state Convenience Method
// ============================================================================

TEST_CASE("Print characterization: get_print_job_state returns correct enum",
          "[characterization][print][api]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("returns STANDBY by default") {
        REQUIRE(state.get_print_job_state() == PrintJobState::STANDBY);
    }

    SECTION("returns PRINTING when printing") {
        json status = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(status);

        REQUIRE(state.get_print_job_state() == PrintJobState::PRINTING);
    }

    SECTION("returns PAUSED when paused") {
        json status = {{"print_stats", {{"state", "paused"}}}};
        state.update_from_status(status);

        REQUIRE(state.get_print_job_state() == PrintJobState::PAUSED);
    }

    SECTION("returns COMPLETE when complete") {
        json status = {{"print_stats", {{"state", "complete"}}}};
        state.update_from_status(status);

        REQUIRE(state.get_print_job_state() == PrintJobState::COMPLETE);
    }
}

// ============================================================================
// parse_print_job_state Function Tests
// ============================================================================

TEST_CASE("Print characterization: parse_print_job_state function",
          "[characterization][print][parse]") {
    SECTION("parses 'standby' to STANDBY") {
        REQUIRE(parse_print_job_state("standby") == PrintJobState::STANDBY);
    }

    SECTION("parses 'printing' to PRINTING") {
        REQUIRE(parse_print_job_state("printing") == PrintJobState::PRINTING);
    }

    SECTION("parses 'paused' to PAUSED") {
        REQUIRE(parse_print_job_state("paused") == PrintJobState::PAUSED);
    }

    SECTION("parses 'complete' to COMPLETE") {
        REQUIRE(parse_print_job_state("complete") == PrintJobState::COMPLETE);
    }

    SECTION("parses 'cancelled' to CANCELLED") {
        REQUIRE(parse_print_job_state("cancelled") == PrintJobState::CANCELLED);
    }

    SECTION("parses 'error' to ERROR") {
        REQUIRE(parse_print_job_state("error") == PrintJobState::ERROR);
    }

    SECTION("parses unknown string to STANDBY") {
        REQUIRE(parse_print_job_state("unknown") == PrintJobState::STANDBY);
    }

    SECTION("parses nullptr to STANDBY") {
        REQUIRE(parse_print_job_state(nullptr) == PrintJobState::STANDBY);
    }
}

// ============================================================================
// print_job_state_to_string Function Tests
// ============================================================================

TEST_CASE("Print characterization: print_job_state_to_string function",
          "[characterization][print][string]") {
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::STANDBY)) == "Standby");
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::PRINTING)) == "Printing");
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::PAUSED)) == "Paused");
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::COMPLETE)) == "Complete");
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::CANCELLED)) == "Cancelled");
    REQUIRE(std::string(print_job_state_to_string(PrintJobState::ERROR)) == "Error");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("Print characterization: edge cases", "[characterization][print][edge]") {
    lv_init_safe();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();
    state.init_subjects(false);

    SECTION("empty status does not crash") {
        json empty = json::object();
        state.update_from_status(empty);

        // Values should remain at defaults
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::STANDBY));
    }

    SECTION("status with missing print_stats does not crash") {
        json status = {{"toolhead", {{"position", {0.0, 0.0, 0.0}}}}};
        state.update_from_status(status);

        // Print state should remain at default
        REQUIRE(lv_subject_get_int(state.get_print_state_enum_subject()) ==
                static_cast<int>(PrintJobState::STANDBY));
    }

    SECTION("status with missing virtual_sdcard does not crash") {
        json status = {{"print_stats", {{"state", "printing"}}}};
        state.update_from_status(status);

        // Progress should remain at 0
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    }

    SECTION("very long filename is handled") {
        std::string long_name(200, 'a');
        long_name += ".gcode";
        json status = {{"print_stats", {{"filename", long_name}}}};
        state.update_from_status(status);

        // Should be stored (buffer is 256 chars)
        const char* stored = lv_subject_get_string(state.get_print_filename_subject());
        REQUIRE(std::strlen(stored) > 0);
    }
}
