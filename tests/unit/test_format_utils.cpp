// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "format_utils.h"

#include <cstring>
#include <string>

#include "../catch_amalgamated.hpp"

using namespace helix::fmt;

// =============================================================================
// UNAVAILABLE constant
// =============================================================================

TEST_CASE("UNAVAILABLE constant is em dash", "[format_utils]") {
    CHECK(std::string(UNAVAILABLE) == "—");
}

// =============================================================================
// Percentage formatting
// =============================================================================

TEST_CASE("format_percent basic cases", "[format_utils][percent]") {
    char buf[16];

    SECTION("formats integer percentages") {
        CHECK(std::string(format_percent(0, buf, sizeof(buf))) == "0%");
        CHECK(std::string(format_percent(45, buf, sizeof(buf))) == "45%");
        CHECK(std::string(format_percent(100, buf, sizeof(buf))) == "100%");
    }

    SECTION("handles boundary values") {
        CHECK(std::string(format_percent(-5, buf, sizeof(buf))) == "-5%");
        CHECK(std::string(format_percent(255, buf, sizeof(buf))) == "255%");
    }
}

TEST_CASE("format_percent_or_unavailable", "[format_utils][percent]") {
    char buf[16];

    SECTION("returns formatted percent when available") {
        CHECK(std::string(format_percent_or_unavailable(50, true, buf, sizeof(buf))) == "50%");
    }

    SECTION("returns UNAVAILABLE when not available") {
        CHECK(std::string(format_percent_or_unavailable(50, false, buf, sizeof(buf))) == "—");
    }
}

TEST_CASE("format_percent_float with decimals", "[format_utils][percent]") {
    char buf[16];

    SECTION("formats with 0 decimals") {
        CHECK(std::string(format_percent_float(45.7, 0, buf, sizeof(buf))) == "46%");
        CHECK(std::string(format_percent_float(100.0, 0, buf, sizeof(buf))) == "100%");
    }

    SECTION("formats with 1 decimal") {
        CHECK(std::string(format_percent_float(45.5, 1, buf, sizeof(buf))) == "45.5%");
        CHECK(std::string(format_percent_float(99.9, 1, buf, sizeof(buf))) == "99.9%");
    }

    SECTION("formats with 2 decimals") {
        CHECK(std::string(format_percent_float(45.55, 2, buf, sizeof(buf))) == "45.55%");
    }
}

TEST_CASE("format_humidity from x10 value", "[format_utils][percent]") {
    char buf[16];

    SECTION("converts x10 values to whole percent") {
        CHECK(std::string(format_humidity(455, buf, sizeof(buf))) == "45%");
        CHECK(std::string(format_humidity(1000, buf, sizeof(buf))) == "100%");
        CHECK(std::string(format_humidity(0, buf, sizeof(buf))) == "0%");
    }

    SECTION("rounds correctly") {
        CHECK(std::string(format_humidity(456, buf, sizeof(buf))) == "45%");
        CHECK(std::string(format_humidity(459, buf, sizeof(buf))) == "45%");
    }
}

// =============================================================================
// Distance formatting
// =============================================================================

TEST_CASE("format_distance_mm with precision", "[format_utils][distance]") {
    char buf[32];

    SECTION("formats with specified precision") {
        CHECK(std::string(format_distance_mm(1.234, 2, buf, sizeof(buf))) == "1.23 mm");
        CHECK(std::string(format_distance_mm(0.1, 3, buf, sizeof(buf))) == "0.100 mm");
        CHECK(std::string(format_distance_mm(10.0, 0, buf, sizeof(buf))) == "10 mm");
    }

    SECTION("handles negative values") {
        CHECK(std::string(format_distance_mm(-0.5, 2, buf, sizeof(buf))) == "-0.50 mm");
    }
}

TEST_CASE("format_diameter_mm fixed 2 decimals", "[format_utils][distance]") {
    char buf[32];

    CHECK(std::string(format_diameter_mm(1.75f, buf, sizeof(buf))) == "1.75 mm");
    CHECK(std::string(format_diameter_mm(2.85f, buf, sizeof(buf))) == "2.85 mm");
    CHECK(std::string(format_diameter_mm(1.0f, buf, sizeof(buf))) == "1.00 mm");
}

// =============================================================================
// Speed formatting
// =============================================================================

TEST_CASE("format_speed_mm_s", "[format_utils][speed]") {
    char buf[32];

    CHECK(std::string(format_speed_mm_s(150.0, buf, sizeof(buf))) == "150 mm/s");
    CHECK(std::string(format_speed_mm_s(0.0, buf, sizeof(buf))) == "0 mm/s");
    CHECK(std::string(format_speed_mm_s(300.5, buf, sizeof(buf))) == "300 mm/s");
}

