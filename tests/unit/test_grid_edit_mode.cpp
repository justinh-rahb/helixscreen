// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"
#include "grid_layout.h"
#include "panel_widget_config.h"
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

// =============================================================================
// build_default_grid — anchor positions and auto-place defaults
// =============================================================================

TEST_CASE("build_default_grid only sets positions for anchor widgets", "[grid]") {
    auto entries = PanelWidgetConfig::build_default_grid();
    REQUIRE(entries.size() > 3); // At least the 3 anchors + some auto-place widgets

    // Find anchor entries and verify their fixed positions
    const PanelWidgetEntry* printer_image = nullptr;
    const PanelWidgetEntry* print_status = nullptr;
    const PanelWidgetEntry* tips = nullptr;

    for (const auto& e : entries) {
        if (e.id == "printer_image")
            printer_image = &e;
        if (e.id == "print_status")
            print_status = &e;
        if (e.id == "tips")
            tips = &e;
    }

    REQUIRE(printer_image != nullptr);
    CHECK(printer_image->col == 0);
    CHECK(printer_image->row == 0);
    CHECK(printer_image->colspan == 2);
    CHECK(printer_image->rowspan == 2);
    CHECK(printer_image->has_grid_position());

    REQUIRE(print_status != nullptr);
    CHECK(print_status->col == 0);
    CHECK(print_status->row == 2);
    CHECK(print_status->colspan == 2);
    CHECK(print_status->rowspan == 2);
    CHECK(print_status->has_grid_position());

    REQUIRE(tips != nullptr);
    CHECK(tips->col == 2);
    CHECK(tips->row == 0);
    CHECK(tips->colspan == 4);
    CHECK(tips->rowspan == 1);
    CHECK(tips->has_grid_position());

    // All non-anchor entries must have col=-1, row=-1 (auto-place)
    for (const auto& e : entries) {
        if (e.id == "printer_image" || e.id == "print_status" || e.id == "tips") {
            continue;
        }
        INFO("Widget '" << e.id << "' should be auto-place (col=-1, row=-1)");
        CHECK(e.col == -1);
        CHECK(e.row == -1);
        CHECK_FALSE(e.has_grid_position());
    }
}

// =============================================================================
// GridLayout bottom-right packing — free cell ordering
// =============================================================================

TEST_CASE("GridLayout bottom-right packing fills cells correctly", "[grid]") {
    // Breakpoint 2 = MEDIUM = 6x4 grid
    GridLayout grid(2);
    REQUIRE(grid.cols() == 6);
    REQUIRE(grid.rows() == 4);

    // Place the 3 anchor widgets
    REQUIRE(grid.place({"printer_image", 0, 0, 2, 2}));
    REQUIRE(grid.place({"print_status", 0, 2, 2, 2}));
    REQUIRE(grid.place({"tips", 2, 0, 4, 1}));

    // Collect free cells scanning bottom-right to top-left (same as populate_widgets)
    int grid_cols = grid.cols();
    int grid_rows = grid.rows();

    std::vector<std::pair<int, int>> free_cells;
    for (int r = grid_rows - 1; r >= 0; --r) {
        for (int c = grid_cols - 1; c >= 0; --c) {
            if (!grid.is_occupied(c, r)) {
                free_cells.push_back({c, r});
            }
        }
    }

    // Expected free cells in bottom-right to top-left order:
    // Row 3: (5,3), (4,3), (3,3), (2,3)  — cols 0-1 occupied by print_status
    // Row 2: (5,2), (4,2), (3,2), (2,2)  — cols 0-1 occupied by print_status
    // Row 1: (5,1), (4,1), (3,1), (2,1)  — cols 0-1 occupied by printer_image, cols 2-5 free
    // Row 0: all occupied (printer_image 0-1, tips 2-5)
    REQUIRE(free_cells.size() == 12);

    CHECK(free_cells[0] == std::make_pair(5, 3));
    CHECK(free_cells[1] == std::make_pair(4, 3));
    CHECK(free_cells[2] == std::make_pair(3, 3));
    CHECK(free_cells[3] == std::make_pair(2, 3));
    CHECK(free_cells[4] == std::make_pair(5, 2));
    CHECK(free_cells[5] == std::make_pair(4, 2));
    CHECK(free_cells[6] == std::make_pair(3, 2));
    CHECK(free_cells[7] == std::make_pair(2, 2));
    CHECK(free_cells[8] == std::make_pair(5, 1));
    CHECK(free_cells[9] == std::make_pair(4, 1));
    CHECK(free_cells[10] == std::make_pair(3, 1));
    CHECK(free_cells[11] == std::make_pair(2, 1));

    // With 4 auto-place widgets, the mapping is:
    //   widget i of n_auto → cell (n_auto - 1 - i)
    // So: widget 0 → cell 3 = (2,3)
    //     widget 1 → cell 2 = (3,3)
    //     widget 2 → cell 1 = (4,3)
    //     widget 3 → cell 0 = (5,3)
    // Result: left-to-right fill in the bottom row
    size_t n_auto = 4;
    std::vector<std::pair<int, int>> assigned;
    for (size_t i = 0; i < n_auto; ++i) {
        size_t cell_idx = n_auto - 1 - i;
        REQUIRE(cell_idx < free_cells.size());
        assigned.push_back(free_cells[cell_idx]);
    }

    CHECK(assigned[0] == std::make_pair(2, 3));
    CHECK(assigned[1] == std::make_pair(3, 3));
    CHECK(assigned[2] == std::make_pair(4, 3));
    CHECK(assigned[3] == std::make_pair(5, 3));
}

