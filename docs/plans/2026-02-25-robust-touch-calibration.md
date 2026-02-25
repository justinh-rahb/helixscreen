# Robust Touch Calibration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the touch calibration wizard reliable on resistive panels with noisy ADC output (ns2009/Nebula Pad).

**Architecture:** Three additive defense layers: (1) multi-sample input filtering with ADC saturation rejection, (2) post-compute matrix validation via back-transform residual check, (3) smart auto-revert during verify that detects broken matrices within 3 seconds. All changes are in the calibration state machine and its helpers — no changes to the LVGL evdev pipeline.

**Tech Stack:** C++, LVGL 9.5, Catch2 (tests), spdlog

**Design Doc:** `docs/plans/2026-02-25-robust-touch-calibration-design.md`

---

### Task 1: Add `validate_calibration_result()` function

Pure function with no dependencies — test first, easiest to verify.

**Files:**
- Modify: `include/touch_calibration.h` (add declaration after line 54)
- Modify: `src/ui/touch_calibration.cpp` (add implementation after line 119)
- Modify: `tests/unit/test_touch_calibration.cpp` (add new test section)

**Step 1: Write the failing tests**

Add to `tests/unit/test_touch_calibration.cpp`:

```cpp
// ============================================================================
// Post-Compute Validation Tests
// ============================================================================

TEST_CASE("TouchCalibration: validate_calibration_result accepts good calibration",
          "[touch-calibration][validate]") {
    // Identity calibration: residuals should be 0
    Point screen_points[3] = {{120, 86}, {400, 408}, {680, 86}};
    Point touch_points[3] = {{120, 86}, {400, 408}, {680, 86}};
    TouchCalibration cal;
    compute_calibration(screen_points, touch_points, cal);

    REQUIRE(validate_calibration_result(cal, screen_points, touch_points, 800, 480) == true);
}

TEST_CASE("TouchCalibration: validate_calibration_result rejects high residual",
          "[touch-calibration][validate]") {
    // Manually craft a calibration with large back-transform error
    TouchCalibration cal;
    cal.valid = true;
    cal.a = 0.5f; cal.b = 0.0f; cal.c = 0.0f;  // Scales X to half
    cal.d = 0.0f; cal.e = 0.5f; cal.f = 0.0f;  // Scales Y to half

    // These screen points expect full-range output, but the matrix halves everything
    Point screen_points[3] = {{120, 86}, {400, 408}, {680, 86}};
    // Touch points that would need a=1 to produce those screen coords
    Point touch_points[3] = {{120, 86}, {400, 408}, {680, 86}};

    // Back-transform error: screen expects 680 but cal produces 340 → residual = 340
    REQUIRE(validate_calibration_result(cal, screen_points, touch_points, 800, 480) == false);
}

TEST_CASE("TouchCalibration: validate_calibration_result rejects off-screen center",
          "[touch-calibration][validate]") {
    // Matrix that maps center of touch range way off screen
    TouchCalibration cal;
    cal.valid = true;
    cal.a = 1.0f; cal.b = 0.0f; cal.c = 5000.0f;  // Huge X offset
    cal.d = 0.0f; cal.e = 1.0f; cal.f = 0.0f;

    Point screen_points[3] = {{120, 86}, {400, 408}, {680, 86}};
    Point touch_points[3] = {{500, 500}, {2000, 3500}, {3500, 500}};

    REQUIRE(validate_calibration_result(cal, screen_points, touch_points, 800, 480) == false);
}

TEST_CASE("TouchCalibration: validate_calibration_result accepts real ns2009 calibration",
          "[touch-calibration][validate]") {
    // Real guppy coefficients from issue #135 (Nebula Pad ns2009)
    TouchCalibration cal;
    cal.valid = true;
    cal.a = 0.1258f; cal.b = -0.0025f; cal.c = -12.63f;
    cal.d = -0.0005f; cal.e = 0.0748f; cal.f = -16.20f;

    // Approximate raw→screen mapping for 480x272 display with 12-bit ADC
    // Simulated 3-point pairs that would produce these coefficients
    Point screen_points[3] = {{72, 49}, {240, 231}, {408, 49}};
    Point touch_points[3] = {{673, 872}, {2007, 3307}, {3342, 872}};

    // Back-transform these and check — residuals should be small
    // (This tests that real-world coefficients pass validation)
    REQUIRE(validate_calibration_result(cal, screen_points, touch_points, 480, 272) == true);
}
```

