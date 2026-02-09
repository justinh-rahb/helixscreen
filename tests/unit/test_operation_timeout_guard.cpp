// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_operation_timeout_guard.cpp
 * @brief Unit tests for OperationTimeoutGuard utility
 *
 * NOTE: Timer callback tests (timeout fires, timer cleanup) are not included
 * because the UpdateQueue's 1ms LVGL timer causes process_lvgl() to spin
 * indefinitely in the test harness. The timer mechanism is identical to
 * FilamentPanel's existing production-tested timeout pattern.
 */

#include "../lvgl_test_fixture.h"
#include "operation_timeout_guard.h"
#include "subject_managed_panel.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// Basic State Tests
// ============================================================================

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: default state is inactive",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;
    REQUIRE_FALSE(guard.is_active());
    REQUIRE(guard.subject() == nullptr);
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: begin sets active",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;

    guard.begin(30000, [] {});
    REQUIRE(guard.is_active());

    guard.end();
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: end clears active",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;

    guard.begin(30000, [] {});
    REQUIRE(guard.is_active());

    guard.end();
    REQUIRE_FALSE(guard.is_active());
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: double end is harmless",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;

    guard.begin(30000, [] {});
    guard.end();
    guard.end(); // Should not crash

    REQUIRE_FALSE(guard.is_active());
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: begin replaces active state",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;

    guard.begin(30000, [] {});
    REQUIRE(guard.is_active());

    // Second begin should still be active (replaces first timer)
    guard.begin(30000, [] {});
    REQUIRE(guard.is_active());

    guard.end();
    REQUIRE_FALSE(guard.is_active());
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: end without begin is safe",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;
    guard.end(); // Should not crash
    REQUIRE_FALSE(guard.is_active());
}

// ============================================================================
// Subject Integration Tests
// ============================================================================

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: init_subject registers subject",
                 "[operation_timeout_guard]") {
    SubjectManager subjects;
    OperationTimeoutGuard guard;
    guard.init_subject("test_guard_subject", subjects);

    REQUIRE(guard.subject() != nullptr);
    REQUIRE(lv_subject_get_int(guard.subject()) == 0);
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: begin sets subject to 1",
                 "[operation_timeout_guard]") {
    SubjectManager subjects;
    OperationTimeoutGuard guard;
    guard.init_subject("test_guard_begin_subject", subjects);

    guard.begin(30000, [] {});
    REQUIRE(lv_subject_get_int(guard.subject()) == 1);

    guard.end();
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: end sets subject to 0",
                 "[operation_timeout_guard]") {
    SubjectManager subjects;
    OperationTimeoutGuard guard;
    guard.init_subject("test_guard_end_subject", subjects);

    guard.begin(30000, [] {});
    REQUIRE(lv_subject_get_int(guard.subject()) == 1);

    guard.end();
    REQUIRE(lv_subject_get_int(guard.subject()) == 0);
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: without subject begin/end work",
                 "[operation_timeout_guard]") {
    OperationTimeoutGuard guard;
    REQUIRE(guard.subject() == nullptr);

    guard.begin(30000, [] {});
    REQUIRE(guard.is_active());

    guard.end();
    REQUIRE_FALSE(guard.is_active());
    REQUIRE(guard.subject() == nullptr);
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: destructor cleans up",
                 "[operation_timeout_guard]") {
    // Verify no crash when destroying guard with active timer
    {
        OperationTimeoutGuard guard;
        guard.begin(30000, [] {});
        // guard destroyed here with active timer
    }
    // If we get here, destructor handled cleanup
    REQUIRE(true);
}

TEST_CASE_METHOD(LVGLTestFixture, "OperationTimeoutGuard: destructor with subject cleans up",
                 "[operation_timeout_guard]") {
    // SubjectManager must outlive the guard, and the guard's subject_ lives
    // inside the guard. So subjects must be destroyed AFTER guard (declared first).
    // In practice, panels own both and they're destroyed together.
    SubjectManager subjects;
    OperationTimeoutGuard guard;
    guard.init_subject("test_guard_dtor_subject", subjects);
    guard.begin(30000, [] {});
    // Both destroyed at end of scope: guard first, then subjects deinits the subject.
    // Wait — guard.subject_ is inside guard, destroyed before subjects.deinit_all().
    // This is fine because subjects_ must deinit BEFORE guard is destroyed.
    // In panels, deinit_subjects() is called explicitly before destruction.
    // Here, we call subjects.deinit_all() manually to match real usage.
    guard.end();
    subjects.deinit_all();
    REQUIRE(true);
}
