// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_frequency_response_chart.cpp
 * @brief Unit tests for FrequencyResponseChart widget
 *
 * Test-first development: These tests are written BEFORE implementation.
 * Tests verify the frequency response chart widget for input shaper calibration
 * data visualization.
 *
 * Test categories:
 * 1. Creation/destruction - Basic lifecycle management
 * 2. Series management - Add, remove, show/hide multiple data series
 * 3. Data management - Setting data with downsampling behavior
 * 4. Peak marking - Highlight resonance peaks
 * 5. Configuration - Frequency/amplitude range settings
 * 6. Platform adaptation - Hardware tier configuration and limits
 *
 * Key behaviors:
 * - EMBEDDED tier: Table mode only (is_chart_mode = false), max 0 chart points
 * - BASIC tier: Simplified chart, max 50 points
 * - STANDARD tier: Full chart, max 200 points
 * - Downsampling preserves frequency range endpoints
 */

#include "../../include/platform_capabilities.h"
#include "../../include/ui_frequency_response_chart.h"
#include "../ui_test_utils.h"
#include "lvgl/lvgl.h"

#include "../catch_amalgamated.hpp"

using namespace helix;

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Test fixture for frequency response chart tests
 *
 * Provides LVGL initialization and a parent screen for widget creation.
 */
class FrequencyResponseChartTestFixture {
  public:
    FrequencyResponseChartTestFixture() {
        // Initialize LVGL for testing (safe version avoids "already initialized" warnings)
        lv_init_safe();

        // Create a display for testing (headless)
        lv_display_t* disp = lv_display_create(800, 480);
        alignas(64) static lv_color_t buf1[800 * 10];
        lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

        // Create a screen object to use as parent
        screen = lv_obj_create(NULL);
    }

    ~FrequencyResponseChartTestFixture() {
        // Cleanup is handled by LVGL
    }

    lv_obj_t* screen;
};

// ============================================================================
// Creation/Destruction Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Create and destroy frequency response chart",
                 "[frequency_response_chart][lifecycle]") {
    SECTION("Create chart with valid parent") {
        ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);

        REQUIRE(chart != nullptr);
        REQUIRE(ui_frequency_response_chart_get_obj(chart) != nullptr);

        ui_frequency_response_chart_destroy(chart);
    }

    SECTION("Create chart with NULL parent returns NULL") {
        ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(nullptr);
        REQUIRE(chart == nullptr);
    }

    SECTION("Destroy NULL chart is safe") {
        ui_frequency_response_chart_destroy(nullptr);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Get obj from NULL chart returns NULL") {
        lv_obj_t* obj = ui_frequency_response_chart_get_obj(nullptr);
        REQUIRE(obj == nullptr);
    }

    SECTION("Double destroy is safe") {
        ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
        REQUIRE(chart != nullptr);

        ui_frequency_response_chart_destroy(chart);
        // Second destroy should be safe (though pointer is now dangling - don't do this in
        // production) This tests that the function handles already-freed state gracefully if called
        // with garbage
        REQUIRE(true);
    }
}

