// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_active_print_media_manager.cpp
 * @brief Unit tests for ActivePrintMediaManager class
 *
 * Tests the media manager that:
 * - Observes print_filename_ subject from PrinterState
 * - Processes raw filename to display name
 * - Loads thumbnails via MoonrakerAPI
 * - Updates print_display_filename_ and print_thumbnail_path_ subjects
 * - Uses generation counter for stale callback detection
 */

#include "ui_update_queue.h"

#include "active_print_media_manager.h"
#include "printer_state.h"

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <functional>
#include <string>

#include "../catch_amalgamated.hpp"
#include "../ui_test_utils.h"

using json = nlohmann::json;

// ============================================================================
// Test Fixture for ActivePrintMediaManager tests
// ============================================================================

class ActivePrintMediaManagerTestFixture {
  public:
    ActivePrintMediaManagerTestFixture() {
        // Suppress spdlog output during tests
        static bool logger_initialized = false;
        if (!logger_initialized) {
            auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
            auto null_logger = std::make_shared<spdlog::logger>("null", null_sink);
            spdlog::set_default_logger(null_logger);
            logger_initialized = true;
        }

        // Initialize LVGL (safe version avoids "already initialized" warnings)
        lv_init_safe();

        // Initialize update queue once (static guard) - CRITICAL for ui_queue_update()
        static bool queue_initialized = false;
        if (!queue_initialized) {
            ui_update_queue_init();
            queue_initialized = true;
        }

        // Create a headless display for testing
        if (!display_created_) {
            display_ = lv_display_create(480, 320);
            alignas(64) static lv_color_t buf[480 * 10];
            lv_display_set_buffers(display_, buf, nullptr, sizeof(buf),
                                   LV_DISPLAY_RENDER_MODE_PARTIAL);
            lv_display_set_flush_cb(display_, [](lv_display_t* disp, const lv_area_t*, uint8_t*) {
                lv_display_flush_ready(disp);
            });
            display_created_ = true;
        }

        // Reset PrinterState for test isolation
        state_.reset_for_testing();

        // Initialize subjects (without XML registration in tests)
        state_.init_subjects(false);

        // Create ActivePrintMediaManager for this test
        manager_ = std::make_unique<helix::ActivePrintMediaManager>(state_);
    }

    ~ActivePrintMediaManagerTestFixture() {
        // Destroy manager first (it observes state_)
        manager_.reset();

        // Reset after each test
        state_.reset_for_testing();
    }

  protected:
    PrinterState& state() {
        return state_;
    }

    helix::ActivePrintMediaManager& manager() {
        return *manager_;
    }

    // Helper to update print filename via status JSON (simulates Moonraker notification)
    void set_print_filename(const std::string& filename) {
        json status = {{"print_stats", {{"filename", filename}}}};
        state_.update_from_status(status);
        // Process queued UI updates - call drain_queue directly instead of lv_timer_handler
        // to avoid potential infinite loops from the 1ms timer period
        helix::ui::UpdateQueue::instance().drain_queue_for_testing();
    }

    // Get current print_filename (raw)
    std::string get_print_filename() {
        return lv_subject_get_string(state_.get_print_filename_subject());
    }

    // Get current print_display_filename (processed for UI)
    std::string get_display_filename() {
        return lv_subject_get_string(state_.get_print_display_filename_subject());
    }

    // Get current print_thumbnail_path
    std::string get_thumbnail_path() {
        return lv_subject_get_string(state_.get_print_thumbnail_path_subject());
    }

  private:
    PrinterState state_;
    std::unique_ptr<helix::ActivePrintMediaManager> manager_;
    static lv_display_t* display_;
    static bool display_created_;
};

lv_display_t* ActivePrintMediaManagerTestFixture::display_ = nullptr;
bool ActivePrintMediaManagerTestFixture::display_created_ = false;

// ============================================================================
// Display Name Formatting Tests
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: simple filename produces correct display name",
                 "[media][display_name]") {
    set_print_filename("benchy.gcode");

    REQUIRE(get_print_filename() == "benchy.gcode");
    REQUIRE(get_display_filename() == "benchy");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: filename with path produces correct display name",
                 "[media][display_name]") {
    set_print_filename("my_models/benchy.gcode");

    REQUIRE(get_print_filename() == "my_models/benchy.gcode");
    REQUIRE(get_display_filename() == "benchy");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: helix_temp filename resolves to original",
                 "[media][display_name]") {
    // When HelixScreen modifies G-code, it creates temp files like:
    // .helix_temp/modified_1234567890_Original_Model.gcode
    // The display name should show "Original_Model", not the temp filename
    set_print_filename(".helix_temp/modified_1234567890_Body1.gcode");

    REQUIRE(get_print_filename() == ".helix_temp/modified_1234567890_Body1.gcode");
    REQUIRE(get_display_filename() == "Body1");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: complex helix_temp path resolves correctly",
                 "[media][display_name]") {
    set_print_filename(".helix_temp/modified_9876543210_My_Cool_Print.gcode");

    REQUIRE(get_display_filename() == "My_Cool_Print");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: deeply nested path produces correct display name",
                 "[media][display_name]") {
    set_print_filename("projects/2025/january/test_models/benchy_0.2mm_PLA.gcode");

    REQUIRE(get_print_filename() == "projects/2025/january/test_models/benchy_0.2mm_PLA.gcode");
    REQUIRE(get_display_filename() == "benchy_0.2mm_PLA");
}

