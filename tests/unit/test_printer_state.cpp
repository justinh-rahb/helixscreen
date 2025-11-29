/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "../catch_amalgamated.hpp"
#include "app_globals.h"
#include "moonraker_client.h"
#include "printer_state.h"
#include "../ui_test_utils.h"

using Catch::Approx;

// ============================================================================
// Singleton Behavior Tests
// ============================================================================

TEST_CASE("PrinterState: Singleton returns same instance", "[printer_state][singleton]") {
    lv_init();

    PrinterState& instance1 = get_printer_state();
    PrinterState& instance2 = get_printer_state();

    // Should be the exact same object (same memory address)
    REQUIRE(&instance1 == &instance2);
}

TEST_CASE("PrinterState: Singleton persists modifications", "[printer_state][singleton]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.init_subjects();

    // Modify a value through one reference
    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Connected");

    // Read it back through another reference
    PrinterState& state2 = get_printer_state();
    REQUIRE(lv_subject_get_int(state2.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::CONNECTED));
}

TEST_CASE("PrinterState: Singleton subjects have consistent addresses", "[printer_state][singleton]") {
    lv_init();

    PrinterState& state1 = get_printer_state();
    state1.init_subjects();

    lv_subject_t* subject1 = state1.get_printer_connection_state_subject();

    PrinterState& state2 = get_printer_state();
    lv_subject_t* subject2 = state2.get_printer_connection_state_subject();

    // Subject pointers must be identical (not just equal values)
    REQUIRE(subject1 == subject2);
}

// ============================================================================
// Observer Pattern Tests
// ============================================================================

TEST_CASE("PrinterState: Observer fires when printer connection state changes", "[printer_state][observer]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Allow re-initialization after lv_init()
    state.init_subjects(false);  // Skip XML registration

    // Register observer
    auto observer_cb = [](lv_observer_t* observer, lv_subject_t* subject) {
        int* count_ptr = static_cast<int*>(lv_observer_get_user_data(observer));
        int* value_ptr = count_ptr + 1;

        (*count_ptr)++;
        *value_ptr = lv_subject_get_int(subject);
    };

    int user_data[2] = {0, -1}; // [callback_count, last_value]

    lv_subject_add_observer(state.get_printer_connection_state_subject(),
                           observer_cb, user_data);

    // LVGL auto-notifies observers when first added (fires immediately with current value)
    REQUIRE(user_data[0] == 1); // Callback fired immediately with initial value (0)
    REQUIRE(user_data[1] == 0); // Initial value is DISCONNECTED (0)

    // Change state - should trigger observer again
    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTING), "Connecting...");

    REQUIRE(user_data[0] == 2); // Callback fired again with new value
    REQUIRE(user_data[1] == static_cast<int>(ConnectionState::CONNECTING));

    // Change again
    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Connected");

    REQUIRE(user_data[0] == 3); // Callback fired three times total (initial + 2 changes)
    REQUIRE(user_data[1] == static_cast<int>(ConnectionState::CONNECTED));
}

TEST_CASE("PrinterState: Observer fires when network status changes", "[printer_state][observer][network]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Allow re-initialization after lv_init()
    state.init_subjects(false);  // Skip XML registration

    int callback_count = 0;

    auto observer_cb = [](lv_observer_t* observer, lv_subject_t*) {
        int* count_ptr = static_cast<int*>(lv_observer_get_user_data(observer));
        (*count_ptr)++;
    };

    lv_subject_add_observer(state.get_network_status_subject(),
                           observer_cb, &callback_count);

    // LVGL auto-notifies observers when first added (fires immediately with current value)
    // Note: init_subjects() initializes network_status to CONNECTED (2) as mock mode default
    REQUIRE(callback_count == 1); // Callback fired immediately with initial value

    // Change network status to a DIFFERENT value - should trigger observer again
    state.set_network_status(static_cast<int>(NetworkStatus::DISCONNECTED));

    REQUIRE(callback_count == 2); // Callback fired again with new value
    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::DISCONNECTED));
}

