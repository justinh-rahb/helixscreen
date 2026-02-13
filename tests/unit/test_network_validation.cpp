// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
#include "utils/network_validation.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// IP Address Validation Tests (via shared utils)
// ============================================================================

TEST_CASE("Network validation: Valid IPv4 addresses", "[validation][network][ip]") {
    REQUIRE(is_valid_ip_or_hostname("192.168.1.1") == true);
    REQUIRE(is_valid_ip_or_hostname("10.0.0.1") == true);
    REQUIRE(is_valid_ip_or_hostname("172.16.0.1") == true);
    REQUIRE(is_valid_ip_or_hostname("127.0.0.1") == true);
    REQUIRE(is_valid_ip_or_hostname("255.255.255.255") == true);
    REQUIRE(is_valid_ip_or_hostname("0.0.0.0") == true);
}

TEST_CASE("Network validation: Invalid IPv4 addresses", "[validation][network][ip]") {
    REQUIRE(is_valid_ip_or_hostname("999.1.1.1") == false);
    REQUIRE(is_valid_ip_or_hostname("192.168.1.256") == false);
    REQUIRE(is_valid_ip_or_hostname("192.168.1") == false);
    REQUIRE(is_valid_ip_or_hostname("192.168.1.1.1") == false);
    REQUIRE(is_valid_ip_or_hostname("192.168..1") == false);
    REQUIRE(is_valid_ip_or_hostname("192.168.1.") == false);
    REQUIRE(is_valid_ip_or_hostname(".192.168.1.1") == false);
}

TEST_CASE("Network validation: Valid hostnames", "[validation][network][hostname]") {
    REQUIRE(is_valid_ip_or_hostname("printer") == true);
    REQUIRE(is_valid_ip_or_hostname("printer.local") == true);
    REQUIRE(is_valid_ip_or_hostname("my-printer") == true);
    REQUIRE(is_valid_ip_or_hostname("my_printer") == true);
    REQUIRE(is_valid_ip_or_hostname("PRINTER123") == true);
    REQUIRE(is_valid_ip_or_hostname("voron-2.4") == true);
    REQUIRE(is_valid_ip_or_hostname("k1.local") == true);
}

TEST_CASE("Network validation: Invalid hostnames", "[validation][network][hostname]") {
    REQUIRE(is_valid_ip_or_hostname("") == false);
    REQUIRE(is_valid_ip_or_hostname("-printer") == false);
    REQUIRE(is_valid_ip_or_hostname("!invalid") == false);
    REQUIRE(is_valid_ip_or_hostname("print@r") == false);
    REQUIRE(is_valid_ip_or_hostname("print er") == false);
}

TEST_CASE("Network validation: Valid ports", "[validation][network][port]") {
    REQUIRE(is_valid_port("1") == true);
    REQUIRE(is_valid_port("80") == true);
    REQUIRE(is_valid_port("443") == true);
    REQUIRE(is_valid_port("7125") == true);
    REQUIRE(is_valid_port("8080") == true);
    REQUIRE(is_valid_port("65535") == true);
}

TEST_CASE("Network validation: Invalid ports", "[validation][network][port]") {
    REQUIRE(is_valid_port("") == false);
    REQUIRE(is_valid_port("0") == false);
    REQUIRE(is_valid_port("65536") == false);
    REQUIRE(is_valid_port("99999") == false);
    REQUIRE(is_valid_port("-1") == false);
    REQUIRE(is_valid_port("abc") == false);
    REQUIRE(is_valid_port("12.34") == false);
    REQUIRE(is_valid_port("80a") == false);
    REQUIRE(is_valid_port("   ") == false);
}

TEST_CASE("Network validation: Whitespace trimming", "[validation][network][trim]") {
    // Port: leading/trailing whitespace should be trimmed
    REQUIRE(is_valid_port(" 80") == true);
    REQUIRE(is_valid_port("80 ") == true);
    REQUIRE(is_valid_port("  7125  ") == true);
    REQUIRE(is_valid_port("\t443\t") == true);
    REQUIRE(is_valid_port(" 0") == false);      // Still invalid after trim
    REQUIRE(is_valid_port(" 65536 ") == false); // Still invalid after trim

    // IP/hostname: leading/trailing whitespace should be trimmed
    REQUIRE(is_valid_ip_or_hostname(" 192.168.1.1") == true);
    REQUIRE(is_valid_ip_or_hostname("192.168.1.1 ") == true);
    REQUIRE(is_valid_ip_or_hostname("  printer.local  ") == true);
    REQUIRE(is_valid_ip_or_hostname("\t127.0.0.1\t") == true);
    REQUIRE(is_valid_ip_or_hostname("  ") == false); // All whitespace
}

TEST_CASE("Network validation: Edge cases", "[validation][network][edge]") {
    REQUIRE(is_valid_ip_or_hostname("localhost") == true);
    REQUIRE(is_valid_ip_or_hostname("raspberrypi") == true);
    REQUIRE(is_valid_ip_or_hostname("mainsailos") == true);
    REQUIRE(is_valid_port("1") == true);
    REQUIRE(is_valid_port("65535") == true);
}
