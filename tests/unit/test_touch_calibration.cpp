// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025-2026 356C LLC

#include "touch_calibration.h"

#include "../catch_amalgamated.hpp"

using Catch::Approx;
using namespace helix;

// ============================================================================
// Coefficient Computation Tests
// ============================================================================

TEST_CASE("TouchCalibration: identity transformation", "[touch-calibration][compute]") {
    // When screen points equal touch points, coefficients should give identity
    // identity: a=1, b=0, c=0, d=0, e=1, f=0
    Point screen_points[3] = {{0, 0}, {100, 0}, {0, 100}};
    Point touch_points[3] = {{0, 0}, {100, 0}, {0, 100}};

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);
    REQUIRE(cal.a == Approx(1.0f));
    REQUIRE(cal.b == Approx(0.0f));
    REQUIRE(cal.c == Approx(0.0f));
    REQUIRE(cal.d == Approx(0.0f));
    REQUIRE(cal.e == Approx(1.0f));
    REQUIRE(cal.f == Approx(0.0f));
}

TEST_CASE("TouchCalibration: simple scaling", "[touch-calibration][compute]") {
    // Touch range 0-1000 maps to screen 0-800 x 0-480
    Point screen_points[3] = {{0, 0}, {800, 0}, {0, 480}};
    Point touch_points[3] = {{0, 0}, {1000, 0}, {0, 1000}};

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);

    // Verify transformation produces correct screen coordinates
    Point p1 = transform_point(cal, {1000, 0});
    REQUIRE(p1.x == Approx(800).margin(1));
    REQUIRE(p1.y == Approx(0).margin(1));

    Point p2 = transform_point(cal, {0, 1000});
    REQUIRE(p2.x == Approx(0).margin(1));
    REQUIRE(p2.y == Approx(480).margin(1));

    Point p3 = transform_point(cal, {500, 500});
    REQUIRE(p3.x == Approx(400).margin(1));
    REQUIRE(p3.y == Approx(240).margin(1));
}

TEST_CASE("TouchCalibration: translation offset", "[touch-calibration][compute]") {
    // Touch 0,0 maps to screen 100,100 (offset mapping)
    Point screen_points[3] = {{100, 100}, {700, 100}, {100, 380}};
    Point touch_points[3] = {{0, 0}, {600, 0}, {0, 280}};

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);

    // transform(0, 0) should give (100, 100)
    Point p = transform_point(cal, {0, 0});
    REQUIRE(p.x == Approx(100).margin(1));
    REQUIRE(p.y == Approx(100).margin(1));

    // transform(600, 0) should give (700, 100)
    Point p2 = transform_point(cal, {600, 0});
    REQUIRE(p2.x == Approx(700).margin(1));
    REQUIRE(p2.y == Approx(100).margin(1));

    // transform(0, 280) should give (100, 380)
    Point p3 = transform_point(cal, {0, 280});
    REQUIRE(p3.x == Approx(100).margin(1));
    REQUIRE(p3.y == Approx(380).margin(1));
}

TEST_CASE("TouchCalibration: AD5M-like calibration", "[touch-calibration][compute][ad5m]") {
    // Real-world scenario: 800x480 screen with 15% inset calibration points
    // Screen points at 15% inset from edges
    Point screen_points[3] = {
        {120, 144}, // 15% from left, 30% from top
        {400, 408}, // center-ish X, 85% from top
        {680, 72}   // 85% from left, 15% from top
    };

    // Simulated raw touch values from resistive touchscreen
    Point touch_points[3] = {
        {500, 3200}, // Top-left region
        {2040, 900}, // Bottom-center region
        {3580, 3500} // Top-right region
    };

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);

    // Verify the calibration points transform correctly
    Point p1 = transform_point(cal, {500, 3200});
    REQUIRE(p1.x == Approx(120).margin(2));
    REQUIRE(p1.y == Approx(144).margin(2));

    Point p2 = transform_point(cal, {2040, 900});
    REQUIRE(p2.x == Approx(400).margin(2));
    REQUIRE(p2.y == Approx(408).margin(2));

    Point p3 = transform_point(cal, {3580, 3500});
    REQUIRE(p3.x == Approx(680).margin(2));
    REQUIRE(p3.y == Approx(72).margin(2));
}

