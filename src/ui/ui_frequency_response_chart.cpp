// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_frequency_response_chart.cpp
 * @brief Frequency response chart widget implementation
 *
 * Displays frequency domain data from accelerometer measurements during input
 * shaper calibration. Supports multiple data series, peak marking, and automatic
 * hardware adaptation based on platform tier.
 */

#include "ui_frequency_response_chart.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

// Maximum number of series supported
static constexpr int MAX_SERIES = 8;

// Maximum series name length
static constexpr size_t MAX_NAME_LEN = 32;

/**
 * @brief Internal series data structure
 */
struct FrequencySeriesData {
    int id = -1;                            ///< Series ID (-1 = unused slot)
    char name[MAX_NAME_LEN] = {0};          ///< Series name
    lv_color_t color = {};                  ///< Line color
    bool visible = true;                    ///< Visibility state
    lv_chart_series_t* lv_series = nullptr; ///< LVGL chart series (chart mode only)

    // Peak marker data
    bool has_peak = false;
    float peak_freq = 0.0f;
    float peak_amplitude = 0.0f;

    // Stored data (for table mode or re-rendering)
    std::vector<float> frequencies;
    std::vector<float> amplitudes;
};

/**
 * @brief Main chart structure
 */
struct ui_frequency_response_chart_t {
    lv_obj_t* root = nullptr;  ///< Container widget
    lv_obj_t* chart = nullptr; ///< LVGL chart widget (nullptr in table mode)

    helix::PlatformTier tier = helix::PlatformTier::EMBEDDED;
    size_t max_points = 0;
    bool chart_mode = false;

    float freq_min = 0.0f;
    float freq_max = 200.0f;
    float amp_min = 0.0f;
    float amp_max = 1e9f;

    FrequencySeriesData series[MAX_SERIES];
    int next_series_id = 0;
};

// ============================================================================
// Internal helpers
// ============================================================================

/**
 * @brief Find series by ID
 * @return Pointer to series data or nullptr if not found
 */
static FrequencySeriesData* find_series(ui_frequency_response_chart_t* chart, int series_id) {
    if (!chart || series_id < 0) {
        return nullptr;
    }

    for (int i = 0; i < MAX_SERIES; i++) {
        if (chart->series[i].id == series_id) {
            return &chart->series[i];
        }
    }
    return nullptr;
}

/**
 * @brief Find first available series slot
 * @return Slot index or -1 if none available
 */
