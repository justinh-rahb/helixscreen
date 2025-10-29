/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#include "wifi_backend_mock.h"
#include "safe_log.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <chrono>

WifiBackendMock::WifiBackendMock()
    : running_(false)
    , connected_(false)
    , connected_signal_(0)
    , scan_timer_(nullptr)
    , connect_timer_(nullptr)
    , rng_(std::chrono::steady_clock::now().time_since_epoch().count())
{
    spdlog::debug("[WifiBackend] Mock backend initialized");
    init_mock_networks();
}

WifiBackendMock::~WifiBackendMock() {
    stop();
    // Use fprintf - spdlog may be destroyed during static cleanup
    fprintf(stderr, "[WifiBackend] Mock backend destroyed\n");
}

// ============================================================================
// Lifecycle Management
// ============================================================================

WiFiError WifiBackendMock::start() {
    if (running_) {
        spdlog::debug("[WifiBackend] Mock backend already running");
        return WiFiErrorHelper::success();
    }

    running_ = true;
    spdlog::info("[WifiBackend] Mock backend started (simulator mode)");
    return WiFiErrorHelper::success();
}

void WifiBackendMock::stop() {
    if (!running_) return;

    // Clean up timers
    if (scan_timer_) {
        lv_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }
    if (connect_timer_) {
        lv_timer_delete(connect_timer_);
        connect_timer_ = nullptr;
    }

    running_ = false;
    connected_ = false;
    connected_ssid_.clear();
    connected_ip_.clear();

    // Use fprintf - spdlog may be destroyed during static cleanup
    fprintf(stderr, "[WifiBackend] Mock backend stopped\n");
}

bool WifiBackendMock::is_running() const {
    return running_;
}

// ============================================================================
// Event System
// ============================================================================

void WifiBackendMock::register_event_callback(const std::string& name,
                                              std::function<void(const std::string&)> callback) {
    callbacks_[name] = callback;
    spdlog::debug("[WifiBackend] Mock: Registered callback for '{}'", name);
}

void WifiBackendMock::fire_event(const std::string& event_name, const std::string& data) {
    auto it = callbacks_.find(event_name);
    if (it != callbacks_.end()) {
        spdlog::debug("[WifiBackend] Mock: Firing event '{}'", event_name);
        it->second(data);
    }
}

// ============================================================================
// Network Scanning
// ============================================================================

WiFiError WifiBackendMock::trigger_scan() {
    if (!running_) {
        spdlog::warn("[WifiBackend] Mock: trigger_scan called but not running");
        return WiFiError(WiFiResult::NOT_INITIALIZED,
                        "Mock backend not running",
                        "WiFi scanner not ready",
                        "Initialize the WiFi system first");
    }

    spdlog::debug("[WifiBackend] Mock: Triggering network scan");

    // Clean up existing scan timer
    if (scan_timer_) {
        lv_timer_delete(scan_timer_);
    }

    // Simulate scan delay (2 seconds)
    scan_timer_ = lv_timer_create(scan_timer_callback, 2000, this);
    lv_timer_set_repeat_count(scan_timer_, 1);  // One-shot

    return WiFiErrorHelper::success();
}

WiFiError WifiBackendMock::get_scan_results(std::vector<WiFiNetwork>& networks) {
    if (!running_) {
        networks.clear();
        return WiFiError(WiFiResult::NOT_INITIALIZED,
                        "Mock backend not running",
                        "WiFi scanner not ready",
                        "Initialize the WiFi system first");
    }

    // Add some realism - vary signal strengths slightly
    vary_signal_strengths();

    // Sort by signal strength (strongest first)
    networks = mock_networks_;
    std::sort(networks.begin(), networks.end(),
              [](const WiFiNetwork& a, const WiFiNetwork& b) {
                  return a.signal_strength > b.signal_strength;
              });

    spdlog::debug("[WifiBackend] Mock: Returning {} scan results", networks.size());
    return WiFiErrorHelper::success();
}

void WifiBackendMock::scan_timer_callback(lv_timer_t* timer) {
    WifiBackendMock* backend = static_cast<WifiBackendMock*>(lv_timer_get_user_data(timer));
    backend->scan_timer_ = nullptr;  // Timer auto-deleted for one-shot

    spdlog::debug("[WifiBackend] Mock: Scan completed");
    backend->fire_event("SCAN_COMPLETE");
}

// ============================================================================
// Connection Management
// ============================================================================

WiFiError WifiBackendMock::connect_network(const std::string& ssid, const std::string& password) {
    if (!running_) {
        spdlog::warn("[WifiBackend] Mock: connect_network called but not running");
        return WiFiError(WiFiResult::NOT_INITIALIZED,
                        "Mock backend not running",
                        "WiFi system not ready",
                        "Initialize the WiFi system first");
    }

    // Check if network exists in our mock list
    auto it = std::find_if(mock_networks_.begin(), mock_networks_.end(),
                          [&ssid](const WiFiNetwork& net) { return net.ssid == ssid; });

    if (it == mock_networks_.end()) {
        spdlog::warn("[WifiBackend] Mock: Network '{}' not found in scan results", ssid);
        return WiFiErrorHelper::network_not_found(ssid);
    }

    // Validate password for secured networks
    if (it->is_secured && password.empty()) {
        spdlog::warn("[WifiBackend] Mock: No password provided for secured network '{}'", ssid);
        return WiFiError(WiFiResult::INVALID_PARAMETERS,
                        "Password required for secured network: " + ssid,
                        "This network requires a password",
                        "Enter the network password and try again");
    }

    spdlog::info("[WifiBackend] Mock: Connecting to '{}'...", ssid);

    connecting_ssid_ = ssid;
    connecting_password_ = password;

    // Clean up existing connect timer
    if (connect_timer_) {
        lv_timer_delete(connect_timer_);
    }

    // Simulate connection delay (2-3 seconds)
    int delay_ms = 2000 + (rng_() % 1000);  // 2-3 seconds
    connect_timer_ = lv_timer_create(connect_timer_callback, delay_ms, this);
    lv_timer_set_repeat_count(connect_timer_, 1);  // One-shot

    return WiFiErrorHelper::success();
}

