// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_streaming_policy.cpp
 * @brief Unit tests for StreamingPolicy
 *
 * Tests the streaming policy decision logic for file operations.
 */

#include "streaming_policy.h"

#include "../catch_amalgamated.hpp"

using namespace helix;

// ============================================================================
// Test Fixture - Reset policy state between tests
// ============================================================================

struct StreamingPolicyFixture {
    StreamingPolicyFixture() {
        // Reset to defaults before each test
        auto& policy = StreamingPolicy::instance();
        policy.set_threshold_bytes(0);     // Auto-detect mode
        policy.set_force_streaming(false); // No forcing
    }
};

// ============================================================================
// Constants Tests
// ============================================================================

TEST_CASE("StreamingPolicy: constants are reasonable", "[streaming_policy]") {
    SECTION("threshold bounds make sense") {
        REQUIRE(StreamingPolicy::MIN_THRESHOLD < StreamingPolicy::MAX_THRESHOLD);
        REQUIRE(StreamingPolicy::FALLBACK_THRESHOLD >= StreamingPolicy::MIN_THRESHOLD);
        REQUIRE(StreamingPolicy::FALLBACK_THRESHOLD <= StreamingPolicy::MAX_THRESHOLD);
    }

    SECTION("RAM percentage is reasonable") {
        REQUIRE(StreamingPolicy::RAM_THRESHOLD_PERCENT > 0.0);
        REQUIRE(StreamingPolicy::RAM_THRESHOLD_PERCENT < 1.0);
    }

    SECTION("MIN_THRESHOLD is 5MB") {
        REQUIRE(StreamingPolicy::MIN_THRESHOLD == 5 * 1024 * 1024);
    }

    SECTION("MAX_THRESHOLD is 100MB") {
        REQUIRE(StreamingPolicy::MAX_THRESHOLD == 100 * 1024 * 1024);
    }

    SECTION("FALLBACK_THRESHOLD is 10MB") {
        REQUIRE(StreamingPolicy::FALLBACK_THRESHOLD == 10 * 1024 * 1024);
    }
}

// ============================================================================
// Force Streaming Tests
// ============================================================================

TEST_CASE_METHOD(StreamingPolicyFixture, "StreamingPolicy: force streaming mode",
                 "[streaming_policy]") {
    auto& policy = StreamingPolicy::instance();

    SECTION("force streaming defaults off") {
        REQUIRE_FALSE(policy.is_force_streaming());
    }

    SECTION("can enable force streaming") {
        policy.set_force_streaming(true);
        REQUIRE(policy.is_force_streaming());
    }

    SECTION("force streaming affects all file sizes") {
        policy.set_force_streaming(true);

        // Even tiny files should stream when forced
        REQUIRE(policy.should_stream(0));
        REQUIRE(policy.should_stream(1));
        REQUIRE(policy.should_stream(100));
        REQUIRE(policy.should_stream(1024));
    }

    SECTION("can disable force streaming") {
        policy.set_force_streaming(true);
        REQUIRE(policy.is_force_streaming());

        policy.set_force_streaming(false);
        REQUIRE_FALSE(policy.is_force_streaming());
    }
}

// ============================================================================
// Explicit Threshold Tests
// ============================================================================

TEST_CASE_METHOD(StreamingPolicyFixture, "StreamingPolicy: explicit threshold",
                 "[streaming_policy]") {
    auto& policy = StreamingPolicy::instance();

    SECTION("can set explicit threshold") {
        constexpr size_t threshold = 50 * 1024 * 1024; // 50MB
        policy.set_threshold_bytes(threshold);
        REQUIRE(policy.get_threshold_bytes() == threshold);
    }

    SECTION("files below threshold don't stream") {
        constexpr size_t threshold = 10 * 1024 * 1024; // 10MB
        policy.set_threshold_bytes(threshold);

        REQUIRE_FALSE(policy.should_stream(0));
        REQUIRE_FALSE(policy.should_stream(1024));
        REQUIRE_FALSE(policy.should_stream(1 * 1024 * 1024)); // 1MB
        REQUIRE_FALSE(policy.should_stream(9 * 1024 * 1024)); // 9MB
        REQUIRE_FALSE(policy.should_stream(threshold - 1));   // Just under
    }

    SECTION("files above threshold do stream (boundary is exclusive)") {
        constexpr size_t threshold = 10 * 1024 * 1024; // 10MB
        policy.set_threshold_bytes(threshold);

        // Note: threshold is exclusive - files must be LARGER than threshold
        REQUIRE_FALSE(policy.should_stream(threshold));    // Exactly at threshold - doesn't stream
        REQUIRE(policy.should_stream(threshold + 1));      // Just over - streams
        REQUIRE(policy.should_stream(20 * 1024 * 1024));   // 20MB
        REQUIRE(policy.should_stream(100 * 1024 * 1024));  // 100MB
        REQUIRE(policy.should_stream(1024 * 1024 * 1024)); // 1GB
    }

    SECTION("threshold of 0 means auto-detect") {
        policy.set_threshold_bytes(0);
        // With 0 configured, get_threshold_bytes returns auto-detected value
        size_t detected = policy.get_threshold_bytes();
        REQUIRE(detected >= StreamingPolicy::MIN_THRESHOLD);
        REQUIRE(detected <= StreamingPolicy::MAX_THRESHOLD);
    }
}

