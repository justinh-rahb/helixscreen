// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "format_utils.h"

#include <algorithm>
#include <cstdio>

namespace helix::fmt {

std::string duration(int total_seconds) {
    // Handle negative or zero
    if (total_seconds <= 0) {
        return "0s";
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    char buf[32];

    if (hours == 0 && minutes == 0) {
        // Under 1 minute: show seconds
        std::snprintf(buf, sizeof(buf), "%ds", seconds);
    } else if (hours == 0) {
        // Under 1 hour: show minutes only
        std::snprintf(buf, sizeof(buf), "%dm", minutes);
    } else if (minutes == 0) {
        // Exact hours
        std::snprintf(buf, sizeof(buf), "%dh", hours);
    } else {
        // Hours and minutes
        std::snprintf(buf, sizeof(buf), "%dh %dm", hours, minutes);
    }

    return std::string(buf);
}

std::string duration_remaining(int total_seconds) {
    // Handle negative or zero
    if (total_seconds <= 0) {
        return "0 min left";
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;

    char buf[32];

    if (hours > 0) {
        // Show as H:MM format for longer durations
        std::snprintf(buf, sizeof(buf), "%d:%02d left", hours, minutes);
    } else {
        // Show as "X min left" for shorter durations
        std::snprintf(buf, sizeof(buf), "%d min left", minutes > 0 ? minutes : 1);
    }

    return std::string(buf);
}

std::string duration_from_minutes(int total_minutes) {
    // Handle negative or zero
    if (total_minutes <= 0) {
        return "0 min";
    }

    int hours = total_minutes / 60;
    int minutes = total_minutes % 60;

    char buf[32];

    if (hours == 0) {
        // Under 1 hour: show minutes
        std::snprintf(buf, sizeof(buf), "%d min", minutes);
    } else if (minutes == 0) {
        // Exact hours
        std::snprintf(buf, sizeof(buf), "%dh", hours);
    } else {
        // Hours and minutes
        std::snprintf(buf, sizeof(buf), "%dh %dm", hours, minutes);
    }

    return std::string(buf);
}

size_t duration_to_buffer(char* buf, size_t buf_size, int total_seconds) {
    if (buf == nullptr || buf_size == 0) {
        return 0;
    }

    // Handle negative or zero
    if (total_seconds <= 0) {
        return static_cast<size_t>(std::snprintf(buf, buf_size, "0s"));
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    int written = 0;

    if (hours == 0 && minutes == 0) {
        // Under 1 minute: show seconds
        written = std::snprintf(buf, buf_size, "%ds", seconds);
    } else if (hours == 0) {
        // Under 1 hour: show minutes only
        written = std::snprintf(buf, buf_size, "%dm", minutes);
    } else if (minutes == 0) {
        // Exact hours
        written = std::snprintf(buf, buf_size, "%dh", hours);
    } else {
        // Hours and minutes
        written = std::snprintf(buf, buf_size, "%dh %dm", hours, minutes);
    }

    return written > 0 ? static_cast<size_t>(written) : 0;
}

std::string duration_padded(int total_seconds) {
    // Handle negative or zero
    if (total_seconds <= 0) {
        return "0m";
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;

    char buf[32];

    if (hours == 0) {
        // Under 1 hour: show minutes only
        std::snprintf(buf, sizeof(buf), "%dm", minutes);
    } else {
        // Hours and zero-padded minutes
        std::snprintf(buf, sizeof(buf), "%dh %02dm", hours, minutes);
    }

    return std::string(buf);
}

HeaterDisplayResult heater_display(int current_centi, int target_centi) {
    HeaterDisplayResult result;

    // Convert centi-degrees to degrees (integer division is fine for display)
    int current_deg = current_centi / 100;
    int target_deg = target_centi / 100;

    // Format temperature string
    char buf[32];
    if (target_centi > 0) {
        std::snprintf(buf, sizeof(buf), "%d / %d°C", current_deg, target_deg);
    } else {
        std::snprintf(buf, sizeof(buf), "%d°C", current_deg);
    }
    result.temp = buf;

    // Calculate percentage (clamped to 0-100)
    if (target_centi <= 0) {
        result.pct = 0;
    } else {
        int pct = (current_centi * 100) / target_centi;
        result.pct = std::clamp(pct, 0, 100);
    }

    // Determine status
    if (target_centi <= 0) {
        result.status = "Off";
    } else if (result.pct >= 98) {
        result.status = "Ready";
    } else {
        result.status = "Heating...";
    }

    return result;
}

} // namespace helix::fmt
