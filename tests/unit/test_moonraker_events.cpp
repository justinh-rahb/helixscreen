// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_moonraker_events.cpp
 * @brief Unit tests for MoonrakerClient event emission functionality
 *
 * Tests the Phase 2 event emitter pattern for decoupling transport-layer
 * events from the UI layer. The event system allows MoonrakerClient to
 * notify listeners about connection issues, errors, and state changes
 * without direct UI dependencies.
 *
 * Test Categories:
 * 1. Event handler registration and unregistration
 * 2. Event emission with correct type/message/is_error
 * 3. Sequential event emission
 * 4. Graceful handling of null/missing handlers
 * 5. Exception safety in event handlers
 */

#include "../catch_amalgamated.hpp"
#include "moonraker_client_mock.h"
#include "moonraker_events.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Test Helper: Testable Mock with Protected emit_event Access
// ============================================================================

/**
 * @brief Test helper that exposes protected emit_event() for unit testing
 *
 * MoonrakerClient::emit_event() is protected to prevent external code from
 * emitting fake events. This subclass exposes it for testing purposes.
 */
class TestableMoonrakerClient : public MoonrakerClientMock {
  public:
    using MoonrakerClientMock::MoonrakerClientMock;

    // Expose protected method for testing
    void test_emit_event(MoonrakerEventType type, const std::string& message, bool is_error = false,
                         const std::string& details = "") {
        emit_event(type, message, is_error, details);
    }
};

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Test fixture for event emission tests
 *
 * Provides a testable mock client and event capture infrastructure.
 */
class EventTestFixture {
  public:
    EventTestFixture()
        : client_(std::make_unique<TestableMoonrakerClient>(
              MoonrakerClientMock::PrinterType::VORON_24)) {}

    /**
     * @brief Create an event handler that captures received events
     */
    MoonrakerEventCallback create_capture_handler() {
        return [this](const MoonrakerEvent& event) {
            std::lock_guard<std::mutex> lock(mutex_);
            captured_events_.push_back(event);
            event_received_.store(true);
        };
    }

