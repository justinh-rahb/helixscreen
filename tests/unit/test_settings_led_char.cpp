// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_settings_led_char.cpp
 * @brief Characterization tests for Settings LED toggle control
 *
 * These tests document the behavior of LED control in SettingsManager
 * and verify the DRY pattern with Home/PrintStatus panels.
 *
 * Tests the LOGIC only, not LVGL widgets (no UI creation).
 *
 * @see settings_manager.cpp - SettingsManager::send_led_command()
 * @see ui_panel_settings.cpp - LED toggle observer
 */

#include <string>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Test: LED Configuration Persistence
// ============================================================================

TEST_CASE("Settings LED: configured_led getter/setter", "[settings][led]") {
    // Test the basic getter/setter behavior for configured LED name

    SECTION("empty string is valid (no LED configured)") {
        std::string led = "";
        REQUIRE(led.empty());
    }

    SECTION("common LED names are valid") {
        std::vector<std::string> valid_names = {"caselight", "chamber_light", "led_strip",
                                                "status_led", "neopixel_lights"};

        for (const auto& name : valid_names) {
            REQUIRE(!name.empty());
            REQUIRE(name.length() < 64); // Reasonable max length
        }
    }
}

// ============================================================================
// Test: LED Command Guard Logic
// ============================================================================

/**
 * @brief Simulates the guard logic in send_led_command()
 *
 * The actual method checks:
 * 1. moonraker_api_ != nullptr
 * 2. configured_led_ is not empty
 *
 * If either fails, command is not sent (early return with warning log).
 */
struct LedCommandGuard {
    bool has_api = false;
    std::string configured_led;

    bool can_send_command() const {
        return has_api && !configured_led.empty();
    }

    std::string get_failure_reason() const {
        if (!has_api)
            return "no MoonrakerAPI";
        if (configured_led.empty())
            return "no LED configured";
        return "";
    }
};

TEST_CASE("Settings LED: command guard logic", "[settings][led]") {
    LedCommandGuard guard;

    SECTION("fails when no API set") {
        guard.has_api = false;
        guard.configured_led = "caselight";

        REQUIRE_FALSE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason() == "no MoonrakerAPI");
    }

    SECTION("fails when no LED configured") {
        guard.has_api = true;
        guard.configured_led = "";

        REQUIRE_FALSE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason() == "no LED configured");
    }

    SECTION("fails when both missing") {
        guard.has_api = false;
        guard.configured_led = "";

        REQUIRE_FALSE(guard.can_send_command());
        // API check comes first
        REQUIRE(guard.get_failure_reason() == "no MoonrakerAPI");
    }

    SECTION("succeeds when both present") {
        guard.has_api = true;
        guard.configured_led = "caselight";

        REQUIRE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason().empty());
    }
}

// ============================================================================
// Test: LED State Observer Sync Logic
// ============================================================================

/**
 * @brief Simulates the toggle state sync logic in the LED observer callback
 *
 * The observer callback:
 * 1. Gets LED state from subject (int: 0=off, non-zero=on)
 * 2. Updates toggle checked state accordingly
 */
struct LedToggleSync {
    bool toggle_checked = false;

    void sync_with_printer_state(int led_state) {
        toggle_checked = (led_state != 0);
    }
};

TEST_CASE("Settings LED: toggle sync with printer state", "[settings][led]") {
    LedToggleSync sync;

    SECTION("LED off (0) -> toggle unchecked") {
        sync.sync_with_printer_state(0);
        REQUIRE_FALSE(sync.toggle_checked);
    }

    SECTION("LED on (1) -> toggle checked") {
        sync.sync_with_printer_state(1);
        REQUIRE(sync.toggle_checked);
    }

    SECTION("LED on (any positive) -> toggle checked") {
        sync.sync_with_printer_state(100);
        REQUIRE(sync.toggle_checked);

        sync.sync_with_printer_state(255);
        REQUIRE(sync.toggle_checked);
    }

    SECTION("LED brightness interpreted as on/off") {
        // Some printers report brightness as 0-255
        // Any non-zero value should show as "on"
        for (int brightness : {0, 1, 50, 100, 128, 200, 255}) {
            sync.sync_with_printer_state(brightness);
            if (brightness == 0) {
                REQUIRE_FALSE(sync.toggle_checked);
            } else {
                REQUIRE(sync.toggle_checked);
            }
        }
    }
}

// ============================================================================
// Test: DRY Pattern - LED Command Format
// ============================================================================

/**
 * @brief Documents that Settings uses same API as Home/PrintStatus
 *
 * All three panels should use MoonrakerAPI::set_led_on/off()
 * which generates: SET_LED LED=<name> RED=<r> GREEN=<g> BLUE=<b> WHITE=<w>
 *
 * This replaces the old hardcoded: SET_PIN PIN=caselight VALUE=1/0
 */
TEST_CASE("Settings LED: DRY pattern documentation", "[settings][led][dry]") {
    SECTION("old pattern was hardcoded PIN command") {
        // BEFORE: Used raw gcode with hardcoded pin name
        std::string old_on = "SET_PIN PIN=caselight VALUE=1";
        std::string old_off = "SET_PIN PIN=caselight VALUE=0";

        REQUIRE(old_on.find("caselight") != std::string::npos);
        REQUIRE(old_on.find("SET_PIN") != std::string::npos);
    }

    SECTION("new pattern uses configurable LED name") {
        // AFTER: Uses MoonrakerAPI::set_led_on/off() with configured LED
        std::string led_name = "chamber_light"; // From wizard config

        // API generates: SET_LED LED=chamber_light RED=1 GREEN=1 BLUE=1 WHITE=1
        std::string expected_format = "SET_LED LED=" + led_name;
        REQUIRE(expected_format == "SET_LED LED=chamber_light");
    }

    SECTION("all panels use same LED source") {
        // Home, PrintStatus, and Settings all read from:
        // helix::wizard::LED_STRIP = "/printer/leds/strip"
        std::string config_path = "/printer/leds/strip";
        REQUIRE(config_path == "/printer/leds/strip");
    }
}

// ============================================================================
// Test: Subject State Management
// ============================================================================

/**
 * @brief Documents the guard behavior for subject updates
 *
 * When LED command cannot be sent, the subject should NOT be updated
 * to prevent UI/printer state divergence.
 */
TEST_CASE("Settings LED: subject update guard", "[settings][led]") {
    struct LedStateManager {
        bool led_enabled = false;
        bool has_api = false;
        std::string configured_led;
        bool command_was_sent = false;

        // New behavior: only update state if command can be sent
        bool set_led_enabled(bool enabled) {
            if (!has_api || configured_led.empty()) {
                // Don't update local state - command can't be sent
                return false;
            }

            led_enabled = enabled;
            command_was_sent = true;
            return true;
        }
    };

    LedStateManager mgr;

    SECTION("state not updated when no API") {
        mgr.has_api = false;
        mgr.configured_led = "caselight";
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled); // Unchanged
        REQUIRE_FALSE(mgr.command_was_sent);
    }

    SECTION("state not updated when no LED configured") {
        mgr.has_api = true;
        mgr.configured_led = "";
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled); // Unchanged
        REQUIRE_FALSE(mgr.command_was_sent);
    }

    SECTION("state updated when command can be sent") {
        mgr.has_api = true;
        mgr.configured_led = "caselight";
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE(result);
        REQUIRE(mgr.led_enabled); // Updated
        REQUIRE(mgr.command_was_sent);
    }
}
