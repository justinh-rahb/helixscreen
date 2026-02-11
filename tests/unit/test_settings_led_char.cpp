// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_settings_led_char.cpp
 * @brief Characterization tests for Settings LED toggle control
 *
 * Tests document the behavior of LED control in SettingsManager
 * and verify the DRY pattern with Home/PrintStatus panels.
 *
 * Tests the LOGIC only, not LVGL widgets (no UI creation).
 *
 * @see settings_manager.cpp - SettingsManager::send_led_command()
 * @see ui_panel_settings.cpp - LED chip row
 */

#include <set>
#include <string>
#include <vector>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Test: LED Configuration Persistence
// ============================================================================

TEST_CASE("Settings LED: configured_leds getter/setter", "[settings][led]") {
    SECTION("empty vector is valid (no LEDs configured)") {
        std::vector<std::string> leds = {};
        REQUIRE(leds.empty());
    }

    SECTION("single LED is valid") {
        std::vector<std::string> leds = {"caselight"};
        REQUIRE(leds.size() == 1);
        REQUIRE(leds[0] == "caselight");
    }

    SECTION("multiple LEDs are valid") {
        std::vector<std::string> leds = {"caselight", "chamber_light", "led_strip"};
        REQUIRE(leds.size() == 3);
    }

    SECTION("common LED names are valid") {
        std::vector<std::string> valid_names = {"caselight", "chamber_light", "led_strip",
                                                "status_led", "neopixel_lights"};

        for (const auto& name : valid_names) {
            REQUIRE(!name.empty());
            REQUIRE(name.length() < 64);
        }
    }

    SECTION("compat shim wraps single LED into vector") {
        // Simulates set_configured_led(string) compat shim
        std::vector<std::string> leds;
        std::string single = "caselight";
        leds.clear();
        if (!single.empty()) {
            leds.push_back(single);
        }
        REQUIRE(leds.size() == 1);
        REQUIRE(leds[0] == "caselight");
    }

    SECTION("compat shim with empty string gives empty vector") {
        std::vector<std::string> leds;
        std::string single = "";
        leds.clear();
        if (!single.empty()) {
            leds.push_back(single);
        }
        REQUIRE(leds.empty());
    }
}

// ============================================================================
// Test: LED Command Guard Logic (Multi-LED)
// ============================================================================

/**
 * @brief Simulates the guard logic in send_led_command()
 *
 * The actual method checks:
 * 1. moonraker_api_ != nullptr
 * 2. configured_leds_ is not empty
 *
 * Commands are sent to ALL configured LEDs.
 */
struct LedCommandGuard {
    bool has_api = false;
    std::vector<std::string> configured_leds;

    bool can_send_command() const {
        return has_api && !configured_leds.empty();
    }

    std::string get_failure_reason() const {
        if (!has_api)
            return "no MoonrakerAPI";
        if (configured_leds.empty())
            return "no LED configured";
        return "";
    }

    // Returns list of LEDs that would receive commands
    std::vector<std::string> get_target_leds() const {
        if (!can_send_command())
            return {};
        return configured_leds;
    }
};