**Step 2: Run tests to verify they fail**

Run: `make test && ./build/bin/helix-tests "[validate]" -v`
Expected: FAIL — `validate_calibration_result` not defined

**Step 3: Add declaration to header**

In `include/touch_calibration.h`, after the `is_calibration_valid()` declaration (line 54), add:

```cpp
/**
 * @brief Validate calibration result by checking back-transform residuals
 *
 * Transforms each raw calibration point through the matrix and checks how close
 * the result is to the expected screen position. Also checks that the center
 * of the touch range maps to somewhere on-screen.
 *
 * @param cal Computed calibration to validate
 * @param screen_points 3 expected screen positions
 * @param touch_points 3 raw touch positions used to compute cal
 * @param screen_width Display width in pixels
 * @param screen_height Display height in pixels
 * @param max_residual Maximum allowed back-transform error in pixels (default: 10)
 * @return true if calibration passes validation
 */
bool validate_calibration_result(const TouchCalibration& cal, const Point screen_points[3],
                                 const Point touch_points[3], int screen_width, int screen_height,
                                 float max_residual = 10.0f);
```

**Step 4: Implement the function**

In `src/ui/touch_calibration.cpp`, after `is_calibration_valid()` (line 119), add:

```cpp
bool validate_calibration_result(const TouchCalibration& cal, const Point screen_points[3],
                                 const Point touch_points[3], int screen_width, int screen_height,
                                 float max_residual) {
    if (!cal.valid) {
        return false;
    }

    // Check 1: Back-transform residuals
    // Transform each raw touch point through the matrix and compare to expected screen position
    for (int i = 0; i < 3; i++) {
        Point transformed = transform_point(cal, touch_points[i]);
        float dx = static_cast<float>(transformed.x - screen_points[i].x);
        float dy = static_cast<float>(transformed.y - screen_points[i].y);
        float residual = std::sqrt(dx * dx + dy * dy);

        if (residual > max_residual) {
            spdlog::warn("[TouchCalibration] Back-transform residual {:.1f}px at point {} "
                         "(expected ({},{}), got ({},{}))",
                         residual, i, screen_points[i].x, screen_points[i].y,
                         transformed.x, transformed.y);
            return false;
        }
    }

    // Check 2: Center of touch range should map to somewhere near the screen
    // Find the center of the raw touch points
    int center_x = (touch_points[0].x + touch_points[1].x + touch_points[2].x) / 3;
    int center_y = (touch_points[0].y + touch_points[1].y + touch_points[2].y) / 3;
    Point center_transformed = transform_point(cal, {center_x, center_y});

    // Allow 50% overshoot beyond screen bounds
    int margin_x = screen_width / 2;
    int margin_y = screen_height / 2;
    if (center_transformed.x < -margin_x || center_transformed.x > screen_width + margin_x ||
        center_transformed.y < -margin_y || center_transformed.y > screen_height + margin_y) {
        spdlog::warn("[TouchCalibration] Center of touch range ({},{}) maps to ({},{}), "
                     "which is far off-screen ({}x{})",
                     center_x, center_y, center_transformed.x, center_transformed.y,
                     screen_width, screen_height);
        return false;
    }

    return true;
}
```

**Step 5: Run tests to verify they pass**

Run: `make test && ./build/bin/helix-tests "[validate]" -v`
Expected: All 4 tests PASS

**Step 6: Commit**

```bash
git add include/touch_calibration.h src/ui/touch_calibration.cpp tests/unit/test_touch_calibration.cpp
git commit -m "feat(touch): add post-compute calibration validation (prestonbrown/helixscreen#198)"
```

---

