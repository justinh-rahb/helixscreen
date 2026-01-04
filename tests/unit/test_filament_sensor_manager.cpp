// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_filament_sensor_manager.cpp
 * @brief Unit tests for FilamentSensorManager
 *
 * Tests cover:
 * - Sensor discovery from Klipper object names
 * - Role assignment and uniqueness enforcement
 * - Enable/disable functionality (per-sensor and master)
 * - State updates from Moonraker status JSON
 * - Subject value correctness for UI binding
 * - State change callbacks
 * - Missing sensor handling
 */

#include "filament_sensor_manager.h"
#include "filament_sensor_types.h"

#include <spdlog/spdlog.h>

#include <vector>

#include "../catch_amalgamated.hpp"
#include "../ui_test_utils.h"

using namespace helix;
using json = nlohmann::json;

// ============================================================================
// Test Fixture
// ============================================================================

class FilamentSensorTestFixture {
  public:
    FilamentSensorTestFixture() {
        // Initialize LVGL (safe version avoids "already initialized" warnings)
        lv_init_safe();

        // Create a headless display for testing
        if (!display_created_) {
            display_ = lv_display_create(480, 320);
            alignas(64) static lv_color_t buf[480 * 10];
            lv_display_set_buffers(display_, buf, nullptr, sizeof(buf),
                                   LV_DISPLAY_RENDER_MODE_PARTIAL);
            lv_display_set_flush_cb(display_,
                                    [](lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
                                        lv_display_flush_ready(disp);
                                    });
            display_created_ = true;
        }

        // Initialize subjects (idempotent)
        mgr().init_subjects();

        // Reset state for test isolation
        mgr().reset_for_testing();
    }

    ~FilamentSensorTestFixture() {
        // Reset after each test
        mgr().reset_for_testing();
    }

  protected:
    FilamentSensorManager& mgr() {
        return FilamentSensorManager::instance();
    }

    // Helper to discover standard test sensors
    void discover_test_sensors() {
        std::vector<std::string> sensors = {"filament_switch_sensor runout",
                                            "filament_switch_sensor toolhead",
                                            "filament_motion_sensor encoder"};
        mgr().discover_sensors(sensors);
    }

    // Helper to simulate Moonraker status update
    void update_sensor_state(const std::string& klipper_name, bool detected) {
        json status;
        status[klipper_name]["filament_detected"] = detected;
        mgr().update_from_status(status);
    }

  private:
    static lv_display_t* display_;
    static bool display_created_;
};

// Static members
lv_display_t* FilamentSensorTestFixture::display_ = nullptr;
bool FilamentSensorTestFixture::display_created_ = false;

// ============================================================================
// Type Helper Tests (filament_sensor_types.h)
// ============================================================================

TEST_CASE("FilamentSensorTypes - role string conversion", "[filament][types]") {
    SECTION("role_to_display_string") {
        REQUIRE(std::string(role_to_display_string(FilamentSensorRole::NONE)) == "Unassigned");
        REQUIRE(std::string(role_to_display_string(FilamentSensorRole::RUNOUT)) == "Runout Sensor");
        REQUIRE(std::string(role_to_display_string(FilamentSensorRole::TOOLHEAD)) ==
                "Toolhead Sensor");
        REQUIRE(std::string(role_to_display_string(FilamentSensorRole::ENTRY)) == "Entry Sensor");
    }

    SECTION("role_to_config_string") {
        REQUIRE(std::string(role_to_config_string(FilamentSensorRole::NONE)) == "none");
        REQUIRE(std::string(role_to_config_string(FilamentSensorRole::RUNOUT)) == "runout");
        REQUIRE(std::string(role_to_config_string(FilamentSensorRole::TOOLHEAD)) == "toolhead");
        REQUIRE(std::string(role_to_config_string(FilamentSensorRole::ENTRY)) == "entry");
    }

    SECTION("role_from_config_string") {
        REQUIRE(role_from_config_string("none") == FilamentSensorRole::NONE);
        REQUIRE(role_from_config_string("runout") == FilamentSensorRole::RUNOUT);
        REQUIRE(role_from_config_string("toolhead") == FilamentSensorRole::TOOLHEAD);
        REQUIRE(role_from_config_string("entry") == FilamentSensorRole::ENTRY);
        REQUIRE(role_from_config_string("invalid") == FilamentSensorRole::NONE);
        REQUIRE(role_from_config_string("") == FilamentSensorRole::NONE);
    }
}

