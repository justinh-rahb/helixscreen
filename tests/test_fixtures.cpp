// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_fixtures.h"

#include "ui_card.h"
#include "ui_icon.h"
#include "ui_temp_display.h"
#include "ui_text.h"

#include "spdlog/spdlog.h"

// ============================================================================
// MoonrakerTestFixture Implementation
// ============================================================================

MoonrakerTestFixture::MoonrakerTestFixture() {
    // Initialize printer state with subjects (skip XML registration for tests)
    m_state.init_subjects(false);

    // Create disconnected client - validation happens before network I/O
    m_client = std::make_unique<MoonrakerClient>();

    // Create API with client and state
    m_api = std::make_unique<MoonrakerAPI>(*m_client, m_state);

    spdlog::debug("[MoonrakerTestFixture] Initialized with disconnected client");
}

MoonrakerTestFixture::~MoonrakerTestFixture() {
    // Ensure API is destroyed before client (API holds reference to client)
    m_api.reset();
    m_client.reset();
    spdlog::debug("[MoonrakerTestFixture] Cleaned up");
}

// ============================================================================
// UITestFixture Implementation
// ============================================================================

UITestFixture::UITestFixture() {
    // Initialize UITest virtual input device
    UITest::init(test_screen());
    spdlog::debug("[UITestFixture] Initialized with virtual input device");
}

UITestFixture::~UITestFixture() {
    // Clean up virtual input device
    UITest::cleanup();
    spdlog::debug("[UITestFixture] Cleaned up virtual input device");
}

// ============================================================================
// FullMoonrakerTestFixture Implementation
// ============================================================================

FullMoonrakerTestFixture::FullMoonrakerTestFixture() {
    // Initialize UITest virtual input device
    // (MoonrakerTestFixture constructor already ran)
    UITest::init(test_screen());
    spdlog::debug("[FullMoonrakerTestFixture] Initialized with Moonraker + UITest");
}

FullMoonrakerTestFixture::~FullMoonrakerTestFixture() {
    // Clean up virtual input device
    UITest::cleanup();
    spdlog::debug("[FullMoonrakerTestFixture] Cleaned up");
}

// ============================================================================
// XMLTestFixture Implementation
// ============================================================================

/**
 * No-op callback for optional event handlers in XML components.
 * When a component has an optional callback prop with default="",
 * LVGL tries to find a callback named "" which doesn't exist.
 * Registering this no-op callback silences those warnings.
 */
static void xml_test_noop_event_callback(lv_event_t* /*e*/) {
    // Intentionally empty - used for optional callbacks that weren't provided
}

XMLTestFixture::XMLTestFixture() : MoonrakerTestFixture() {
    // The parent constructor created a test_screen, but we need to initialize
    // the theme BEFORE any screens exist to avoid hanging. Delete it temporarily.
    if (m_test_screen != nullptr) {
        lv_obj_delete(m_test_screen);
        m_test_screen = nullptr;
    }

    // MoonrakerTestFixture called m_state.init_subjects(false), which skipped XML registration.
    // For XML testing, we need subjects registered with LVGL XML system.
    // Reset and reinitialize with XML registration enabled.
    m_state.reset_for_testing();
    m_state.init_subjects(true); // Enable XML registration

    // 1. Register fonts (required before theme)
    AssetManager::register_all();

    // 2. Register globals.xml (required for constants - must come before theme)
    lv_xml_register_component_from_file("A:ui_xml/globals.xml");

    // 3. Initialize theme (uses globals constants, registers responsive values)
    // Theme initialization happens with no screens present, avoiding infinite recursion.
    ui_theme_init(lv_display_get_default(), false); // light mode for tests
    m_theme_initialized = true;

    // 4. Register custom widgets (must be done before loading components that use them)
    // Order matters: base widgets first, then widgets that depend on them
    ui_icon_register_widget(); // icon component
    ui_text_init();            // text_heading, text_body, text_small, text_xs
    ui_card_register();        // ui_card
    ui_temp_display_init();    // temp_display

    // 5. Register no-op callbacks for event handlers in XML components
    // These callbacks are used in panels but aren't needed for binding tests
    lv_xml_register_event_cb(nullptr, "", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_header_back_clicked", xml_test_noop_event_callback);
    // Nozzle temp panel callbacks
    lv_xml_register_event_cb(nullptr, "on_nozzle_preset_off_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_nozzle_preset_pla_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_nozzle_preset_petg_clicked",
                             xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_nozzle_preset_abs_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_nozzle_custom_clicked", xml_test_noop_event_callback);
    // Bed temp panel callbacks
    lv_xml_register_event_cb(nullptr, "on_bed_preset_off_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_bed_preset_pla_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_bed_preset_petg_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_bed_preset_abs_clicked", xml_test_noop_event_callback);
    lv_xml_register_event_cb(nullptr, "on_bed_custom_clicked", xml_test_noop_event_callback);

    // Subjects were already registered by init_subjects(true) above
    m_subjects_registered = true;

    // NOW recreate the test screen (with theme already applied)
    m_test_screen = lv_obj_create(nullptr);
    lv_screen_load(m_test_screen);

    spdlog::debug("[XMLTestFixture] Initialized with fonts, theme, widgets, and subjects");
}

XMLTestFixture::~XMLTestFixture() {
    // Theme cleanup handled by LVGL deinit
    spdlog::debug("[XMLTestFixture] Cleaned up");
}

bool XMLTestFixture::register_component(const char* component_name) {
    char path[256];
    snprintf(path, sizeof(path), "A:ui_xml/%s.xml", component_name);
    lv_result_t result = lv_xml_register_component_from_file(path);
    if (result != LV_RESULT_OK) {
        spdlog::warn("[XMLTestFixture] Failed to register component '{}' from {}", component_name,
                     path);
        return false;
    }
    spdlog::debug("[XMLTestFixture] Registered component '{}'", component_name);
    return true;
}

lv_obj_t* XMLTestFixture::create_component(const char* component_name) {
    return create_component(component_name, nullptr);
}

lv_obj_t* XMLTestFixture::create_component(const char* component_name, const char** attrs) {
    if (!m_subjects_registered) {
        register_subjects();
    }
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_xml_create(test_screen(), component_name, attrs));
    if (obj == nullptr) {
        spdlog::warn("[XMLTestFixture] Failed to create component '{}'", component_name);
    } else {
        spdlog::debug("[XMLTestFixture] Created component '{}'", component_name);
    }
    return obj;
}

void XMLTestFixture::register_subjects() {
    if (m_subjects_registered) {
        spdlog::debug("[XMLTestFixture] Subjects already registered");
        return;
    }

    // PrinterState subjects are already registered via init_subjects(true) in constructor.
    // This method exists for manual control if tests need to modify state() before
    // subjects are registered, but normally that's not needed.
    spdlog::debug("[XMLTestFixture] register_subjects() called - subjects already registered in "
                  "constructor");
    m_subjects_registered = true;
}