### Task 2: Add multi-sample input filtering to `TouchCalibrationPanel`

Changes the point capture from single-sample to multi-sample with median filter and ADC saturation rejection.

**Files:**
- Modify: `include/touch_calibration_panel.h` (add sample buffer, new method)
- Modify: `src/ui/touch_calibration_panel.cpp` (rewrite `capture_point()`, add filtering)
- Modify: `tests/unit/test_touch_calibration.cpp` (add filtering tests)

**Step 1: Write the failing tests**

Add to `tests/unit/test_touch_calibration.cpp`. These test the `TouchCalibrationPanel` state machine with the new multi-sample API:

```cpp
// ============================================================================
// Multi-Sample Input Filtering Tests
// ============================================================================

TEST_CASE("TouchCalibrationPanel: accepts clean samples after threshold",
          "[touch-calibration][filtering]") {
    helix::TouchCalibrationPanel panel;
    panel.set_screen_size(800, 480);
    panel.start();

    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_1);

    // Feed 7 clean samples — should advance to POINT_2
    for (int i = 0; i < 7; i++) {
        panel.add_sample({1000, 2000});
    }
    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_2);
}

TEST_CASE("TouchCalibrationPanel: rejects ADC-saturated samples",
          "[touch-calibration][filtering]") {
    helix::TouchCalibrationPanel panel;
    panel.set_screen_size(800, 480);
    panel.start();

    // Feed 4 clean + 3 saturated (X=4095) — should still advance (4 valid ≥ 3 minimum)
    for (int i = 0; i < 4; i++) {
        panel.add_sample({1000, 2000});
    }
    for (int i = 0; i < 3; i++) {
        panel.add_sample({4095, 2000});
    }
    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_2);
}

TEST_CASE("TouchCalibrationPanel: fails when too many saturated samples",
          "[touch-calibration][filtering]") {
    helix::TouchCalibrationPanel panel;
    panel.set_screen_size(800, 480);

    bool failure_called = false;
    panel.set_failure_callback([&](const char*) { failure_called = true; });
    panel.start();

    // Feed 2 clean + 5 saturated — only 2 valid, below minimum of 3
    for (int i = 0; i < 2; i++) {
        panel.add_sample({1000, 2000});
    }
    for (int i = 0; i < 5; i++) {
        panel.add_sample({4095, 3500});
    }

    // Should still be on POINT_1 (not advanced) and failure callback fired
    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_1);
    REQUIRE(failure_called == true);
}

TEST_CASE("TouchCalibrationPanel: median filter removes outliers",
          "[touch-calibration][filtering]") {
    helix::TouchCalibrationPanel panel;
    panel.set_screen_size(800, 480);

    // Track what calibration is computed by running through all 3 points
    panel.start();

    // Point 1: mostly 1000,2000 with one outlier
    panel.add_sample({1000, 2000});
    panel.add_sample({1000, 2000});
    panel.add_sample({1000, 2000});
    panel.add_sample({500, 3000});  // outlier
    panel.add_sample({1000, 2000});
    panel.add_sample({1000, 2000});
    panel.add_sample({1000, 2000});
    // Median should be (1000, 2000), not skewed by outlier

    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_2);
}
```

**Step 2: Run tests to verify they fail**

Run: `make test && ./build/bin/helix-tests "[filtering]" -v`
Expected: FAIL — `add_sample` not defined

**Step 3: Add sample buffer and new API to header**

In `include/touch_calibration_panel.h`, add to private members (after line 196):

```cpp
    // Multi-sample filtering
    static constexpr int SAMPLES_REQUIRED = 7;
    static constexpr int MIN_VALID_SAMPLES = 3;

    struct RawSample {
        int x = 0;
        int y = 0;
    };
    RawSample sample_buffer_[SAMPLES_REQUIRED]{};
    int sample_count_ = 0;

    /// Check if a sample has ADC-saturated values
    static bool is_saturated_sample(const Point& sample);

    /// Compute median from valid samples in the buffer, returns false if not enough valid samples
    bool compute_median_point(Point& out);

    /// Reset sample buffer for new point capture
    void reset_samples();
```

