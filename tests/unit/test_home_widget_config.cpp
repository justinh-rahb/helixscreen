// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include "home_widget_config.h"
#include "home_widget_registry.h"

#include <set>

#include "../catch_amalgamated.hpp"

using namespace helix;

// ============================================================================
// Test fixture — access Config internals via friend declaration
// ============================================================================

namespace helix {
class HomeWidgetConfigFixture {
  protected:
    Config config;

    void setup_empty_config() {
        config.data = json::object();
    }

    void setup_with_widgets(const json& widgets_json) {
        config.data = json::object();
        config.data["home_widgets"] = widgets_json;
    }

    json& get_data() {
        return config.data;
    }
};
} // namespace helix

// ============================================================================
// Registry tests
// ============================================================================

TEST_CASE("HomeWidgetRegistry: returns all widget definitions", "[home][widget_config]") {
    const auto& defs = get_all_widget_defs();
    REQUIRE(defs.size() == 12);
}

TEST_CASE("HomeWidgetRegistry: all widget IDs are unique", "[home][widget_config]") {
    const auto& defs = get_all_widget_defs();
    std::set<std::string> ids;
    for (const auto& def : defs) {
        REQUIRE(ids.insert(def.id).second); // insert returns false if duplicate
    }
}

TEST_CASE("HomeWidgetRegistry: can look up widget by ID", "[home][widget_config]") {
    const auto* def = find_widget_def("temperature");
    REQUIRE(def != nullptr);
    REQUIRE(std::string(def->display_name) == "Temperature");
}

TEST_CASE("HomeWidgetRegistry: unknown ID returns nullptr", "[home][widget_config]") {
    const auto* def = find_widget_def("nonexistent_widget");
    REQUIRE(def == nullptr);
}

TEST_CASE("HomeWidgetRegistry: widget_def_count matches vector size", "[home][widget_config]") {
    REQUIRE(widget_def_count() == get_all_widget_defs().size());
}

// ============================================================================
// Config tests — default behavior
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: default config produces all widgets enabled in default order",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    const auto& entries = wc.entries();
    const auto& defs = get_all_widget_defs();
    REQUIRE(entries.size() == defs.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        REQUIRE(entries[i].id == defs[i].id);
        REQUIRE(entries[i].enabled == defs[i].default_enabled);
    }
}

// ============================================================================
// Config tests — load from explicit JSON
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: load from explicit JSON preserves order and enabled state",
                 "[home][widget_config]") {
    json widgets = json::array({
        {{"id", "temperature"}, {"enabled", true}},
        {{"id", "led"}, {"enabled", false}},
        {{"id", "network"}, {"enabled", true}},
    });
    setup_with_widgets(widgets);

    HomeWidgetConfig wc(config);
    wc.load();

    const auto& entries = wc.entries();
    // 3 explicit + remaining from registry appended
    REQUIRE(entries.size() == widget_def_count());

    // First 3 should match our explicit order
    REQUIRE(entries[0].id == "temperature");
    REQUIRE(entries[0].enabled == true);
    REQUIRE(entries[1].id == "led");
    REQUIRE(entries[1].enabled == false);
    REQUIRE(entries[2].id == "network");
    REQUIRE(entries[2].enabled == true);

    // Remaining should be appended with their default_enabled value
    const auto& all_defs = get_all_widget_defs();
    for (size_t i = 3; i < entries.size(); ++i) {
        const auto* def = find_widget_def(entries[i].id);
        REQUIRE(def);
        REQUIRE(entries[i].enabled == def->default_enabled);
    }
}

// ============================================================================
// Config tests — save produces expected JSON
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: save produces expected JSON structure",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    // Disable one widget for variety
    wc.set_enabled(2, false);
    wc.save();

    // Check the JSON was written to config
    auto& saved = get_data()["home_widgets"];
    REQUIRE(saved.is_array());
    REQUIRE(saved.size() == widget_def_count());

    // Each entry should have id and enabled
    for (const auto& item : saved) {
        REQUIRE(item.contains("id"));
        REQUIRE(item.contains("enabled"));
        REQUIRE(item["id"].is_string());
        REQUIRE(item["enabled"].is_boolean());
    }

    // The third entry should be disabled
    REQUIRE(saved[2]["enabled"].get<bool>() == false);
}

