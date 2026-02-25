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