Add to public section (after `capture_point()` at line 131):

```cpp
    /**
     * @brief Add a raw touch sample to the current capture buffer
     *
     * Collects multiple samples per calibration point. When SAMPLES_REQUIRED
     * samples have been collected, filters out ADC-saturated values, computes
     * the median, and advances the state machine.
     *
     * @param raw Raw touch coordinates from touch controller
     */
    void add_sample(Point raw);
```

**Step 4: Implement filtering logic**

In `src/ui/touch_calibration_panel.cpp`, add after `capture_point()` (line 116):

```cpp
bool TouchCalibrationPanel::is_saturated_sample(const Point& sample) {
    // Common ADC saturation values for resistive touch controllers
    // 12-bit: 4095, 16-bit: 65535
    return sample.x == 4095 || sample.y == 4095 ||
           sample.x == 65535 || sample.y == 65535;
}

void TouchCalibrationPanel::reset_samples() {
    sample_count_ = 0;
}

bool TouchCalibrationPanel::compute_median_point(Point& out) {
    // Collect valid (non-saturated) samples
    std::vector<int> valid_x, valid_y;
    for (int i = 0; i < sample_count_; i++) {
        Point p{sample_buffer_[i].x, sample_buffer_[i].y};
        if (!is_saturated_sample(p)) {
            valid_x.push_back(p.x);
            valid_y.push_back(p.y);
        }
    }

    if (static_cast<int>(valid_x.size()) < MIN_VALID_SAMPLES) {
        spdlog::warn("[TouchCalibrationPanel] Only {}/{} valid samples (need {})",
                     valid_x.size(), sample_count_, MIN_VALID_SAMPLES);
        return false;
    }

    // Compute median
    std::sort(valid_x.begin(), valid_x.end());
    std::sort(valid_y.begin(), valid_y.end());
    size_t mid = valid_x.size() / 2;
    out.x = valid_x[mid];
    out.y = valid_y[mid];

    spdlog::debug("[TouchCalibrationPanel] Median from {}/{} valid samples: ({}, {})",
                  valid_x.size(), sample_count_, out.x, out.y);
    return true;
}

void TouchCalibrationPanel::add_sample(Point raw) {
    // Only collect during point capture states
    if (state_ != State::POINT_1 && state_ != State::POINT_2 && state_ != State::POINT_3) {
        return;
    }

    if (sample_count_ < SAMPLES_REQUIRED) {
        sample_buffer_[sample_count_] = {raw.x, raw.y};
        sample_count_++;
    }

    // Once we have enough samples, compute median and capture
    if (sample_count_ >= SAMPLES_REQUIRED) {
        Point median;
        if (compute_median_point(median)) {
            capture_point(median);
        } else {
            // Not enough valid samples — notify and stay on same point
            if (failure_callback_) {
                failure_callback_("Touch was noisy, please tap again.");
            }
        }
        reset_samples();
    }
}
```

Also add `#include <vector>` and `#include <algorithm>` to the includes.

Update `start()` and `retry()` to call `reset_samples()`:

In `start()` (line 74), add after line 76:
```cpp
    reset_samples();
```

In `retry()` (line 131), add after line 139:
```cpp
    reset_samples();
```

**Step 5: Run tests to verify they pass**

Run: `make test && ./build/bin/helix-tests "[filtering]" -v`
Expected: All 4 tests PASS

**Step 6: Commit**

```bash
git add include/touch_calibration_panel.h src/ui/touch_calibration_panel.cpp tests/unit/test_touch_calibration.cpp
git commit -m "feat(touch): add multi-sample input filtering with ADC saturation rejection (prestonbrown/helixscreen#198)"
```

---

### Task 3: Wire validation into the capture state machine

Integrate `validate_calibration_result()` into the POINT_3 → VERIFY transition.

**Files:**
- Modify: `src/ui/touch_calibration_panel.cpp:94-110` (add validation after `compute_calibration()`)

**Step 1: Write the failing test**