// =============================================================================
// Auto-place entries get positions written back after placement
// =============================================================================

TEST_CASE("auto-place entries get positions written back after placement", "[grid]") {
    // Simulate the populate_widgets writeback logic without LVGL.
    // Build entries: 3 anchors with positions + 4 auto-place widgets.
    std::vector<PanelWidgetEntry> entries = {
        {"printer_image", true, {}, 0, 0, 2, 2}, {"print_status", true, {}, 0, 2, 2, 2},
        {"tips", true, {}, 2, 0, 4, 1},          {"widget_a", true, {}, -1, -1, 1, 1},
        {"widget_b", true, {}, -1, -1, 1, 1},    {"widget_c", true, {}, -1, -1, 1, 1},
        {"widget_d", true, {}, -1, -1, 1, 1},
    };

    // Verify auto-place entries start without positions
    for (size_t i = 3; i < entries.size(); ++i) {
        REQUIRE_FALSE(entries[i].has_grid_position());
    }

    // Replicate the two-pass placement from populate_widgets
    int breakpoint = 2; // MEDIUM = 6x4
    GridLayout grid(breakpoint);

    struct PlacedSlot {
        size_t entry_index;
        int col, row, colspan, rowspan;
    };
    std::vector<PlacedSlot> placed;
    std::vector<size_t> auto_place_indices;

    // First pass: place entries with explicit positions
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].has_grid_position()) {
            bool ok = grid.place({entries[i].id, entries[i].col, entries[i].row, entries[i].colspan,
                                  entries[i].rowspan});
            REQUIRE(ok);
            placed.push_back(
                {i, entries[i].col, entries[i].row, entries[i].colspan, entries[i].rowspan});
        } else {
            auto_place_indices.push_back(i);
        }
    }

    REQUIRE(placed.size() == 3);
    REQUIRE(auto_place_indices.size() == 4);

    // Second pass: bottom-right packing for 1x1 auto-place widgets
    int grid_cols = grid.cols();
    int grid_rows = grid.rows();

    std::vector<std::pair<int, int>> free_cells;
    for (int r = grid_rows - 1; r >= 0; --r) {
        for (int c = grid_cols - 1; c >= 0; --c) {
            if (!grid.is_occupied(c, r)) {
                free_cells.push_back({c, r});
            }
        }
    }

    size_t n_auto = auto_place_indices.size();
    for (size_t i = 0; i < n_auto; ++i) {
        size_t entry_idx = auto_place_indices[i];
        int colspan = entries[entry_idx].colspan;
        int rowspan = entries[entry_idx].rowspan;

        if (colspan == 1 && rowspan == 1) {
            size_t cell_idx = n_auto - 1 - i;
            if (cell_idx < free_cells.size()) {
                auto [col, row] = free_cells[cell_idx];
                if (grid.place({entries[entry_idx].id, col, row, 1, 1})) {
                    placed.push_back({entry_idx, col, row, 1, 1});
                    continue;
                }
            }
        }

        // Fallback
        auto pos = grid.find_available(colspan, rowspan);
        REQUIRE(pos.has_value());
        REQUIRE(grid.place({entries[entry_idx].id, pos->first, pos->second, colspan, rowspan}));
        placed.push_back({entry_idx, pos->first, pos->second, colspan, rowspan});
    }

    REQUIRE(placed.size() == 7); // All 7 widgets placed

    // Write computed positions back to entries (same as populate_widgets)
    for (const auto& p : placed) {
        entries[p.entry_index].col = p.col;
        entries[p.entry_index].row = p.row;
        entries[p.entry_index].colspan = p.colspan;
        entries[p.entry_index].rowspan = p.rowspan;
    }

    // Verify: all entries now have valid grid positions
    for (const auto& e : entries) {
        INFO("Widget '" << e.id << "' should have valid position after writeback");
        CHECK(e.has_grid_position());
        CHECK(e.col >= 0);
        CHECK(e.row >= 0);
        CHECK(e.colspan >= 1);
        CHECK(e.rowspan >= 1);
    }

    // Verify anchors kept their original positions
    CHECK(entries[0].col == 0); // printer_image
    CHECK(entries[0].row == 0);
    CHECK(entries[1].col == 0); // print_status
    CHECK(entries[1].row == 2);
    CHECK(entries[2].col == 2); // tips
    CHECK(entries[2].row == 0);

    // Verify auto-placed widgets landed in the bottom row (row 3) left-to-right
    CHECK(entries[3].col == 2); // widget_a
    CHECK(entries[3].row == 3);
    CHECK(entries[4].col == 3); // widget_b
    CHECK(entries[4].row == 3);
    CHECK(entries[5].col == 4); // widget_c
    CHECK(entries[5].row == 3);
    CHECK(entries[6].col == 5); // widget_d
    CHECK(entries[6].row == 3);

    // Verify no two widgets occupy the same cell
    for (size_t i = 0; i < entries.size(); ++i) {
        for (size_t j = i + 1; j < entries.size(); ++j) {
            // Check that bounding boxes don't overlap
            bool overlap = entries[i].col < entries[j].col + entries[j].colspan &&
                           entries[j].col < entries[i].col + entries[i].colspan &&
                           entries[i].row < entries[j].row + entries[j].rowspan &&
                           entries[j].row < entries[i].row + entries[i].rowspan;
            INFO("Widgets '" << entries[i].id << "' and '" << entries[j].id
                             << "' should not overlap");
            CHECK_FALSE(overlap);
        }
    }
}
