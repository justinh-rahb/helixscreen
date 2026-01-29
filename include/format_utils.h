// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>

namespace helix::fmt {

/**
 * @brief Format duration in seconds to human-readable string
 *
 * Produces output like:
 * - "30s" for durations under 1 minute
 * - "45m" for durations under 1 hour (no seconds shown)
 * - "2h" for exact hours
 * - "2h 15m" for hours with minutes
 *
 * @param total_seconds Duration in seconds (negative values treated as 0)
 * @return Formatted string
 */
std::string duration(int total_seconds);

/**
 * @brief Format duration with "remaining" suffix for countdowns
 *
 * Produces output like:
 * - "45 min left" for durations under 1 hour
 * - "1:30 left" for durations 1 hour or more (HH:MM format)
 *
 * @param total_seconds Duration in seconds (negative values treated as 0)
 * @return Formatted string with " left" suffix
 */
std::string duration_remaining(int total_seconds);

/**
 * @brief Format print time estimate from minutes
 *
 * Produces output like:
 * - "45 min" for durations under 1 hour
 * - "2h" for exact hours
 * - "2h 15m" for hours with minutes
 *
 * @param total_minutes Duration in minutes (negative values treated as 0)
 * @return Formatted string
 */
std::string duration_from_minutes(int total_minutes);

/**
 * @brief Format duration to a fixed-size buffer (for legacy code)
 *
 * Same output format as duration() but writes to a provided buffer.
 * Useful for code that needs to avoid allocations or use C-style buffers.
 *
 * @param buf Output buffer
 * @param buf_size Size of output buffer (recommended minimum: 16)
 * @param total_seconds Duration in seconds
 * @return Number of characters written (excluding null terminator), or 0 on error
 */
size_t duration_to_buffer(char* buf, size_t buf_size, int total_seconds);

/**
 * @brief Format duration with zero-padded minutes (for progress displays)
 *
 * Produces output like:
 * - "45m" for durations under 1 hour
 * - "2h 05m" for durations 1 hour or more (minutes always 2 digits)
 *
 * @param total_seconds Duration in seconds (negative values treated as 0)
 * @return Formatted string
 */
std::string duration_padded(int total_seconds);

/**
 * @brief Result of formatting a heater display
 *
 * Contains all the information needed to display a heater status:
 * - temp: formatted temperature string (e.g., "150°C" or "150 / 200°C")
 * - status: semantic status ("Off", "Heating...", or "Ready")
 * - pct: percentage towards target (0-100, clamped)
 */
struct HeaterDisplayResult {
    std::string temp;
    std::string status;
    int pct;
};

/**
 * @brief Format heater display information from centi-degree values
 *
 * Takes current and target temperatures in centi-degrees (100 = 1°C) and
 * produces a consistent display result used across all heater displays.
 *
 * Status logic:
 * - target <= 0: "Off"
 * - pct >= 98: "Ready"
 * - else: "Heating..."
 *
 * @param current_centi Current temperature in centi-degrees
 * @param target_centi Target temperature in centi-degrees (0 = off)
 * @return HeaterDisplayResult with formatted temp, status, and percentage
 */
HeaterDisplayResult heater_display(int current_centi, int target_centi);

} // namespace helix::fmt
