// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ethernet_backend.h"
#include "ethernet_manager.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// Info Retrieval Tests
// ============================================================================

TEST_CASE("Ethernet Manager: get_info returns valid struct", "[network][info]") {
    EthernetManager manager;

    EthernetInfo info = manager.get_info();

    // Should return a valid info struct
    REQUIRE(!info.status.empty()); // Status should not be empty

    // If connected, should have valid IP
    if (info.connected) {
        REQUIRE(!info.ip_address.empty());
        REQUIRE(!info.interface.empty());
    }
}

// ============================================================================
// IP Address Retrieval Tests
// ============================================================================

TEST_CASE("Ethernet Manager: get_ip_address behavior", "[network][ip]") {
    EthernetManager manager;

    std::string ip = manager.get_ip_address();

    // Should return empty string if not connected, valid IP if connected
    if (ip.empty()) {
        // Not connected - verify get_info also shows not connected
        EthernetInfo info = manager.get_info();
        REQUIRE(info.connected == false);
    } else {
        // Connected - verify IP format
        // Basic check: should contain dots for IPv4
        bool has_dots = (ip.find('.') != std::string::npos);
        bool has_colons = (ip.find(':') != std::string::npos);

        // Should be either IPv4 (dots) or IPv6 (colons)
        REQUIRE((has_dots || has_colons));

        // Verify get_info also shows connected
        EthernetInfo info = manager.get_info();
        REQUIRE(info.connected == true);
        REQUIRE(info.ip_address == ip);
    }
}

// ============================================================================
// Mock Backend Tests (if using mock)
// ============================================================================

#ifdef USE_MOCK_ETHERNET
TEST_CASE("Ethernet Manager: Mock backend returns expected values", "[network][mock]") {
    EthernetManager manager;

    SECTION("Mock has interface") {
        REQUIRE(manager.has_interface() == true);
    }

    SECTION("Mock returns mock IP") {
        EthernetInfo info = manager.get_info();
        REQUIRE(info.connected == true);
        REQUIRE(info.ip_address == "192.168.1.100");
        REQUIRE(info.interface_name == "eth0");
    }

    SECTION("Mock get_ip_address") {
        std::string ip = manager.get_ip_address();
        REQUIRE(ip == "192.168.1.100");
    }
}
#endif

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_CASE("Ethernet Manager: Multiple info queries", "[network][info]") {
    EthernetManager manager;

    // Should handle multiple queries without issues
    EthernetInfo info1 = manager.get_info();
    EthernetInfo info2 = manager.get_info();
    EthernetInfo info3 = manager.get_info();

    // Results should be consistent
    REQUIRE(info1.connected == info2.connected);
    REQUIRE(info2.connected == info3.connected);

    if (info1.connected) {
        REQUIRE(info1.ip_address == info2.ip_address);
        REQUIRE(info2.ip_address == info3.ip_address);
    }
}

TEST_CASE("Ethernet Manager: Repeated interface checks", "[network][interface]") {
    EthernetManager manager;

    bool result1 = manager.has_interface();
    bool result2 = manager.has_interface();
    bool result3 = manager.has_interface();

    // Results should be consistent
    REQUIRE(result1 == result2);
    REQUIRE(result2 == result3);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Ethernet Manager: Interface and info consistency", "[network][integration]") {
    EthernetManager manager;

    bool has_interface = manager.has_interface();
    EthernetInfo info = manager.get_info();

    if (has_interface) {
        // If we have an interface, info should not indicate backend error
        REQUIRE(info.status != "Backend error");
    }
}

TEST_CASE("Ethernet Manager: IP address and info consistency", "[network][integration]") {
    EthernetManager manager;

    std::string ip = manager.get_ip_address();
    EthernetInfo info = manager.get_info();

    // IP from get_ip_address() should match info.ip_address if connected
    if (info.connected) {
        REQUIRE(ip == info.ip_address);
    } else {
        REQUIRE(ip.empty());
    }
}
