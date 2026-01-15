// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025-2026 356C LLC

#pragma once

namespace helix {

struct Point {
    int x = 0;
    int y = 0;
};

struct TouchCalibration {
    bool valid = false;
    float a = 1.0f, b = 0.0f, c = 0.0f; // screen_x = a*x + b*y + c
    float d = 0.0f, e = 1.0f, f = 0.0f; // screen_y = d*x + e*y + f
};

/**
 * @brief Compute affine calibration coefficients from 3 point pairs
 *
 * Uses the Maxim Integrated AN5296 algorithm (determinant-based).
 * Screen points are where targets appear on display.
 * Touch points are raw coordinates from touch controller.
 *
 * @param screen_points 3 screen coordinate targets
 * @param touch_points 3 corresponding raw touch coordinates
 * @param out Output calibration coefficients
 * @return true if successful, false if points are degenerate (collinear)
 */
bool compute_calibration(const Point screen_points[3], const Point touch_points[3],
                         TouchCalibration& out);

/**
 * @brief Transform raw touch point to screen coordinates
 *
 * @param cal Calibration coefficients (must be valid)
 * @param raw Raw touch point from controller
 * @return Transformed screen coordinates (or raw if cal.valid is false)
 */
Point transform_point(const TouchCalibration& cal, Point raw);

} // namespace helix
