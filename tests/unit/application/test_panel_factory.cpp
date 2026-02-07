// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_panel_factory.cpp
 * @brief Unit tests for PanelFactory class
 *
 * Tests panel discovery, setup, and overlay creation.
 * Full tests require LVGL and XML components - marked as .integration.
 */

#include "ui_nav.h" // For UI_PANEL_COUNT

#include "panel_factory.h"

#include <cstring>

#include "../../catch_amalgamated.hpp"

// ============================================================================
// PanelFactory Constants Tests
// ============================================================================

TEST_CASE("PanelFactory has correct panel count", "[application][panels]") {
    REQUIRE(UI_PANEL_COUNT == 6);
}

TEST_CASE("Panel enum values are sequential", "[application][panels]") {
    REQUIRE(UI_PANEL_HOME == 0);
    REQUIRE(UI_PANEL_PRINT_SELECT == 1);
    REQUIRE(UI_PANEL_CONTROLS == 2);
    REQUIRE(UI_PANEL_FILAMENT == 3);
    REQUIRE(UI_PANEL_SETTINGS == 4);
    REQUIRE(UI_PANEL_ADVANCED == 5);
}

TEST_CASE("PanelFactory PANEL_NAMES has correct entries", "[application][panels]") {
    // Verify the panel names array matches expected values
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_HOME], "home_panel") == 0);
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_PRINT_SELECT], "print_select_panel") ==
            0);
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_CONTROLS], "controls_panel") == 0);
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_FILAMENT], "filament_panel") == 0);
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_SETTINGS], "settings_panel") == 0);
    REQUIRE(std::strcmp(PanelFactory::PANEL_NAMES[UI_PANEL_ADVANCED], "advanced_panel") == 0);
}

TEST_CASE("PanelFactory PANEL_NAMES count matches UI_PANEL_COUNT", "[application][panels]") {
    // Verify the array has the right number of entries
    size_t count = sizeof(PanelFactory::PANEL_NAMES) / sizeof(PanelFactory::PANEL_NAMES[0]);
    REQUIRE(count == UI_PANEL_COUNT);
}

TEST_CASE("PanelFactory starts with null panels", "[application][panels]") {
    PanelFactory factory;

    // All panel pointers should be null initially
    for (int i = 0; i < UI_PANEL_COUNT; i++) {
        REQUIRE(factory.panels()[i] == nullptr);
    }

    // Print status overlay should be null
    REQUIRE(factory.print_status_panel() == nullptr);
}
