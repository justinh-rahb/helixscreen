// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "moonraker_api.h"

#include "moonraker_api_internal.h"
#include "spdlog/spdlog.h"

#include <sstream>

using namespace moonraker_internal;

// ============================================================================
// MoonrakerAPI Implementation
// ============================================================================

MoonrakerAPI::MoonrakerAPI(MoonrakerClient& client, PrinterState& state) : client_(client) {
    // state parameter reserved for future use
    (void)state;
}

MoonrakerAPI::~MoonrakerAPI() {
    // Signal shutdown and wait for all HTTP threads
    shutting_down_.store(true);

    std::list<std::thread> threads_to_join;
    {
        std::lock_guard<std::mutex> lock(http_threads_mutex_);
        threads_to_join = std::move(http_threads_);
    }

    for (auto& t : threads_to_join) {
        if (t.joinable()) {
            t.join();
        }
    }
}

bool MoonrakerAPI::ensure_http_base_url() {
    if (!http_base_url_.empty()) {
        return true;
    }

    // Try to derive from WebSocket URL
    const std::string& ws_url = client_.get_last_url();
    if (!ws_url.empty() && ws_url.find("ws://") == 0) {
        // Convert ws://host:port/websocket -> http://host:port
        std::string host_port = ws_url.substr(5); // Skip "ws://"
        auto slash_pos = host_port.find('/');
        if (slash_pos != std::string::npos) {
            host_port = host_port.substr(0, slash_pos);
        }
        http_base_url_ = "http://" + host_port;
        spdlog::info("[Moonraker API] Auto-derived HTTP base URL from WebSocket: {}",
                     http_base_url_);
        return true;
    }

    spdlog::error("[Moonraker API] HTTP base URL not configured and cannot derive from WebSocket");
    return false;
}

void MoonrakerAPI::launch_http_thread(std::function<void()> func) {
    // Check if we're shutting down - don't spawn new threads
    if (shutting_down_.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(http_threads_mutex_);

    // Clean up any finished threads first (they've set themselves to non-joinable id)
    http_threads_.remove_if([](std::thread& t) { return !t.joinable(); });

    // Launch the new thread
    http_threads_.emplace_back([func = std::move(func)]() {
        func();
        // Thread auto-removed during next launch or destructor
    });
}

// ============================================================================
// G-code Generation Helpers
// ============================================================================

std::string MoonrakerAPI::generate_home_gcode(const std::string& axes) {
    if (axes.empty()) {
        return "G28"; // Home all axes
    } else {
        std::ostringstream gcode;
        gcode << "G28";
        for (char axis : axes) {
            gcode << " " << static_cast<char>(std::toupper(axis));
        }
        return gcode.str();
    }
}

std::string MoonrakerAPI::generate_move_gcode(char axis, double distance, double feedrate) {
    std::ostringstream gcode;
    gcode << "G91\n"; // Relative positioning
    gcode << "G0 " << static_cast<char>(std::toupper(axis)) << distance;
    if (feedrate > 0) {
        gcode << " F" << feedrate;
    }
    gcode << "\nG90"; // Back to absolute positioning
    return gcode.str();
}

std::string MoonrakerAPI::generate_absolute_move_gcode(char axis, double position,
                                                       double feedrate) {
    std::ostringstream gcode;
    gcode << "G90\n"; // Absolute positioning
    gcode << "G0 " << static_cast<char>(std::toupper(axis)) << position;
    if (feedrate > 0) {
        gcode << " F" << feedrate;
    }
    return gcode.str();
}
