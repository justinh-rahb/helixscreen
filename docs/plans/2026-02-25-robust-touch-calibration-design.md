# Robust Touch Calibration for Resistive Panels

**Date:** 2026-02-25
**Issue:** #198 (Touch problems Nebula Pad), related: #135, #186, #40
**Status:** Approved

## Problem

The touch calibration wizard produces bad affine matrices on resistive panels with noisy ADC output (e.g. ns2009 on Nebula Pad). Raw values saturate at 4095 near screen edges, single-sample capture is vulnerable to noise, and a bad matrix leaves the UI untouchable with no escape except a service restart.

## Approach: Defense in Depth

Three additive layers that improve input quality, validate output, and recover from failures.

## Layer 1: Input Filtering

**Location:** `TouchCalibrationPanel` point capture logic

When the user taps a calibration target:

1. Collect **7 raw samples** over ~300ms (at typical 50-100Hz report rate)
2. **Reject saturated values** — discard samples where X or Y equals ADC max (4095 for 12-bit, 65535 for 16-bit)
3. If fewer than 3 valid samples remain, show toast "Touch was noisy, please tap again" and stay on the same point
4. **Median filter** the remaining samples (more robust than mean against outliers)
5. Use the median X,Y as the captured point

## Layer 2: Post-Compute Validation

**Location:** After `compute_calibration()` in `TouchCalibrationPanel`, before applying

1. **Back-transform check** — transform each of the 3 raw calibration points through the computed matrix, compare to expected screen positions. Compute max residual error (Euclidean distance).
2. If max residual > **10px**, reject the matrix
3. **Sanity check** — transform the center of the raw touch range through the matrix. If the result is outside screen bounds by more than 50%, reject.
4. On rejection: show "Calibration produced unusual results, please try again" and restart from POINT_1

## Layer 3: Smart Auto-Revert During Verify

**Location:** Verify state in `TouchCalibrationPanel` + calibrated read callback

1. **Reduce timeout** from 15s to **10s**
2. **Track transformed touch events** during verify — count events that land within screen bounds
3. **Fast revert:** If **3 seconds** pass with **zero on-screen touch events** while raw touch events ARE arriving (user is tapping but everything maps off-screen), auto-revert immediately
4. Show toast: "Calibration was too far off — reverting. Please try again."

The 3-second window distinguishes "user isn't touching" from "user IS touching but matrix is broken" by checking if raw events arrive but all transformed results are out-of-bounds.

## Files Affected

| File | Changes |
|------|---------|
| `src/ui/touch_calibration_panel.cpp` | Multi-sample capture, post-compute validation, timeout changes |
| `include/touch_calibration_panel.h` | Sample buffer, validation state |
| `src/ui/touch_calibration.cpp` | Back-transform validation function |
| `include/touch_calibration.h` | Validation API |
| `src/api/display_backend_fbdev.cpp` | Touch event tracking during verify |
| `src/ui/ui_touch_calibration_overlay.cpp` | Timeout constant, fast-revert UI |
| `src/ui/ui_wizard_touch_calibration.cpp` | Same timeout/revert changes |
| `tests/unit/test_touch_calibration.cpp` | Tests for filtering, validation, edge cases |

## Non-Goals

- Guppy calibration import (separate feature)
- Per-quadrant / 9-point calibration (diminishing returns)
- Adaptive sample count (unnecessary complexity)