TEST_CASE("FilamentSensorTypes - type string conversion", "[filament][types]") {
    SECTION("type_to_config_string") {
        REQUIRE(std::string(type_to_config_string(FilamentSensorType::SWITCH)) == "switch");
        REQUIRE(std::string(type_to_config_string(FilamentSensorType::MOTION)) == "motion");
    }

    SECTION("type_from_config_string") {
        REQUIRE(type_from_config_string("switch") == FilamentSensorType::SWITCH);
        REQUIRE(type_from_config_string("motion") == FilamentSensorType::MOTION);
        REQUIRE(type_from_config_string("invalid") == FilamentSensorType::SWITCH);
        REQUIRE(type_from_config_string("") == FilamentSensorType::SWITCH);
    }
}

// ============================================================================
// Sensor Discovery Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - discovery",
                 "[filament][discovery]") {
    SECTION("Discovers switch sensors") {
        std::vector<std::string> sensors = {"filament_switch_sensor fsensor"};
        mgr().discover_sensors(sensors);

        REQUIRE(mgr().has_sensors());
        REQUIRE(mgr().sensor_count() == 1);

        auto configs = mgr().get_sensors();
        REQUIRE(configs.size() == 1);
        REQUIRE(configs[0].klipper_name == "filament_switch_sensor fsensor");
        REQUIRE(configs[0].sensor_name == "fsensor");
        REQUIRE(configs[0].type == FilamentSensorType::SWITCH);
        REQUIRE(configs[0].enabled == true);
        REQUIRE(configs[0].role == FilamentSensorRole::NONE);
    }

    SECTION("Discovers motion sensors") {
        std::vector<std::string> sensors = {"filament_motion_sensor encoder"};
        mgr().discover_sensors(sensors);

        REQUIRE(mgr().sensor_count() == 1);

        auto configs = mgr().get_sensors();
        REQUIRE(configs[0].type == FilamentSensorType::MOTION);
        REQUIRE(configs[0].sensor_name == "encoder");
    }

    SECTION("Discovers multiple sensors") {
        std::vector<std::string> sensors = {"filament_switch_sensor runout",
                                            "filament_switch_sensor toolhead",
                                            "filament_motion_sensor encoder"};
        mgr().discover_sensors(sensors);

        REQUIRE(mgr().sensor_count() == 3);

        auto configs = mgr().get_sensors();
        REQUIRE(configs[0].sensor_name == "runout");
        REQUIRE(configs[1].sensor_name == "toolhead");
        REQUIRE(configs[2].sensor_name == "encoder");
        REQUIRE(configs[2].type == FilamentSensorType::MOTION);
    }

    SECTION("Ignores invalid sensor names") {
        std::vector<std::string> sensors = {"filament_switch_sensor valid",
                                            "invalid_sensor_name",    // Missing proper prefix
                                            "filament_switch_sensor", // Missing sensor name
                                            "temperature_sensor chamber"};
        mgr().discover_sensors(sensors);

        REQUIRE(mgr().sensor_count() == 1);
        REQUIRE(mgr().get_sensors()[0].sensor_name == "valid");
    }

    SECTION("Empty sensor list clears previous sensors") {
        // First discover some sensors
        std::vector<std::string> sensors = {"filament_switch_sensor test"};
        mgr().discover_sensors(sensors);
        REQUIRE(mgr().sensor_count() == 1);

        // Then discover empty list
        mgr().discover_sensors({});
        REQUIRE(mgr().sensor_count() == 0);
        REQUIRE_FALSE(mgr().has_sensors());
    }

    SECTION("Re-discovery replaces sensor list") {
        std::vector<std::string> sensors1 = {"filament_switch_sensor old"};
        mgr().discover_sensors(sensors1);
        REQUIRE(mgr().get_sensors()[0].sensor_name == "old");

        std::vector<std::string> sensors2 = {"filament_switch_sensor new"};
        mgr().discover_sensors(sensors2);
        REQUIRE(mgr().sensor_count() == 1);
        REQUIRE(mgr().get_sensors()[0].sensor_name == "new");
    }

    SECTION("Sensor count subject is updated") {
        lv_subject_t* count_subject = mgr().get_sensor_count_subject();
        REQUIRE(lv_subject_get_int(count_subject) == 0);

        discover_test_sensors();
        REQUIRE(lv_subject_get_int(count_subject) == 3);

        mgr().discover_sensors({});
        REQUIRE(lv_subject_get_int(count_subject) == 0);
    }
}

