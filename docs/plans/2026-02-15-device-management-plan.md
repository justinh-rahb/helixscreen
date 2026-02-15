# Device Management System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete per-backend defaults for AFC and Happy Hare, fix mock backend to accurately represent both, and move hardcoded AFC actions into shared defaults.

**Architecture:** Per-backend defaults files define sections + actions. Mock picks from the right defaults based on mode. Real backends overlay dynamic values. Three shared section IDs (setup, speed, maintenance) are a naming convention.

**Tech Stack:** C++17, Catch2 testing, LVGL 9.4

**Design doc:** `docs/plans/2026-02-15-device-management-design.md`

---

### Task 1: Rename calibration→setup and fold LED into setup (afc_defaults)

**Files:**
- Modify: `src/printer/afc_defaults.cpp`
- Modify: `tests/unit/test_afc_defaults.cpp`

**Step 1: Update afc_defaults.cpp sections**

In `afc_default_sections()` (line 8-19):
- Change `{"calibration", "Calibration", 0, "Bowden length and lane calibration"}` → `{"setup", "Setup", 0, "Calibration, LED, and system configuration"}`
- Remove `{"led", "LED & Modes", 3, "LED control and quiet mode"}`
- Renumber: hub→3, tip_forming→4, purge→5, config→6
- Result: 7 sections instead of 8

**Step 2: Update afc_defaults.cpp actions**

In `afc_default_actions()` (line 21-216):
- Change `calibration_wizard` action's `section` field from `"calibration"` to `"setup"`
- Change `bowden_length` action's `section` field from `"calibration"` to `"setup"`
- Change `led_toggle` action's `section` field from `"led"` to `"setup"`
- Change `quiet_mode` action's `section` field from `"led"` to `"setup"`

**Step 3: Add hub/tip_forming/purge/config actions to defaults**

In `afc_default_actions()`, add after the existing actions (after the quiet_mode action around line 213):

```cpp
// --- Hub & Cutter section ---
{
    DeviceAction a;
    a.id = "hub_cut_enabled";
    a.label = "Cutter Enabled";
    a.section = "hub";
    a.type = ActionType::TOGGLE;
    a.current_value = false;
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "hub_cut_dist";
    a.label = "Cut Distance";
    a.section = "hub";
    a.type = ActionType::SLIDER;
    a.current_value = 50.0;
    a.min_value = 0.0;
    a.max_value = 100.0;
    a.unit = "mm";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "hub_bowden_length";
    a.label = "Hub Bowden Length";
    a.section = "hub";
    a.type = ActionType::SLIDER;
    a.current_value = 450.0;
    a.min_value = 100.0;
    a.max_value = 2000.0;
    a.unit = "mm";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "assisted_retract";
    a.label = "Assisted Retract";
    a.section = "hub";
    a.type = ActionType::TOGGLE;
    a.current_value = false;
    a.enabled = true;
    actions.push_back(std::move(a));
}

// --- Tip Forming section ---
{
    DeviceAction a;
    a.id = "ramming_volume";
    a.label = "Ramming Volume";
    a.section = "tip_forming";
    a.type = ActionType::SLIDER;
    a.current_value = 0.0;
    a.min_value = 0.0;
    a.max_value = 100.0;
    a.unit = "";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "unloading_speed_start";
    a.label = "Unloading Start Speed";
    a.section = "tip_forming";
    a.type = ActionType::SLIDER;
    a.current_value = 80.0;
    a.min_value = 0.0;
    a.max_value = 200.0;
    a.unit = "mm/s";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "cooling_tube_length";
    a.label = "Cooling Tube Length";
    a.section = "tip_forming";
    a.type = ActionType::SLIDER;
    a.current_value = 15.0;
    a.min_value = 0.0;
    a.max_value = 100.0;
    a.unit = "mm";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "cooling_tube_retraction";
    a.label = "Cooling Tube Retraction";
    a.section = "tip_forming";
    a.type = ActionType::SLIDER;
    a.current_value = 0.0;
    a.min_value = 0.0;
    a.max_value = 100.0;
    a.unit = "mm";
    a.enabled = true;
    actions.push_back(std::move(a));
}

// --- Purge & Wipe section ---
{
    DeviceAction a;
    a.id = "purge_enabled";
    a.label = "Enable Purge";
    a.section = "purge";
    a.type = ActionType::TOGGLE;
    a.current_value = false;
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "purge_length";
    a.label = "Purge Length";
    a.section = "purge";
    a.type = ActionType::SLIDER;
    a.current_value = 50.0;
    a.min_value = 0.0;
    a.max_value = 200.0;
    a.unit = "mm";
    a.enabled = true;
    actions.push_back(std::move(a));
}
{
    DeviceAction a;
    a.id = "brush_enabled";
    a.label = "Enable Brush Wipe";
    a.section = "purge";
    a.type = ActionType::TOGGLE;
    a.current_value = false;
    a.enabled = true;
    actions.push_back(std::move(a));
}

// --- Configuration section ---
{
    DeviceAction a;
    a.id = "save_restart";
    a.label = "Save & Restart";
    a.section = "config";
    a.type = ActionType::BUTTON;
    a.enabled = false;
    a.disable_reason = "No unsaved changes";
    actions.push_back(std::move(a));
}
```