// ============================================================================
// Empty Filename Handling Tests
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: empty filename clears display name", "[media][empty]") {
    // First set a filename
    set_print_filename("test.gcode");
    REQUIRE(get_print_filename() == "test.gcode");
    REQUIRE(get_display_filename() == "test");

    // Then clear it (printer goes to standby)
    set_print_filename("");
    REQUIRE(get_print_filename() == "");

    // Display filename should also be cleared
    REQUIRE(get_display_filename() == "");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: empty filename clears thumbnail path",
                 "[media][empty]") {
    // Set a filename first (to trigger the manager to process)
    set_print_filename("test.gcode");

    // Manually set a thumbnail path (simulating a loaded thumbnail)
    state().set_print_thumbnail_path("A:/tmp/thumbnail_abc123.bin");
    REQUIRE(get_thumbnail_path() == "A:/tmp/thumbnail_abc123.bin");

    // When filename is cleared, manager should clear thumbnail path
    set_print_filename("");

    REQUIRE(get_thumbnail_path() == "");
}

// ============================================================================
// Thumbnail Source Override Tests
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: manual thumbnail source takes precedence",
                 "[media][thumbnail][override]") {
    // When PrintPreparationManager starts a modified print, it knows the original filename
    // and can provide it via set_thumbnail_source() for proper resolution

    // Set the thumbnail source BEFORE the filename arrives
    manager().set_thumbnail_source("original_model.gcode");

    // Now when a temp filename arrives, the source override should be used
    set_print_filename(".helix_temp/modified_12345_original_model.gcode");

    // Display name should use the source override
    REQUIRE(get_display_filename() == "original_model");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: clear_thumbnail_source resets state",
                 "[media][thumbnail][override]") {
    // Set up initial state
    set_print_filename("first.gcode");
    REQUIRE(get_display_filename() == "first");

    // Set an override
    manager().set_thumbnail_source("override.gcode");

    // Clear the override
    manager().clear_thumbnail_source();

    // Next filename should be processed normally (no override)
    set_print_filename("second.gcode");
    REQUIRE(get_display_filename() == "second");
}

// ============================================================================
// Generation Counter / Stale Callback Detection Tests
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: rapid filename changes use latest generation",
                 "[media][generation]") {
    // When filename changes rapidly (user quickly switches prints),
    // only the last one should be reflected

    set_print_filename("print1.gcode");
    set_print_filename("print2.gcode");
    set_print_filename("print3.gcode");

    // Only print3 should be reflected in the display name
    REQUIRE(get_print_filename() == "print3.gcode");
    REQUIRE(get_display_filename() == "print3");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: idempotent on repeated same filename",
                 "[media][generation]") {
    // Setting the same filename multiple times should not trigger redundant processing
    set_print_filename("same_file.gcode");
    REQUIRE(get_display_filename() == "same_file");

    // Set again - should be idempotent
    set_print_filename("same_file.gcode");
    REQUIRE(get_display_filename() == "same_file");
}

// ============================================================================
// Integration with PrinterState Subjects
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: updates print_display_filename subject",
                 "[media][subjects]") {
    set_print_filename("test_model.gcode");

    REQUIRE(get_display_filename() == "test_model");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: observer fires on display_filename change",
                 "[media][subjects][observer]") {
    int observer_count = 0;
    auto observer_cb = [](lv_observer_t* observer, lv_subject_t*) {
        int* count = static_cast<int*>(lv_observer_get_user_data(observer));
        (*count)++;
    };

    lv_subject_add_observer(state().get_print_display_filename_subject(), observer_cb,
                            &observer_count);

    // Initial observer registration fires once
    REQUIRE(observer_count == 1);

    // Change filename - should fire observer after processing
    set_print_filename("new_model.gcode");

    // Observer should have fired again
    REQUIRE(observer_count == 2);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: handles filename with special characters",
                 "[media][edge_case]") {
    set_print_filename("My Model (v2) - Final.gcode");

    REQUIRE(get_print_filename() == "My Model (v2) - Final.gcode");
    REQUIRE(get_display_filename() == "My Model (v2) - Final");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: handles very long filename", "[media][edge_case]") {
    // Test handling of very long filenames (within buffer limits)
    std::string long_name(100, 'x');
    long_name += ".gcode";

    set_print_filename(long_name);

    // Should handle gracefully (may be truncated to buffer size)
    REQUIRE_FALSE(get_display_filename().empty());
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: no API means no thumbnail load", "[media][no_api]") {
    // Without set_api() being called, thumbnail loading should be skipped gracefully
    set_print_filename("model.gcode");

    // Display name should still work
    REQUIRE(get_display_filename() == "model");

    // Thumbnail path should remain empty (no API to load from)
    REQUIRE(get_thumbnail_path() == "");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: uppercase extension handled", "[media][edge_case]") {
    set_print_filename("Model.GCODE");

    REQUIRE(get_display_filename() == "Model");
}

TEST_CASE_METHOD(ActivePrintMediaManagerTestFixture,
                 "ActivePrintMediaManager: mixed case extension handled", "[media][edge_case]") {
    set_print_filename("Model.GCode");

    REQUIRE(get_display_filename() == "Model");
}