TEST_CASE("PrinterState: Multiple observers on same subject all fire", "[printer_state][observer]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Allow re-initialization after lv_init()
    state.init_subjects(false);  // Skip XML registration

    int count1 = 0, count2 = 0, count3 = 0;

    auto observer_cb = [](lv_observer_t* observer, lv_subject_t*) {
        int* count_ptr = static_cast<int*>(lv_observer_get_user_data(observer));
        (*count_ptr)++;
    };

    // Register three observers on printer connection state
    lv_subject_add_observer(state.get_printer_connection_state_subject(), observer_cb, &count1);
    lv_subject_add_observer(state.get_printer_connection_state_subject(), observer_cb, &count2);
    lv_subject_add_observer(state.get_printer_connection_state_subject(), observer_cb, &count3);

    // LVGL auto-notifies observers when first added (each fires immediately with current value)
    REQUIRE(count1 == 1); // First observer fired immediately
    REQUIRE(count2 == 1); // Second observer fired immediately
    REQUIRE(count3 == 1); // Third observer fired immediately

    // Single state change should fire all three again
    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Connected");

    REQUIRE(count1 == 2); // All observers fired again
    REQUIRE(count2 == 2);
    REQUIRE(count3 == 2);
}

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_CASE("PrinterState: Initialization sets default values", "[printer_state][init]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Reset singleton state from previous tests
    state.init_subjects();

    // Temperature subjects should be initialized to 0
    REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_extruder_target_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_bed_temp_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_bed_target_subject()) == 0);

    // Print progress should be 0
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);

    // Print state should be "standby"
    const char* print_state = lv_subject_get_string(state.get_print_state_subject());
    REQUIRE(std::string(print_state) == "standby");

    // Position should be 0
    REQUIRE(lv_subject_get_int(state.get_position_x_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_position_y_subject()) == 0);
    REQUIRE(lv_subject_get_int(state.get_position_z_subject()) == 0);

    // Speed/flow factors should be 100%
    REQUIRE(lv_subject_get_int(state.get_speed_factor_subject()) == 100);
    REQUIRE(lv_subject_get_int(state.get_flow_factor_subject()) == 100);

    // Fan speed should be 0
    REQUIRE(lv_subject_get_int(state.get_fan_speed_subject()) == 0);

    // Printer connection state should be DISCONNECTED
    REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::DISCONNECTED));

    // Network status is initialized to CONNECTED (mock mode default)
    // In production, actual network status comes from EthernetManager/WiFiManager
    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTED));
}

// ============================================================================
// Temperature Update Tests
// ============================================================================

TEST_CASE("PrinterState: Update extruder temperature from notification", "[printer_state][temp]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"extruder", {
                    {"temperature", 205.3},
                    {"target", 210.0}
                }}
            },
            1234567890.0 // eventtime
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 205);
    REQUIRE(lv_subject_get_int(state.get_extruder_target_subject()) == 210);
}

TEST_CASE("PrinterState: Update bed temperature from notification", "[printer_state][temp]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"heater_bed", {
                    {"temperature", 60.5},
                    {"target", 60.0}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_bed_temp_subject()) == 60);
    REQUIRE(lv_subject_get_int(state.get_bed_target_subject()) == 60);
}

TEST_CASE("PrinterState: Temperature rounding edge cases", "[printer_state][temp][edge]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    SECTION("Round down: 205.4 -> 205") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"extruder", {{"temperature", 205.4}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 205);
    }

    SECTION("Round up: 205.6 -> 205") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"extruder", {{"temperature", 205.6}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 205);
    }

    SECTION("Exact integer: 210.0 -> 210") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"extruder", {{"temperature", 210.0}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 210);
    }
}

// ============================================================================
// Print Progress Tests
// ============================================================================

TEST_CASE("PrinterState: Update print progress from notification", "[printer_state][progress]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"virtual_sdcard", {
                    {"progress", 0.45}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 45);
}

TEST_CASE("PrinterState: Update print state and filename", "[printer_state][progress]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"print_stats", {
                    {"state", "printing"},
                    {"filename", "benchy.gcode"}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    const char* print_state = lv_subject_get_string(state.get_print_state_subject());
    REQUIRE(std::string(print_state) == "printing");

    const char* filename = lv_subject_get_string(state.get_print_filename_subject());
    REQUIRE(std::string(filename) == "benchy.gcode");
}

TEST_CASE("PrinterState: Progress percentage edge cases", "[printer_state][progress][edge]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    SECTION("0% progress") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"virtual_sdcard", {{"progress", 0.0}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 0);
    }

    SECTION("100% progress") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"virtual_sdcard", {{"progress", 1.0}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 100);
    }

    SECTION("67.3% progress -> 67%") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"virtual_sdcard", {{"progress", 0.673}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 67);
    }
}