Add to `tests/unit/test_touch_calibration.cpp`:

```cpp
TEST_CASE("TouchCalibrationPanel: rejects calibration with bad matrix",
          "[touch-calibration][panel-validate]") {
    helix::TouchCalibrationPanel panel;
    panel.set_screen_size(800, 480);

    bool failure_called = false;
    std::string failure_reason;
    panel.set_failure_callback([&](const char* reason) {
        failure_called = true;
        failure_reason = reason;
    });
    panel.start();

    // Capture 3 points that produce a valid but terrible matrix
    // Points are very close together (not collinear, so compute_calibration succeeds)
    // but the resulting matrix will have huge residuals when back-transformed
    panel.capture_point({100, 100});
    panel.capture_point({102, 100});
    panel.capture_point({100, 102});

    // These nearly-identical touch points should pass compute_calibration (determinant != 0)
    // but fail validate_calibration_result (residuals way over 10px)
    REQUIRE(panel.get_state() == helix::TouchCalibrationPanel::State::POINT_1); // restarted
    REQUIRE(failure_called == true);
    REQUIRE(failure_reason.find("unusual") != std::string::npos);
}
```

**Step 2: Run test to verify it fails**

Run: `make test && ./build/bin/helix-tests "[panel-validate]" -v`
Expected: FAIL — panel enters VERIFY instead of restarting

**Step 3: Add validation to `capture_point()`**

In `src/ui/touch_calibration_panel.cpp`, modify the POINT_3 case (lines 94-110):

```cpp
    case State::POINT_3:
        touch_points_[2] = raw;
        // Compute calibration and check for degenerate case (collinear points)
        if (compute_calibration(screen_points_, touch_points_, calibration_)) {
            // Validate the matrix produces reasonable results
            if (validate_calibration_result(calibration_, screen_points_, touch_points_,
                                            screen_width_, screen_height_)) {
                state_ = State::VERIFY;
                start_countdown_timer();
            } else {
                // Matrix is valid but produces bad results (noisy input)
                spdlog::warn("[TouchCalibrationPanel] Calibration matrix failed validation, "
                             "restarting");
                state_ = State::POINT_1;
                calibration_.valid = false;
                if (failure_callback_) {
                    failure_callback_(
                        "Calibration produced unusual results. Please try again.");
                }
            }
        } else {
            // Calibration failed (collinear/duplicate points) - restart from POINT_1
            spdlog::warn(
                "[TouchCalibrationPanel] Calibration failed (degenerate points), restarting");
            state_ = State::POINT_1;
            calibration_.valid = false;
            if (failure_callback_) {
                failure_callback_("Touch points too close together. Please try again.");
            }
        }
        break;
```

**Step 4: Run tests to verify they pass**

Run: `make test && ./build/bin/helix-tests "[panel-validate]" -v`
Expected: PASS

Also run all calibration tests: `./build/bin/helix-tests "[touch-calibration]" -v`
Expected: All PASS

**Step 5: Commit**

```bash
git add src/ui/touch_calibration_panel.cpp tests/unit/test_touch_calibration.cpp
git commit -m "feat(touch): wire post-compute validation into calibration state machine (prestonbrown/helixscreen#198)"
```

---

### Task 4: Wire `add_sample()` into the overlay and wizard UIs

Replace the single `capture_point()` calls with `add_sample()` in both the settings overlay and the first-run wizard.

**Files:**
- Modify: `src/ui/ui_touch_calibration_overlay.cpp:557` (change `capture_point` to `add_sample`)
- Modify: `src/ui/ui_wizard_touch_calibration.cpp` (same change in equivalent touch handler)

**Step 1: Update the overlay**

In `src/ui/ui_touch_calibration_overlay.cpp`, line 557, change:

```cpp
    panel_->capture_point({point.x, point.y});
```

to:

```cpp
    panel_->add_sample({point.x, point.y});
```

The overlay calls this from its touch event handler. Previously it captured a single point per tap. Now each touch event feeds into the sample buffer, and the panel auto-advances after collecting 7 samples.