WiFiError WifiBackendMock::disconnect_network() {
    if (!connected_) {
        spdlog::debug("[WifiBackend] Mock: disconnect_network called but not connected");
        return WiFiErrorHelper::success();  // Not an error - idempotent operation
    }

    spdlog::info("[WifiBackend] Mock: Disconnecting from '{}'", connected_ssid_);

    connected_ = false;
    std::string old_ssid = connected_ssid_;
    connected_ssid_.clear();
    connected_ip_.clear();
    connected_signal_ = 0;

    fire_event("DISCONNECTED", "reason=user_request");
    return WiFiErrorHelper::success();
}

void WifiBackendMock::connect_timer_callback(lv_timer_t* timer) {
    WifiBackendMock* backend = static_cast<WifiBackendMock*>(lv_timer_get_user_data(timer));
    backend->connect_timer_ = nullptr;  // Timer auto-deleted for one-shot

    // Find the network we're trying to connect to
    auto it = std::find_if(backend->mock_networks_.begin(), backend->mock_networks_.end(),
                          [&](const WiFiNetwork& net) {
                              return net.ssid == backend->connecting_ssid_;
                          });

    if (it == backend->mock_networks_.end()) {
        spdlog::error("[WifiBackend] Mock: Network '{}' disappeared during connection",
                     backend->connecting_ssid_);
        backend->fire_event("DISCONNECTED", "reason=network_not_found");
        return;
    }

    // Simulate authentication failure for secured networks with wrong password
    if (it->is_secured && backend->connecting_password_.empty()) {
        spdlog::info("[WifiBackend] Mock: Auth failed - no password for secured network");
        backend->fire_event("AUTH_FAILED", "reason=no_password");
        return;
    }

    // Simulate occasional auth failures (5% chance for secured networks)
    if (it->is_secured && (backend->rng_() % 100) < 5) {
        spdlog::info("[WifiBackend] Mock: Auth failed - simulated wrong password");
        backend->fire_event("AUTH_FAILED", "reason=wrong_password");
        return;
    }

    // Connection successful!
    backend->connected_ = true;
    backend->connected_ssid_ = backend->connecting_ssid_;
    backend->connected_signal_ = it->signal_strength;

    // Generate mock IP address
    int subnet = 100 + (backend->rng_() % 155);  // 192.168.1.100-255
    backend->connected_ip_ = "192.168.1." + std::to_string(subnet);

    spdlog::info("[WifiBackend] Mock: Connected to '{}', IP: {}",
                backend->connected_ssid_, backend->connected_ip_);

    backend->fire_event("CONNECTED", "ip=" + backend->connected_ip_);
}

// ============================================================================
// Status Queries
// ============================================================================

WifiBackend::ConnectionStatus WifiBackendMock::get_status() {
    ConnectionStatus status = {};
    status.connected = connected_;
    status.ssid = connected_ssid_;
    status.ip_address = connected_ip_;
    status.signal_strength = connected_signal_;

    // Generate mock BSSID
    if (connected_) {
        status.bssid = "aa:bb:cc:dd:ee:ff";  // Mock MAC address
    }

    return status;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void WifiBackendMock::init_mock_networks() {
    mock_networks_ = {
        WiFiNetwork("HomeNetwork-5G", 92, true, "WPA2"),      // Strong, encrypted
        WiFiNetwork("Office-Main", 78, true, "WPA2"),         // Strong, encrypted
        WiFiNetwork("Printers-WiFi", 85, true, "WPA2"),       // Strong, encrypted
        WiFiNetwork("CoffeeShop_Free", 68, false, "Open"),    // Medium, open
        WiFiNetwork("IoT-Devices", 55, true, "WPA"),          // Medium, encrypted
        WiFiNetwork("Guest-Access", 48, false, "Open"),       // Medium, open
        WiFiNetwork("Neighbor-Network", 38, true, "WPA3"),    // Weak, encrypted
        WiFiNetwork("Public-Hotspot", 25, false, "Open"),     // Weak, open
        WiFiNetwork("SmartHome-Net", 32, true, "WPA3"),       // Weak, encrypted
        WiFiNetwork("Distant-Router", 18, true, "WPA2")       // Weak, encrypted
    };

    spdlog::debug("[WifiBackend] Mock: Initialized {} mock networks", mock_networks_.size());
}

void WifiBackendMock::vary_signal_strengths() {
    for (auto& network : mock_networks_) {
        // Vary signal strength by ±5% for realism
        int original = network.signal_strength;
        int variation = (rng_() % 11) - 5;  // -5 to +5
        network.signal_strength = std::max(0, std::min(100, original + variation));
    }
}