Note: The function currently returns a vector directly. It will need to be changed to build a `std::vector<DeviceAction> actions;` and push into it, then return. Match the existing action construction pattern in the file.

**Step 4: Update test_afc_defaults.cpp**

Update the following test expectations:
- Section count: 8 → 7
- Known section IDs: replace `"calibration"` with `"setup"`, remove `"led"`
- Action count: 11 → 23
- Known action IDs: add all 12 new action IDs
- Section assignments: update `calibration_wizard`→`setup`, `bowden_length`→`setup`, `led_toggle`→`setup`, `quiet_mode`→`setup`
- Add section assignment checks for new actions (hub, tip_forming, purge, config)

**Step 5: Run tests**

Run: `make test-run 2>&1 | grep -E "afc_defaults|PASSED|FAILED"`
Expected: All afc_defaults tests pass

**Step 6: Build**

Run: `make -j`
Expected: Clean build

**Step 7: Commit**

```bash
git add src/printer/afc_defaults.cpp tests/unit/test_afc_defaults.cpp
git commit -m "refactor(ams): rename calibration→setup, fold LED, add hub/tip/purge/config defaults"
```

---

### Task 2: Update AFC backend to use defaults for hub/tip/purge/config

**Files:**
- Modify: `src/printer/ams_backend_afc.cpp`

**Context:** The AFC backend's `get_device_actions()` (line 1968-2252) currently starts from `afc_default_actions()` and then hardcodes hub/tip_forming/purge/config actions. After Task 1, those actions are in defaults. The backend now only needs to **overlay dynamic values** from config files onto the defaults — not define the actions from scratch.

**Step 1: Refactor get_device_actions()**

Replace the hardcoded hub/tip_forming/purge/config action blocks (lines ~2024-2249) with a pattern that:
1. Starts from `afc_default_actions()` (already does this)
2. Overlays dynamic values for ALL actions, not just calibration/speed ones
3. For hub actions: overlay values from `afc_hub_config_` if loaded, otherwise mark disabled with "AFC config not loaded"
4. For tip_forming actions: overlay values from `macro_vars_` if loaded, otherwise mark disabled
5. For purge actions: overlay values from `macro_vars_` if loaded, otherwise mark disabled
6. For config action: enable `save_restart` only if `has_unsaved_changes()`

The overlay pattern should iterate over actions and match by ID:
```cpp
for (auto& action : actions) {
    if (action.id == "hub_cut_enabled") {
        if (afc_hub_config_loaded_) {
            action.current_value = hub_cut_enabled_;
            action.enabled = true;
        } else {
            action.enabled = false;
            action.disable_reason = "AFC config not loaded";
        }
    }
    // ... etc for each config-backed action
}
```

**Step 2: Update section ID references**

In the same file, any references to `"calibration"` section must change to `"setup"`. Search for string `"calibration"` in the file.

**Step 3: Build and test**

Run: `make -j && make test-run 2>&1 | grep -E "afc|PASSED|FAILED"`
Expected: Clean build, all AMS tests pass

**Step 4: Commit**

```bash
git add src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams): AFC backend overlays dynamic values on shared defaults"
```

---

### Task 3: Create Happy Hare defaults

**Files:**
- Create: `include/hh_defaults.h`
- Create: `src/printer/hh_defaults.cpp`
- Create: `tests/unit/test_hh_defaults.cpp`

**Step 1: Write the failing test**

Create `tests/unit/test_hh_defaults.cpp`:

```cpp
// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hh_defaults.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>

using namespace helix::printer;

// ============================================================================
// Section Tests
// ============================================================================

TEST_CASE("HH default sections: count", "[ams][hh_defaults]") {
    auto sections = hh_default_sections();
    REQUIRE(sections.size() == 3);
}

TEST_CASE("HH default sections: required fields", "[ams][hh_defaults]") {
    for (const auto& s : hh_default_sections()) {
        REQUIRE(!s.id.empty());
        REQUIRE(!s.label.empty());
        REQUIRE(s.display_order >= 0);
        REQUIRE(!s.description.empty());
    }
}

TEST_CASE("HH default sections: known IDs", "[ams][hh_defaults]") {
    auto sections = hh_default_sections();
    std::set<std::string> ids;
    for (const auto& s : sections) {
        ids.insert(s.id);
    }
    REQUIRE(ids.count("setup") == 1);
    REQUIRE(ids.count("speed") == 1);
    REQUIRE(ids.count("maintenance") == 1);
}

TEST_CASE("HH default sections: display order", "[ams][hh_defaults]") {
    auto sections = hh_default_sections();
    for (size_t i = 1; i < sections.size(); i++) {
        REQUIRE(sections[i].display_order > sections[i - 1].display_order);
    }
}

TEST_CASE("HH default sections: unique IDs", "[ams][hh_defaults]") {
    auto sections = hh_default_sections();
    std::set<std::string> ids;
    for (const auto& s : sections) {
        ids.insert(s.id);
    }
    REQUIRE(ids.size() == sections.size());
}

// ============================================================================
// Action Tests
// ============================================================================

TEST_CASE("HH default actions: count", "[ams][hh_defaults]") {
    auto actions = hh_default_actions();
    REQUIRE(actions.size() == 15);
}

TEST_CASE("HH default actions: required fields", "[ams][hh_defaults]") {
    for (const auto& a : hh_default_actions()) {
        REQUIRE(!a.id.empty());
        REQUIRE(!a.label.empty());
        REQUIRE(!a.section.empty());
    }
}

TEST_CASE("HH default actions: unique IDs", "[ams][hh_defaults]") {
    auto actions = hh_default_actions();
    std::set<std::string> ids;
    for (const auto& a : actions) {
        ids.insert(a.id);
    }
    REQUIRE(ids.size() == actions.size());
}

TEST_CASE("HH default actions: known IDs", "[ams][hh_defaults]") {
    auto actions = hh_default_actions();
    std::set<std::string> ids;
    for (const auto& a : actions) {
        ids.insert(a.id);
    }
    // Setup
    REQUIRE(ids.count("calibrate_bowden") == 1);
    REQUIRE(ids.count("calibrate_encoder") == 1);
    REQUIRE(ids.count("calibrate_gear") == 1);
    REQUIRE(ids.count("calibrate_gates") == 1);
    REQUIRE(ids.count("led_mode") == 1);
    REQUIRE(ids.count("calibrate_servo") == 1);
    // Speed
    REQUIRE(ids.count("gear_load_speed") == 1);
    REQUIRE(ids.count("gear_unload_speed") == 1);
    REQUIRE(ids.count("selector_speed") == 1);
    // Maintenance
    REQUIRE(ids.count("test_grip") == 1);
    REQUIRE(ids.count("test_load") == 1);
    REQUIRE(ids.count("motors_toggle") == 1);
    REQUIRE(ids.count("servo_buzz") == 1);
    REQUIRE(ids.count("reset_servo_counter") == 1);
    REQUIRE(ids.count("reset_blade_counter") == 1);
}

TEST_CASE("HH default actions: section assignments", "[ams][hh_defaults]") {
    auto actions = hh_default_actions();
    auto sections = hh_default_sections();

    std::set<std::string> valid_sections;
    for (const auto& s : sections) {
        valid_sections.insert(s.id);
    }

    for (const auto& a : actions) {
        REQUIRE(valid_sections.count(a.section) == 1);
    }
}

TEST_CASE("HH default actions: slider ranges valid", "[ams][hh_defaults]") {
    for (const auto& a : hh_default_actions()) {
        if (a.type == ActionType::SLIDER) {
            REQUIRE(a.min_value < a.max_value);
            REQUIRE(!a.unit.empty());
        }
    }
}

TEST_CASE("HH default actions: dropdown has options", "[ams][hh_defaults]") {
    for (const auto& a : hh_default_actions()) {
        if (a.type == ActionType::DROPDOWN) {
            REQUIRE(a.options.size() >= 2);
        }
    }
}
```

**Step 2: Run test to verify it fails**

Run: `make test 2>&1 | tail -5`
Expected: FAIL — `hh_defaults.h` not found