TEST_CASE("TouchCalibration: Y-axis inversion", "[touch-calibration][compute]") {
    // Common on resistive touchscreens: raw Y increases but screen Y decreases
    // Screen: origin at top-left, Y increases downward
    // Touch: origin at bottom-left, Y increases upward
    Point screen_points[3] = {{0, 0}, {800, 0}, {0, 480}};
    Point touch_points[3] = {{0, 480}, {800, 480}, {0, 0}}; // Y inverted

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);

    // Touch (0, 480) -> Screen (0, 0)
    Point p1 = transform_point(cal, {0, 480});
    REQUIRE(p1.x == Approx(0).margin(1));
    REQUIRE(p1.y == Approx(0).margin(1));

    // Touch (0, 0) -> Screen (0, 480)
    Point p2 = transform_point(cal, {0, 0});
    REQUIRE(p2.x == Approx(0).margin(1));
    REQUIRE(p2.y == Approx(480).margin(1));

    // Touch (400, 240) -> Screen (400, 240) - center stays center
    Point p3 = transform_point(cal, {400, 240});
    REQUIRE(p3.x == Approx(400).margin(1));
    REQUIRE(p3.y == Approx(240).margin(1));
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("TouchCalibration: degenerate points - collinear", "[touch-calibration][edge]") {
    // All three touch points on a line - cannot compute unique transform
    Point screen_points[3] = {{0, 0}, {100, 100}, {200, 200}};
    Point touch_points[3] = {{0, 0}, {100, 100}, {200, 200}}; // All on diagonal

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == false);
    REQUIRE(cal.valid == false);
}

TEST_CASE("TouchCalibration: degenerate points - duplicates", "[touch-calibration][edge]") {
    // Two identical touch points
    Point screen_points[3] = {{0, 0}, {100, 0}, {0, 100}};
    Point touch_points[3] = {{50, 50}, {50, 50}, {100, 100}}; // First two identical

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == false);
    REQUIRE(cal.valid == false);
}

TEST_CASE("TouchCalibration: degenerate points - nearly collinear", "[touch-calibration][edge]") {
    // Points almost on a line - should detect and fail
    Point screen_points[3] = {{0, 0}, {100, 100}, {200, 201}}; // Third point barely off line
    Point touch_points[3] = {{0, 0}, {100, 100}, {200, 200}};  // Collinear

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == false);
    REQUIRE(cal.valid == false);
}

// ============================================================================
// Point Transformation Tests
// ============================================================================

TEST_CASE("TouchCalibration: transform maintains precision", "[touch-calibration][transform]") {
    // Set up a known scaling transformation
    Point screen_points[3] = {{0, 0}, {100, 0}, {0, 100}};
    Point touch_points[3] = {{0, 0}, {200, 0}, {0, 200}}; // 2x touch range

    TouchCalibration cal;
    REQUIRE(compute_calibration(screen_points, touch_points, cal) == true);

    // Test various points
    SECTION("origin") {
        Point p = transform_point(cal, {0, 0});
        REQUIRE(p.x == Approx(0).margin(1));
        REQUIRE(p.y == Approx(0).margin(1));
    }

    SECTION("max x") {
        Point p = transform_point(cal, {200, 0});
        REQUIRE(p.x == Approx(100).margin(1));
        REQUIRE(p.y == Approx(0).margin(1));
    }

    SECTION("max y") {
        Point p = transform_point(cal, {0, 200});
        REQUIRE(p.x == Approx(0).margin(1));
        REQUIRE(p.y == Approx(100).margin(1));
    }

    SECTION("center") {
        Point p = transform_point(cal, {100, 100});
        REQUIRE(p.x == Approx(50).margin(1));
        REQUIRE(p.y == Approx(50).margin(1));
    }
}

TEST_CASE("TouchCalibration: transform with rotation", "[touch-calibration][transform]") {
    // 90-degree rotation: touch X becomes screen Y, touch Y becomes -screen X
    Point screen_points[3] = {{0, 0}, {0, 100}, {100, 0}}; // Rotated
    Point touch_points[3] = {{0, 0}, {100, 0}, {0, 100}};  // Normal

    TouchCalibration cal;
    bool result = compute_calibration(screen_points, touch_points, cal);

    REQUIRE(result == true);
    REQUIRE(cal.valid == true);

    // Touch (100, 0) -> Screen (0, 100)
    Point p1 = transform_point(cal, {100, 0});
    REQUIRE(p1.x == Approx(0).margin(1));
    REQUIRE(p1.y == Approx(100).margin(1));

    // Touch (0, 100) -> Screen (100, 0)
    Point p2 = transform_point(cal, {0, 100});
    REQUIRE(p2.x == Approx(100).margin(1));
    REQUIRE(p2.y == Approx(0).margin(1));
}