// ============================================================================
// Motion/Position Tests
// ============================================================================

TEST_CASE("PrinterState: Update toolhead position", "[printer_state][motion]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"toolhead", {
                    {"position", {125.5, 87.3, 45.2, 1234.5}},
                    {"homed_axes", "xyz"}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_position_x_subject()) == 125);
    REQUIRE(lv_subject_get_int(state.get_position_y_subject()) == 87);
    REQUIRE(lv_subject_get_int(state.get_position_z_subject()) == 45);

    const char* homed = lv_subject_get_string(state.get_homed_axes_subject());
    REQUIRE(std::string(homed) == "xyz");
}

TEST_CASE("PrinterState: Homed axes variations", "[printer_state][motion]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    SECTION("Only X and Y homed") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"toolhead", {{"homed_axes", "xy"}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        const char* homed = lv_subject_get_string(state.get_homed_axes_subject());
        REQUIRE(std::string(homed) == "xy");
    }

    SECTION("No axes homed") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", {{{"toolhead", {{"homed_axes", ""}}}}, 0.0}}
        };
        state.update_from_notification(notification);
        const char* homed = lv_subject_get_string(state.get_homed_axes_subject());
        REQUIRE(std::string(homed) == "");
    }
}

// ============================================================================
// Speed/Flow Factor Tests
// ============================================================================

TEST_CASE("PrinterState: Update speed and flow factors", "[printer_state][speed]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"gcode_move", {
                    {"speed_factor", 1.25},
                    {"extrude_factor", 0.95}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_speed_factor_subject()) == 125);
    REQUIRE(lv_subject_get_int(state.get_flow_factor_subject()) == 95);
}

TEST_CASE("PrinterState: Update fan speed", "[printer_state][fan]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"fan", {
                    {"speed", 0.75}
                }}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    REQUIRE(lv_subject_get_int(state.get_fan_speed_subject()) == 75);
}

// ============================================================================
// Connection State Tests
// ============================================================================

TEST_CASE("PrinterState: Set printer connection state", "[printer_state][connection]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Connected");

    REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::CONNECTED));

    const char* message = lv_subject_get_string(state.get_printer_connection_message_subject());
    REQUIRE(std::string(message) == "Connected");
}

TEST_CASE("PrinterState: Connection state transitions", "[printer_state][connection]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    SECTION("Disconnected -> Connecting") {
        state.set_printer_connection_state(static_cast<int>(ConnectionState::DISCONNECTED), "Disconnected");
        state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTING), "Connecting...");
        REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::CONNECTING));
    }

    SECTION("Connecting -> Connected") {
        state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTING), "Connecting...");
        state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Ready");
        REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::CONNECTED));
    }

    SECTION("Connected -> Reconnecting") {
        state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Ready");
        state.set_printer_connection_state(static_cast<int>(ConnectionState::RECONNECTING), "Reconnecting...");
        REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::RECONNECTING));
    }

    SECTION("Failed connection") {
        state.set_printer_connection_state(static_cast<int>(ConnectionState::FAILED), "Connection failed");
        REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::FAILED));
    }
}

// ============================================================================
// Network Status Tests
// ============================================================================

TEST_CASE("PrinterState: Network status initialization", "[printer_state][network]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Reset singleton state from previous tests
    state.init_subjects();

    // Network status is initialized to CONNECTED (mock mode default)
    // In production, actual network status comes from EthernetManager/WiFiManager
    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTED));
}

TEST_CASE("PrinterState: Set network status updates subject", "[printer_state][network]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.init_subjects();

    state.set_network_status(static_cast<int>(NetworkStatus::CONNECTED));

    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTED));
}

TEST_CASE("PrinterState: Network status enum values", "[printer_state][network]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.init_subjects();

    SECTION("DISCONNECTED") {
        state.set_network_status(static_cast<int>(NetworkStatus::DISCONNECTED));
        REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::DISCONNECTED));
    }

    SECTION("CONNECTING") {
        state.set_network_status(static_cast<int>(NetworkStatus::CONNECTING));
        REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTING));
    }

    SECTION("CONNECTED") {
        state.set_network_status(static_cast<int>(NetworkStatus::CONNECTED));
        REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTED));
    }
}

