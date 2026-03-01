// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lvgl_test_fixture.h"
#include "test_helpers/update_queue_test_access.h"

#include "../catch_amalgamated.hpp"

using helix::ui::UpdateQueue;
using helix::ui::UpdateQueueTestAccess;

TEST_CASE_METHOD(LVGLTestFixture, "ScopedFreeze discards queued callbacks", "[update_queue]") {
    auto& q = UpdateQueue::instance();
    bool ran = false;

    {
        auto freeze = q.scoped_freeze();
        q.queue([&ran]() { ran = true; });
        UpdateQueueTestAccess::drain(q);
    }

    REQUIRE_FALSE(ran);
}

TEST_CASE_METHOD(LVGLTestFixture, "drain works before freeze", "[update_queue]") {
    auto& q = UpdateQueue::instance();
    bool first_ran = false;
    bool second_ran = false;

    // Queue and drain before freezing — callback should run
    q.queue([&first_ran]() { first_ran = true; });
    UpdateQueueTestAccess::drain(q);
    REQUIRE(first_ran);

    // Now freeze and queue — callback should be discarded
    {
        auto freeze = q.scoped_freeze();
        q.queue([&second_ran]() { second_ran = true; });
        UpdateQueueTestAccess::drain(q);
    }

    REQUIRE_FALSE(second_ran);
}

TEST_CASE_METHOD(LVGLTestFixture, "ScopedFreeze is RAII — thaw on scope exit", "[update_queue]") {
    auto& q = UpdateQueue::instance();
    bool ran = false;

    // Freeze in inner scope
    {
        auto freeze = q.scoped_freeze();
    }

    // After scope exit, queue should work again
    q.queue([&ran]() { ran = true; });
    UpdateQueueTestAccess::drain(q);

    REQUIRE(ran);
}

TEST_CASE_METHOD(LVGLTestFixture, "queue resumes after thaw", "[update_queue]") {
    auto& q = UpdateQueue::instance();
    bool discarded_ran = false;
    bool resumed_ran = false;

    // Freeze — queued callback should be discarded
    {
        auto freeze = q.scoped_freeze();
        q.queue([&discarded_ran]() { discarded_ran = true; });
    }

    // After thaw, queue a new callback — should run
    q.queue([&resumed_ran]() { resumed_ran = true; });
    UpdateQueueTestAccess::drain(q);

    REQUIRE_FALSE(discarded_ran);
    REQUIRE(resumed_ran);
}