**Step 3: Create header**

Create `include/hh_defaults.h`:

```cpp
// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ams_types.h"

#include <vector>

/**
 * @file hh_defaults.h
 * @brief Default device sections and actions for Happy Hare backends
 *
 * Provides the canonical section/action definitions for Happy Hare MMU systems.
 * Used by both the real backend (as a starting point for dynamic overlay) and
 * the mock backend (directly).
 *
 * Shared section IDs with AFC: "setup", "speed", "maintenance"
 */
namespace helix::printer {

/**
 * @brief Default device sections for Happy Hare
 *
 * Returns 3 sections: setup, speed, maintenance.
 * Section IDs match AFC convention for UI consistency.
 */
std::vector<DeviceSection> hh_default_sections();

/**
 * @brief Default device actions for Happy Hare
 *
 * Returns core essential actions for each section.
 * Real backend overlays dynamic values from MMU state.
 */
std::vector<DeviceAction> hh_default_actions();

} // namespace helix::printer
```

**Step 4: Create implementation**

Create `src/printer/hh_defaults.cpp`:

```cpp
// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hh_defaults.h"

namespace helix::printer {

std::vector<DeviceSection> hh_default_sections() {
    return {
        {"setup", "Setup", 0, "Calibration and system configuration"},
        {"speed", "Speed", 1, "Motor speeds and acceleration"},
        {"maintenance", "Maintenance", 2, "Testing, servo, and motor operations"},
    };
}

std::vector<DeviceAction> hh_default_actions() {
    std::vector<DeviceAction> actions;

    // --- Setup section ---
    {
        DeviceAction a;
        a.id = "calibrate_bowden";
        a.label = "Calibrate Bowden";
        a.section = "setup";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "calibrate_encoder";
        a.label = "Calibrate Encoder";
        a.section = "setup";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "calibrate_gear";
        a.label = "Calibrate Gear";
        a.section = "setup";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "calibrate_gates";
        a.label = "Calibrate Gates";
        a.section = "setup";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "led_mode";
        a.label = "LED Mode";
        a.section = "setup";
        a.type = ActionType::DROPDOWN;
        a.options = {"off", "gate_status", "filament_color", "on"};
        a.current_value = std::string("off");
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "calibrate_servo";
        a.label = "Calibrate Servo";
        a.section = "setup";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }

    // --- Speed section ---
    {
        DeviceAction a;
        a.id = "gear_load_speed";
        a.label = "Gear Load Speed";
        a.section = "speed";
        a.type = ActionType::SLIDER;
        a.current_value = 150.0;
        a.min_value = 10.0;
        a.max_value = 300.0;
        a.unit = "mm/s";
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "gear_unload_speed";
        a.label = "Gear Unload Speed";
        a.section = "speed";
        a.type = ActionType::SLIDER;
        a.current_value = 150.0;
        a.min_value = 10.0;
        a.max_value = 300.0;
        a.unit = "mm/s";
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "selector_speed";
        a.label = "Selector Speed";
        a.section = "speed";
        a.type = ActionType::SLIDER;
        a.current_value = 200.0;
        a.min_value = 10.0;
        a.max_value = 300.0;
        a.unit = "mm/s";
        a.enabled = true;
        actions.push_back(std::move(a));
    }

    // --- Maintenance section ---
    {
        DeviceAction a;
        a.id = "test_grip";
        a.label = "Test Grip";
        a.section = "maintenance";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "test_load";
        a.label = "Test Load";
        a.section = "maintenance";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "motors_toggle";
        a.label = "Motors";
        a.section = "maintenance";
        a.type = ActionType::TOGGLE;
        a.current_value = true;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "servo_buzz";
        a.label = "Buzz Servo";
        a.section = "maintenance";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "reset_servo_counter";
        a.label = "Reset Servo Counter";
        a.section = "maintenance";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }
    {
        DeviceAction a;
        a.id = "reset_blade_counter";
        a.label = "Reset Blade Counter";
        a.section = "maintenance";
        a.type = ActionType::BUTTON;
        a.enabled = true;
        actions.push_back(std::move(a));
    }

    return actions;
}

} // namespace helix::printer
```

**Step 5: Run tests**

Run: `make test-run 2>&1 | grep -E "hh_defaults|PASSED|FAILED"`
Expected: All hh_defaults tests pass

**Step 6: Commit**

```bash
git add include/hh_defaults.h src/printer/hh_defaults.cpp tests/unit/test_hh_defaults.cpp
git commit -m "feat(ams): add Happy Hare defaults (setup, speed, maintenance)"
```