// ============================================================================
// Role Assignment Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - role assignment",
                 "[filament][roles]") {
    discover_test_sensors();

    SECTION("Assign role to sensor") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);

        auto configs = mgr().get_sensors();
        auto it = std::find_if(configs.begin(), configs.end(),
                               [](const auto& c) { return c.sensor_name == "runout"; });
        REQUIRE(it != configs.end());
        REQUIRE(it->role == FilamentSensorRole::RUNOUT);
    }

    SECTION("Role assignment is unique - assigning same role clears previous") {
        // Assign RUNOUT to first sensor
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);

        // Assign RUNOUT to second sensor - should clear from first
        mgr().set_sensor_role("filament_switch_sensor toolhead", FilamentSensorRole::RUNOUT);

        auto configs = mgr().get_sensors();

        // First sensor should now have NONE
        auto runout_it = std::find_if(configs.begin(), configs.end(),
                                      [](const auto& c) { return c.sensor_name == "runout"; });
        REQUIRE(runout_it->role == FilamentSensorRole::NONE);

        // Second sensor should have RUNOUT
        auto toolhead_it = std::find_if(configs.begin(), configs.end(),
                                        [](const auto& c) { return c.sensor_name == "toolhead"; });
        REQUIRE(toolhead_it->role == FilamentSensorRole::RUNOUT);
    }

    SECTION("Can assign NONE without affecting other sensors") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);
        mgr().set_sensor_role("filament_switch_sensor toolhead", FilamentSensorRole::TOOLHEAD);

        // Clear runout assignment
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::NONE);

        auto configs = mgr().get_sensors();
        auto runout_it = std::find_if(configs.begin(), configs.end(),
                                      [](const auto& c) { return c.sensor_name == "runout"; });
        auto toolhead_it = std::find_if(configs.begin(), configs.end(),
                                        [](const auto& c) { return c.sensor_name == "toolhead"; });

        REQUIRE(runout_it->role == FilamentSensorRole::NONE);
        REQUIRE(toolhead_it->role == FilamentSensorRole::TOOLHEAD);
    }

    SECTION("Assigning role to unknown sensor does nothing") {
        mgr().set_sensor_role("filament_switch_sensor nonexistent", FilamentSensorRole::RUNOUT);

        // No sensor should have RUNOUT assigned
        for (const auto& config : mgr().get_sensors()) {
            REQUIRE(config.role == FilamentSensorRole::NONE);
        }
    }
}