static int find_empty_slot(ui_frequency_response_chart_t* chart) {
    if (!chart) {
        return -1;
    }

    for (int i = 0; i < MAX_SERIES; i++) {
        if (chart->series[i].id == -1) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Downsample data arrays to fit within max_points
 *
 * Uses simple linear interpolation, preserving first and last points
 * to maintain frequency range endpoints.
 */
static void downsample_data(const float* src_freqs, const float* src_amps, size_t src_count,
                            std::vector<float>& dst_freqs, std::vector<float>& dst_amps,
                            size_t max_points) {
    if (src_count == 0 || max_points == 0) {
        dst_freqs.clear();
        dst_amps.clear();
        return;
    }

    if (src_count <= max_points) {
        // No downsampling needed
        dst_freqs.assign(src_freqs, src_freqs + src_count);
        dst_amps.assign(src_amps, src_amps + src_count);
        return;
    }

    // Handle edge case: single output point
    if (max_points == 1) {
        dst_freqs.resize(1);
        dst_amps.resize(1);
        // Take last point to preserve frequency range endpoint
        dst_freqs[0] = src_freqs[src_count - 1];
        dst_amps[0] = src_amps[src_count - 1];
        return;
    }

    // Downsample by selecting evenly spaced points
    dst_freqs.resize(max_points);
    dst_amps.resize(max_points);

    for (size_t i = 0; i < max_points; i++) {
        // Map output index to input index
        size_t src_idx;
        if (i == max_points - 1) {
            // Ensure last point is exactly the last source point
            src_idx = src_count - 1;
        } else {
            src_idx = (i * (src_count - 1)) / (max_points - 1);
        }

        dst_freqs[i] = src_freqs[src_idx];
        dst_amps[i] = src_amps[src_idx];
    }
}

/**
 * @brief Update LVGL chart with series data
 */
static void update_chart_series(ui_frequency_response_chart_t* chart, FrequencySeriesData* series) {
    if (!chart || !series || !chart->chart || !series->lv_series) {
        return;
    }

    // Clear existing data
    lv_chart_set_all_values(chart->chart, series->lv_series, LV_CHART_POINT_NONE);

    // Add new points
    size_t count = series->frequencies.size();
    for (size_t i = 0; i < count; i++) {
        // Scale amplitude to chart range (LVGL chart uses int32_t)
        float amp = series->amplitudes[i];
        float amp_range = chart->amp_max - chart->amp_min;
        int32_t scaled;
        if (amp_range > 0) {
            scaled = static_cast<int32_t>(((amp - chart->amp_min) / amp_range) * 1000.0f);
        } else {
            scaled = 0;
        }
        lv_chart_set_next_value(chart->chart, series->lv_series, scaled);
    }

    lv_chart_refresh(chart->chart);
}

// ============================================================================
// Creation/Destruction
// ============================================================================

ui_frequency_response_chart_t* ui_frequency_response_chart_create(lv_obj_t* parent) {
    if (!parent) {
        spdlog::error("[FreqChart] NULL parent");
        return nullptr;
    }

    auto chart_ptr = std::make_unique<ui_frequency_response_chart_t>();
    if (!chart_ptr) {
        spdlog::error("[FreqChart] Failed to allocate chart structure");
        return nullptr;
    }

    ui_frequency_response_chart_t* chart = chart_ptr.get();

    // Initialize all series slots to unused
    for (int i = 0; i < MAX_SERIES; i++) {
        chart->series[i].id = -1;
    }

    // Create root container
    chart->root = lv_obj_create(parent);
    if (!chart->root) {
        spdlog::error("[FreqChart] Failed to create root container");
        return nullptr;
    }

    // Basic styling for container
    lv_obj_set_size(chart->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(chart->root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart->root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart->root, LV_OPA_TRANSP, LV_PART_MAIN);

    // Store pointer in user data for retrieval
    lv_obj_set_user_data(chart->root, chart);

    // Default to EMBEDDED tier (configure_for_platform will set up properly)
    chart->tier = helix::PlatformTier::EMBEDDED;
    chart->max_points = 0;
    chart->chart_mode = false;
    chart->chart = nullptr;

    spdlog::debug("[FreqChart] Created frequency response chart");

    return chart_ptr.release();
}

void ui_frequency_response_chart_destroy(ui_frequency_response_chart_t* chart) {
    if (!chart) {
        return;
    }

    // Transfer to RAII wrapper for automatic cleanup
    std::unique_ptr<ui_frequency_response_chart_t> chart_ptr(chart);

    // Remove all series
    for (int i = 0; i < MAX_SERIES; i++) {
        if (chart_ptr->series[i].id != -1 && chart_ptr->series[i].lv_series && chart_ptr->chart) {
            lv_chart_remove_series(chart_ptr->chart, chart_ptr->series[i].lv_series);
        }
        chart_ptr->series[i].frequencies.clear();
        chart_ptr->series[i].amplitudes.clear();
    }

    // Delete root widget (also deletes chart if present)
    if (chart_ptr->root) {
        lv_obj_del(chart_ptr->root);
    }

    spdlog::debug("[FreqChart] Destroyed");
}

// ============================================================================
// Series Management
// ============================================================================

int ui_frequency_response_chart_add_series(ui_frequency_response_chart_t* chart, const char* name,
                                           lv_color_t color) {
    if (!chart) {
        spdlog::error("[FreqChart] NULL chart");
        return -1;
    }

    if (!name) {
        spdlog::error("[FreqChart] NULL name");
        return -1;
    }

    int slot = find_empty_slot(chart);
    if (slot == -1) {
        spdlog::error("[FreqChart] No available series slots");
        return -1;
    }

    FrequencySeriesData* series = &chart->series[slot];
    series->id = chart->next_series_id++;
    series->color = color;
    series->visible = true;
    series->has_peak = false;
    series->lv_series = nullptr;
    series->frequencies.clear();
    series->amplitudes.clear();

    // Copy name with truncation
    strncpy(series->name, name, MAX_NAME_LEN - 1);
    series->name[MAX_NAME_LEN - 1] = '\0';

    // Create LVGL series if in chart mode
    if (chart->chart_mode && chart->chart) {
        series->lv_series = lv_chart_add_series(chart->chart, color, LV_CHART_AXIS_PRIMARY_Y);
        if (series->lv_series) {
            lv_chart_set_all_values(chart->chart, series->lv_series, LV_CHART_POINT_NONE);
        }
    }

    spdlog::debug("[FreqChart] Added series {} '{}' (slot {}, color 0x{:06X})", series->id,
                  series->name, slot, lv_color_to_u32(color) & 0xFFFFFF);

    return series->id;
}

void ui_frequency_response_chart_remove_series(ui_frequency_response_chart_t* chart,
                                               int series_id) {
    if (!chart) {
        return;
    }

    FrequencySeriesData* series = find_series(chart, series_id);
    if (!series) {
        return;
    }

    // Remove LVGL series if present
    if (series->lv_series && chart->chart) {
        lv_chart_remove_series(chart->chart, series->lv_series);
    }

    // Clear data
    series->frequencies.clear();
    series->amplitudes.clear();

    // Mark slot as unused
    series->id = -1;
    series->lv_series = nullptr;

    spdlog::debug("[FreqChart] Removed series {}", series_id);
}

void ui_frequency_response_chart_show_series(ui_frequency_response_chart_t* chart, int series_id,
                                             bool visible) {
    if (!chart) {
        return;
    }

    FrequencySeriesData* series = find_series(chart, series_id);
    if (!series) {
        return;
    }

    series->visible = visible;

    // Update LVGL series visibility if in chart mode
    if (chart->chart && series->lv_series) {
        lv_chart_hide_series(chart->chart, series->lv_series, !visible);
        lv_obj_invalidate(chart->chart);
    }

    spdlog::debug("[FreqChart] Series {} visibility: {}", series_id, visible);
}

// ============================================================================
// Data Management
// ============================================================================

void ui_frequency_response_chart_set_data(ui_frequency_response_chart_t* chart, int series_id,
                                          const float* frequencies, const float* amplitudes,
                                          size_t count) {
    if (!chart) {
        return;
    }

    if (!frequencies || !amplitudes || count == 0) {
        return;
    }

    FrequencySeriesData* series = find_series(chart, series_id);
    if (!series) {
        return;
    }

    // Store data (possibly downsampled) for chart rendering
    if (chart->max_points > 0 && count > chart->max_points) {
        downsample_data(frequencies, amplitudes, count, series->frequencies, series->amplitudes,
                        chart->max_points);
        spdlog::debug("[FreqChart] Downsampled series {} from {} to {} points", series_id, count,
                      series->frequencies.size());
    } else if (chart->max_points > 0) {
        series->frequencies.assign(frequencies, frequencies + count);
        series->amplitudes.assign(amplitudes, amplitudes + count);
    } else {
        // EMBEDDED mode (max_points = 0): store all data for table view
        series->frequencies.assign(frequencies, frequencies + count);
        series->amplitudes.assign(amplitudes, amplitudes + count);
    }

    // Update LVGL chart if in chart mode
    if (chart->chart_mode && chart->chart && series->lv_series) {
        update_chart_series(chart, series);
    }

    spdlog::debug("[FreqChart] Set data for series {}: {} points", series_id, count);
}

void ui_frequency_response_chart_clear(ui_frequency_response_chart_t* chart) {
    if (!chart) {
        return;
    }

    for (int i = 0; i < MAX_SERIES; i++) {
        if (chart->series[i].id != -1) {
            chart->series[i].frequencies.clear();
            chart->series[i].amplitudes.clear();

            if (chart->chart && chart->series[i].lv_series) {
                lv_chart_set_all_values(chart->chart, chart->series[i].lv_series,
                                        LV_CHART_POINT_NONE);
            }
        }
    }

    if (chart->chart) {
        lv_chart_refresh(chart->chart);
    }

    spdlog::debug("[FreqChart] Cleared all data");
}

// ============================================================================
// Peak Marking
// ============================================================================

void ui_frequency_response_chart_mark_peak(ui_frequency_response_chart_t* chart, int series_id,
                                           float peak_freq, float peak_amplitude) {
    if (!chart) {
        return;
    }

    FrequencySeriesData* series = find_series(chart, series_id);
    if (!series) {
        return;
    }

    series->has_peak = true;
    series->peak_freq = peak_freq;
    series->peak_amplitude = peak_amplitude;

    // Invalidate chart to redraw with peak marker
    if (chart->chart) {
        lv_obj_invalidate(chart->chart);
    }

    spdlog::debug("[FreqChart] Marked peak for series {}: {:.1f} Hz @ {:.2e}", series_id, peak_freq,
                  peak_amplitude);
}

void ui_frequency_response_chart_clear_peak(ui_frequency_response_chart_t* chart, int series_id) {
    if (!chart) {
        return;
    }

    FrequencySeriesData* series = find_series(chart, series_id);
    if (!series) {
        return;
    }

    series->has_peak = false;

    if (chart->chart) {
        lv_obj_invalidate(chart->chart);
    }

    spdlog::debug("[FreqChart] Cleared peak for series {}", series_id);
}

// ============================================================================
// Configuration
// ============================================================================

void ui_frequency_response_chart_set_freq_range(ui_frequency_response_chart_t* chart, float min,
                                                float max) {
    if (!chart) {
        return;
    }

    chart->freq_min = min;
    chart->freq_max = max;

    // Update chart X-axis range if in chart mode
    // Note: LVGL charts don't have a direct X-axis range for line charts,
    // but we store it for potential custom drawing/labels

    spdlog::debug("[FreqChart] Frequency range: {:.1f} - {:.1f} Hz", min, max);
}

void ui_frequency_response_chart_set_amplitude_range(ui_frequency_response_chart_t* chart,
                                                     float min, float max) {
    if (!chart) {
        return;
    }

    chart->amp_min = min;
    chart->amp_max = max;

    // Update chart Y-axis range if in chart mode
    if (chart->chart) {
        // Scale to internal range (0-1000) for LVGL chart
        lv_chart_set_axis_range(chart->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    }

    spdlog::debug("[FreqChart] Amplitude range: {:.2e} - {:.2e}", min, max);
}

lv_obj_t* ui_frequency_response_chart_get_obj(ui_frequency_response_chart_t* chart) {
    return chart ? chart->root : nullptr;
}

// ============================================================================
// Hardware Adaptation
// ============================================================================

void ui_frequency_response_chart_configure_for_platform(ui_frequency_response_chart_t* chart,
                                                        helix::PlatformTier tier) {
    if (!chart) {
        return;
    }

    chart->tier = tier;

    // Determine capabilities based on tier
    switch (tier) {
    case helix::PlatformTier::STANDARD:
        chart->max_points = helix::PlatformCapabilities::STANDARD_CHART_POINTS;
        chart->chart_mode = true;
        break;
    case helix::PlatformTier::BASIC:
        chart->max_points = helix::PlatformCapabilities::BASIC_CHART_POINTS;
        chart->chart_mode = true;
        break;
    case helix::PlatformTier::EMBEDDED:
    default:
        chart->max_points = 0;
        chart->chart_mode = false;
        break;
    }

    // Create or destroy LVGL chart widget based on mode
    if (chart->chart_mode && !chart->chart) {
        // Create LVGL chart
        chart->chart = lv_chart_create(chart->root);
        if (chart->chart) {
            lv_obj_set_size(chart->chart, LV_PCT(100), LV_PCT(100));
            lv_chart_set_type(chart->chart, LV_CHART_TYPE_LINE);
            lv_chart_set_update_mode(chart->chart, LV_CHART_UPDATE_MODE_SHIFT);
            lv_chart_set_point_count(chart->chart, static_cast<uint32_t>(chart->max_points));
            lv_chart_set_axis_range(chart->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);

            // Style
            lv_obj_set_style_bg_opa(chart->chart, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(chart->chart, 0, LV_PART_MAIN);
            lv_obj_set_style_line_width(chart->chart, 2, LV_PART_ITEMS);
            lv_obj_set_style_width(chart->chart, 0, LV_PART_INDICATOR);
            lv_obj_set_style_height(chart->chart, 0, LV_PART_INDICATOR);

            // Create LVGL series for existing series data
            for (int i = 0; i < MAX_SERIES; i++) {
                if (chart->series[i].id != -1 && !chart->series[i].lv_series) {
                    chart->series[i].lv_series = lv_chart_add_series(
                        chart->chart, chart->series[i].color, LV_CHART_AXIS_PRIMARY_Y);
                    if (chart->series[i].lv_series) {
                        lv_chart_set_all_values(chart->chart, chart->series[i].lv_series,
                                                LV_CHART_POINT_NONE);
                        if (!chart->series[i].frequencies.empty()) {
                            update_chart_series(chart, &chart->series[i]);
                        }
                    }
                }
            }
        }
    } else if (!chart->chart_mode && chart->chart) {
        // Remove existing chart series
        for (int i = 0; i < MAX_SERIES; i++) {
            if (chart->series[i].lv_series) {
                lv_chart_remove_series(chart->chart, chart->series[i].lv_series);
                chart->series[i].lv_series = nullptr;
            }
        }
        // Delete chart widget
        lv_obj_del(chart->chart);
        chart->chart = nullptr;
    } else if (chart->chart_mode && chart->chart) {
        // Update point count if chart exists and mode is chart
        lv_chart_set_point_count(chart->chart, static_cast<uint32_t>(chart->max_points));
    }

    spdlog::debug("[FreqChart] Configured for {} tier: max_points={}, chart_mode={}",
                  helix::platform_tier_to_string(tier), chart->max_points, chart->chart_mode);
}

size_t ui_frequency_response_chart_get_max_points(ui_frequency_response_chart_t* chart) {
    return chart ? chart->max_points : 0;
}

bool ui_frequency_response_chart_is_chart_mode(ui_frequency_response_chart_t* chart) {
    return chart ? chart->chart_mode : false;
}