TEST_CASE("PrinterState: Printer and network status are independent", "[printer_state][integration]") {
    lv_init();

    PrinterState& state = get_printer_state();
    state.init_subjects();

    // Set printer connected but network disconnected
    state.set_printer_connection_state(static_cast<int>(ConnectionState::CONNECTED), "Connected");
    state.set_network_status(static_cast<int>(NetworkStatus::DISCONNECTED));

    REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::CONNECTED));
    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::DISCONNECTED));

    // Set network connected but printer disconnected
    state.set_printer_connection_state(static_cast<int>(ConnectionState::DISCONNECTED), "Disconnected");
    state.set_network_status(static_cast<int>(NetworkStatus::CONNECTED));

    REQUIRE(lv_subject_get_int(state.get_printer_connection_state_subject()) == static_cast<int>(ConnectionState::DISCONNECTED));
    REQUIRE(lv_subject_get_int(state.get_network_status_subject()) == static_cast<int>(NetworkStatus::CONNECTED));
}

// ============================================================================
// Invalid/Malformed Notification Tests
// ============================================================================

TEST_CASE("PrinterState: Ignore invalid notification methods", "[printer_state][error]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Reset singleton state from previous tests
    state.init_subjects();

    json notification = {
        {"method", "some_other_method"},
        {"params", {
            {{"extruder", {{"temperature", 999.9}}}}
        }}
    };

    state.update_from_notification(notification);

    // Temperature should remain at default (0)
    REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 0);
}

TEST_CASE("PrinterState: Handle missing fields gracefully", "[printer_state][error]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.reset_for_testing();  // Reset singleton state from previous tests
    state.init_subjects();

    SECTION("Missing 'method' field") {
        json notification = {
            {"params", {
                {{"extruder", {{"temperature", 999.9}}}}
            }}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 0);
    }

    SECTION("Missing 'params' field") {
        json notification = {
            {"method", "notify_status_update"}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 0);
    }

    SECTION("Empty params array") {
        json notification = {
            {"method", "notify_status_update"},
            {"params", json::array()}
        };
        state.update_from_notification(notification);
        REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 0);
    }
}

// ============================================================================
// Comprehensive State Update Tests
// ============================================================================

TEST_CASE("PrinterState: Complete printing state update", "[printer_state][integration]") {
    lv_init();
    PrinterState& state = get_printer_state();
    state.init_subjects();

    json notification = {
        {"method", "notify_status_update"},
        {"params", {
            {
                {"extruder", {{"temperature", 210.5}, {"target", 210.0}}},
                {"heater_bed", {{"temperature", 60.2}, {"target", 60.0}}},
                {"virtual_sdcard", {{"progress", 0.67}}},
                {"print_stats", {{"state", "printing"}, {"filename", "model.gcode"}}},
                {"toolhead", {{"position", {125.0, 87.0, 45.0, 1234.0}}, {"homed_axes", "xyz"}}},
                {"gcode_move", {{"speed_factor", 1.0}, {"extrude_factor", 1.0}}},
                {"fan", {{"speed", 0.5}}}
            },
            1234567890.0
        }}
    };

    state.update_from_notification(notification);

    // Verify all values updated correctly
    REQUIRE(lv_subject_get_int(state.get_extruder_temp_subject()) == 210);
    REQUIRE(lv_subject_get_int(state.get_extruder_target_subject()) == 210);
    REQUIRE(lv_subject_get_int(state.get_bed_temp_subject()) == 60);
    REQUIRE(lv_subject_get_int(state.get_bed_target_subject()) == 60);
    REQUIRE(lv_subject_get_int(state.get_print_progress_subject()) == 67);
    REQUIRE(std::string(lv_subject_get_string(state.get_print_state_subject())) == "printing");
    REQUIRE(std::string(lv_subject_get_string(state.get_print_filename_subject())) == "model.gcode");
    REQUIRE(lv_subject_get_int(state.get_position_x_subject()) == 125);
    REQUIRE(lv_subject_get_int(state.get_position_y_subject()) == 87);
    REQUIRE(lv_subject_get_int(state.get_position_z_subject()) == 45);
    REQUIRE(std::string(lv_subject_get_string(state.get_homed_axes_subject())) == "xyz");
    REQUIRE(lv_subject_get_int(state.get_speed_factor_subject()) == 100);
    REQUIRE(lv_subject_get_int(state.get_flow_factor_subject()) == 100);
    REQUIRE(lv_subject_get_int(state.get_fan_speed_subject()) == 50);
}
