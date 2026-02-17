// SPDX-License-Identifier: GPL-3.0-or-later

#include "../lvgl_test_fixture.h"
#include "ams_types.h"
#include "theme_manager.h"
#include "ui/ams_drawing_utils.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// SlotInfo::is_present tests
// ============================================================================

TEST_CASE("SlotInfo::is_present returns false for EMPTY", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::EMPTY;
    REQUIRE_FALSE(slot.is_present());
}

TEST_CASE("SlotInfo::is_present returns false for UNKNOWN", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::UNKNOWN;
    REQUIRE_FALSE(slot.is_present());
}

TEST_CASE("SlotInfo::is_present returns true for AVAILABLE", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::AVAILABLE;
    REQUIRE(slot.is_present());
}

TEST_CASE("SlotInfo::is_present returns true for LOADED", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::LOADED;
    REQUIRE(slot.is_present());
}

TEST_CASE("SlotInfo::is_present returns true for FROM_BUFFER", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::FROM_BUFFER;
    REQUIRE(slot.is_present());
}

TEST_CASE("SlotInfo::is_present returns true for BLOCKED", "[ams_draw][slot_info]") {
    SlotInfo slot;
    slot.status = SlotStatus::BLOCKED;
    REQUIRE(slot.is_present());
}

// ============================================================================
// Color utility tests
// ============================================================================

TEST_CASE("ams_draw::lighten_color adds amount clamped to 255", "[ams_draw][color]") {
    lv_color_t c = lv_color_make(100, 200, 250);
    lv_color_t result = ams_draw::lighten_color(c, 50);
    REQUIRE(result.red == 150);
    REQUIRE(result.green == 250);
    REQUIRE(result.blue == 255);
}

TEST_CASE("ams_draw::darken_color subtracts amount clamped to 0", "[ams_draw][color]") {
    lv_color_t c = lv_color_make(30, 100, 200);
    lv_color_t result = ams_draw::darken_color(c, 50);
    REQUIRE(result.red == 0);
    REQUIRE(result.green == 50);
    REQUIRE(result.blue == 150);
}

TEST_CASE("ams_draw::blend_color interpolates between colors", "[ams_draw][color]") {
    lv_color_t black = lv_color_make(0, 0, 0);
    lv_color_t white = lv_color_make(255, 255, 255);

    lv_color_t at_zero = ams_draw::blend_color(black, white, 0.0f);
    REQUIRE(at_zero.red == 0);

    lv_color_t at_one = ams_draw::blend_color(black, white, 1.0f);
    REQUIRE(at_one.red == 255);

    lv_color_t mid = ams_draw::blend_color(black, white, 0.5f);
    REQUIRE(mid.red >= 126);
    REQUIRE(mid.red <= 128);
}

TEST_CASE("ams_draw::blend_color clamps factor to [0,1]", "[ams_draw][color]") {
    lv_color_t a = lv_color_make(100, 100, 100);
    lv_color_t b = lv_color_make(200, 200, 200);

    lv_color_t below = ams_draw::blend_color(a, b, -1.0f);
    REQUIRE(below.red == 100);

    lv_color_t above = ams_draw::blend_color(a, b, 2.0f);
    REQUIRE(above.red == 200);
}

// ============================================================================
// Severity & Error tests
// ============================================================================

TEST_CASE_METHOD(LVGLTestFixture, "ams_draw::severity_color maps ERROR to danger",
                 "[ams_draw][severity]") {
    lv_color_t result = ams_draw::severity_color(SlotError::ERROR);
    lv_color_t expected = theme_manager_get_color("danger");
    REQUIRE(result.red == expected.red);
    REQUIRE(result.green == expected.green);
    REQUIRE(result.blue == expected.blue);
}

TEST_CASE_METHOD(LVGLTestFixture, "ams_draw::severity_color maps WARNING to warning",
                 "[ams_draw][severity]") {
    lv_color_t result = ams_draw::severity_color(SlotError::WARNING);
    lv_color_t expected = theme_manager_get_color("warning");
    REQUIRE(result.red == expected.red);
}

