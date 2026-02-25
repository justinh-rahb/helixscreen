// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "../catch_amalgamated.hpp"

using namespace helix;

TEST_CASE("GridEditMode: starts inactive", "[grid_edit][edit_mode]") {
    GridEditMode em;
    REQUIRE_FALSE(em.is_active());
}

TEST_CASE("GridEditMode: enter/exit toggles state", "[grid_edit][edit_mode]") {
    GridEditMode em;
    em.enter(nullptr, nullptr); // null container/config OK for state test
    REQUIRE(em.is_active());
    em.exit();
    REQUIRE_FALSE(em.is_active());
}

TEST_CASE("GridEditMode: exit when not active is no-op", "[grid_edit][edit_mode]") {
    GridEditMode em;
    em.exit(); // Should not crash
    REQUIRE_FALSE(em.is_active());
}

TEST_CASE("GridEditMode: double enter is no-op", "[grid_edit][edit_mode]") {
    GridEditMode em;
    em.enter(nullptr, nullptr);
    em.enter(nullptr, nullptr); // Second enter should be ignored
    REQUIRE(em.is_active());
    em.exit();
    REQUIRE_FALSE(em.is_active());
}

TEST_CASE("GridEditMode: select/deselect widget tracking", "[grid_edit][selection]") {
    GridEditMode em;
    em.enter(nullptr, nullptr);

    REQUIRE(em.selected_widget() == nullptr);

    // Use a reinterpret_cast'd pointer for tracking test (lv_obj_t is opaque)
    int dummy = 0;
    auto* fake = reinterpret_cast<lv_obj_t*>(&dummy);
    em.select_widget(fake);
    REQUIRE(em.selected_widget() == fake);

    em.select_widget(nullptr);
    REQUIRE(em.selected_widget() == nullptr);

    // Selection clears on exit
    em.select_widget(fake);
    em.exit();
    REQUIRE(em.selected_widget() == nullptr);
}

TEST_CASE("GridEditMode: selecting same widget is no-op", "[grid_edit][selection]") {
    GridEditMode em;
    em.enter(nullptr, nullptr);

    int dummy = 0;
    auto* fake = reinterpret_cast<lv_obj_t*>(&dummy);
    em.select_widget(fake);
    REQUIRE(em.selected_widget() == fake);

    // Selecting same widget again should not crash or change state
    em.select_widget(fake);
    REQUIRE(em.selected_widget() == fake);

    em.exit();
}

TEST_CASE("GridEditMode: select_widget when not active is no-op", "[grid_edit][selection]") {
    GridEditMode em;
    int dummy = 0;
    auto* fake = reinterpret_cast<lv_obj_t*>(&dummy);

    em.select_widget(fake);
    REQUIRE(em.selected_widget() == nullptr);
}