// ============================================================================
// Auto-Detection Tests
// ============================================================================

TEST_CASE_METHOD(StreamingPolicyFixture, "StreamingPolicy: auto-detection", "[streaming_policy]") {
    auto& policy = StreamingPolicy::instance();

    SECTION("auto-detected threshold is within bounds") {
        size_t threshold = policy.auto_detect_threshold();
        REQUIRE(threshold >= StreamingPolicy::MIN_THRESHOLD);
        REQUIRE(threshold <= StreamingPolicy::MAX_THRESHOLD);
    }

    SECTION("auto-detect is used when threshold is 0") {
        policy.set_threshold_bytes(0);
        size_t effective = policy.get_threshold_bytes();
        size_t auto_detected = policy.auto_detect_threshold();
        REQUIRE(effective == auto_detected);
    }

    SECTION("explicit threshold overrides auto-detect") {
        constexpr size_t explicit_threshold = 25 * 1024 * 1024;
        policy.set_threshold_bytes(explicit_threshold);
        REQUIRE(policy.get_threshold_bytes() == explicit_threshold);
        // auto_detect_threshold should still return the same value
        // but get_threshold_bytes uses explicit
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE_METHOD(StreamingPolicyFixture, "StreamingPolicy: edge cases", "[streaming_policy]") {
    auto& policy = StreamingPolicy::instance();

    SECTION("zero-byte files never stream (unless forced)") {
        policy.set_threshold_bytes(1); // Stream anything >= 1 byte
        REQUIRE_FALSE(policy.should_stream(0));

        // But with force, even 0 streams
        policy.set_force_streaming(true);
        REQUIRE(policy.should_stream(0));
    }

    SECTION("very large threshold still works") {
        constexpr size_t huge = 10ULL * 1024 * 1024 * 1024; // 10GB
        policy.set_threshold_bytes(huge);
        REQUIRE(policy.get_threshold_bytes() == huge);

        // Files under 10GB shouldn't stream
        REQUIRE_FALSE(policy.should_stream(1ULL * 1024 * 1024 * 1024)); // 1GB
        REQUIRE_FALSE(policy.should_stream(5ULL * 1024 * 1024 * 1024)); // 5GB

        // Files at threshold don't stream (boundary is exclusive)
        REQUIRE_FALSE(policy.should_stream(huge));
        // Files over 10GB should stream
        REQUIRE(policy.should_stream(huge + 1));
    }

    SECTION("force streaming takes precedence over high threshold") {
        constexpr size_t huge = 10ULL * 1024 * 1024 * 1024; // 10GB
        policy.set_threshold_bytes(huge);
        policy.set_force_streaming(true);

        // Even small files stream when forced
        REQUIRE(policy.should_stream(1));
        REQUIRE(policy.should_stream(1024));
    }
}

// ============================================================================
// Singleton Tests
// ============================================================================

TEST_CASE("StreamingPolicy: singleton behavior", "[streaming_policy]") {
    SECTION("instance returns same object") {
        auto& a = StreamingPolicy::instance();
        auto& b = StreamingPolicy::instance();
        REQUIRE(&a == &b);
    }

    SECTION("state persists across instance calls") {
        auto& policy = StreamingPolicy::instance();
        policy.set_force_streaming(true);

        auto& policy2 = StreamingPolicy::instance();
        REQUIRE(policy2.is_force_streaming());

        // Clean up
        policy.set_force_streaming(false);
    }
}
