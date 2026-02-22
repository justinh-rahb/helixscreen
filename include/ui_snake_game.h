// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_snake_game.h
 * @brief Snake easter egg - filament tube edition
 *
 * Hidden Snake game triggered by tapping the Printer Name row 7 times in Settings.
 * Snake rendered as 3D filament tube, food as spool boxes.
 * Controls: swipe gestures (touchscreen) + arrow keys (dev/testing).
 *
 * @pattern Static show/hide overlay (like BusyOverlay)
 * @threading Main thread only
 */

#pragma once

#include "lvgl/lvgl.h"

namespace helix {

/**
 * @class SnakeGame
 * @brief Full-screen Snake game overlay rendered with filament tube aesthetics
 *
 * ## Usage:
 * @code
 * SnakeGame::show();  // Launch game
 * SnakeGame::hide();  // Close game
 * @endcode
 */
class SnakeGame {
  public:
    /// Launch the snake game as a full-screen overlay on lv_layer_top()
    static void show();

    /// Close the game and clean up all resources
    static void hide();

    /// Check if game is currently visible
    static bool is_visible();
};

} // namespace helix