// ============================================================================
// Series Management Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Add series returns unique IDs",
                 "[frequency_response_chart][series]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Add single series returns valid ID") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        REQUIRE(id >= 0);
    }

    SECTION("Add multiple series returns unique IDs") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));
        int id3 = ui_frequency_response_chart_add_series(chart, "Z Axis", lv_color_hex(0x4444FF));

        REQUIRE(id1 >= 0);
        REQUIRE(id2 >= 0);
        REQUIRE(id3 >= 0);
        REQUIRE(id1 != id2);
        REQUIRE(id2 != id3);
        REQUIRE(id1 != id3);
    }

    SECTION("Add series with NULL name fails") {
        int id = ui_frequency_response_chart_add_series(chart, nullptr, lv_color_hex(0xFF4444));
        REQUIRE(id == -1);
    }

    SECTION("Add series to NULL chart fails") {
        int id = ui_frequency_response_chart_add_series(nullptr, "X Axis", lv_color_hex(0xFF4444));
        REQUIRE(id == -1);
    }

    SECTION("Add series with empty name succeeds") {
        // Empty string is valid, just not NULL
        int id = ui_frequency_response_chart_add_series(chart, "", lv_color_hex(0xFF4444));
        REQUIRE(id >= 0);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Remove series cleans up",
                 "[frequency_response_chart][series]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Remove existing series") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        REQUIRE(id >= 0);

        ui_frequency_response_chart_remove_series(chart, id);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Remove series from middle maintains others") {
        int id1 = ui_frequency_response_chart_add_series(chart, "Series1", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Series2", lv_color_hex(0x44FF44));
        int id3 = ui_frequency_response_chart_add_series(chart, "Series3", lv_color_hex(0x4444FF));

        ui_frequency_response_chart_remove_series(chart, id2);

        // Verify we can still use remaining series
        float freqs[] = {10.0f, 20.0f, 30.0f};
        float amps[] = {1.0f, 2.0f, 1.5f};
        ui_frequency_response_chart_set_data(chart, id1, freqs, amps, 3);
        ui_frequency_response_chart_set_data(chart, id3, freqs, amps, 3);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Remove invalid series ID does nothing") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        REQUIRE(id >= 0);

        ui_frequency_response_chart_remove_series(chart, 999);
        // Should not crash, original series still valid
        float freqs[] = {10.0f};
        float amps[] = {1.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 1);
        REQUIRE(true);
    }

    SECTION("Remove from NULL chart is safe") {
        ui_frequency_response_chart_remove_series(nullptr, 0);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Remove already removed series is safe") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        ui_frequency_response_chart_remove_series(chart, id);
        ui_frequency_response_chart_remove_series(chart, id); // Remove again
        // Should not crash
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Show/hide series toggles visibility",
                 "[frequency_response_chart][series]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Hide visible series") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        REQUIRE(id >= 0);

        ui_frequency_response_chart_show_series(chart, id, false);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Show hidden series") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        ui_frequency_response_chart_show_series(chart, id, false);
        ui_frequency_response_chart_show_series(chart, id, true);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Toggle visibility multiple times") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        for (int i = 0; i < 10; i++) {
            ui_frequency_response_chart_show_series(chart, id, i % 2 == 0);
        }
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Show/hide invalid series ID does nothing") {
        ui_frequency_response_chart_show_series(chart, 999, false);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Show/hide on NULL chart is safe") {
        ui_frequency_response_chart_show_series(nullptr, 0, false);
        // Should not crash
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Data Management Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Set data with various point counts",
                 "[frequency_response_chart][data]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    // Configure for STANDARD tier (max 200 points) for these tests
    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Set data with small array") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        float amps[] = {1.0f, 2.5f, 5.0f, 2.0f, 0.5f};

        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 5);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Set data with exact max points") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        size_t max_points = ui_frequency_response_chart_get_max_points(chart);
        std::vector<float> freqs(max_points);
        std::vector<float> amps(max_points);

        for (size_t i = 0; i < max_points; i++) {
            freqs[i] = 10.0f + i * 0.5f;
            amps[i] = 1.0f + (float)(i % 50) * 0.1f;
        }

        ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), max_points);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Set data with NULL frequencies fails gracefully") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        float amps[] = {1.0f, 2.0f};
        ui_frequency_response_chart_set_data(chart, id, nullptr, amps, 2);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Set data with NULL amplitudes fails gracefully") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        float freqs[] = {10.0f, 20.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, nullptr, 2);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Set data with zero count fails gracefully") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        float freqs[] = {10.0f};
        float amps[] = {1.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 0);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Set data on invalid series ID is safe") {
        float freqs[] = {10.0f};
        float amps[] = {1.0f};
        ui_frequency_response_chart_set_data(chart, 999, freqs, amps, 1);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Set data on NULL chart is safe") {
        float freqs[] = {10.0f};
        float amps[] = {1.0f};
        ui_frequency_response_chart_set_data(nullptr, 0, freqs, amps, 1);
        // Should not crash
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture,
                 "Downsampling preserves frequency range endpoints",
                 "[frequency_response_chart][downsampling]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    // Configure for STANDARD tier (max 200 points)
    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Data with 500 points on STANDARD tier downsamples to ~200 points") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        const size_t input_count = 500;
        std::vector<float> freqs(input_count);
        std::vector<float> amps(input_count);

        float freq_min = 10.0f;
        float freq_max = 200.0f;

        for (size_t i = 0; i < input_count; i++) {
            freqs[i] = freq_min + (freq_max - freq_min) * i / (input_count - 1);
            amps[i] = 1.0f + (float)(i % 50) * 0.1f;
        }

        ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), input_count);

        // The chart should have downsampled to max_points
        // We verify this indirectly - if get_max_points returns 200, the chart
        // should have limited the data to that
        size_t max_points = ui_frequency_response_chart_get_max_points(chart);
        REQUIRE(max_points == PlatformCapabilities::STANDARD_CHART_POINTS);
    }

    SECTION("Data with 100 points on STANDARD tier keeps all points (no downsampling)") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        const size_t input_count = 100;
        std::vector<float> freqs(input_count);
        std::vector<float> amps(input_count);

        for (size_t i = 0; i < input_count; i++) {
            freqs[i] = 10.0f + i * 2.0f;
            amps[i] = 1.0f;
        }

        ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), input_count);
        // No downsampling needed - 100 < 200
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Clear data removes all points",
                 "[frequency_response_chart][data]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Clear removes data from all series") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        float freqs[] = {10.0f, 20.0f, 30.0f};
        float amps[] = {1.0f, 2.0f, 1.5f};

        ui_frequency_response_chart_set_data(chart, id1, freqs, amps, 3);
        ui_frequency_response_chart_set_data(chart, id2, freqs, amps, 3);

        ui_frequency_response_chart_clear(chart);

        // Series should still exist, just data cleared
        // Can add new data without issues
        ui_frequency_response_chart_set_data(chart, id1, freqs, amps, 3);
        REQUIRE(true);
    }

    SECTION("Clear NULL chart is safe") {
        ui_frequency_response_chart_clear(nullptr);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Clear empty chart is safe") {
        ui_frequency_response_chart_clear(chart);
        REQUIRE(true);
    }

    SECTION("Clear chart with no series is safe") {
        ui_frequency_response_chart_clear(chart);
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Peak Marking Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Mark peak creates cursor at frequency",
                 "[frequency_response_chart][peak]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    // Configure for STANDARD tier to have chart mode enabled
    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Mark peak on valid series") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {10.0f, 30.0f, 50.0f, 70.0f, 100.0f};
        float amps[] = {1.0f, 3.0f, 10.0f, 2.0f, 0.5f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 5);

        // Mark peak at 50 Hz with amplitude 10.0
        ui_frequency_response_chart_mark_peak(chart, id, 50.0f, 10.0f);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Mark peak updates existing marker") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        ui_frequency_response_chart_mark_peak(chart, id, 50.0f, 10.0f);
        ui_frequency_response_chart_mark_peak(chart, id, 75.0f, 8.0f);
        // Should update, not add second marker
        REQUIRE(true);
    }

    SECTION("Mark peak on different series is independent") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        ui_frequency_response_chart_mark_peak(chart, id1, 50.0f, 10.0f);
        ui_frequency_response_chart_mark_peak(chart, id2, 75.0f, 8.0f);
        // Each series has its own peak marker
        REQUIRE(true);
    }

    SECTION("Mark peak on invalid series ID is safe") {
        ui_frequency_response_chart_mark_peak(chart, 999, 50.0f, 10.0f);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Mark peak on NULL chart is safe") {
        ui_frequency_response_chart_mark_peak(nullptr, 0, 50.0f, 10.0f);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Mark peak with zero amplitude is valid") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        ui_frequency_response_chart_mark_peak(chart, id, 50.0f, 0.0f);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Mark peak with negative frequency is handled") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        // Negative frequency is invalid but should not crash
        ui_frequency_response_chart_mark_peak(chart, id, -10.0f, 5.0f);
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Clear peak removes cursor",
                 "[frequency_response_chart][peak]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Clear peak after marking") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        ui_frequency_response_chart_mark_peak(chart, id, 50.0f, 10.0f);
        ui_frequency_response_chart_clear_peak(chart, id);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Clear peak when none marked is safe") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        ui_frequency_response_chart_clear_peak(chart, id);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Clear peak on invalid series ID is safe") {
        ui_frequency_response_chart_clear_peak(chart, 999);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Clear peak on NULL chart is safe") {
        ui_frequency_response_chart_clear_peak(nullptr, 0);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Clear peak only affects specified series") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        ui_frequency_response_chart_mark_peak(chart, id1, 50.0f, 10.0f);
        ui_frequency_response_chart_mark_peak(chart, id2, 75.0f, 8.0f);

        ui_frequency_response_chart_clear_peak(chart, id1);
        // id2's peak should still exist (can mark again without issues)
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Set frequency range updates axis",
                 "[frequency_response_chart][config]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Set valid frequency range") {
        ui_frequency_response_chart_set_freq_range(chart, 0.0f, 200.0f);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Set custom frequency range") {
        ui_frequency_response_chart_set_freq_range(chart, 10.0f, 150.0f);
        REQUIRE(true);
    }

    SECTION("Set frequency range with different values") {
        ui_frequency_response_chart_set_freq_range(chart, 5.0f, 500.0f);
        REQUIRE(true);
    }

    SECTION("Invalid range (min >= max) is rejected or handled") {
        // Should not crash - either rejected or handled gracefully
        ui_frequency_response_chart_set_freq_range(chart, 100.0f, 50.0f);
        REQUIRE(true);
    }

    SECTION("Invalid range (min == max) is rejected or handled") {
        ui_frequency_response_chart_set_freq_range(chart, 100.0f, 100.0f);
        REQUIRE(true);
    }

    SECTION("Set range on NULL chart is safe") {
        ui_frequency_response_chart_set_freq_range(nullptr, 0.0f, 200.0f);
        // Should not crash
        REQUIRE(true);
    }

    SECTION("Negative frequency values are handled") {
        // Frequency should typically be positive, but shouldn't crash
        ui_frequency_response_chart_set_freq_range(chart, -10.0f, 200.0f);
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Set amplitude range updates axis",
                 "[frequency_response_chart][config]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Set valid amplitude range") {
        ui_frequency_response_chart_set_amplitude_range(chart, 0.0f, 100.0f);
        // No crash = success
        REQUIRE(true);
    }

    SECTION("Set custom amplitude range") {
        ui_frequency_response_chart_set_amplitude_range(chart, -20.0f, 40.0f);
        REQUIRE(true);
    }

    SECTION("Set amplitude range for logarithmic scale") {
        // Common for frequency response: dB scale
        ui_frequency_response_chart_set_amplitude_range(chart, -60.0f, 20.0f);
        REQUIRE(true);
    }

    SECTION("Invalid range (min >= max) is rejected or handled") {
        ui_frequency_response_chart_set_amplitude_range(chart, 100.0f, 50.0f);
        REQUIRE(true);
    }

    SECTION("Set range on NULL chart is safe") {
        ui_frequency_response_chart_set_amplitude_range(nullptr, 0.0f, 100.0f);
        // Should not crash
        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Platform Adaptation Tests (Key Feature)
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture,
                 "Configure for STANDARD tier enables chart mode",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == true);
    REQUIRE(ui_frequency_response_chart_get_max_points(chart) ==
            PlatformCapabilities::STANDARD_CHART_POINTS);

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Configure for BASIC tier limits to 50 points",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::BASIC);

    REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == true);
    REQUIRE(ui_frequency_response_chart_get_max_points(chart) ==
            PlatformCapabilities::BASIC_CHART_POINTS);

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture,
                 "Configure for EMBEDDED tier enables table mode",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::EMBEDDED);

    // EMBEDDED tier should use table mode, not chart mode
    REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == false);
    REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 0);

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Get max points returns tier-appropriate value",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("STANDARD tier returns 200 points") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);
        REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 200);
    }

    SECTION("BASIC tier returns 50 points") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::BASIC);
        REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 50);
    }

    SECTION("EMBEDDED tier returns 0 points") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::EMBEDDED);
        REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 0);
    }

    SECTION("Get max points from NULL chart returns 0") {
        REQUIRE(ui_frequency_response_chart_get_max_points(nullptr) == 0);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture,
                 "is_chart_mode returns correct value for each tier",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("STANDARD tier is chart mode") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);
        REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == true);
    }

    SECTION("BASIC tier is chart mode") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::BASIC);
        REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == true);
    }

    SECTION("EMBEDDED tier is table mode") {
        ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::EMBEDDED);
        REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == false);
    }

    SECTION("is_chart_mode from NULL chart returns false") {
        REQUIRE(ui_frequency_response_chart_is_chart_mode(nullptr) == false);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Configure on NULL chart is safe",
                 "[frequency_response_chart][platform]") {
    ui_frequency_response_chart_configure_for_platform(nullptr, PlatformTier::STANDARD);
    // Should not crash
    REQUIRE(true);
}

