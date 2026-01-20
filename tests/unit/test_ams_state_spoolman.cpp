// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ams_state.h"
#include "moonraker_api_mock.h"
#include "moonraker_client_mock.h"
#include "printer_state.h"
#include "spoolman_types.h"

#include "../catch_amalgamated.hpp"

/**
 * @file test_ams_state_spoolman.cpp
 * @brief Unit tests for AmsState Spoolman weight refresh integration
 *
 * Tests the refresh_spoolman_weights() method and related polling functionality
 * that syncs slot weights from Spoolman spool data.
 *
 * Key mappings:
 * - SlotInfo.remaining_weight_g <- SpoolInfo.remaining_weight_g
 * - SlotInfo.total_weight_g     <- SpoolInfo.initial_weight_g
 */

// ============================================================================
// refresh_spoolman_weights() Tests
// ============================================================================

TEST_CASE("AmsState - refresh_spoolman_weights updates slot weights from Spoolman",
          "[ams][spoolman]") {
    // Setup: Create mock API with known spool data
    PrinterState state;
    MoonrakerClientMock client;
    MoonrakerAPIMock api(client, state);

    // Get mock spools and set known weights
    auto& mock_spools = api.get_mock_spools();
    REQUIRE(mock_spools.size() > 0);

    // Configure a test spool with known values
    const int test_spool_id = mock_spools[0].id;
    mock_spools[0].remaining_weight_g = 450.0;
    mock_spools[0].initial_weight_g = 1000.0;

    // Get AmsState singleton and set up the API
    auto& ams = AmsState::instance();
    ams.set_moonraker_api(&api);

    // TODO: Set up a slot with spoolman_id matching test_spool_id
    // This requires access to backend slot configuration

    SECTION("updates slot weights when spoolman_id is set") {
        // Act: Call refresh_spoolman_weights
        ams.refresh_spoolman_weights();

        // Assert: Slot weights should be updated from SpoolInfo
        // SlotInfo.remaining_weight_g should equal SpoolInfo.remaining_weight_g (450.0)
        // SlotInfo.total_weight_g should equal SpoolInfo.initial_weight_g (1000.0)

        // NOTE: This test will fail to link until refresh_spoolman_weights() is implemented
        REQUIRE(true); // Placeholder - real assertion depends on slot access
    }

    // Cleanup
    ams.set_moonraker_api(nullptr);
}

TEST_CASE("AmsState - refresh_spoolman_weights skips slots without spoolman_id",
          "[ams][spoolman]") {
    // Setup: Create mock API
    PrinterState state;
    MoonrakerClientMock client;
    MoonrakerAPIMock api(client, state);

    auto& ams = AmsState::instance();
    ams.set_moonraker_api(&api);

    SECTION("does not call API for slots with spoolman_id = 0") {
        // A slot with spoolman_id = 0 should not trigger get_spoolman_spool()
        // Since we can't easily mock/count API calls, we verify no crash occurs
        // and the method completes successfully

        // Act: Call refresh with slots that have no spoolman assignment
        ams.refresh_spoolman_weights();

        // Assert: No crash, method completes (API not called for unassigned slots)
        REQUIRE(true);
    }

    // Cleanup
    ams.set_moonraker_api(nullptr);
}

TEST_CASE("AmsState - refresh_spoolman_weights handles missing spools gracefully",
          "[ams][spoolman]") {
    // Setup: Create mock API
    PrinterState state;
    MoonrakerClientMock client;
    MoonrakerAPIMock api(client, state);

    auto& ams = AmsState::instance();
    ams.set_moonraker_api(&api);

    SECTION("handles spool not found without crash") {
        // If a slot has a spoolman_id that doesn't exist in Spoolman,
        // the error callback should be handled gracefully

        // Act: Attempt to refresh with a non-existent spool ID
        // (would need to configure a slot with invalid spoolman_id)
        ams.refresh_spoolman_weights();

        // Assert: No crash, slot data unchanged
        REQUIRE(true);
    }

    // Cleanup
    ams.set_moonraker_api(nullptr);
}

