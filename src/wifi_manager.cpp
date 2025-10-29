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

#include "wifi_manager.h"
#include "safe_log.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>

// ============================================================================
// Constructor / Destructor
// ============================================================================

WiFiManager::WiFiManager()
    : scan_timer_(nullptr)
{
    spdlog::info("[WiFiManager] Initializing with backend system");

    // Create platform-appropriate backend
    backend_ = WifiBackend::create();
    if (!backend_) {
        spdlog::error("[WiFiManager] Failed to create WiFi backend");
        return;
    }

    // Register event callbacks
    backend_->register_event_callback("SCAN_COMPLETE",
        [this](const std::string& data) { handle_scan_complete(data); });
    backend_->register_event_callback("CONNECTED",
        [this](const std::string& data) { handle_connected(data); });
    backend_->register_event_callback("DISCONNECTED",
        [this](const std::string& data) { handle_disconnected(data); });
    backend_->register_event_callback("AUTH_FAILED",
        [this](const std::string& data) { handle_auth_failed(data); });

    // Start backend
    WiFiError start_result = backend_->start();
    if (!start_result.success()) {
        spdlog::error("[WiFiManager] Failed to start WiFi backend: {}", start_result.technical_msg);
        if (!start_result.user_msg.empty()) {
            spdlog::info("[WiFiManager] User message: {}", start_result.user_msg);
        }
        if (!start_result.suggestion.empty()) {
            spdlog::info("[WiFiManager] Suggestion: {}", start_result.suggestion);
        }
    } else {
        spdlog::info("[WiFiManager] WiFi backend started successfully");
    }
}

WiFiManager::~WiFiManager() {
    // Use fprintf - spdlog may be destroyed during static cleanup
    fprintf(stderr, "[WiFiManager] Destructor called\n");

    // Clean up scanning
    stop_scan();

    // Stop backend
    if (backend_) {
        backend_->stop();
    }
}

// ============================================================================
// Network Scanning
// ============================================================================

std::vector<WiFiNetwork> WiFiManager::scan_once() {
    if (!backend_) {
        spdlog::warn("[WiFiManager] No backend available for scan");
        return {};
    }

    spdlog::debug("[WiFiManager] Performing single scan");

    // Trigger scan and wait briefly for results
    WiFiError scan_result = backend_->trigger_scan();
    if (!scan_result.success()) {
        spdlog::warn("[WiFiManager] Failed to trigger scan: {}", scan_result.technical_msg);
        return {};
    }

    // For synchronous scan, we need to get existing results
    // Note: This may not include the just-triggered scan results immediately
    std::vector<WiFiNetwork> networks;
    WiFiError get_result = backend_->get_scan_results(networks);
    if (!get_result.success()) {
        spdlog::warn("[WiFiManager] Failed to get scan results: {}", get_result.technical_msg);
        return {};
    }

    return networks;
}

void WiFiManager::start_scan(std::function<void(const std::vector<WiFiNetwork>&)> on_networks_updated) {
    if (!backend_) {
        spdlog::error("[WiFiManager] No backend available for scanning");
        return;
    }

    scan_callback_ = on_networks_updated;

    // Stop existing timer if running
    stop_scan();

    spdlog::info("[WiFiManager] Starting periodic network scan (every 7 seconds)");

    // Create timer for periodic scanning
    scan_timer_ = lv_timer_create(scan_timer_callback, 7000, this);

    // Trigger immediate scan
    WiFiError scan_result = backend_->trigger_scan();
    if (!scan_result.success()) {
        spdlog::warn("[WiFiManager] Failed to trigger initial scan: {}", scan_result.technical_msg);
    }
}

void WiFiManager::stop_scan() {
    if (scan_timer_) {
        lv_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
        spdlog::info("[WiFiManager] Stopped network scanning");
    }
    scan_callback_ = nullptr;
}

void WiFiManager::scan_timer_callback(lv_timer_t* timer) {
    WiFiManager* manager = static_cast<WiFiManager*>(lv_timer_get_user_data(timer));
    if (manager && manager->backend_) {
        // Trigger scan - results will arrive via SCAN_COMPLETE event
        WiFiError result = manager->backend_->trigger_scan();
        if (!result.success()) {
            spdlog::warn("[WiFiManager] Periodic scan failed: {}", result.technical_msg);
        }
    }
}

// ============================================================================
// Connection Management
// ============================================================================

void WiFiManager::connect(const std::string& ssid,
                         const std::string& password,
                         std::function<void(bool success, const std::string& error)> on_complete) {
    if (!backend_) {
        spdlog::error("[WiFiManager] No backend available for connection");
        if (on_complete) {
            on_complete(false, "No WiFi backend available");
        }
        return;
    }

    spdlog::info("[WiFiManager] Connecting to '{}'", ssid);

    connect_callback_ = on_complete;

    // Use backend's connect method
    WiFiError result = backend_->connect_network(ssid, password);
    if (!result.success()) {
        spdlog::error("[WiFiManager] Backend failed to initiate connection: {}", result.technical_msg);
        if (connect_callback_) {
            connect_callback_(false, result.user_msg.empty() ? result.technical_msg : result.user_msg);
            connect_callback_ = nullptr;
        }
    }
    // Success/failure will be reported via CONNECTED/AUTH_FAILED events
}