// ============================================================================
// Enable/Disable Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - enable/disable",
                 "[filament][enable]") {
    discover_test_sensors();

    SECTION("Sensors start enabled by default") {
        for (const auto& config : mgr().get_sensors()) {
            REQUIRE(config.enabled == true);
        }
    }

    SECTION("Can disable individual sensor") {
        mgr().set_sensor_enabled("filament_switch_sensor runout", false);

        auto configs = mgr().get_sensors();
        auto it = std::find_if(configs.begin(), configs.end(),
                               [](const auto& c) { return c.sensor_name == "runout"; });
        REQUIRE(it->enabled == false);

        // Other sensors should still be enabled
        auto other = std::find_if(configs.begin(), configs.end(),
                                  [](const auto& c) { return c.sensor_name == "toolhead"; });
        REQUIRE(other->enabled == true);
    }

    SECTION("Master enable defaults to true") {
        REQUIRE(mgr().is_master_enabled() == true);
    }

    SECTION("Master enable can be toggled") {
        mgr().set_master_enabled(false);
        REQUIRE(mgr().is_master_enabled() == false);

        mgr().set_master_enabled(true);
        REQUIRE(mgr().is_master_enabled() == true);
    }

    SECTION("Master enabled subject is updated") {
        lv_subject_t* subject = mgr().get_master_enabled_subject();
        REQUIRE(lv_subject_get_int(subject) == 1);

        mgr().set_master_enabled(false);
        REQUIRE(lv_subject_get_int(subject) == 0);

        mgr().set_master_enabled(true);
        REQUIRE(lv_subject_get_int(subject) == 1);
    }
}

// ============================================================================
// State Update Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - state updates",
                 "[filament][state]") {
    discover_test_sensors();
    mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);

    SECTION("Updates filament_detected from status JSON") {
        // Initially no state set
        auto state = mgr().get_sensor_state(FilamentSensorRole::RUNOUT);
        REQUIRE(state.has_value());
        REQUIRE(state->filament_detected == false);

        // Update via status
        json status;
        status["filament_switch_sensor runout"]["filament_detected"] = true;
        mgr().update_from_status(status);

        state = mgr().get_sensor_state(FilamentSensorRole::RUNOUT);
        REQUIRE(state->filament_detected == true);
    }

    SECTION("Motion sensor updates include detection_count") {
        mgr().set_sensor_role("filament_motion_sensor encoder", FilamentSensorRole::ENTRY);

        json status;
        status["filament_motion_sensor encoder"]["filament_detected"] = true;
        status["filament_motion_sensor encoder"]["enabled"] = true;
        status["filament_motion_sensor encoder"]["detection_count"] = 42;
        mgr().update_from_status(status);

        auto state = mgr().get_sensor_state(FilamentSensorRole::ENTRY);
        REQUIRE(state.has_value());
        REQUIRE(state->filament_detected == true);
        REQUIRE(state->detection_count == 42);
    }

    SECTION("State change callback is fired") {
        bool callback_fired = false;
        std::string changed_sensor;
        bool old_detected = false;
        bool new_detected = false;

        mgr().set_state_change_callback([&](const std::string& name,
                                            const FilamentSensorState& old_state,
                                            const FilamentSensorState& new_state) {
            callback_fired = true;
            changed_sensor = name;
            old_detected = old_state.filament_detected;
            new_detected = new_state.filament_detected;
        });

        // Trigger state change
        update_sensor_state("filament_switch_sensor runout", true);

        REQUIRE(callback_fired);
        REQUIRE(changed_sensor == "filament_switch_sensor runout");
        REQUIRE(old_detected == false);
        REQUIRE(new_detected == true);
    }

    SECTION("No callback when state doesn't change") {
        // Set initial state
        update_sensor_state("filament_switch_sensor runout", true);

        int callback_count = 0;
        mgr().set_state_change_callback([&](const std::string&, const FilamentSensorState&,
                                            const FilamentSensorState&) { callback_count++; });

        // Update with same value
        update_sensor_state("filament_switch_sensor runout", true);

        REQUIRE(callback_count == 0);
    }
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - state queries",
                 "[filament][queries]") {
    discover_test_sensors();
    mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);
    update_sensor_state("filament_switch_sensor runout", true);

    SECTION("is_filament_detected returns correct state") {
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::RUNOUT) == true);

        update_sensor_state("filament_switch_sensor runout", false);
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::RUNOUT) == false);
    }

    SECTION("is_filament_detected returns false when master disabled") {
        mgr().set_master_enabled(false);
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::RUNOUT) == false);
    }

    SECTION("is_filament_detected returns false when sensor disabled") {
        mgr().set_sensor_enabled("filament_switch_sensor runout", false);
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::RUNOUT) == false);
    }

    SECTION("is_filament_detected returns false for unassigned role") {
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::TOOLHEAD) == false);
    }

    SECTION("is_filament_detected returns false for NONE role") {
        REQUIRE(mgr().is_filament_detected(FilamentSensorRole::NONE) == false);
    }

    SECTION("is_sensor_available checks role assignment and enabled") {
        REQUIRE(mgr().is_sensor_available(FilamentSensorRole::RUNOUT) == true);
        REQUIRE(mgr().is_sensor_available(FilamentSensorRole::TOOLHEAD) == false);

        mgr().set_sensor_enabled("filament_switch_sensor runout", false);
        REQUIRE(mgr().is_sensor_available(FilamentSensorRole::RUNOUT) == false);
    }

    SECTION("get_sensor_state returns nullopt for unassigned role") {
        auto state = mgr().get_sensor_state(FilamentSensorRole::TOOLHEAD);
        REQUIRE_FALSE(state.has_value());
    }

    SECTION("has_any_runout detects runout condition") {
        // Filament present = no runout
        REQUIRE(mgr().has_any_runout() == false);

        // Remove filament = runout
        update_sensor_state("filament_switch_sensor runout", false);
        REQUIRE(mgr().has_any_runout() == true);
    }

    SECTION("has_any_runout ignores unassigned sensors") {
        // Clear role from sensor
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::NONE);
        update_sensor_state("filament_switch_sensor runout", false);

        // Should not report runout since sensor has no role
        REQUIRE(mgr().has_any_runout() == false);
    }

    SECTION("has_any_runout returns false when master disabled") {
        update_sensor_state("filament_switch_sensor runout", false);
        mgr().set_master_enabled(false);

        REQUIRE(mgr().has_any_runout() == false);
    }
}