---

### Task 4: Fix mock backend — AFC mode uses all defaults, HH mode uses hh_defaults

**Files:**
- Modify: `src/printer/ams_backend_mock.cpp`
- Modify: `src/printer/ams_backend_mock.cpp` (constructor default init)

**Step 1: Add hh_defaults include**

At the top of `ams_backend_mock.cpp`, add:
```cpp
#include "hh_defaults.h"
```

**Step 2: Fix AFC mode — remove section filtering**

In `set_afc_mode(true)` (around line 1151-1173), replace the section filtering with:
```cpp
mock_device_sections_ = helix::printer::afc_default_sections();
mock_device_actions_ = helix::printer::afc_default_actions();
```

No more filtering by section ID. The mock gets ALL AFC sections and actions.

For the `save_restart` action, mark it disabled for mock:
```cpp
for (auto& action : mock_device_actions_) {
    if (action.id == "save_restart") {
        action.enabled = false;
        action.disable_reason = "Not available in mock mode";
    }
}
```

**Step 3: Fix HH mode — use hh_defaults instead of AFC subset**

In `set_afc_mode(false)` (around line 1190-1208), replace the section filtering with:
```cpp
mock_device_sections_ = helix::printer::hh_default_sections();
mock_device_actions_ = helix::printer::hh_default_actions();
```

**Step 4: Fix constructor default init**

In the constructor (around line 57-197), the mock starts in Happy Hare mode. After `init_mock_data()`, set:
```cpp
mock_device_sections_ = helix::printer::hh_default_sections();
mock_device_actions_ = helix::printer::hh_default_actions();
```

Check: the constructor may not currently set sections/actions at all for the default HH mode. Search for where `mock_device_sections_` is first populated — it might only happen in `set_afc_mode()`. If so, add it to the constructor or `init_mock_data()`.

**Step 5: Update section ID references**

Search for `"calibration"` in the mock backend. The calibration_wizard action_prompt handler (around line 1820) references `action_id == "calibration_wizard"` — this stays the same (it's an action ID, not a section ID). But verify no section ID filtering remains.

**Step 6: Build and test**

Run: `make -j && make test-run 2>&1 | grep -E "ams|mock|PASSED|FAILED"`
Expected: Clean build, all tests pass

**Step 7: Commit**

```bash
git add src/printer/ams_backend_mock.cpp
git commit -m "fix(ams): mock uses complete AFC defaults and HH defaults per mode"
```

---

### Task 5: Update UI icon mapping

**Files:**
- Modify: `src/ui/ui_ams_device_operations_overlay.cpp`

**Step 1: Update section_icon_for_id()**

In `section_icon_for_id()` (around line 285-304):
- Change `if (id == "calibration") return "wrench";` → `if (id == "setup") return "cog";`
- Remove `if (id == "led") return "lightbulb_outline";`
- Everything else stays (hub, tip_forming, purge, config, maintenance, speed all have correct icons)

**Step 2: Build**

Run: `make -j`
Expected: Clean build

**Step 3: Commit**

```bash
git add src/ui/ui_ams_device_operations_overlay.cpp
git commit -m "fix(ams): update section icon mapping for setup rename"
```

---

### Task 6: Verification

**Step 1: Run all AMS tests**

Run: `make test-run 2>&1 | grep -E "\[ams\]|PASSED|FAILED|assertions"`
Expected: All passing. Note pre-existing failures (AFC_HOME/AFC_RESET) are unrelated.

**Step 2: Visual verification**

Run: `./build/bin/helix-screen --test -vv`

Verify in AFC mock mode (`HELIX_MOCK_AMS_TYPE=afc`):
- AMS Management shows 7 sections: Setup, Speed, Maintenance, Hub & Cutter, Tip Forming, Purge & Wipe, Configuration
- Setup section contains: calibration wizard, bowden length, LED toggle, quiet mode
- Hub/Tip/Purge sections show their controls
- Configuration shows disabled "Save & Restart"
- Calibration wizard button triggers action_prompt dialog

Verify in default HH mock mode:
- AMS Management shows 3 sections: Setup, Speed, Maintenance
- Setup contains: 4 calibration buttons, LED mode dropdown, servo calibration
- Speed contains: 3 speed sliders
- Maintenance contains: test buttons, motor toggle, counter resets

**Step 3: Commit any fixes**

If visual verification reveals issues, fix and commit.