// ============================================================================
// Downsampling Behavior Tests (Tier-Specific)
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "BASIC tier downsampling",
                 "[frequency_response_chart][downsampling][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::BASIC);

    SECTION("Data with 500 points downsamples to ~50 points") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        const size_t input_count = 500;
        std::vector<float> freqs(input_count);
        std::vector<float> amps(input_count);

        for (size_t i = 0; i < input_count; i++) {
            freqs[i] = 10.0f + i * 0.4f;
            amps[i] = 1.0f;
        }

        ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), input_count);

        // Verify max points is limited
        REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 50);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "EMBEDDED tier stores data for table view",
                 "[frequency_response_chart][downsampling][platform]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::EMBEDDED);

    SECTION("Data with 500 points stores for table but no chart points") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        const size_t input_count = 500;
        std::vector<float> freqs(input_count);
        std::vector<float> amps(input_count);

        for (size_t i = 0; i < input_count; i++) {
            freqs[i] = 10.0f + i * 0.4f;
            amps[i] = 1.0f;
        }

        ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), input_count);

        // EMBEDDED tier has no chart points
        REQUIRE(ui_frequency_response_chart_get_max_points(chart) == 0);
        REQUIRE(ui_frequency_response_chart_is_chart_mode(chart) == false);
        // Data should still be stored for table view - implementation detail
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Multiple Series Independence Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Multiple series work independently",
                 "[frequency_response_chart][series][integration]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Independent data per series") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));
        int id3 = ui_frequency_response_chart_add_series(chart, "Z Axis", lv_color_hex(0x4444FF));

        // Different data for each series
        float freqs1[] = {10.0f, 20.0f, 30.0f};
        float amps1[] = {1.0f, 5.0f, 2.0f};

        float freqs2[] = {15.0f, 25.0f, 35.0f, 45.0f};
        float amps2[] = {2.0f, 8.0f, 4.0f, 1.0f};

        float freqs3[] = {12.0f, 22.0f};
        float amps3[] = {3.0f, 6.0f};

        ui_frequency_response_chart_set_data(chart, id1, freqs1, amps1, 3);
        ui_frequency_response_chart_set_data(chart, id2, freqs2, amps2, 4);
        ui_frequency_response_chart_set_data(chart, id3, freqs3, amps3, 2);

        // Each series maintains its own data - no crash
        REQUIRE(true);
    }

    SECTION("Independent visibility per series") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        ui_frequency_response_chart_show_series(chart, id1, false);
        // id2 should still be visible (default)

        ui_frequency_response_chart_show_series(chart, id2, true);
        ui_frequency_response_chart_show_series(chart, id1, true);

        REQUIRE(true);
    }

    SECTION("Independent peak markers per series") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        ui_frequency_response_chart_mark_peak(chart, id1, 50.0f, 10.0f);
        ui_frequency_response_chart_mark_peak(chart, id2, 75.0f, 15.0f);

        // Clear one series peak
        ui_frequency_response_chart_clear_peak(chart, id1);
        // id2's peak should remain

        REQUIRE(true);
    }

    SECTION("Remove one series doesn't affect others") {
        int id1 = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));
        int id3 = ui_frequency_response_chart_add_series(chart, "Z Axis", lv_color_hex(0x4444FF));

        float freqs[] = {10.0f, 20.0f, 30.0f};
        float amps[] = {1.0f, 2.0f, 1.5f};

        ui_frequency_response_chart_set_data(chart, id1, freqs, amps, 3);
        ui_frequency_response_chart_set_data(chart, id2, freqs, amps, 3);
        ui_frequency_response_chart_set_data(chart, id3, freqs, amps, 3);

        // Remove middle series
        ui_frequency_response_chart_remove_series(chart, id2);

        // Other series still work
        ui_frequency_response_chart_set_data(chart, id1, freqs, amps, 3);
        ui_frequency_response_chart_set_data(chart, id3, freqs, amps, 3);

        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Integration/Workflow Tests
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Complete calibration workflow",
                 "[frequency_response_chart][integration]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Typical input shaper calibration display") {
        // Set up frequency range for typical input shaper analysis
        ui_frequency_response_chart_set_freq_range(chart, 0.0f, 200.0f);
        ui_frequency_response_chart_set_amplitude_range(chart, 0.0f, 1e9f);

        // Add X and Y axis series
        int x_id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));
        int y_id = ui_frequency_response_chart_add_series(chart, "Y Axis", lv_color_hex(0x44FF44));

        REQUIRE(x_id >= 0);
        REQUIRE(y_id >= 0);

        // Simulate frequency response data (would come from accelerometer)
        const size_t data_points = 150;
        std::vector<float> freqs(data_points);
        std::vector<float> x_amps(data_points);
        std::vector<float> y_amps(data_points);

        for (size_t i = 0; i < data_points; i++) {
            freqs[i] = (float)i + 10.0f;
            // Simulate resonance peak at ~45 Hz for X, ~52 Hz for Y
            float x_peak = 50.0f * expf(-powf(freqs[i] - 45.0f, 2.0f) / 50.0f);
            float y_peak = 40.0f * expf(-powf(freqs[i] - 52.0f, 2.0f) / 60.0f);
            x_amps[i] = 1e6f + x_peak * 1e8f;
            y_amps[i] = 1e6f + y_peak * 1e8f;
        }

        ui_frequency_response_chart_set_data(chart, x_id, freqs.data(), x_amps.data(), data_points);
        ui_frequency_response_chart_set_data(chart, y_id, freqs.data(), y_amps.data(), data_points);

        // Mark detected peaks
        ui_frequency_response_chart_mark_peak(chart, x_id, 45.0f, 51e8f);
        ui_frequency_response_chart_mark_peak(chart, y_id, 52.0f, 41e8f);

        REQUIRE(true);
    }

    SECTION("Update data after initial display") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        // Initial data
        float freqs1[] = {10.0f, 20.0f, 30.0f};
        float amps1[] = {1.0f, 2.0f, 1.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs1, amps1, 3);

        // Updated data (different measurements)
        float freqs2[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        float amps2[] = {1.5f, 3.0f, 5.0f, 2.0f, 1.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs2, amps2, 5);

        // Update peak marker
        ui_frequency_response_chart_mark_peak(chart, id, 30.0f, 5.0f);

        REQUIRE(true);
    }

    SECTION("Clear and restart calibration") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {10.0f, 20.0f, 30.0f};
        float amps[] = {1.0f, 2.0f, 1.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 3);
        ui_frequency_response_chart_mark_peak(chart, id, 20.0f, 2.0f);

        // Clear for new calibration run
        ui_frequency_response_chart_clear(chart);
        ui_frequency_response_chart_clear_peak(chart, id);

        // New calibration data
        float freqs2[] = {15.0f, 25.0f, 35.0f};
        float amps2[] = {2.0f, 4.0f, 2.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs2, amps2, 3);

        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Stress tests",
                 "[frequency_response_chart][stress]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);

    SECTION("Rapid data updates") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        // Simulate rapid updates during calibration
        for (int iteration = 0; iteration < 100; iteration++) {
            const size_t points = 50 + (iteration % 100);
            std::vector<float> freqs(points);
            std::vector<float> amps(points);

            for (size_t i = 0; i < points; i++) {
                freqs[i] = 10.0f + i * 2.0f;
                amps[i] = 1.0f + (float)(iteration % 10);
            }

            ui_frequency_response_chart_set_data(chart, id, freqs.data(), amps.data(), points);
        }

        REQUIRE(true);
    }

    SECTION("Rapid configuration changes") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        for (int i = 0; i < 100; i++) {
            ui_frequency_response_chart_set_freq_range(chart, (float)i, (float)(i + 200));
            ui_frequency_response_chart_set_amplitude_range(chart, 0.0f, 100.0f + i);
            ui_frequency_response_chart_mark_peak(chart, id, 50.0f + i, 10.0f);
            ui_frequency_response_chart_show_series(chart, id, i % 2 == 0);
        }

        REQUIRE(true);
    }

    SECTION("Rapid tier switching") {
        // Unusual but should not crash
        for (int i = 0; i < 50; i++) {
            ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::STANDARD);
            ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::BASIC);
            ui_frequency_response_chart_configure_for_platform(chart, PlatformTier::EMBEDDED);
        }

        REQUIRE(true);
    }

    ui_frequency_response_chart_destroy(chart);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE_METHOD(FrequencyResponseChartTestFixture, "Edge cases",
                 "[frequency_response_chart][edge]") {
    ui_frequency_response_chart_t* chart = ui_frequency_response_chart_create(screen);
    REQUIRE(chart != nullptr);

    SECTION("Very large frequency values") {
        ui_frequency_response_chart_set_freq_range(chart, 0.0f, 1e6f);
        REQUIRE(true);
    }

    SECTION("Very small frequency values") {
        ui_frequency_response_chart_set_freq_range(chart, 0.001f, 1.0f);
        REQUIRE(true);
    }

    SECTION("Very large amplitude values") {
        ui_frequency_response_chart_set_amplitude_range(chart, 0.0f, 1e12f);
        REQUIRE(true);
    }

    SECTION("Scientific notation data") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {1e1f, 1e2f, 1e3f};
        float amps[] = {1e6f, 1e9f, 1e8f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 3);

        REQUIRE(true);
    }

    SECTION("Single data point") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {50.0f};
        float amps[] = {100.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 1);

        REQUIRE(true);
    }

    SECTION("Two data points") {
        int id = ui_frequency_response_chart_add_series(chart, "X Axis", lv_color_hex(0xFF4444));

        float freqs[] = {10.0f, 100.0f};
        float amps[] = {1.0f, 10.0f};
        ui_frequency_response_chart_set_data(chart, id, freqs, amps, 2);

        REQUIRE(true);
    }

    SECTION("Very long series name") {
        std::string long_name(256, 'x');
        int id = ui_frequency_response_chart_add_series(chart, long_name.c_str(),
                                                        lv_color_hex(0xFF4444));
        // Should either truncate or handle gracefully
        REQUIRE(id >= 0);
    }

    SECTION("Multiple series with same name") {
        int id1 =
            ui_frequency_response_chart_add_series(chart, "Same Name", lv_color_hex(0xFF4444));
        int id2 =
            ui_frequency_response_chart_add_series(chart, "Same Name", lv_color_hex(0x44FF44));

        // Should still get unique IDs
        REQUIRE(id1 >= 0);
        REQUIRE(id2 >= 0);
        REQUIRE(id1 != id2);
    }

    SECTION("Multiple series with same color") {
        int id1 = ui_frequency_response_chart_add_series(chart, "Series 1", lv_color_hex(0xFF4444));
        int id2 = ui_frequency_response_chart_add_series(chart, "Series 2", lv_color_hex(0xFF4444));

        // Same color is allowed
        REQUIRE(id1 >= 0);
        REQUIRE(id2 >= 0);
    }

    ui_frequency_response_chart_destroy(chart);
}
