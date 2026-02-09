// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "backlight_backend.h"
#include "runtime_config.h"

#include "../catch_amalgamated.hpp"

// RAII guard to temporarily enable test mode on global RuntimeConfig
struct TestModeGuard {
    RuntimeConfig* rc;
    bool prev;
    explicit TestModeGuard(RuntimeConfig* r) : rc(r), prev(r->test_mode) {
        rc->test_mode = true;
    }
    ~TestModeGuard() {
        rc->test_mode = prev;
    }
};

// ============================================================================
// BacklightBackend::supports_hardware_blank() Tests
// ============================================================================

TEST_CASE("BacklightBackend supports_hardware_blank defaults to false", "[api][backlight]") {
    // Factory creates None backend (no hardware). Key invariant: non-Allwinner
    // backends must NOT claim hardware blank support.
    auto backend = BacklightBackend::create();
    REQUIRE(backend != nullptr);
    REQUIRE_FALSE(backend->supports_hardware_blank());
}

TEST_CASE("BacklightBackend factory creates None backend without test mode", "[api][backlight]") {
    // Without test_mode, on a non-Linux (macOS) host, factory falls through to None
    auto backend = BacklightBackend::create();
    REQUIRE(backend != nullptr);
    REQUIRE(std::string(backend->name()) == "None");
    REQUIRE_FALSE(backend->is_available());
}

TEST_CASE("BacklightBackend factory creates Simulated backend in test mode", "[api][backlight]") {
    TestModeGuard guard(get_runtime_config());

    auto backend = BacklightBackend::create();
    REQUIRE(backend != nullptr);
    REQUIRE(std::string(backend->name()) == "Simulated");
    REQUIRE(backend->is_available());
    REQUIRE_FALSE(backend->supports_hardware_blank());

    // Simulated backend round-trips brightness
    REQUIRE(backend->set_brightness(75));
    REQUIRE(backend->get_brightness() == 75);

    REQUIRE(backend->set_brightness(0));
    REQUIRE(backend->get_brightness() == 0);

    REQUIRE(backend->set_brightness(100));
    REQUIRE(backend->get_brightness() == 100);
}
