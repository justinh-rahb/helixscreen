// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_callback_helpers.cpp
 * @brief Unit tests for ui_callback_helpers.h batch registration and widget lookup helpers
 */

#include "ui_callback_helpers.h"

#include "../lvgl_test_fixture.h"

#include <spdlog/spdlog.h>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Test Callbacks (static functions matching lv_event_cb_t signature)
// ============================================================================

static int g_callback_a_count = 0;
static int g_callback_b_count = 0;

static void test_callback_a(lv_event_t* /*e*/) {
    g_callback_a_count++;
}

static void test_callback_b(lv_event_t* /*e*/) {
    g_callback_b_count++;
}

// ============================================================================
// register_xml_callbacks Tests
// ============================================================================

TEST_CASE_METHOD(LVGLTestFixture, "register_xml_callbacks registers without crash",
                 "[callback_helpers]") {
    // Registering callbacks should not crash
    REQUIRE_NOTHROW(register_xml_callbacks({
        {"test_cb_a", test_callback_a},
        {"test_cb_b", test_callback_b},
    }));

    // Verify callbacks are retrievable via LVGL XML API
    lv_event_cb_t retrieved_a = lv_xml_get_event_cb(nullptr, "test_cb_a");
    lv_event_cb_t retrieved_b = lv_xml_get_event_cb(nullptr, "test_cb_b");
    REQUIRE(retrieved_a == test_callback_a);
    REQUIRE(retrieved_b == test_callback_b);
}

TEST_CASE_METHOD(LVGLTestFixture, "register_xml_callbacks handles empty list",
                 "[callback_helpers]") {
    REQUIRE_NOTHROW(register_xml_callbacks({}));
}

TEST_CASE_METHOD(LVGLTestFixture, "register_xml_callbacks handles single entry",
                 "[callback_helpers]") {
    REQUIRE_NOTHROW(register_xml_callbacks({
        {"test_single_cb", test_callback_a},
    }));

    lv_event_cb_t retrieved = lv_xml_get_event_cb(nullptr, "test_single_cb");
    REQUIRE(retrieved == test_callback_a);
}

// ============================================================================
// find_required_widget Tests
// ============================================================================

TEST_CASE_METHOD(LVGLTestFixture, "find_required_widget returns widget when found",
                 "[callback_helpers]") {
    lv_obj_t* parent = lv_obj_create(test_screen());
    lv_obj_t* child = lv_obj_create(parent);
    lv_obj_set_name(child, "test_widget");

    lv_obj_t* found = find_required_widget(parent, "test_widget", "[Test]");
    REQUIRE(found == child);
}

TEST_CASE_METHOD(LVGLTestFixture, "find_required_widget returns nullptr for missing widget",
                 "[callback_helpers]") {
    lv_obj_t* parent = lv_obj_create(test_screen());

    lv_obj_t* found = find_required_widget(parent, "nonexistent_widget", "[Test]");
    REQUIRE(found == nullptr);
}

TEST_CASE_METHOD(LVGLTestFixture, "find_required_widget finds nested widget",
                 "[callback_helpers]") {
    lv_obj_t* parent = lv_obj_create(test_screen());
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_t* nested = lv_obj_create(container);
    lv_obj_set_name(nested, "deeply_nested");

    lv_obj_t* found = find_required_widget(parent, "deeply_nested", "[Test]");
    REQUIRE(found == nested);
}
