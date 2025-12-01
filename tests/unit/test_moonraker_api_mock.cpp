// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_moonraker_api_mock.cpp
 * @brief Unit tests for MoonrakerAPIMock - HTTP file transfer mocking
 *
 * Tests the mock API's ability to:
 * - Download files from test assets regardless of working directory
 * - Upload files (mock always succeeds)
 * - Handle missing files with proper error callbacks
 *
 * TDD: These tests are written BEFORE the implementation is complete.
 */

#include "../catch_amalgamated.hpp"
#include "moonraker_api_mock.h"
#include "moonraker_client_mock.h"
#include "printer_state.h"

#include <atomic>
#include <filesystem>
#include <fstream>

// ============================================================================
// Test Fixture
// ============================================================================

class MoonrakerAPIMockTestFixture {
  public:
    MoonrakerAPIMockTestFixture() : client_(MoonrakerClientMock::PrinterType::VORON_24) {
        state_.init_subjects(false); // Don't register XML bindings in tests
        api_ = std::make_unique<MoonrakerAPIMock>(client_, state_);
    }

  protected:
    MoonrakerClientMock client_;
    PrinterState state_;
    std::unique_ptr<MoonrakerAPIMock> api_;
};

// ============================================================================
// download_file Tests
// ============================================================================

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file finds existing test file",
                 "[moonraker][mock][api][download]") {
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};
    std::string downloaded_content;

    api_->download_file(
        "gcodes", "3DBenchy.gcode",
        [&](const std::string& content) {
            downloaded_content = content;
            success_called.store(true);
        },
        [&](const MoonrakerError&) { error_called.store(true); });

    REQUIRE(success_called.load());
    REQUIRE_FALSE(error_called.load());
    REQUIRE(downloaded_content.size() > 100); // Should have substantial content
    // Verify it looks like G-code
    REQUIRE(downloaded_content.find("G") != std::string::npos);
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file returns FILE_NOT_FOUND for missing file",
                 "[moonraker][mock][api][download]") {
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};
    MoonrakerError captured_error;

    api_->download_file(
        "gcodes", "nonexistent_file_xyz123.gcode",
        [&](const std::string&) { success_called.store(true); },
        [&](const MoonrakerError& err) {
            captured_error = err;
            error_called.store(true);
        });

    REQUIRE_FALSE(success_called.load());
    REQUIRE(error_called.load());
    REQUIRE(captured_error.type == MoonrakerErrorType::FILE_NOT_FOUND);
    REQUIRE(captured_error.method == "download_file");
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file strips directory from path",
                 "[moonraker][mock][api][download]") {
    // Test that paths like "subdir/file.gcode" still find "file.gcode" in test assets
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};

    api_->download_file(
        "gcodes", "some/nested/path/3DBenchy.gcode",
        [&](const std::string& content) {
            success_called.store(true);
            // Verify we got actual content
            REQUIRE(content.size() > 100);
        },
        [&](const MoonrakerError&) { error_called.store(true); });

    REQUIRE(success_called.load());
    REQUIRE_FALSE(error_called.load());
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file works regardless of CWD",
                 "[moonraker][mock][api][download]") {
    // This test verifies the fallback path search works
    // The implementation should try multiple paths:
    // - assets/test_gcodes/
    // - ../assets/test_gcodes/
    // - ../../assets/test_gcodes/

    std::atomic<bool> success_called{false};

    api_->download_file(
        "gcodes", "3DBenchy.gcode", [&](const std::string&) { success_called.store(true); },
        [&](const MoonrakerError& err) {
            // Log the error for debugging if this fails
            INFO("download_file error: " << err.message);
        });

    // Should succeed from project root or build/bin/
    REQUIRE(success_called.load());
}

// ============================================================================
// upload_file Tests
// ============================================================================

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture, "MoonrakerAPIMock upload_file always succeeds",
                 "[moonraker][mock][api][upload]") {
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};

    api_->upload_file("gcodes", "test_upload.gcode", "G28\nG1 X100 Y100 F3000\n",
                      [&]() { success_called.store(true); },
                      [&](const MoonrakerError&) { error_called.store(true); });

    REQUIRE(success_called.load());
    REQUIRE_FALSE(error_called.load());
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock upload_file_with_name always succeeds",
                 "[moonraker][mock][api][upload]") {
    std::atomic<bool> success_called{false};
    std::atomic<bool> error_called{false};

    api_->upload_file_with_name("gcodes", "subdir/test.gcode", "custom_filename.gcode",
                                "G28\nM104 S200\n", [&]() { success_called.store(true); },
                                [&](const MoonrakerError&) { error_called.store(true); });

    REQUIRE(success_called.load());
    REQUIRE_FALSE(error_called.load());
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock upload_file handles large content",
                 "[moonraker][mock][api][upload]") {
    std::atomic<bool> success_called{false};

    // Generate a large G-code content (simulate realistic file)
    std::string large_content;
    large_content.reserve(1024 * 100); // ~100KB
    for (int i = 0; i < 5000; i++) {
        large_content += "G1 X" + std::to_string(i % 200) + " Y" + std::to_string(i % 200) +
                         " E" + std::to_string(i * 0.1) + "\n";
    }

    api_->upload_file("gcodes", "large_file.gcode", large_content,
                      [&]() { success_called.store(true); }, [&](const MoonrakerError&) {});

    REQUIRE(success_called.load());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file handles null success callback",
                 "[moonraker][mock][api][download]") {
    // Should not crash when success callback is null
    REQUIRE_NOTHROW(api_->download_file("gcodes", "3DBenchy.gcode", nullptr,
                                        [](const MoonrakerError&) {}));
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock download_file handles null error callback",
                 "[moonraker][mock][api][download]") {
    // Should not crash when error callback is null (for missing file)
    REQUIRE_NOTHROW(
        api_->download_file("gcodes", "nonexistent.gcode", [](const std::string&) {}, nullptr));
}

TEST_CASE_METHOD(MoonrakerAPIMockTestFixture,
                 "MoonrakerAPIMock upload_file handles null success callback",
                 "[moonraker][mock][api][upload]") {
    // Should not crash when success callback is null
    REQUIRE_NOTHROW(
        api_->upload_file("gcodes", "test.gcode", "G28", nullptr, [](const MoonrakerError&) {}));
}
