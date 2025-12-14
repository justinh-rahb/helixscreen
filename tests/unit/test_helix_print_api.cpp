// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2025 HelixScreen Contributors
 *
 * Unit tests for the helix_print Moonraker plugin API integration.
 *
 * Tests cover:
 * 1. Plugin detection (check_helix_plugin)
 * 2. Modified print API (start_modified_print)
 * 3. Fallback behavior when plugin unavailable
 * 4. Error handling and validation
 */

#include "../../include/moonraker_api.h"
#include "../../include/moonraker_client.h"
#include "../../include/printer_state.h"
#include "../../lvgl/lvgl.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Global LVGL Initialization
// ============================================================================

struct LVGLInitializerHelixPrint {
    LVGLInitializerHelixPrint() {
        static bool initialized = false;
        if (!initialized) {
            lv_init();
            lv_display_t* disp = lv_display_create(800, 480);
            alignas(64) static lv_color_t buf[800 * 10];
            lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
            initialized = true;
        }
    }
};

static LVGLInitializerHelixPrint lvgl_init_helix;

// ============================================================================
// Test Fixtures
// ============================================================================

class HelixPrintAPITestFixture {
  public:
    HelixPrintAPITestFixture() {
        state.init_subjects();
        client = std::make_unique<MoonrakerClient>();
        api = std::make_unique<MoonrakerAPI>(*client, state);
        reset_callbacks();
    }

    void reset_callbacks() {
        success_called = false;
        error_called = false;
        bool_result = false;
        error_message.clear();
        modified_print_result = {};
    }

    PrinterState state;
    std::unique_ptr<MoonrakerClient> client;
    std::unique_ptr<MoonrakerAPI> api;

    // Callback tracking
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};
    std::atomic<bool> bool_result{false};
    std::string error_message;
    ModifiedPrintResult modified_print_result;
};

// ============================================================================
// Plugin Detection Tests
// ============================================================================

TEST_CASE_METHOD(HelixPrintAPITestFixture, "HelixPrint API - has_helix_plugin initial state",
                 "[helix_print][api]") {
    // Initially, plugin should not be detected (no check performed yet)
    REQUIRE(api->has_helix_plugin() == false);
}

TEST_CASE_METHOD(HelixPrintAPITestFixture,
                 "HelixPrint API - check_helix_plugin with disconnected client",
                 "[helix_print][api]") {
    // With disconnected client, check should report plugin unavailable
    // (error callback path returns false, not error)

    api->check_helix_plugin(
        [this](bool available) {
            bool_result = available;
            success_called = true;
        },
        [this](const MoonrakerError& err) {
            error_message = err.message;
            error_called = true;
        });

    // Give async operation time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should complete (either success with false, or trigger error->false path)
    // The implementation treats errors as "plugin not available"
    // So either way, has_helix_plugin should be false
    REQUIRE(api->has_helix_plugin() == false);
}

// ============================================================================
// Modified Print API Validation Tests
// ============================================================================