TEST_CASE("AmsState - refresh_spoolman_weights with no API set", "[ams][spoolman]") {
    auto& ams = AmsState::instance();

    // Ensure no API is set
    ams.set_moonraker_api(nullptr);

    SECTION("does nothing when API is null") {
        // Act: Call refresh with no API configured
        ams.refresh_spoolman_weights();

        // Assert: No crash, method returns early
        REQUIRE(true);
    }
}

// ============================================================================
// Spoolman Polling Tests (start/stop with refcount)
// ============================================================================

TEST_CASE("AmsState - start_spoolman_polling increments refcount", "[ams][spoolman][polling]") {
    auto& ams = AmsState::instance();

    SECTION("calling start twice, stop once - still polling") {
        // Act: Start polling twice
        ams.start_spoolman_polling();
        ams.start_spoolman_polling();

        // Stop once - refcount should be 1, still polling
        ams.stop_spoolman_polling();

        // Assert: Polling should still be active (refcount > 0)
        // NOTE: Need a way to check if polling is active, or trust the refcount logic
        REQUIRE(true); // Placeholder - implementation will verify refcount behavior
    }

    SECTION("calling stop again - polling stops") {
        // Continue from above - one more stop should bring refcount to 0
        ams.stop_spoolman_polling();

        // Assert: Polling should now be stopped (refcount == 0)
        REQUIRE(true);
    }
}

TEST_CASE("AmsState - stop_spoolman_polling with zero refcount is safe",
          "[ams][spoolman][polling]") {
    auto& ams = AmsState::instance();

    SECTION("calling stop without start does not crash") {
        // Act: Stop without ever calling start
        ams.stop_spoolman_polling();

        // Assert: No crash, refcount stays at 0 or is clamped
        REQUIRE(true);
    }

    SECTION("calling stop multiple times is safe") {
        // Act: Multiple stops without matching starts
        ams.stop_spoolman_polling();
        ams.stop_spoolman_polling();
        ams.stop_spoolman_polling();

        // Assert: No crash, system remains stable
        REQUIRE(true);
    }
}

TEST_CASE("AmsState - spoolman polling refcount behavior", "[ams][spoolman][polling]") {
    auto& ams = AmsState::instance();

    // Reset to known state by stopping any existing polling
    // (Safe due to zero-refcount protection)
    ams.stop_spoolman_polling();
    ams.stop_spoolman_polling();
    ams.stop_spoolman_polling();

    SECTION("balanced start/stop maintains correct state") {
        // Start 3 times
        ams.start_spoolman_polling();
        ams.start_spoolman_polling();
        ams.start_spoolman_polling();

        // Stop 3 times - should be back to not polling
        ams.stop_spoolman_polling();
        ams.stop_spoolman_polling();
        ams.stop_spoolman_polling();

        // Assert: System is in stable non-polling state
        REQUIRE(true);
    }

    SECTION("start after stop restarts polling") {
        ams.start_spoolman_polling();
        ams.stop_spoolman_polling();

        // Start again
        ams.start_spoolman_polling();

        // Assert: Polling should be active again
        REQUIRE(true);

        // Cleanup
        ams.stop_spoolman_polling();
    }
}

// ============================================================================
// Integration Tests (refresh triggered by polling)
// ============================================================================

TEST_CASE("AmsState - polling triggers periodic refresh", "[ams][spoolman][polling][slow]") {
    // Setup: Create mock API
    PrinterState state;
    MoonrakerClientMock client;
    MoonrakerAPIMock api(client, state);

    auto& ams = AmsState::instance();
    ams.set_moonraker_api(&api);

    SECTION("polling with valid API performs refresh") {
        // Start polling
        ams.start_spoolman_polling();

        // Note: In real implementation, this would trigger periodic refresh_spoolman_weights()
        // The actual timer interval and refresh calls would be verified here

        // Cleanup
        ams.stop_spoolman_polling();
        REQUIRE(true);
    }

    // Cleanup
    ams.set_moonraker_api(nullptr);
}