TEST_CASE_METHOD(LVGLTestFixture, "ams_draw::severity_color maps INFO to text_muted",
                 "[ams_draw][severity]") {
    lv_color_t result = ams_draw::severity_color(SlotError::INFO);
    lv_color_t expected = theme_manager_get_color("text_muted");
    REQUIRE(result.red == expected.red);
}

TEST_CASE("ams_draw::worst_unit_severity returns INFO for no errors", "[ams_draw][severity]") {
    AmsUnit unit;
    unit.slots.resize(4);
    REQUIRE(ams_draw::worst_unit_severity(unit) == SlotError::INFO);
}

TEST_CASE("ams_draw::worst_unit_severity finds ERROR among warnings", "[ams_draw][severity]") {
    AmsUnit unit;
    unit.slots.resize(4);
    unit.slots[1].error = SlotError{"warn", SlotError::WARNING};
    unit.slots[3].error = SlotError{"err", SlotError::ERROR};
    REQUIRE(ams_draw::worst_unit_severity(unit) == SlotError::ERROR);
}

// ============================================================================
// Fill percent tests
// ============================================================================

TEST_CASE("ams_draw::fill_percent_from_slot with known weight", "[ams_draw][fill]") {
    SlotInfo slot;
    slot.remaining_weight_g = 500.0f;
    slot.total_weight_g = 1000.0f;
    REQUIRE(ams_draw::fill_percent_from_slot(slot) == 50);
}

TEST_CASE("ams_draw::fill_percent_from_slot clamps to min_pct", "[ams_draw][fill]") {
    SlotInfo slot;
    slot.remaining_weight_g = 1.0f;
    slot.total_weight_g = 1000.0f;
    REQUIRE(ams_draw::fill_percent_from_slot(slot) == 5);
}

TEST_CASE("ams_draw::fill_percent_from_slot returns 100 for unknown weight", "[ams_draw][fill]") {
    SlotInfo slot;
    slot.remaining_weight_g = -1.0f;
    slot.total_weight_g = 0.0f;
    REQUIRE(ams_draw::fill_percent_from_slot(slot) == 100);
}

TEST_CASE("ams_draw::fill_percent_from_slot custom min_pct", "[ams_draw][fill]") {
    SlotInfo slot;
    slot.remaining_weight_g = 0.0f;
    slot.total_weight_g = 1000.0f;
    REQUIRE(ams_draw::fill_percent_from_slot(slot, 0) == 0);
    REQUIRE(ams_draw::fill_percent_from_slot(slot, 10) == 10);
}

// ============================================================================
// Bar width tests
// ============================================================================

TEST_CASE("ams_draw::calc_bar_width distributes evenly", "[ams_draw][bar_width]") {
    int32_t w = ams_draw::calc_bar_width(100, 4, 2, 6, 14);
    REQUIRE(w == 14);
}

TEST_CASE("ams_draw::calc_bar_width respects min", "[ams_draw][bar_width]") {
    int32_t w = ams_draw::calc_bar_width(20, 16, 2, 6, 14);
    REQUIRE(w == 6);
}

TEST_CASE("ams_draw::calc_bar_width with container_pct", "[ams_draw][bar_width]") {
    int32_t w = ams_draw::calc_bar_width(100, 1, 2, 6, 14, 90);
    REQUIRE(w == 14);
}

TEST_CASE("ams_draw::calc_bar_width handles zero slots", "[ams_draw][bar_width]") {
    int32_t w = ams_draw::calc_bar_width(100, 0, 2, 6, 14);
    REQUIRE(w == 14);
}

// ============================================================================
// Display name tests
// ============================================================================

TEST_CASE("ams_draw::get_unit_display_name uses name when set", "[ams_draw][display_name]") {
    AmsUnit unit;
    unit.name = "Box Turtle 1";
    REQUIRE(ams_draw::get_unit_display_name(unit, 0) == "Box Turtle 1");
}

TEST_CASE("ams_draw::get_unit_display_name falls back to Unit N", "[ams_draw][display_name]") {
    AmsUnit unit;
    REQUIRE(ams_draw::get_unit_display_name(unit, 0) == "Unit 1");
    REQUIRE(ams_draw::get_unit_display_name(unit, 2) == "Unit 3");
}