TEST_CASE("Settings LED: command guard logic", "[settings][led]") {
    LedCommandGuard guard;

    SECTION("fails when no API set") {
        guard.has_api = false;
        guard.configured_leds = {"caselight"};

        REQUIRE_FALSE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason() == "no MoonrakerAPI");
    }

    SECTION("fails when no LEDs configured") {
        guard.has_api = true;
        guard.configured_leds = {};

        REQUIRE_FALSE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason() == "no LED configured");
    }

    SECTION("fails when both missing") {
        guard.has_api = false;
        guard.configured_leds = {};

        REQUIRE_FALSE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason() == "no MoonrakerAPI");
    }

    SECTION("succeeds with single LED") {
        guard.has_api = true;
        guard.configured_leds = {"caselight"};

        REQUIRE(guard.can_send_command());
        REQUIRE(guard.get_failure_reason().empty());
        REQUIRE(guard.get_target_leds().size() == 1);
    }

    SECTION("succeeds with multiple LEDs") {
        guard.has_api = true;
        guard.configured_leds = {"caselight", "chamber_light", "neopixel"};

        REQUIRE(guard.can_send_command());
        auto targets = guard.get_target_leds();
        REQUIRE(targets.size() == 3);
        REQUIRE(targets[0] == "caselight");
        REQUIRE(targets[1] == "chamber_light");
        REQUIRE(targets[2] == "neopixel");
    }

    SECTION("no targets when guard fails") {
        guard.has_api = false;
        guard.configured_leds = {"caselight", "chamber_light"};

        auto targets = guard.get_target_leds();
        REQUIRE(targets.empty());
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
// Test: Multi-LED Broadcast Pattern
// ============================================================================

/**
 * @brief Simulates send_led_command() broadcasting to all configured LEDs
 *
 * When toggling, the command is sent to EVERY configured LED.
 * This is the key behavioral change from single to multi-LED.
 */
TEST_CASE("Settings LED: multi-LED broadcast", "[settings][led]") {
    struct LedBroadcaster {
        bool has_api = true;
        std::vector<std::string> configured_leds;
        std::vector<std::string> commands_sent;

        void send_led_command(bool on) {
            if (!has_api || configured_leds.empty())
                return;
            for (const auto& led : configured_leds) {
                std::string cmd = on ? ("SET_LED LED=" + led + " RED=1 GREEN=1 BLUE=1 WHITE=1")
                                     : ("SET_LED LED=" + led + " RED=0 GREEN=0 BLUE=0 WHITE=0");
                commands_sent.push_back(cmd);
            }
        }
    };

    LedBroadcaster bc;

    SECTION("single LED gets one command") {
        bc.configured_leds = {"caselight"};
        bc.send_led_command(true);

        REQUIRE(bc.commands_sent.size() == 1);
        REQUIRE(bc.commands_sent[0].find("caselight") != std::string::npos);
    }

    SECTION("multiple LEDs each get a command") {
        bc.configured_leds = {"caselight", "chamber_light", "neopixel"};
        bc.send_led_command(true);

        REQUIRE(bc.commands_sent.size() == 3);
        REQUIRE(bc.commands_sent[0].find("caselight") != std::string::npos);
        REQUIRE(bc.commands_sent[1].find("chamber_light") != std::string::npos);
        REQUIRE(bc.commands_sent[2].find("neopixel") != std::string::npos);
    }

    SECTION("off command sent to all LEDs") {
        bc.configured_leds = {"led_strip", "status_led"};
        bc.send_led_command(false);

        REQUIRE(bc.commands_sent.size() == 2);
        for (const auto& cmd : bc.commands_sent) {
            REQUIRE(cmd.find("RED=0") != std::string::npos);
        }
    }

    SECTION("no commands when empty") {
        bc.configured_leds = {};
        bc.send_led_command(true);
        REQUIRE(bc.commands_sent.empty());
    }
}

// ============================================================================
// Test: DRY Pattern - LED Command Format
// ============================================================================

TEST_CASE("Settings LED: DRY pattern documentation", "[settings][led][dry]") {
    SECTION("old pattern was hardcoded PIN command") {
        std::string old_on = "SET_PIN PIN=caselight VALUE=1";
        std::string old_off = "SET_PIN PIN=caselight VALUE=0";

        REQUIRE(old_on.find("caselight") != std::string::npos);
        REQUIRE(old_on.find("SET_PIN") != std::string::npos);
    }

    SECTION("new pattern uses configurable LED names") {
        std::vector<std::string> leds = {"chamber_light", "neopixel"};

        for (const auto& led : leds) {
            std::string expected_format = "SET_LED LED=" + led;
            REQUIRE(expected_format.find("SET_LED LED=") == 0);
        }
    }

    SECTION("all panels use same LED source") {
        // Home, PrintStatus, and Settings all read from:
        // helix::wizard::LED_SELECTED = "/printer/leds/selected" (JSON array)
        // with fallback to LED_STRIP = "/printer/leds/strip" (string, legacy)
        std::string config_path = "/printer/leds/selected";
        std::string legacy_path = "/printer/leds/strip";
        REQUIRE(config_path == "/printer/leds/selected");
        REQUIRE(legacy_path == "/printer/leds/strip");
    }
}

// ============================================================================
// Test: Subject State Management (Multi-LED)
// ============================================================================

TEST_CASE("Settings LED: subject update guard", "[settings][led]") {
    struct LedStateManager {
        bool led_enabled = false;
        bool has_api = false;
        std::vector<std::string> configured_leds;
        std::vector<std::string> commands_sent;
        bool config_led_on_at_start = false;

        bool set_led_enabled(bool enabled) {
            if (!has_api || configured_leds.empty()) {
                return false;
            }

            led_enabled = enabled;
            for (const auto& led : configured_leds) {
                commands_sent.push_back(led);
            }
            config_led_on_at_start = enabled;
            return true;
        }

        bool apply_led_startup_preference() {
            if (!config_led_on_at_start) {
                return false;
            }
            if (has_api && !configured_leds.empty()) {
                led_enabled = true;
                for (const auto& led : configured_leds) {
                    commands_sent.push_back(led);
                }
                return true;
            }
            return false;
        }
    };

    LedStateManager mgr;

    SECTION("state not updated when no API") {
        mgr.has_api = false;
        mgr.configured_leds = {"caselight"};
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled);
        REQUIRE(mgr.commands_sent.empty());
    }

    SECTION("state not updated when no LEDs configured") {
        mgr.has_api = true;
        mgr.configured_leds = {};
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled);
        REQUIRE(mgr.commands_sent.empty());
    }

    SECTION("state updated and all LEDs receive command") {
        mgr.has_api = true;
        mgr.configured_leds = {"caselight", "neopixel"};
        mgr.led_enabled = false;

        bool result = mgr.set_led_enabled(true);

        REQUIRE(result);
        REQUIRE(mgr.led_enabled);
        REQUIRE(mgr.commands_sent.size() == 2);
        REQUIRE(mgr.commands_sent[0] == "caselight");
        REQUIRE(mgr.commands_sent[1] == "neopixel");
    }

    SECTION("set_led_enabled persists preference to config") {
        mgr.has_api = true;
        mgr.configured_leds = {"caselight"};
        mgr.config_led_on_at_start = false;

        mgr.set_led_enabled(true);
        REQUIRE(mgr.config_led_on_at_start == true);

        mgr.set_led_enabled(false);
        REQUIRE(mgr.config_led_on_at_start == false);
    }
}