// ============================================================================
// Config tests — round-trip
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: round-trip load-save-reload preserves state",
                 "[home][widget_config]") {
    setup_empty_config();

    // First load + customize
    HomeWidgetConfig wc1(config);
    wc1.load();
    wc1.set_enabled(1, false);
    wc1.reorder(0, 3);
    wc1.save();

    // Second load from same config
    HomeWidgetConfig wc2(config);
    wc2.load();

    const auto& e1 = wc1.entries();
    const auto& e2 = wc2.entries();
    REQUIRE(e1.size() == e2.size());

    for (size_t i = 0; i < e1.size(); ++i) {
        REQUIRE(e1[i].id == e2[i].id);
        REQUIRE(e1[i].enabled == e2[i].enabled);
    }
}

// ============================================================================
// Config tests — reorder
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: reorder moves item from index 2 to index 0",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    std::string moved_id = wc.entries()[2].id;
    std::string was_first = wc.entries()[0].id;
    wc.reorder(2, 0);

    REQUIRE(wc.entries()[0].id == moved_id);
    REQUIRE(wc.entries()[1].id == was_first);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: reorder moves item from index 0 to index 3",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    std::string moved_id = wc.entries()[0].id;
    std::string was_at_1 = wc.entries()[1].id;
    wc.reorder(0, 3);

    // After removing from 0 and inserting at 3, old index 1 becomes 0
    REQUIRE(wc.entries()[0].id == was_at_1);
    REQUIRE(wc.entries()[3].id == moved_id);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: reorder same index is no-op",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    auto before = wc.entries();
    wc.reorder(2, 2);
    auto after = wc.entries();

    REQUIRE(before.size() == after.size());
    for (size_t i = 0; i < before.size(); ++i) {
        REQUIRE(before[i].id == after[i].id);
    }
}

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: reorder out of bounds is no-op",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    auto before = wc.entries();
    wc.reorder(100, 0);
    auto after = wc.entries();

    REQUIRE(before.size() == after.size());
    for (size_t i = 0; i < before.size(); ++i) {
        REQUIRE(before[i].id == after[i].id);
    }
}

// ============================================================================
// Config tests — toggle enabled
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: toggle disable a widget",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    REQUIRE(wc.entries()[0].enabled == true);
    wc.set_enabled(0, false);
    REQUIRE(wc.entries()[0].enabled == false);
    REQUIRE(wc.is_enabled(wc.entries()[0].id) == false);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: toggle re-enable a widget",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    wc.set_enabled(0, false);
    REQUIRE(wc.entries()[0].enabled == false);

    wc.set_enabled(0, true);
    REQUIRE(wc.entries()[0].enabled == true);
    REQUIRE(wc.is_enabled(wc.entries()[0].id) == true);
}

// ============================================================================
// Config tests — new widget appended
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: new registry widget gets appended with default_enabled",
                 "[home][widget_config]") {
    // Save config with only a subset of widgets
    json widgets = json::array({
        {{"id", "power"}, {"enabled", true}},
        {{"id", "network"}, {"enabled", false}},
    });
    setup_with_widgets(widgets);

    HomeWidgetConfig wc(config);
    wc.load();

    // Should have all registry widgets
    REQUIRE(wc.entries().size() == widget_def_count());

    // First two should match saved order/state
    REQUIRE(wc.entries()[0].id == "power");
    REQUIRE(wc.entries()[0].enabled == true);
    REQUIRE(wc.entries()[1].id == "network");
    REQUIRE(wc.entries()[1].enabled == false);

    // Rest should be appended with their default_enabled value
    for (size_t i = 2; i < wc.entries().size(); ++i) {
        const auto* def = find_widget_def(wc.entries()[i].id);
        REQUIRE(def != nullptr);
        REQUIRE(wc.entries()[i].enabled == def->default_enabled);
    }
}

// ============================================================================
// Config tests — unknown widget IDs dropped
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: unknown widget ID in saved JSON gets dropped",
                 "[home][widget_config]") {
    json widgets = json::array({
        {{"id", "power"}, {"enabled", true}},
        {{"id", "bogus_widget"}, {"enabled", true}},
        {{"id", "network"}, {"enabled", false}},
    });
    setup_with_widgets(widgets);

    HomeWidgetConfig wc(config);
    wc.load();

    // bogus_widget should be dropped, so total is still widget_def_count
    REQUIRE(wc.entries().size() == widget_def_count());

    // First should be power, second should be network (bogus skipped)
    REQUIRE(wc.entries()[0].id == "power");
    REQUIRE(wc.entries()[1].id == "network");
}

// ============================================================================
// Config tests — reset to defaults
// ============================================================================

