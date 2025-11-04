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
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#include "../catch_amalgamated.hpp"
#include "runtime_config.h"
#include <cstring>
#include <vector>
#include <string>

// Mock global test config for testing
static RuntimeConfig test_config_instance;

// Mock accessor functions
const RuntimeConfig& get_runtime_config() {
    return test_config_instance;
}

RuntimeConfig* get_mutable_runtime_config() {
    return &test_config_instance;
}

// Helper function to simulate command-line parsing
bool parse_test_args(const std::vector<std::string>& args) {
    // Reset config before each test
    test_config_instance = RuntimeConfig{};

    // Parse arguments
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--test") {
            test_config_instance.test_mode = true;
        } else if (args[i] == "--real-wifi") {
            test_config_instance.use_real_wifi = true;
        } else if (args[i] == "--real-ethernet") {
            test_config_instance.use_real_ethernet = true;
        } else if (args[i] == "--real-moonraker") {
            test_config_instance.use_real_moonraker = true;
        } else if (args[i] == "--real-files") {
            test_config_instance.use_real_files = true;
        } else {
            return false;  // Unknown argument
        }
    }

    // Validate: --real-* flags require --test mode
    if ((test_config_instance.use_real_wifi ||
         test_config_instance.use_real_ethernet ||
         test_config_instance.use_real_moonraker ||
         test_config_instance.use_real_files) &&
        !test_config_instance.test_mode) {
        return false;  // Invalid configuration
    }

    return true;
}

TEST_CASE("TestConfig default initialization", "[test_config]") {
    RuntimeConfig config;

    SECTION("All flags are false by default") {
        REQUIRE(config.test_mode == false);
        REQUIRE(config.use_real_wifi == false);
        REQUIRE(config.use_real_ethernet == false);
        REQUIRE(config.use_real_moonraker == false);
        REQUIRE(config.use_real_files == false);
    }

    SECTION("Helper methods return false in production mode") {
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
        REQUIRE(config.is_test_mode() == false);
    }
}

TEST_CASE("TestConfig test mode without real components", "[test_config]") {
    RuntimeConfig config;
    config.test_mode = true;

    SECTION("All components use mocks by default in test mode") {
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
        REQUIRE(config.is_test_mode() == true);
    }
}

TEST_CASE("TestConfig test mode with selective real components", "[test_config]") {
    RuntimeConfig config;
    config.test_mode = true;

    SECTION("Real WiFi overrides mock") {
        config.use_real_wifi = true;
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Real Ethernet overrides mock") {
        config.use_real_ethernet = true;
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Real Moonraker overrides mock") {
        config.use_real_moonraker = true;
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Real files override test data") {
        config.use_real_files = true;
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == false);
    }

    SECTION("Multiple real components") {
        config.use_real_wifi = true;
        config.use_real_moonraker = true;
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("All real components in test mode") {
        config.use_real_wifi = true;
        config.use_real_ethernet = true;
        config.use_real_moonraker = true;
        config.use_real_files = true;
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
    }
}

TEST_CASE("TestConfig production mode ignores real flags", "[test_config]") {
    RuntimeConfig config;
    config.test_mode = false;  // Production mode

    SECTION("Real flags have no effect without test mode") {
        config.use_real_wifi = true;
        config.use_real_ethernet = true;
        config.use_real_moonraker = true;
        config.use_real_files = true;

        // In production, we never use mocks regardless of flags
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
        REQUIRE(config.is_test_mode() == false);
    }
}

TEST_CASE("Command-line argument parsing", "[test_config]") {
    SECTION("No arguments - production mode") {
        REQUIRE(parse_test_args({}) == true);
        const auto& config = get_runtime_config();
        REQUIRE(config.test_mode == false);
        REQUIRE(config.should_mock_wifi() == false);
    }

    SECTION("Test mode only") {
        REQUIRE(parse_test_args({"--test"}) == true);
        const auto& config = get_runtime_config();
        REQUIRE(config.test_mode == true);
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Test mode with real WiFi") {
        REQUIRE(parse_test_args({"--test", "--real-wifi"}) == true);
        const auto& config = get_runtime_config();
        REQUIRE(config.test_mode == true);
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == true);
    }

    SECTION("Test mode with multiple real components") {
        REQUIRE(parse_test_args({"--test", "--real-wifi", "--real-moonraker"}) == true);
        const auto& config = get_runtime_config();
        REQUIRE(config.test_mode == true);
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Real flags without test mode should fail") {
        REQUIRE(parse_test_args({"--real-wifi"}) == false);
        REQUIRE(parse_test_args({"--real-ethernet"}) == false);
        REQUIRE(parse_test_args({"--real-moonraker"}) == false);
        REQUIRE(parse_test_args({"--real-files"}) == false);
    }

    SECTION("Unknown arguments should fail") {
        REQUIRE(parse_test_args({"--unknown"}) == false);
        REQUIRE(parse_test_args({"--test", "--unknown"}) == false);
    }

    SECTION("Order independence") {
        // --test can come after --real-* flags
        REQUIRE(parse_test_args({"--real-wifi", "--test"}) == true);
        const auto& config = get_runtime_config();
        REQUIRE(config.test_mode == true);
        REQUIRE(config.should_mock_wifi() == false);
    }
}

TEST_CASE("TestConfig accessor functions", "[test_config]") {
    SECTION("get_test_config returns const reference") {
        const RuntimeConfig& config = get_runtime_config();
        // Should compile - const reference
        bool is_test = config.is_test_mode();
        REQUIRE(is_test == false);  // Default state
    }

    SECTION("get_mutable_test_config allows modification") {
        RuntimeConfig* config = get_mutable_runtime_config();
        config->test_mode = true;
        config->use_real_wifi = true;

        // Verify changes are visible through const accessor
        const RuntimeConfig& const_config = get_runtime_config();
        REQUIRE(const_config.test_mode == true);
        REQUIRE(const_config.use_real_wifi == true);
        REQUIRE(const_config.should_mock_wifi() == false);
    }
}

TEST_CASE("TestConfig use cases", "[test_config]") {
    SECTION("Development with no hardware") {
        parse_test_args({"--test"});
        const auto& config = get_runtime_config();

        // Everything should be mocked
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("UI development with real printer") {
        parse_test_args({"--test", "--real-moonraker", "--real-files"});
        const auto& config = get_runtime_config();

        // Network mocked, printer real
        REQUIRE(config.should_mock_wifi() == true);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
    }

    SECTION("Network testing without printer") {
        parse_test_args({"--test", "--real-wifi", "--real-ethernet"});
        const auto& config = get_runtime_config();

        // Network real, printer mocked
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == true);
        REQUIRE(config.should_use_test_files() == true);
    }

    SECTION("Integration testing") {
        parse_test_args({"--test", "--real-wifi", "--real-moonraker", "--real-files"});
        const auto& config = get_runtime_config();

        // WiFi and printer real, Ethernet mocked
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == true);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
    }

    SECTION("Production mode") {
        parse_test_args({});  // No arguments
        const auto& config = get_runtime_config();

        // Nothing should be mocked in production
        REQUIRE(config.should_mock_wifi() == false);
        REQUIRE(config.should_mock_ethernet() == false);
        REQUIRE(config.should_mock_moonraker() == false);
        REQUIRE(config.should_use_test_files() == false);
    }
}