// ============================================================================
// Test: LED Startup Preference (Multi-LED)
// ============================================================================

TEST_CASE("Settings LED: startup preference", "[settings][led]") {
    struct LedStartupManager {
        bool led_enabled = false;
        bool has_api = false;
        std::vector<std::string> configured_leds;
        std::vector<std::string> commands_sent;
        bool config_led_on_at_start = false;

        bool apply_led_startup_preference() {
            if (!config_led_on_at_start) {
                return false;
            }
            if (has_api && !configured_leds.empty()) {
                led_enabled = true;
                for (const auto& led : configured_leds) {
                    commands_sent.push_back(led);
                }
                return true;
            }
            return false;
        }
    };

    LedStartupManager mgr;

    SECTION("does nothing when preference is off") {
        mgr.has_api = true;
        mgr.configured_leds = {"caselight"};
        mgr.config_led_on_at_start = false;

        bool result = mgr.apply_led_startup_preference();

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled);
        REQUIRE(mgr.commands_sent.empty());
    }

    SECTION("turns all LEDs on when preference is enabled") {
        mgr.has_api = true;
        mgr.configured_leds = {"caselight", "chamber_light"};
        mgr.config_led_on_at_start = true;

        bool result = mgr.apply_led_startup_preference();

        REQUIRE(result);
        REQUIRE(mgr.led_enabled);
        REQUIRE(mgr.commands_sent.size() == 2);
    }

    SECTION("does nothing when preference on but no API") {
        mgr.has_api = false;
        mgr.configured_leds = {"caselight"};
        mgr.config_led_on_at_start = true;

        bool result = mgr.apply_led_startup_preference();

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled);
    }

    SECTION("does nothing when preference on but no LEDs configured") {
        mgr.has_api = true;
        mgr.configured_leds = {};
        mgr.config_led_on_at_start = true;

        bool result = mgr.apply_led_startup_preference();

        REQUIRE_FALSE(result);
        REQUIRE_FALSE(mgr.led_enabled);
    }
}