TEST_CASE("TouchCalibration: transform extrapolation beyond calibration points",
          "[touch-calibration][transform]") {
    // Verify transform works for points outside the calibration triangle
    Point screen_points[3] = {{100, 100}, {200, 100}, {100, 200}};
    Point touch_points[3] = {{100, 100}, {200, 100}, {100, 200}}; // Identity at offset

    TouchCalibration cal;
    REQUIRE(compute_calibration(screen_points, touch_points, cal) == true);

    // Point outside calibration triangle
    Point p = transform_point(cal, {300, 300});
    REQUIRE(p.x == Approx(300).margin(1));
    REQUIRE(p.y == Approx(300).margin(1));

    // Point at origin (outside triangle)
    Point p2 = transform_point(cal, {0, 0});
    REQUIRE(p2.x == Approx(0).margin(1));
    REQUIRE(p2.y == Approx(0).margin(1));
}

// ============================================================================
// Coefficient Validation Tests
// ============================================================================

TEST_CASE("TouchCalibration: coefficient values for known transforms",
          "[touch-calibration][coefficients]") {
    SECTION("pure X scaling by 0.8") {
        // screen_x = 0.8 * touch_x + 0 * touch_y + 0
        // screen_y = 0 * touch_x + 1 * touch_y + 0
        Point screen_points[3] = {{0, 0}, {80, 0}, {0, 100}};
        Point touch_points[3] = {{0, 0}, {100, 0}, {0, 100}};

        TouchCalibration cal;
        REQUIRE(compute_calibration(screen_points, touch_points, cal) == true);

        REQUIRE(cal.a == Approx(0.8f).margin(0.001f));
        REQUIRE(cal.b == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.c == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.d == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.e == Approx(1.0f).margin(0.001f));
        REQUIRE(cal.f == Approx(0.0f).margin(0.001f));
    }

    SECTION("pure Y scaling by 0.48") {
        // screen_x = 1 * touch_x + 0 * touch_y + 0
        // screen_y = 0 * touch_x + 0.48 * touch_y + 0
        Point screen_points[3] = {{0, 0}, {100, 0}, {0, 48}};
        Point touch_points[3] = {{0, 0}, {100, 0}, {0, 100}};

        TouchCalibration cal;
        REQUIRE(compute_calibration(screen_points, touch_points, cal) == true);

        REQUIRE(cal.a == Approx(1.0f).margin(0.001f));
        REQUIRE(cal.b == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.c == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.d == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.e == Approx(0.48f).margin(0.001f));
        REQUIRE(cal.f == Approx(0.0f).margin(0.001f));
    }

    SECTION("pure translation") {
        // screen_x = 1 * touch_x + 0 * touch_y + 50
        // screen_y = 0 * touch_x + 1 * touch_y + 30
        Point screen_points[3] = {{50, 30}, {150, 30}, {50, 130}};
        Point touch_points[3] = {{0, 0}, {100, 0}, {0, 100}};

        TouchCalibration cal;
        REQUIRE(compute_calibration(screen_points, touch_points, cal) == true);

        REQUIRE(cal.a == Approx(1.0f).margin(0.001f));
        REQUIRE(cal.b == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.c == Approx(50.0f).margin(0.001f));
        REQUIRE(cal.d == Approx(0.0f).margin(0.001f));
        REQUIRE(cal.e == Approx(1.0f).margin(0.001f));
        REQUIRE(cal.f == Approx(30.0f).margin(0.001f));
    }
}

// ============================================================================
// Invalid Calibration State Tests
// ============================================================================

TEST_CASE("TouchCalibration: default state is invalid", "[touch-calibration][state]") {
    TouchCalibration cal;
    REQUIRE(cal.valid == false);
}

TEST_CASE("TouchCalibration: transform with invalid calibration", "[touch-calibration][state]") {
    TouchCalibration cal;
    cal.valid = false;

    // Transformation with invalid calibration should return input unchanged
    // (or some sensible default behavior)
    Point raw = {500, 300};
    Point result = transform_point(cal, raw);

    // Expected behavior: return input unchanged when calibration is invalid
    REQUIRE(result.x == raw.x);
    REQUIRE(result.y == raw.y);
}
