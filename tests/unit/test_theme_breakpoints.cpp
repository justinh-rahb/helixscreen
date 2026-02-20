// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_theme_breakpoints.cpp
 * @brief Unit tests for breakpoint suffix selection and responsive token fallback
 *
 * Tests the 5-tier breakpoint system: TINY (≤390), SMALL (391-460),
 * MEDIUM (461-550), LARGE (551-700), XLARGE (>700) and the _tiny/_xlarge
 * fallback behavior.
 */

#include "theme_manager.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// Breakpoint suffix selection
// ============================================================================

TEST_CASE("Breakpoint suffix returns _tiny for heights ≤390", "[theme][breakpoints]") {
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(320)) == "_tiny");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(390)) == "_tiny");
}

TEST_CASE("Breakpoint suffix returns _small for heights 391-460", "[theme][breakpoints]") {
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(391)) == "_small");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(400)) == "_small");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(460)) == "_small");
}

TEST_CASE("Breakpoint suffix returns _medium for heights 461-550", "[theme][breakpoints]") {
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(461)) == "_medium");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(480)) == "_medium");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(550)) == "_medium");
}

TEST_CASE("Breakpoint suffix returns _large for heights 551-700", "[theme][breakpoints]") {
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(551)) == "_large");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(600)) == "_large");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(700)) == "_large");
}

TEST_CASE("Breakpoint suffix returns _xlarge for heights >700", "[theme][breakpoints]") {
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(701)) == "_xlarge");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(720)) == "_xlarge");
    REQUIRE(std::string(theme_manager_get_breakpoint_suffix(1080)) == "_xlarge");
}

TEST_CASE("Breakpoint constants have correct values", "[theme][breakpoints]") {
    REQUIRE(UI_BREAKPOINT_TINY_MAX == 390);
    REQUIRE(UI_BREAKPOINT_SMALL_MAX == 460);
    REQUIRE(UI_BREAKPOINT_MEDIUM_MAX == 550);
    REQUIRE(UI_BREAKPOINT_LARGE_MAX == 700);
}

// ============================================================================
// Responsive token fallback behavior (XML-based, uses test fixtures)
// ============================================================================

TEST_CASE("Responsive token discovery includes _tiny suffix", "[theme][breakpoints]") {
    // Verify that _tiny tokens are discoverable from XML
    auto tiny_tokens = theme_manager_parse_all_xml_for_suffix("ui_xml", "px", "_tiny");

    // fan_card_base_width_tiny and fan_card_height_tiny defined in fan_dial.xml
    REQUIRE(tiny_tokens.count("fan_card_base_width") > 0);
    REQUIRE(tiny_tokens.count("fan_card_height") > 0);
}

TEST_CASE("Tokens without _tiny variant still have _small available", "[theme][breakpoints]") {
    // space_2xl has _small/_medium/_large but no _tiny — verify _small exists for fallback
    auto small_tokens = theme_manager_parse_all_xml_for_suffix("ui_xml", "px", "_small");
    REQUIRE(small_tokens.count("space_2xl") > 0);

    auto tiny_tokens = theme_manager_parse_all_xml_for_suffix("ui_xml", "px", "_tiny");
    REQUIRE(tiny_tokens.count("space_2xl") == 0);
}

TEST_CASE("Validation does not require _tiny for complete sets", "[theme][breakpoints]") {
    // _tiny is optional — validation should not warn about missing _tiny
    auto warnings = theme_manager_validate_constant_sets("ui_xml");

    for (const auto& warning : warnings) {
        // No warning should complain about missing _tiny
        REQUIRE(warning.find("_tiny") == std::string::npos);
    }
}

TEST_CASE("Validation does not require _xlarge for complete sets", "[theme][breakpoints]") {
    // _xlarge is optional — validation should not warn about missing _xlarge
    auto warnings = theme_manager_validate_constant_sets("ui_xml");

    for (const auto& warning : warnings) {
        // No warning should complain about missing _xlarge
        REQUIRE(warning.find("_xlarge") == std::string::npos);
    }
}