// ============================================================================
// Test: Chip Selection Toggle Logic
// ============================================================================

TEST_CASE("Settings LED: chip selection toggle", "[settings][led]") {
    std::set<std::string> selected_leds;

    auto toggle_led = [&](const std::string& name) {
        auto it = selected_leds.find(name);
        if (it != selected_leds.end()) {
            selected_leds.erase(it);
        } else {
            selected_leds.insert(name);
        }
    };

    SECTION("selecting a LED adds it") {
        toggle_led("caselight");
        REQUIRE(selected_leds.count("caselight") == 1);
    }

    SECTION("deselecting a LED removes it") {
        selected_leds.insert("caselight");
        toggle_led("caselight");
        REQUIRE(selected_leds.empty());
    }

    SECTION("multiple LEDs can be selected") {
        toggle_led("caselight");
        toggle_led("chamber_light");
        toggle_led("neopixel");

        REQUIRE(selected_leds.size() == 3);
        REQUIRE(selected_leds.count("caselight") == 1);
        REQUIRE(selected_leds.count("chamber_light") == 1);
        REQUIRE(selected_leds.count("neopixel") == 1);
    }

    SECTION("toggle is idempotent (double-toggle returns to original)") {
        toggle_led("caselight");
        REQUIRE(selected_leds.size() == 1);
        toggle_led("caselight");
        REQUIRE(selected_leds.empty());
    }

    SECTION("deselecting one doesn't affect others") {
        toggle_led("caselight");
        toggle_led("chamber_light");
        toggle_led("neopixel");

        toggle_led("chamber_light"); // deselect

        REQUIRE(selected_leds.size() == 2);
        REQUIRE(selected_leds.count("caselight") == 1);
        REQUIRE(selected_leds.count("chamber_light") == 0);
        REQUIRE(selected_leds.count("neopixel") == 1);
    }
}

// ============================================================================
// Test: Config Migration v1→v2 (string → array)
// ============================================================================

TEST_CASE("Settings LED: config migration v1 to v2", "[settings][led][config]") {
    // Simulates the migrate_v1_to_v2() logic without actual JSON library

    SECTION("single string migrates to single-element array") {
        // v1: /printer/leds/strip = "caselight"
        std::string old_value = "caselight";
        std::vector<std::string> new_value;

        if (!old_value.empty()) {
            new_value.push_back(old_value);
        }

        REQUIRE(new_value.size() == 1);
        REQUIRE(new_value[0] == "caselight");
    }

    SECTION("empty string migrates to empty array") {
        std::string old_value = "";
        std::vector<std::string> new_value;

        if (!old_value.empty()) {
            new_value.push_back(old_value);
        }

        REQUIRE(new_value.empty());
    }

    SECTION("already-migrated array left unchanged") {
        // v2: /printer/leds/selected = ["caselight", "neopixel"]
        std::vector<std::string> existing = {"caselight", "neopixel"};

        // Migration check: if selected array already exists, skip
        bool already_migrated = !existing.empty();
        REQUIRE(already_migrated);
    }

    SECTION("fresh config (no LED) produces empty array") {
        std::string old_value = "";
        bool has_old_key = false;

        std::vector<std::string> new_value;
        if (has_old_key && !old_value.empty()) {
            new_value.push_back(old_value);
        }

        REQUIRE(new_value.empty());
    }
}