// ============================================================================
// Subject Value Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - subject values",
                 "[filament][subjects]") {
    discover_test_sensors();

    SECTION("Role subjects show -1 when no sensor assigned") {
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == -1);
        REQUIRE(lv_subject_get_int(mgr().get_toolhead_detected_subject()) == -1);
        REQUIRE(lv_subject_get_int(mgr().get_entry_detected_subject()) == -1);
    }

    SECTION("Role subjects update when sensor assigned and state changes") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);

        // After assignment, should show 0 (no filament) since state defaults to false
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == 0);

        // Update state to detected
        // Note: reset_for_testing() enables sync_mode, so update_from_status()
        // updates subjects synchronously instead of using lv_async_call().
        update_sensor_state("filament_switch_sensor runout", true);
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == 1);

        // Update state to empty
        update_sensor_state("filament_switch_sensor runout", false);
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == 0);
    }

    SECTION("Role subjects show -1 when master disabled") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);
        update_sensor_state("filament_switch_sensor runout", true);

        mgr().set_master_enabled(false);
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == -1);
    }

    SECTION("Role subjects show -1 when sensor disabled") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);
        update_sensor_state("filament_switch_sensor runout", true);

        mgr().set_sensor_enabled("filament_switch_sensor runout", false);
        REQUIRE(lv_subject_get_int(mgr().get_runout_detected_subject()) == -1);
    }

    SECTION("any_runout subject reflects runout state") {
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);
        update_sensor_state("filament_switch_sensor runout", true);

        // Filament detected = no runout
        REQUIRE(lv_subject_get_int(mgr().get_any_runout_subject()) == 0);

        // Filament removed = runout detected
        update_sensor_state("filament_switch_sensor runout", false);
        REQUIRE(lv_subject_get_int(mgr().get_any_runout_subject()) == 1);
    }
}