TEST_CASE_METHOD(
    HomeWidgetConfigFixture,
    "HomeWidgetConfig: reset to defaults restores all widgets enabled in default order",
    "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    // Customize
    wc.set_enabled(0, false);
    wc.reorder(0, 5);

    // Reset
    wc.reset_to_defaults();

    const auto& entries = wc.entries();
    const auto& defs = get_all_widget_defs();
    REQUIRE(entries.size() == defs.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        REQUIRE(entries[i].id == defs[i].id);
        REQUIRE(entries[i].enabled == defs[i].default_enabled);
    }
}

// ============================================================================
// Config tests — duplicate IDs in saved JSON
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: duplicate IDs in saved JSON keeps only first occurrence",
                 "[home][widget_config]") {
    json widgets = json::array({
        {{"id", "power"}, {"enabled", true}},
        {{"id", "network"}, {"enabled", true}},
        {{"id", "power"}, {"enabled", false}}, // duplicate
        {{"id", "temperature"}, {"enabled", true}},
    });
    setup_with_widgets(widgets);

    HomeWidgetConfig wc(config);
    wc.load();

    REQUIRE(wc.entries().size() == widget_def_count());

    // power should appear once, with enabled=true (first occurrence)
    REQUIRE(wc.entries()[0].id == "power");
    REQUIRE(wc.entries()[0].enabled == true);

    // Verify no duplicate power entries
    int power_count = 0;
    for (const auto& e : wc.entries()) {
        if (e.id == "power") {
            ++power_count;
        }
    }
    REQUIRE(power_count == 1);
}

// ============================================================================
// Config tests — is_enabled convenience
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: is_enabled returns false for unknown ID",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    REQUIRE(wc.is_enabled("nonexistent") == false);
}

// ============================================================================
// Config tests — malformed field types
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: malformed field types skip entry but keep others",
                 "[home][widget_config]") {
    json widgets = json::array({
        {{"id", "power"}, {"enabled", true}},
        {{"id", 42}, {"enabled", true}},         // id is not string
        {{"id", "network"}, {"enabled", "yes"}}, // enabled is not bool
        {{"id", "temperature"}, {"enabled", false}},
    });
    setup_with_widgets(widgets);

    HomeWidgetConfig wc(config);
    wc.load();

    // Bad entries skipped, good entries kept, rest appended
    REQUIRE(wc.entries().size() == widget_def_count());
    REQUIRE(wc.entries()[0].id == "power");
    REQUIRE(wc.entries()[0].enabled == true);
    REQUIRE(wc.entries()[1].id == "temperature");
    REQUIRE(wc.entries()[1].enabled == false);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: home_widgets key is not an array falls back to defaults",
                 "[home][widget_config]") {
    get_data()["home_widgets"] = "corrupted";

    HomeWidgetConfig wc(config);
    wc.load();

    const auto& defs = get_all_widget_defs();
    REQUIRE(wc.entries().size() == defs.size());
    for (size_t i = 0; i < defs.size(); ++i) {
        REQUIRE(wc.entries()[i].id == defs[i].id);
        REQUIRE(wc.entries()[i].enabled == defs[i].default_enabled);
    }
}

// ============================================================================
// Config tests — set_enabled out of bounds
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: set_enabled out of bounds is a no-op",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    auto before = wc.entries();
    wc.set_enabled(999, false);
    REQUIRE(wc.entries() == before);
}

// ============================================================================
// Registry tests — field completeness
// ============================================================================

TEST_CASE("HomeWidgetRegistry: all defs have non-null required fields", "[home][widget_config]") {
    const auto& defs = get_all_widget_defs();
    for (const auto& def : defs) {
        CAPTURE(def.id);
        REQUIRE(def.id != nullptr);
        REQUIRE(def.display_name != nullptr);
        REQUIRE(def.icon != nullptr);
        REQUIRE(def.description != nullptr);
        REQUIRE(def.translation_tag != nullptr);
        // hardware_gate_subject CAN be nullptr (always-available widgets)
    }
}

TEST_CASE("HomeWidgetRegistry: all IDs are non-empty strings", "[home][widget_config]") {
    const auto& defs = get_all_widget_defs();
    for (const auto& def : defs) {
        REQUIRE(std::string_view(def.id).size() > 0);
        REQUIRE(std::string_view(def.display_name).size() > 0);
        REQUIRE(std::string_view(def.icon).size() > 0);
        REQUIRE(std::string_view(def.description).size() > 0);
    }
}

