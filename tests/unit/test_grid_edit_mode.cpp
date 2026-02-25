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

// =============================================================================
// screen_to_grid_cell
// =============================================================================

TEST_CASE("GridEditMode: screen_to_grid_cell maps coordinates correctly", "[grid_edit][drag]") {
    // 6-column grid, container at (100, 0) with width 600, height 400, 4 rows
    // Cell size: 100x100
    auto cell = GridEditMode::screen_to_grid_cell(150, 50,  // point inside col 0, row 0
                                                  100, 0,   // container origin
                                                  600, 400, // container size
                                                  6, 4      // cols, rows
    );
    REQUIRE(cell.first == 0);  // col 0
    REQUIRE(cell.second == 0); // row 0

    // Bottom-right corner area: col 5, row 3
    auto cell2 = GridEditMode::screen_to_grid_cell(690, 390, 100, 0, 600, 400, 6, 4);
    REQUIRE(cell2.first == 5);
    REQUIRE(cell2.second == 3);
}

TEST_CASE("GridEditMode: screen_to_grid_cell clamps out-of-bounds coordinates",
          "[grid_edit][drag]") {
    // Point before container origin — should clamp to (0, 0)
    auto cell = GridEditMode::screen_to_grid_cell(50, 10, // before container at (100, 20)
                                                  100, 20, 600, 400, 6, 4);
    CHECK(cell.first == 0);
    CHECK(cell.second == 0);

    // Point beyond container extent — should clamp to (ncols-1, nrows-1)
    auto cell2 =
        GridEditMode::screen_to_grid_cell(800, 500, // beyond container at (100,20) size 600x400
                                          100, 20, 600, 400, 6, 4);
    CHECK(cell2.first == 5);
    CHECK(cell2.second == 3);
}

TEST_CASE("GridEditMode: screen_to_grid_cell center of each cell", "[grid_edit][drag]") {
    // Container at (0,0), 400x300, 4 cols x 3 rows
    // Cell size: 100x100
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            int cx = c * 100 + 50;
            int cy = r * 100 + 50;
            auto cell = GridEditMode::screen_to_grid_cell(cx, cy, 0, 0, 400, 300, 4, 3);
            INFO("Testing center of cell (" << c << "," << r << ") at screen (" << cx << "," << cy
                                            << ")");
            CHECK(cell.first == c);
            CHECK(cell.second == r);
        }
    }
}