// ============================================================================
// Motion Sensor Tests
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - motion sensors",
                 "[filament][motion]") {
    discover_test_sensors();
    mgr().set_sensor_role("filament_motion_sensor encoder", FilamentSensorRole::ENTRY);

    SECTION("Motion sensor type is correctly identified") {
        auto configs = mgr().get_sensors();
        auto it = std::find_if(configs.begin(), configs.end(),
                               [](const auto& c) { return c.sensor_name == "encoder"; });
        REQUIRE(it != configs.end());
        REQUIRE(it->type == FilamentSensorType::MOTION);
    }

    SECTION("is_motion_active requires enabled motion sensor") {
        json status;
        status["filament_motion_sensor encoder"]["filament_detected"] = true;
        status["filament_motion_sensor encoder"]["enabled"] = true;
        mgr().update_from_status(status);

        REQUIRE(mgr().is_motion_active() == true);

        // Disable sensor
        mgr().set_sensor_enabled("filament_motion_sensor encoder", false);
        REQUIRE(mgr().is_motion_active() == false);
    }

    SECTION("motion_active subject updates correctly") {
        json status;
        status["filament_motion_sensor encoder"]["filament_detected"] = true;
        status["filament_motion_sensor encoder"]["enabled"] = true;
        mgr().update_from_status(status);

        REQUIRE(lv_subject_get_int(mgr().get_motion_active_subject()) == 1);

        // Master disable should hide motion
        mgr().set_master_enabled(false);
        REQUIRE(lv_subject_get_int(mgr().get_motion_active_subject()) == 0);
    }
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - edge cases",
                 "[filament][edge]") {
    SECTION("Handles sensors with spaces in names") {
        std::vector<std::string> sensors = {"filament_switch_sensor my runout sensor"};
        mgr().discover_sensors(sensors);

        REQUIRE(mgr().sensor_count() == 1);
        REQUIRE(mgr().get_sensors()[0].sensor_name == "my runout sensor");
    }

    SECTION("Status update for unknown sensor is ignored") {
        discover_test_sensors();

        json status;
        status["filament_switch_sensor unknown"]["filament_detected"] = true;
        mgr().update_from_status(status);

        // Should not crash or affect known sensors
        REQUIRE(mgr().sensor_count() == 3);
    }

    SECTION("Empty status update is handled") {
        discover_test_sensors();

        json status = json::object();
        mgr().update_from_status(status);

        // Should not crash
        REQUIRE(mgr().has_sensors());
    }

    SECTION("Multiple rapid state changes fire callbacks correctly") {
        discover_test_sensors();
        mgr().set_sensor_role("filament_switch_sensor runout", FilamentSensorRole::RUNOUT);

        int callback_count = 0;
        mgr().set_state_change_callback([&](const std::string&, const FilamentSensorState&,
                                            const FilamentSensorState&) { callback_count++; });

        // Rapid changes
        update_sensor_state("filament_switch_sensor runout", true);
        update_sensor_state("filament_switch_sensor runout", false);
        update_sensor_state("filament_switch_sensor runout", true);
        update_sensor_state("filament_switch_sensor runout", false);

        REQUIRE(callback_count == 4);
    }
}

// ============================================================================
// Thread Safety Tests (basic validation)
// ============================================================================

TEST_CASE_METHOD(FilamentSensorTestFixture, "FilamentSensorManager - thread safety basics",
                 "[filament][threading]") {
    discover_test_sensors();

    SECTION("Concurrent get_sensors returns consistent copy") {
        // This tests that get_sensors() returns a copy, not a reference
        // Note: Use "toolhead" sensor because "runout" gets auto-assigned RUNOUT role
        auto copy1 = mgr().get_sensors();

        // Modify manager state
        mgr().set_sensor_role("filament_switch_sensor toolhead", FilamentSensorRole::TOOLHEAD);

        auto copy2 = mgr().get_sensors();

        // copy1 should still have the old state (NONE)
        auto it1 = std::find_if(copy1.begin(), copy1.end(),
                                [](const auto& c) { return c.sensor_name == "toolhead"; });
        REQUIRE(it1->role == FilamentSensorRole::NONE);

        // copy2 should have new state
        auto it2 = std::find_if(copy2.begin(), copy2.end(),
                                [](const auto& c) { return c.sensor_name == "toolhead"; });
        REQUIRE(it2->role == FilamentSensorRole::TOOLHEAD);
    }
}