TEST_CASE("HomeWidgetRegistry: can find every registered widget by ID", "[home][widget_config]") {
    const auto& defs = get_all_widget_defs();
    for (const auto& def : defs) {
        const auto* found = find_widget_def(def.id);
        REQUIRE(found != nullptr);
        REQUIRE(found->id == std::string_view(def.id));
    }
}

TEST_CASE("HomeWidgetRegistry: known hardware-gated widgets have gate subjects",
          "[home][widget_config]") {
    // These widgets require specific hardware
    const char* gated[] = {"power", "ams", "led", "humidity", "width_sensor", "probe", "filament"};
    for (const auto* id : gated) {
        CAPTURE(id);
        const auto* def = find_widget_def(id);
        REQUIRE(def != nullptr);
        REQUIRE(def->hardware_gate_subject != nullptr);
    }
}

TEST_CASE("HomeWidgetRegistry: always-available widgets have no gate subject",
          "[home][widget_config]") {
    const char* always[] = {"network", "firmware_restart", "temperature", "notifications"};
    for (const auto* id : always) {
        CAPTURE(id);
        const auto* def = find_widget_def(id);
        REQUIRE(def != nullptr);
        REQUIRE(def->hardware_gate_subject == nullptr);
    }
}

// ============================================================================
// Config tests — reorder edge cases
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: reorder to last position works",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    size_t last = wc.entries().size() - 1;
    std::string moved_id = wc.entries()[0].id;
    wc.reorder(0, last);

    REQUIRE(wc.entries()[last].id == moved_id);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture, "HomeWidgetConfig: reorder from last to first works",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    size_t last = wc.entries().size() - 1;
    std::string moved_id = wc.entries()[last].id;
    wc.reorder(last, 0);

    REQUIRE(wc.entries()[0].id == moved_id);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: reorder preserves enabled state of moved item",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    wc.set_enabled(3, false);
    std::string moved_id = wc.entries()[3].id;
    wc.reorder(3, 0);

    REQUIRE(wc.entries()[0].id == moved_id);
    REQUIRE(wc.entries()[0].enabled == false);
}

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: multiple reorders produce correct final order",
                 "[home][widget_config]") {
    setup_empty_config();
    HomeWidgetConfig wc(config);
    wc.load();

    // Capture IDs for first 4
    std::string id0 = wc.entries()[0].id;
    std::string id1 = wc.entries()[1].id;
    std::string id2 = wc.entries()[2].id;
    std::string id3 = wc.entries()[3].id;

    // Move 0→2, then 3→1
    wc.reorder(0, 2); // [id1, id2, id0, id3, ...]
    wc.reorder(3, 1); // [id1, id3, id2, id0, ...]

    REQUIRE(wc.entries()[0].id == id1);
    REQUIRE(wc.entries()[1].id == id3);
    REQUIRE(wc.entries()[2].id == id2);
    REQUIRE(wc.entries()[3].id == id0);
}

// ============================================================================
// Config tests — save-load round trip with reorder
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: reorder + toggle + save + reload preserves everything",
                 "[home][widget_config]") {
    setup_empty_config();

    HomeWidgetConfig wc1(config);
    wc1.load();

    // Do several operations
    wc1.set_enabled(0, false);
    wc1.set_enabled(4, false);
    wc1.reorder(2, 8);
    wc1.reorder(0, 5);
    wc1.save();

    // Reload
    HomeWidgetConfig wc2(config);
    wc2.load();

    REQUIRE(wc1.entries().size() == wc2.entries().size());
    for (size_t i = 0; i < wc1.entries().size(); ++i) {
        CAPTURE(i);
        REQUIRE(wc1.entries()[i].id == wc2.entries()[i].id);
        REQUIRE(wc1.entries()[i].enabled == wc2.entries()[i].enabled);
    }
}

// ============================================================================
// Config tests — empty array in JSON
// ============================================================================

TEST_CASE_METHOD(HomeWidgetConfigFixture,
                 "HomeWidgetConfig: empty array in JSON falls back to defaults",
                 "[home][widget_config]") {
    setup_with_widgets(json::array());

    HomeWidgetConfig wc(config);
    wc.load();

    const auto& defs = get_all_widget_defs();
    REQUIRE(wc.entries().size() == defs.size());
    for (size_t i = 0; i < defs.size(); ++i) {
        REQUIRE(wc.entries()[i].id == defs[i].id);
        REQUIRE(wc.entries()[i].enabled == defs[i].default_enabled);
    }
}