**Step 2: Update the wizard**

Find the equivalent `capture_point()` call in `src/ui/ui_wizard_touch_calibration.cpp` and make the same change.

**Step 3: Build and verify**

Run: `make -j`
Expected: Clean build

**Step 4: Manual test (interactive)**

Run: `./build/bin/helix-screen --test -vv -p touch_calibration`

Test: tap a calibration target and observe debug logs showing sample collection and median computation. Verify the state advances after ~7 touch events rather than 1.

**Step 5: Commit**

```bash
git add src/ui/ui_touch_calibration_overlay.cpp src/ui/ui_wizard_touch_calibration.cpp
git commit -m "feat(touch): wire multi-sample capture into overlay and wizard UIs (prestonbrown/helixscreen#198)"
```

---

### Task 5: Reduce verify timeout and add smart auto-revert

Reduce from 15s to 10s. Add fast-revert logic: if 3 seconds pass with raw touches arriving but zero transformed events landing on-screen, auto-revert immediately.

**Files:**
- Modify: `include/touch_calibration_panel.h` (add verify tracking members, fast-revert callback)
- Modify: `src/ui/touch_calibration_panel.cpp` (reduce timeout, add tracking, fast-revert timer)
- Modify: `src/ui/ui_touch_calibration_overlay.cpp` (set fast-revert callback, update timeout text)
- Modify: `src/ui/ui_wizard_touch_calibration.cpp` (same changes)

**Step 1: Add fast-revert callback type and tracking to header**

In `include/touch_calibration_panel.h`, add after `TimeoutCallback` (line 70):

```cpp
    /**
     * @brief Callback invoked when fast-revert triggers (broken matrix detected)
     */
    using FastRevertCallback = std::function<void()>;
```

Add public setter:

```cpp
    /**
     * @brief Set callback for fast-revert (matrix detected as broken during verify)
     */
    void set_fast_revert_callback(FastRevertCallback cb);
```

Add public method for the overlay to report touch events during verify:

```cpp
    /**
     * @brief Report a touch event during verify state
     *
     * Called by the overlay/wizard when a touch event occurs during VERIFY.
     * Tracks whether transformed coordinates land on-screen to detect broken matrices.
     *
     * @param transformed_x Transformed X coordinate (after calibration)
     * @param transformed_y Transformed Y coordinate (after calibration)
     * @param on_screen Whether the transformed point is within screen bounds
     */
    void report_verify_touch(bool on_screen);
```

Add private members:

```cpp
    FastRevertCallback fast_revert_callback_;
    int verify_raw_touch_count_ = 0;      // Total raw touches during verify
    int verify_onscreen_touch_count_ = 0;  // Touches that landed on-screen
    lv_timer_t* fast_revert_timer_ = nullptr;
    static constexpr int FAST_REVERT_CHECK_MS = 3000; // Check after 3 seconds

    void start_fast_revert_timer();
    void stop_fast_revert_timer();
    static void fast_revert_timer_cb(lv_timer_t* timer);
```

**Step 2: Implement fast-revert logic**

In `src/ui/touch_calibration_panel.cpp`:

Change default timeout (line 190 in header):

```cpp
    int verify_timeout_seconds_ = 10;  // Changed from 15
```

Add implementation:

```cpp
void TouchCalibrationPanel::set_fast_revert_callback(FastRevertCallback cb) {
    fast_revert_callback_ = std::move(cb);
}

void TouchCalibrationPanel::report_verify_touch(bool on_screen) {
    if (state_ != State::VERIFY) return;
    verify_raw_touch_count_++;
    if (on_screen) {
        verify_onscreen_touch_count_++;
    }
}

void TouchCalibrationPanel::start_fast_revert_timer() {
    verify_raw_touch_count_ = 0;
    verify_onscreen_touch_count_ = 0;
    fast_revert_timer_ = lv_timer_create(fast_revert_timer_cb, FAST_REVERT_CHECK_MS, this);
    // One-shot: delete after first fire
    lv_timer_set_repeat_count(fast_revert_timer_, 1);
    spdlog::debug("[TouchCalibrationPanel] Started fast-revert timer ({}ms)", FAST_REVERT_CHECK_MS);
}

void TouchCalibrationPanel::stop_fast_revert_timer() {
    if (fast_revert_timer_) {
        lv_timer_delete(fast_revert_timer_);
        fast_revert_timer_ = nullptr;
    }
}

void TouchCalibrationPanel::fast_revert_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<TouchCalibrationPanel*>(lv_timer_get_user_data(timer));
    self->fast_revert_timer_ = nullptr; // Timer auto-deletes (repeat_count=1)

    if (self->state_ != State::VERIFY) return;

    // If user tapped (raw events > 0) but nothing landed on-screen, the matrix is broken
    if (self->verify_raw_touch_count_ > 0 && self->verify_onscreen_touch_count_ == 0) {
        spdlog::warn("[TouchCalibrationPanel] Fast-revert: {} raw touches, 0 on-screen — "
                     "matrix is broken, reverting",
                     self->verify_raw_touch_count_);
        if (self->fast_revert_callback_) {
            self->fast_revert_callback_();
        }
    } else {
        spdlog::debug("[TouchCalibrationPanel] Fast-revert check passed: {}/{} on-screen",
                      self->verify_onscreen_touch_count_, self->verify_raw_touch_count_);
    }
}
```

Start the fast-revert timer when entering VERIFY. In `capture_point()`, after `start_countdown_timer()`:

```cpp
            start_fast_revert_timer();
```

Stop it in `accept()`, `retry()`, `cancel()`, and destructor alongside `stop_countdown_timer()`:

```cpp
    stop_fast_revert_timer();
```

**Step 3: Wire into overlay**

In `src/ui/ui_touch_calibration_overlay.cpp`, in the constructor's callback setup section:

```cpp
panel_->set_fast_revert_callback([this]() {
    // Matrix is so bad nothing maps on-screen — revert immediately
    if (has_backup_) {
        DisplayManager* dm = DisplayManager::instance();
        if (dm) {
            dm->apply_touch_calibration(backup_calibration_);
        }
        has_backup_ = false;
    }
    panel_->retry();
    update_state_subject();
    update_instruction_text();
    update_crosshair_position();
    update_step_progress();
    // Toast feedback
    if (failure_callback_) {
        failure_callback_("Calibration was too far off — reverting. Please try again.");
    }
});
```

In the overlay's verify touch handler (where ripple feedback is shown), add after the ripple logic:

```cpp
// Report to panel for fast-revert tracking
bool on_screen = (point.x >= 0 && point.x < screen_width &&
                  point.y >= 0 && point.y < screen_height);
panel_->report_verify_touch(on_screen);
```

Make the same changes in `ui_wizard_touch_calibration.cpp`.

**Step 4: Build and verify**

Run: `make -j`
Expected: Clean build

Run existing tests: `make test-run`
Expected: All pass (the timeout change doesn't affect unit tests)

**Step 5: Commit**

```bash
git add include/touch_calibration_panel.h src/ui/touch_calibration_panel.cpp \
    src/ui/ui_touch_calibration_overlay.cpp src/ui/ui_wizard_touch_calibration.cpp
git commit -m "feat(touch): add smart auto-revert with 10s timeout and broken-matrix detection (prestonbrown/helixscreen#198)"
```

---

### Task 6: Final integration test and cleanup

**Step 1: Run full test suite**

```bash
make test-run
```

Expected: All pass, no regressions.

**Step 2: Build the full application**

```bash
make -j
```

**Step 3: Manual integration test**

Run: `./build/bin/helix-screen --test -vv`

Test the calibration overlay from Settings:
1. Tap calibration targets — verify sample collection logs show 7 samples per point
2. Verify bad calibration attempt triggers validation rejection with clear message
3. Verify verify state shows 10s countdown (not 15s)

**Step 4: Commit any cleanup**

```bash
git commit -m "chore(touch): cleanup after robust calibration integration (prestonbrown/helixscreen#198)"
```