TEST_CASE_METHOD(HelixPrintAPITestFixture,
                 "HelixPrint API - start_modified_print validates filename",
                 "[helix_print][api][security]") {
    SECTION("Rejects path traversal in filename") {
        api->start_modified_print(
            "../../../etc/passwd", // Malicious path
            "G28\n", {"test_mod"},
            [this](const ModifiedPrintResult&) { success_called = true; },
            [this](const MoonrakerError& err) {
                error_message = err.message;
                error_called = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(error_called == true);
        REQUIRE(success_called == false);
        REQUIRE(error_message.find("directory traversal") != std::string::npos);
    }

    SECTION("Rejects filename with newlines") {
        api->start_modified_print(
            "test\nfile.gcode", // Newline injection
            "G28\n", {"test_mod"},
            [this](const ModifiedPrintResult&) { success_called = true; },
            [this](const MoonrakerError& err) {
                error_message = err.message;
                error_called = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(error_called == true);
        REQUIRE(success_called == false);
    }

    SECTION("Accepts valid filename") {
        // This will fail due to disconnected client, but should pass validation
        api->start_modified_print(
            "benchy.gcode", "G28\nG1 X0 Y0\n", {"bed_leveling_disabled"},
            [this](const ModifiedPrintResult& result) {
                modified_print_result = result;
                success_called = true;
            },
            [this](const MoonrakerError& err) {
                error_message = err.message;
                error_called = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should fail due to disconnected client, not validation
        // Error message should NOT mention "directory traversal" or "illegal characters"
        if (error_called) {
            REQUIRE(error_message.find("directory traversal") == std::string::npos);
            REQUIRE(error_message.find("illegal characters") == std::string::npos);
        }
    }

    SECTION("Accepts filename with subdirectory") {
        api->start_modified_print(
            "prints/2024/benchy.gcode", // Valid subdirectory path
            "G28\n", {"test_mod"},
            [this](const ModifiedPrintResult&) { success_called = true; },
            [this](const MoonrakerError& err) {
                error_message = err.message;
                error_called = true;
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Should NOT fail validation (may fail due to network)
        if (error_called) {
            REQUIRE(error_message.find("directory traversal") == std::string::npos);
        }
    }
}

// ============================================================================
// ModifiedPrintResult Structure Tests
// ============================================================================

TEST_CASE("HelixPrint API - ModifiedPrintResult structure", "[helix_print][api]") {
    ModifiedPrintResult result;

    SECTION("Default values are empty") {
        REQUIRE(result.original_filename.empty());
        REQUIRE(result.print_filename.empty());
        REQUIRE(result.temp_filename.empty());
        REQUIRE(result.status.empty());
    }

    SECTION("Can be populated") {
        result.original_filename = "benchy.gcode";
        result.print_filename = ".helix_print/benchy.gcode";
        result.temp_filename = ".helix_temp/mod_123_benchy.gcode";
        result.status = "printing";

        REQUIRE(result.original_filename == "benchy.gcode");
        REQUIRE(result.print_filename == ".helix_print/benchy.gcode");
        REQUIRE(result.temp_filename == ".helix_temp/mod_123_benchy.gcode");
        REQUIRE(result.status == "printing");
    }
}

// ============================================================================
// Modification List Tests
// ============================================================================

TEST_CASE_METHOD(HelixPrintAPITestFixture, "HelixPrint API - handles empty modifications list",
                 "[helix_print][api]") {
    // Empty modifications list should be valid
    api->start_modified_print(
        "benchy.gcode", "G28\n", {}, // Empty modifications
        [this](const ModifiedPrintResult&) { success_called = true; },
        [this](const MoonrakerError& err) {
            error_message = err.message;
            error_called = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Should not fail validation due to empty modifications
    if (error_called) {
        REQUIRE(error_message.find("modifications") == std::string::npos);
    }
}

TEST_CASE_METHOD(HelixPrintAPITestFixture, "HelixPrint API - handles multiple modifications",
                 "[helix_print][api]") {
    std::vector<std::string> mods = {"bed_leveling_disabled", "z_tilt_disabled", "qgl_disabled",
                                     "nozzle_clean_disabled"};

    api->start_modified_print(
        "benchy.gcode", "G28\n", mods,
        [this](const ModifiedPrintResult&) { success_called = true; },
        [this](const MoonrakerError& err) {
            error_message = err.message;
            error_called = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Should not fail validation
    if (error_called) {
        REQUIRE(error_message.find("directory traversal") == std::string::npos);
    }
}

// ============================================================================
// Large Content Tests
// ============================================================================

TEST_CASE_METHOD(HelixPrintAPITestFixture, "HelixPrint API - handles large G-code content",
                 "[helix_print][api]") {
    // Generate large G-code content (simulating a real print file)
    std::string large_content = "G28\n";
    for (int i = 0; i < 10000; i++) {
        large_content += "G1 X" + std::to_string(i % 200) + " Y" + std::to_string(i % 200) + " Z" +
                         std::to_string(i / 1000.0) + " E0.5 F1200\n";
    }

    api->start_modified_print(
        "large_print.gcode", large_content, {"bed_leveling_disabled"},
        [this](const ModifiedPrintResult&) { success_called = true; },
        [this](const MoonrakerError& err) {
            error_message = err.message;
            error_called = true;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should not crash or fail validation
    // Will fail network send due to disconnected client
    if (error_called) {
        REQUIRE(error_message.find("directory traversal") == std::string::npos);
    }
}
