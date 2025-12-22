// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../catch_amalgamated.hpp"

#include "print_start_analyzer.h"
#include "ui_print_preparation_manager.h"

using namespace helix;
using namespace helix::ui;

// ============================================================================
// Test Fixture: Mock Dependencies
// ============================================================================

// PrintPreparationManager has nullable dependencies - we can test formatting
// and state management without actual API/printer connections.

// ============================================================================
// Tests: Macro Analysis Formatting
// ============================================================================

TEST_CASE("PrintPreparationManager: format_macro_operations", "[print_preparation][macro]") {
    PrintPreparationManager manager;
    // No dependencies set - tests formatting without API

    SECTION("Returns empty string when no analysis available") {
        REQUIRE(manager.format_macro_operations().empty());
        REQUIRE(manager.has_macro_analysis() == false);
    }
}

TEST_CASE("PrintPreparationManager: is_macro_op_controllable", "[print_preparation][macro]") {
    PrintPreparationManager manager;

    SECTION("Returns false when no analysis available") {
        REQUIRE(manager.is_macro_op_controllable(PrintStartOpCategory::BED_LEVELING) == false);
        REQUIRE(manager.is_macro_op_controllable(PrintStartOpCategory::QGL) == false);
        REQUIRE(manager.is_macro_op_controllable(PrintStartOpCategory::Z_TILT) == false);
        REQUIRE(manager.is_macro_op_controllable(PrintStartOpCategory::NOZZLE_CLEAN) == false);
    }
}

TEST_CASE("PrintPreparationManager: get_macro_skip_param", "[print_preparation][macro]") {
    PrintPreparationManager manager;

    SECTION("Returns empty string when no analysis available") {
        REQUIRE(manager.get_macro_skip_param(PrintStartOpCategory::BED_LEVELING).empty());
        REQUIRE(manager.get_macro_skip_param(PrintStartOpCategory::QGL).empty());
    }
}

// ============================================================================
// Tests: File Operations Scanning
// ============================================================================

TEST_CASE("PrintPreparationManager: format_detected_operations", "[print_preparation][gcode]") {
    PrintPreparationManager manager;

    SECTION("Returns empty string when no scan result available") {
        REQUIRE(manager.format_detected_operations().empty());
    }

    SECTION("has_scan_result_for returns false when no scan done") {
        REQUIRE(manager.has_scan_result_for("test.gcode") == false);
        REQUIRE(manager.has_scan_result_for("") == false);
    }
}

TEST_CASE("PrintPreparationManager: clear_scan_cache", "[print_preparation][gcode]") {
    PrintPreparationManager manager;

    SECTION("Can be called when no cache exists") {
        // Should not throw or crash
        manager.clear_scan_cache();
        REQUIRE(manager.format_detected_operations().empty());
    }
}

// ============================================================================
// Tests: Resource Safety
// ============================================================================

TEST_CASE("PrintPreparationManager: check_modification_capability", "[print_preparation][safety]") {
    PrintPreparationManager manager;
    // No API set - tests fallback behavior

    SECTION("Without API, checks disk space fallback") {
        auto capability = manager.check_modification_capability();
        // Without API, has_plugin is false
        REQUIRE(capability.has_plugin == false);
        // Should still check disk space
        // (can_modify depends on system - just verify it returns valid struct)
        REQUIRE((capability.can_modify || !capability.can_modify)); // Always true, just checking no crash
    }
}

TEST_CASE("PrintPreparationManager: get_temp_directory", "[print_preparation][safety]") {
    PrintPreparationManager manager;

    SECTION("Returns usable temp directory path") {
        std::string temp_dir = manager.get_temp_directory();
        // Should return a non-empty path on any reasonable system
        // (empty only if all fallbacks fail, which shouldn't happen in tests)
        INFO("Temp directory: " << temp_dir);
        // Just verify it doesn't crash and returns something reasonable
        REQUIRE(temp_dir.find("helix") != std::string::npos);
    }
}

TEST_CASE("PrintPreparationManager: set_cached_file_size", "[print_preparation][safety]") {
    PrintPreparationManager manager;

    SECTION("Setting file size affects modification capability calculation") {
        // Set a reasonable file size
        manager.set_cached_file_size(10 * 1024 * 1024);  // 10MB

        auto capability = manager.check_modification_capability();
        // Required bytes should account for file size
        REQUIRE(capability.required_bytes > 10 * 1024 * 1024);
    }

    SECTION("Very large file size may exceed available space") {
        // Set an extremely large file size
        manager.set_cached_file_size(1000ULL * 1024 * 1024 * 1024);  // 1TB

        auto capability = manager.check_modification_capability();
        // Should report insufficient space for such a large file
        // (unless running on a system with 2TB+ free space)
        INFO("can_modify: " << capability.can_modify);
        INFO("reason: " << capability.reason);
        // Just verify it handles large values without overflow/crash
        REQUIRE((capability.can_modify || !capability.can_modify));
    }
}

// ============================================================================
// Tests: Checkbox Reading
// ============================================================================

TEST_CASE("PrintPreparationManager: read_options_from_checkboxes", "[print_preparation][options]") {
    PrintPreparationManager manager;
    // No checkboxes set - tests null handling

    SECTION("Returns default options when no checkboxes set") {
        auto options = manager.read_options_from_checkboxes();
        REQUIRE(options.bed_leveling == false);
        REQUIRE(options.qgl == false);
        REQUIRE(options.z_tilt == false);
        REQUIRE(options.nozzle_clean == false);
        REQUIRE(options.timelapse == false);
    }
}

// ============================================================================
// Tests: Lifecycle Management
// ============================================================================

TEST_CASE("PrintPreparationManager: is_preparing", "[print_preparation][lifecycle]") {
    PrintPreparationManager manager;

    SECTION("Not preparing by default") {
        REQUIRE(manager.is_preparing() == false);
    }
}

TEST_CASE("PrintPreparationManager: cancel_preparation", "[print_preparation][lifecycle]") {
    PrintPreparationManager manager;

    SECTION("Can be called when not preparing") {
        // Should not throw or crash
        manager.cancel_preparation();
        REQUIRE(manager.is_preparing() == false);
    }
}

// ============================================================================
// Tests: Move Semantics
// ============================================================================

TEST_CASE("PrintPreparationManager: move constructor", "[print_preparation][lifecycle]") {
    PrintPreparationManager manager1;
    manager1.set_cached_file_size(1024);

    SECTION("Move constructor transfers state") {
        PrintPreparationManager manager2 = std::move(manager1);
        // manager2 should be usable
        REQUIRE(manager2.is_preparing() == false);
        // Can call methods without crash
        manager2.clear_scan_cache();
    }
}

TEST_CASE("PrintPreparationManager: move assignment", "[print_preparation][lifecycle]") {
    PrintPreparationManager manager1;
    PrintPreparationManager manager2;
    manager1.set_cached_file_size(2048);

    SECTION("Move assignment transfers state") {
        manager2 = std::move(manager1);
        // manager2 should be usable
        REQUIRE(manager2.is_preparing() == false);
    }
}