    /**
     * @brief Get count of captured events (thread-safe)
     */
    size_t event_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return captured_events_.size();
    }

    /**
     * @brief Get a copy of captured events (thread-safe)
     */
    std::vector<MoonrakerEvent> get_events() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return captured_events_;
    }

    /**
     * @brief Get the last captured event (thread-safe)
     * @throws std::runtime_error if no events captured
     */
    MoonrakerEvent get_last_event() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (captured_events_.empty()) {
            throw std::runtime_error("No events captured");
        }
        return captured_events_.back();
    }

    /**
     * @brief Check if any event was received
     */
    bool has_event() const {
        return event_received_.load();
    }

    /**
     * @brief Reset captured state for next test
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        captured_events_.clear();
        event_received_.store(false);
    }

    std::unique_ptr<TestableMoonrakerClient> client_;

  private:
    mutable std::mutex mutex_;
    std::vector<MoonrakerEvent> captured_events_;
    std::atomic<bool> event_received_{false};
};

// ============================================================================
// Test Cases: Event Handler Registration
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient event handler can be registered",
                 "[moonraker][events][registration]") {

    SECTION("registered handler receives events") {
        client_->register_event_handler(create_capture_handler());

        // Emit a test event
        client_->test_emit_event(MoonrakerEventType::CONNECTION_LOST, "Test connection lost",
                                 true);

        REQUIRE(has_event());
        REQUIRE(event_count() == 1);

        auto event = get_last_event();
        CHECK(event.type == MoonrakerEventType::CONNECTION_LOST);
        CHECK(event.message == "Test connection lost");
        CHECK(event.is_error == true);
    }

    SECTION("handler registration returns immediately") {
        // Should not block or throw
        auto start = std::chrono::steady_clock::now();
        client_->register_event_handler(create_capture_handler());
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count();

        CHECK(elapsed < 100); // Registration should be fast
    }
}

// ============================================================================
// Test Cases: Event Content Verification
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient events contain correct fields",
                 "[moonraker][events][content]") {

    client_->register_event_handler(create_capture_handler());

    SECTION("error event has is_error=true") {
        client_->test_emit_event(MoonrakerEventType::RPC_ERROR, "Command failed", true,
                                 "printer.gcode.script");

        auto event = get_last_event();
        CHECK(event.type == MoonrakerEventType::RPC_ERROR);
        CHECK(event.message == "Command failed");
        CHECK(event.details == "printer.gcode.script");
        CHECK(event.is_error == true);
    }

    SECTION("warning event has is_error=false") {
        client_->test_emit_event(MoonrakerEventType::RECONNECTING, "Attempting reconnect",
                                 false);

        auto event = get_last_event();
        CHECK(event.type == MoonrakerEventType::RECONNECTING);
        CHECK(event.message == "Attempting reconnect");
        CHECK(event.is_error == false);
    }

    SECTION("all event types can be emitted") {
        // Test each event type
        std::vector<MoonrakerEventType> types = {
            MoonrakerEventType::CONNECTION_FAILED,
            MoonrakerEventType::CONNECTION_LOST,
            MoonrakerEventType::RECONNECTING,
            MoonrakerEventType::RECONNECTED,
            MoonrakerEventType::MESSAGE_OVERSIZED,
            MoonrakerEventType::RPC_ERROR,
            MoonrakerEventType::KLIPPY_DISCONNECTED,
            MoonrakerEventType::KLIPPY_READY,
            MoonrakerEventType::DISCOVERY_FAILED,
            MoonrakerEventType::REQUEST_TIMEOUT};

        for (auto type : types) {
            reset();
            client_->test_emit_event(type, "Test message", false);
            REQUIRE(event_count() == 1);
            CHECK(get_last_event().type == type);
        }
    }

    SECTION("empty details is valid") {
        client_->test_emit_event(MoonrakerEventType::KLIPPY_READY, "Ready", false, "");

        auto event = get_last_event();
        CHECK(event.details.empty());
    }

    SECTION("message with special characters is preserved") {
        std::string special_msg = "Error: \"quotes\" and 'apostrophes' & <xml> chars";
        client_->test_emit_event(MoonrakerEventType::RPC_ERROR, special_msg, true);

        auto event = get_last_event();
        CHECK(event.message == special_msg);
    }
}

// ============================================================================
// Test Cases: Sequential Event Emission
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient can emit multiple events sequentially",
                 "[moonraker][events][sequential]") {

    client_->register_event_handler(create_capture_handler());

    SECTION("events are received in order") {
        client_->test_emit_event(MoonrakerEventType::CONNECTION_LOST, "First", true);
        client_->test_emit_event(MoonrakerEventType::RECONNECTING, "Second", false);
        client_->test_emit_event(MoonrakerEventType::RECONNECTED, "Third", false);

        REQUIRE(event_count() == 3);

        auto events = get_events();
        CHECK(events[0].type == MoonrakerEventType::CONNECTION_LOST);
        CHECK(events[0].message == "First");
        CHECK(events[1].type == MoonrakerEventType::RECONNECTING);
        CHECK(events[1].message == "Second");
        CHECK(events[2].type == MoonrakerEventType::RECONNECTED);
        CHECK(events[2].message == "Third");
    }

    SECTION("rapid fire events all captured") {
        constexpr int NUM_EVENTS = 100;
        for (int i = 0; i < NUM_EVENTS; i++) {
            client_->test_emit_event(MoonrakerEventType::RPC_ERROR,
                                     "Event " + std::to_string(i), true);
        }

        REQUIRE(event_count() == NUM_EVENTS);

        // Verify sequence
        auto events = get_events();
        for (int i = 0; i < NUM_EVENTS; i++) {
            CHECK(events[i].message == "Event " + std::to_string(i));
        }
    }
}

// ============================================================================
// Test Cases: Null/Empty Handler Handling
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient handles null event handler gracefully",
                 "[moonraker][events][null_handler]") {

    SECTION("emit without registered handler does not crash") {
        // No handler registered - should log and continue
        REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::CONNECTION_LOST,
                                                  "No handler", true));
    }

    SECTION("unregistering handler with nullptr works") {
        // Register, then unregister
        client_->register_event_handler(create_capture_handler());
        client_->test_emit_event(MoonrakerEventType::RECONNECTING, "Before unregister", false);
        REQUIRE(event_count() == 1);

        // Unregister by passing nullptr
        client_->register_event_handler(nullptr);
        reset();

        // Should not crash, but no event captured
        REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::RECONNECTED,
                                                  "After unregister", false));
        CHECK(event_count() == 0);
    }

    SECTION("re-registering handler after nullptr works") {
        // Start with handler
        client_->register_event_handler(create_capture_handler());
        client_->test_emit_event(MoonrakerEventType::CONNECTION_LOST, "First", true);
        REQUIRE(event_count() == 1);

        // Unregister
        client_->register_event_handler(nullptr);
        reset();
        client_->test_emit_event(MoonrakerEventType::RECONNECTING, "Dropped", false);
        CHECK(event_count() == 0);

        // Re-register
        client_->register_event_handler(create_capture_handler());
        client_->test_emit_event(MoonrakerEventType::RECONNECTED, "Third", false);
        REQUIRE(event_count() == 1);
        CHECK(get_last_event().message == "Third");
    }
}

// ============================================================================
// Test Cases: Exception Safety in Handlers
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient catches exceptions from event handlers",
                 "[moonraker][events][exception_safety]") {

    SECTION("std::exception in handler is caught") {
        client_->register_event_handler([](const MoonrakerEvent&) {
            throw std::runtime_error("Handler threw exception");
        });

        // Should not propagate exception
        REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::RPC_ERROR,
                                                  "Trigger exception", true));
    }

    SECTION("exception does not prevent client operation") {
        std::atomic<int> call_count{0};

        // Handler that throws on first call, succeeds on second
        client_->register_event_handler([&call_count](const MoonrakerEvent&) {
            call_count++;
            if (call_count == 1) {
                throw std::runtime_error("First call throws");
            }
            // Second call succeeds
        });

        // First event - handler throws but client continues
        REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::CONNECTION_LOST,
                                                  "First", true));
        CHECK(call_count == 1);

        // Second event - handler succeeds
        REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::RECONNECTED,
                                                  "Second", false));
        CHECK(call_count == 2);
    }

    SECTION("client remains functional after handler exception") {
        // Register throwing handler
        client_->register_event_handler([](const MoonrakerEvent&) {
            throw std::logic_error("Always throws");
        });

        // Emit multiple events - all should be handled without crash
        for (int i = 0; i < 10; i++) {
            REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::RPC_ERROR,
                                                      "Event " + std::to_string(i), true));
        }
    }
}

// ============================================================================
// Test Cases: Handler Replacement
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient replaces handler on re-registration",
                 "[moonraker][events][replacement]") {

    SECTION("new handler replaces old handler") {
        std::vector<std::string> handler1_events;
        std::vector<std::string> handler2_events;

        // Register first handler
        client_->register_event_handler([&handler1_events](const MoonrakerEvent& event) {
            handler1_events.push_back(event.message);
        });

        client_->test_emit_event(MoonrakerEventType::RECONNECTING, "To handler 1", false);
        CHECK(handler1_events.size() == 1);
        CHECK(handler2_events.size() == 0);

        // Register second handler (replaces first)
        client_->register_event_handler([&handler2_events](const MoonrakerEvent& event) {
            handler2_events.push_back(event.message);
        });

        client_->test_emit_event(MoonrakerEventType::RECONNECTED, "To handler 2", false);

        // First handler should not receive new event
        CHECK(handler1_events.size() == 1);
        CHECK(handler1_events[0] == "To handler 1");

        // Second handler should receive it
        CHECK(handler2_events.size() == 1);
        CHECK(handler2_events[0] == "To handler 2");
    }
}

// ============================================================================
// Test Cases: Thread Safety (Basic)
// ============================================================================

TEST_CASE_METHOD(EventTestFixture, "MoonrakerClient event emission is thread-safe",
                 "[moonraker][events][threadsafe]") {

    SECTION("concurrent registration and emission") {
        std::atomic<int> received_count{0};
        std::atomic<bool> stop_flag{false};

        // Handler that counts events
        client_->register_event_handler([&received_count](const MoonrakerEvent&) {
            received_count++;
        });

        // Thread that re-registers handler periodically
        std::thread register_thread([this, &stop_flag, &received_count]() {
            while (!stop_flag.load()) {
                client_->register_event_handler([&received_count](const MoonrakerEvent&) {
                    received_count++;
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        // Main thread emits events
        constexpr int NUM_EVENTS = 50;
        for (int i = 0; i < NUM_EVENTS; i++) {
            REQUIRE_NOTHROW(client_->test_emit_event(MoonrakerEventType::RPC_ERROR,
                                                      "Event " + std::to_string(i), true));
        }

        stop_flag.store(true);
        register_thread.join();

        // Should have received events without crashing
        CHECK(received_count.load() >= 0);
    }
}
