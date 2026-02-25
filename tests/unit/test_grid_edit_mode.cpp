// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"
#include "panel_widget_registry.h"

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

// =============================================================================
// clamp_span
// =============================================================================

TEST_CASE("GridEditMode: clamp_span respects min/max from registry", "[grid_edit][resize]") {
    // printer_image: min 1x1, max 4x3 (from registry)
    const auto* def = find_widget_def("printer_image");
    REQUIRE(def != nullptr);
    REQUIRE(def->is_scalable());

    // Over max — clamp down
    auto [c, r] = GridEditMode::clamp_span("printer_image", 5, 4);
    CHECK(c == def->effective_max_colspan());
    CHECK(r == def->effective_max_rowspan());

    // Under min — clamp up
    auto [c2, r2] = GridEditMode::clamp_span("printer_image", 0, 0);
    CHECK(c2 == def->effective_min_colspan());
    CHECK(r2 == def->effective_min_rowspan());

    // Within range — unchanged
    auto [c3, r3] = GridEditMode::clamp_span("printer_image", 2, 2);
    CHECK(c3 == 2);
    CHECK(r3 == 2);
}

TEST_CASE("GridEditMode: clamp_span non-scalable widget stays fixed", "[grid_edit][resize]") {
    // "power" has no min/max overrides, so effective min == max == default (1x1)
    const auto* def = find_widget_def("power");
    REQUIRE(def != nullptr);
    REQUIRE_FALSE(def->is_scalable());

    auto [c, r] = GridEditMode::clamp_span("power", 3, 3);
    CHECK(c == def->effective_min_colspan());
    CHECK(r == def->effective_min_rowspan());
    // Both should equal the default colspan/rowspan (1x1)
    CHECK(c == 1);
    CHECK(r == 1);
}

TEST_CASE("GridEditMode: clamp_span unknown widget returns at least 1x1", "[grid_edit][resize]") {
    auto [c, r] = GridEditMode::clamp_span("nonexistent_widget_xyz", 0, 0);
    CHECK(c >= 1);
    CHECK(r >= 1);
}

TEST_CASE("GridEditMode: clamp_span tips widget respects range", "[grid_edit][resize]") {
    // tips: colspan default=3, min=2, max=6, rowspan default=1, min=1, max=1
    const auto* def = find_widget_def("tips");
    REQUIRE(def != nullptr);
    REQUIRE(def->is_scalable());

    // Max colspan 6, only 1 row allowed
    auto [c, r] = GridEditMode::clamp_span("tips", 10, 5);
    CHECK(c == def->effective_max_colspan());
    CHECK(r == def->effective_max_rowspan());

    // Min colspan 2
    auto [c2, r2] = GridEditMode::clamp_span("tips", 1, 1);
    CHECK(c2 == def->effective_min_colspan());
    CHECK(r2 == 1);
}