TEST_CASE("format_speed_mm_min", "[format_utils][speed]") {
    char buf[32];

    CHECK(std::string(format_speed_mm_min(300.0, buf, sizeof(buf))) == "300 mm/min");
    CHECK(std::string(format_speed_mm_min(0.0, buf, sizeof(buf))) == "0 mm/min");
}

// =============================================================================
// Acceleration formatting
// =============================================================================

TEST_CASE("format_accel_mm_s2", "[format_utils][accel]") {
    char buf[32];

    CHECK(std::string(format_accel_mm_s2(3000.0, buf, sizeof(buf))) == "3000 mm/s²");
    CHECK(std::string(format_accel_mm_s2(500.0, buf, sizeof(buf))) == "500 mm/s²");
    CHECK(std::string(format_accel_mm_s2(0.0, buf, sizeof(buf))) == "0 mm/s²");
}

// =============================================================================
// Frequency formatting
// =============================================================================

TEST_CASE("format_frequency_hz", "[format_utils][frequency]") {
    char buf[32];

    CHECK(std::string(format_frequency_hz(48.5, buf, sizeof(buf))) == "48.5 Hz");
    CHECK(std::string(format_frequency_hz(60.0, buf, sizeof(buf))) == "60.0 Hz");
    CHECK(std::string(format_frequency_hz(0.0, buf, sizeof(buf))) == "0.0 Hz");
}

// =============================================================================
// Buffer safety
// =============================================================================

TEST_CASE("formatters handle small buffers safely", "[format_utils][safety]") {
    char tiny[4];

    SECTION("percent truncates safely") {
        format_percent(100, tiny, sizeof(tiny));
        CHECK(tiny[sizeof(tiny) - 1] == '\0');
    }

    SECTION("distance truncates safely") {
        format_distance_mm(123.456, 2, tiny, sizeof(tiny));
        CHECK(tiny[sizeof(tiny) - 1] == '\0');
    }
}

// ============================================================================
// heater_display() tests
// ============================================================================

TEST_CASE("heater_display() cold heater shows temperature only", "[format_utils][heater_display]") {
    // 2500 centi-degrees = 25.00°C, target 0 = off
    auto result = heater_display(2500, 0);
    REQUIRE(result.temp == "25°C");
    REQUIRE(result.status == "Off");
    REQUIRE(result.pct == 0);
}

TEST_CASE("heater_display() heating shows current/target and percentage",
          "[format_utils][heater_display]") {
    // 15000 centi = 150°C, target 20000 centi = 200°C -> 75%
    auto result = heater_display(15000, 20000);
    REQUIRE(result.temp == "150 / 200°C");
    REQUIRE(result.status == "Heating...");
    REQUIRE(result.pct == 75);
}

TEST_CASE("heater_display() at temperature shows Ready", "[format_utils][heater_display]") {
    // 19800 centi = 198°C / 20000 centi = 200°C target -> 99%
    auto result = heater_display(19800, 20000);
    REQUIRE(result.temp == "198 / 200°C");
    REQUIRE(result.status == "Ready");
    REQUIRE(result.pct == 99);
}

TEST_CASE("heater_display() percentage clamps to 0-100", "[format_utils][heater_display]") {
    SECTION("over target clamps to 100") {
        // 21000 centi = 210°C with 200°C target -> should clamp to 100%
        auto result = heater_display(21000, 20000);
        REQUIRE(result.pct == 100);
        REQUIRE(result.status == "Ready");
    }

    SECTION("negative temperature clamps to 0") {
        // Edge case: negative temp (shouldn't happen but be safe)
        auto result = heater_display(-100, 20000);
        REQUIRE(result.pct == 0);
    }
}

TEST_CASE("heater_display() edge cases", "[format_utils][heater_display]") {
    SECTION("exactly at 98% threshold shows Ready") {
        // 19600 / 20000 = 98%
        auto result = heater_display(19600, 20000);
        REQUIRE(result.pct == 98);
        REQUIRE(result.status == "Ready");
    }

    SECTION("just below 98% threshold shows Heating") {
        // 19400 / 20000 = 97%
        auto result = heater_display(19400, 20000);
        REQUIRE(result.pct == 97);
        REQUIRE(result.status == "Heating...");
    }

    SECTION("very low target temperature") {
        // 4000 centi = 40°C with 50°C target
        auto result = heater_display(4000, 5000);
        REQUIRE(result.temp == "40 / 50°C");
        REQUIRE(result.pct == 80);
    }

    SECTION("zero current temperature") {
        auto result = heater_display(0, 20000);
        REQUIRE(result.temp == "0 / 200°C");
        REQUIRE(result.pct == 0);
        REQUIRE(result.status == "Heating...");
    }
}