void WiFiManager::disconnect() {
    if (!backend_) {
        spdlog::warn("[WiFiManager] No backend available for disconnect");
        return;
    }

    spdlog::info("[WiFiManager] Disconnecting");
    WiFiError result = backend_->disconnect_network();
    if (!result.success()) {
        spdlog::warn("[WiFiManager] Disconnect failed: {}", result.technical_msg);
    }
}

// ============================================================================
// Status Queries
// ============================================================================

bool WiFiManager::is_connected() {
    if (!backend_) return false;

    WifiBackend::ConnectionStatus status = backend_->get_status();
    return status.connected;
}

std::string WiFiManager::get_connected_ssid() {
    if (!backend_) return "";

    WifiBackend::ConnectionStatus status = backend_->get_status();
    return status.ssid;
}

std::string WiFiManager::get_ip_address() {
    if (!backend_) return "";

    WifiBackend::ConnectionStatus status = backend_->get_status();
    return status.ip_address;
}

int WiFiManager::get_signal_strength() {
    if (!backend_) return 0;

    WifiBackend::ConnectionStatus status = backend_->get_status();
    return status.signal_strength;
}

// ============================================================================
// Hardware Detection (Legacy Compatibility)
// ============================================================================

bool WiFiManager::has_hardware() {
    // Backend creation handles hardware availability
    return (backend_ != nullptr);
}

bool WiFiManager::is_enabled() {
    if (!backend_) return false;
    return backend_->is_running();
}

bool WiFiManager::set_enabled(bool enabled) {
    if (!backend_) return false;

    if (enabled) {
        WiFiError result = backend_->start();
        if (!result.success()) {
            spdlog::error("[WiFiManager] Failed to enable WiFi: {}", result.technical_msg);
        }
        return result.success();
    } else {
        backend_->stop();
        return true;
    }
}

bool WiFiManager::has_ethernet() {
#ifdef __APPLE__
    // macOS simulator: always report Ethernet available
    spdlog::debug("[Ethernet] Mock mode: Ethernet detected");
    return true;
#else
    // Linux: Check for Ethernet interfaces (eth*, en*, eno*, ens*)
    DIR* dir = opendir("/sys/class/net");
    if (!dir) {
        spdlog::warn("[Ethernet] Cannot access /sys/class/net");
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;

        std::string iface = entry->d_name;
        // Match common Ethernet interface names
        if (iface.compare(0, 3, "eth") == 0 ||  // eth0, eth1, etc.
            iface.compare(0, 2, "en") == 0 ||   // enp*, eno*, ens*
            iface == "end0") {  // Some systems use end0

            spdlog::info("[Ethernet] Ethernet interface detected: {}", iface);
            closedir(dir);
            return true;
        }
    }
    closedir(dir);
    spdlog::info("[Ethernet] No Ethernet interface detected");
    return false;
#endif
}

std::string WiFiManager::get_ethernet_ip() {
#ifdef __APPLE__
    // macOS simulator: return mock Ethernet IP
    return "192.168.1.150";
#else
    // TODO: Linux implementation
    // For now, return empty (implementation deferred for simplicity)
    spdlog::warn("[Ethernet] IP detection not yet implemented on Linux");
    return "";
#endif
}

// ============================================================================
// Event Handling
// ============================================================================

void WiFiManager::handle_scan_complete(const std::string& event_data) {
    (void)event_data;  // Unused for now

    spdlog::debug("[WiFiManager] Scan complete event received");

    if (scan_callback_) {
        std::vector<WiFiNetwork> networks;
        WiFiError result = backend_->get_scan_results(networks);
        if (result.success()) {
            scan_callback_(networks);
        } else {
            spdlog::warn("[WiFiManager] Failed to get scan results: {}", result.technical_msg);
            // Still call callback with empty results rather than leaving UI hanging
            scan_callback_({});
        }
    }
}

void WiFiManager::handle_connected(const std::string& event_data) {
    (void)event_data;  // Could parse IP address from event data

    spdlog::info("[WiFiManager] Connected event received");

    if (connect_callback_) {
        connect_callback_(true, "");
        connect_callback_ = nullptr;
    }
}

void WiFiManager::handle_disconnected(const std::string& event_data) {
    (void)event_data;  // Could parse reason from event data

    spdlog::info("[WiFiManager] Disconnected event received");

    if (connect_callback_) {
        connect_callback_(false, "Disconnected");
        connect_callback_ = nullptr;
    }
}

void WiFiManager::handle_auth_failed(const std::string& event_data) {
    (void)event_data;  // Could parse specific error from event data

    spdlog::warn("[WiFiManager] Authentication failed event received");

    if (connect_callback_) {
        connect_callback_(false, "Authentication failed");
        connect_callback_ = nullptr;
    }
}
