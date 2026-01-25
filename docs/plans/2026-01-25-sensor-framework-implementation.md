# Sensor Framework Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a unified sensor management framework supporting switch, width, color, humidity, probe, and accelerometer sensors with TDD.

**Architecture:** Template base class + registry pattern. Each sensor category is self-contained with its own types, manager, and subjects. See `docs/plans/2026-01-25-sensor-framework-design.md` for full design.

**Tech Stack:** C++17, LVGL 9.4, Catch2 testing, nlohmann/json

---

## Task 1: Create src/sensors directory

**Files:**
- Create: `src/sensors/.gitkeep`

**Step 1: Create directory**
```bash
mkdir -p src/sensors
touch src/sensors/.gitkeep
```

**Step 2: Commit**
```bash
git add src/sensors/.gitkeep
git commit -m "chore: create src/sensors directory for sensor managers"
```

---

## Task 2: Sensor Registry Interface + Failing Tests

**Files:**
- Create: `include/sensor_registry.h`
- Create: `tests/unit/test_sensor_registry.cpp`

**Step 1: Write the failing test**

Create `tests/unit/test_sensor_registry.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include "sensor_registry.h"

using namespace helix::sensors;

// Mock sensor manager for testing
class MockSensorManager : public ISensorManager {
public:
    std::string name_;
    bool discovered_ = false;
    bool status_updated_ = false;
    nlohmann::json last_status_;

    explicit MockSensorManager(std::string name) : name_(std::move(name)) {}

    std::string category_name() const override { return name_; }

    void discover(const std::vector<std::string>& objects) override {
        discovered_ = true;
    }

    void update_from_status(const nlohmann::json& status) override {
        status_updated_ = true;
        last_status_ = status;
    }

    void load_config(const nlohmann::json& config) override {}
    nlohmann::json save_config() const override { return {}; }
};

TEST_CASE("SensorRegistry registers managers", "[sensors]") {
    SensorRegistry registry;

    auto mock = std::make_unique<MockSensorManager>("test");
    auto* mock_ptr = mock.get();

    registry.register_manager("test", std::move(mock));

    REQUIRE(registry.get_manager("test") == mock_ptr);
    REQUIRE(registry.get_manager("nonexistent") == nullptr);
}

TEST_CASE("SensorRegistry routes discover to all managers", "[sensors]") {
    SensorRegistry registry;

    auto mock1 = std::make_unique<MockSensorManager>("sensor1");
    auto mock2 = std::make_unique<MockSensorManager>("sensor2");
    auto* ptr1 = mock1.get();
    auto* ptr2 = mock2.get();

    registry.register_manager("sensor1", std::move(mock1));
    registry.register_manager("sensor2", std::move(mock2));

    std::vector<std::string> objects = {"filament_switch_sensor foo"};
    registry.discover_all(objects);

    REQUIRE(ptr1->discovered_);
    REQUIRE(ptr2->discovered_);
}

TEST_CASE("SensorRegistry routes status updates to all managers", "[sensors]") {
    SensorRegistry registry;

    auto mock = std::make_unique<MockSensorManager>("test");
    auto* mock_ptr = mock.get();

    registry.register_manager("test", std::move(mock));

    nlohmann::json status = {{"filament_switch_sensor foo", {{"filament_detected", true}}}};
    registry.update_all_from_status(status);

    REQUIRE(mock_ptr->status_updated_);
    REQUIRE(mock_ptr->last_status_ == status);
}
```

**Step 2: Create minimal interface header**

Create `include/sensor_registry.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "hv/json.hpp"

namespace helix::sensors {

/// @brief Interface for sensor category managers
class ISensorManager {
public:
    virtual ~ISensorManager() = default;

    /// @brief Get the category name (e.g., "switch", "humidity")
    [[nodiscard]] virtual std::string category_name() const = 0;

    /// @brief Discover sensors from Klipper object list
    virtual void discover(const std::vector<std::string>& klipper_objects) = 0;

    /// @brief Update state from Moonraker status JSON
    virtual void update_from_status(const nlohmann::json& status) = 0;

    /// @brief Load configuration from JSON
    virtual void load_config(const nlohmann::json& config) = 0;

    /// @brief Save configuration to JSON
    [[nodiscard]] virtual nlohmann::json save_config() const = 0;
};

/// @brief Central registry for all sensor managers
class SensorRegistry {
public:
    SensorRegistry() = default;
    ~SensorRegistry() = default;

    // Non-copyable
    SensorRegistry(const SensorRegistry&) = delete;
    SensorRegistry& operator=(const SensorRegistry&) = delete;

    /// @brief Register a sensor manager
    void register_manager(std::string category, std::unique_ptr<ISensorManager> manager);

    /// @brief Get a manager by category name
    [[nodiscard]] ISensorManager* get_manager(const std::string& category) const;

    /// @brief Discover sensors in all registered managers
    void discover_all(const std::vector<std::string>& klipper_objects);

    /// @brief Route status update to all managers
    void update_all_from_status(const nlohmann::json& status);

    /// @brief Load config for all managers
    void load_config(const nlohmann::json& root_config);

    /// @brief Save config from all managers
    [[nodiscard]] nlohmann::json save_config() const;

private:
    std::map<std::string, std::unique_ptr<ISensorManager>> managers_;
};

}  // namespace helix::sensors
```

**Step 3: Run test to verify it fails**

```bash
make test-run CATCH_ARGS="[sensors]"
```

Expected: FAIL - linker error, SensorRegistry methods not implemented

---

## Task 3: Implement SensorRegistry

**Files:**
- Create: `src/sensors/sensor_registry.cpp`
- Modify: `Makefile` (add to SOURCES)

**Step 1: Implement registry**

Create `src/sensors/sensor_registry.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sensor_registry.h"
#include <spdlog/spdlog.h>

namespace helix::sensors {

void SensorRegistry::register_manager(std::string category, std::unique_ptr<ISensorManager> manager) {
    if (!manager) {
        spdlog::warn("[SensorRegistry] Attempted to register null manager for category '{}'", category);
        return;
    }
    spdlog::info("[SensorRegistry] Registering sensor manager: {}", category);
    managers_[std::move(category)] = std::move(manager);
}

ISensorManager* SensorRegistry::get_manager(const std::string& category) const {
    auto it = managers_.find(category);
    if (it != managers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SensorRegistry::discover_all(const std::vector<std::string>& klipper_objects) {
    for (auto& [category, manager] : managers_) {
        manager->discover(klipper_objects);
    }
}

void SensorRegistry::update_all_from_status(const nlohmann::json& status) {
    for (auto& [category, manager] : managers_) {
        manager->update_from_status(status);
    }
}

void SensorRegistry::load_config(const nlohmann::json& root_config) {
    if (!root_config.contains("sensors")) {
        return;
    }

    const auto& sensors_config = root_config["sensors"];
    for (auto& [category, manager] : managers_) {
        if (sensors_config.contains(category)) {
            manager->load_config(sensors_config[category]);
        }
    }
}

nlohmann::json SensorRegistry::save_config() const {
    nlohmann::json result;
    nlohmann::json sensors_config;

    for (const auto& [category, manager] : managers_) {
        sensors_config[category] = manager->save_config();
    }

    result["sensors"] = sensors_config;
    return result;
}

}  // namespace helix::sensors
```

**Step 2: Add to Makefile**

Find the SOURCES section and add:
```makefile
SOURCES += src/sensors/sensor_registry.cpp
```

**Step 3: Run test to verify it passes**

```bash
make test-run CATCH_ARGS="[sensors]"
```

Expected: PASS - all 3 tests pass

**Step 4: Commit**

```bash
git add include/sensor_registry.h src/sensors/sensor_registry.cpp tests/unit/test_sensor_registry.cpp Makefile
git commit -m "feat(sensors): add SensorRegistry for managing sensor categories"
```

---

## Task 4: Switch Sensor Types Header

**Files:**
- Create: `include/switch_sensor_types.h`

**Step 1: Create types header**

Create `include/switch_sensor_types.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace helix::sensors {

/// @brief Role assigned to a switch sensor
enum class SwitchSensorRole {
    NONE = 0,             ///< Discovered but not assigned
    FILAMENT_RUNOUT = 1,  ///< Primary filament runout detection
    FILAMENT_TOOLHEAD = 2,///< Toolhead filament detection
    FILAMENT_ENTRY = 3,   ///< Entry point filament detection
    Z_PROBE = 10,         ///< Z probing sensor
    DOCK_DETECT = 20,     ///< Dock presence detection
};

/// @brief Type of switch sensor hardware
enum class SwitchSensorType {
    SWITCH = 1,  ///< filament_switch_sensor
    MOTION = 2,  ///< filament_motion_sensor
};

/// @brief Configuration for a switch sensor
struct SwitchSensorConfig {
    std::string klipper_name;  ///< Full Klipper name (e.g., "filament_switch_sensor e1")
    std::string sensor_name;   ///< Short name (e.g., "e1")
    SwitchSensorType type = SwitchSensorType::SWITCH;
    SwitchSensorRole role = SwitchSensorRole::NONE;
    bool enabled = true;
};

/// @brief Runtime state for a switch sensor
struct SwitchSensorState {
    bool triggered = false;    ///< filament_detected or probe triggered
    bool enabled = true;       ///< Sensor enabled flag from Klipper
    int detection_count = 0;   ///< For motion sensors
    bool available = false;    ///< Sensor available in current config
};

/// @brief Convert role enum to config string
[[nodiscard]] inline std::string switch_role_to_string(SwitchSensorRole role) {
    switch (role) {
        case SwitchSensorRole::NONE: return "none";
        case SwitchSensorRole::FILAMENT_RUNOUT: return "filament_runout";
        case SwitchSensorRole::FILAMENT_TOOLHEAD: return "filament_toolhead";
        case SwitchSensorRole::FILAMENT_ENTRY: return "filament_entry";
        case SwitchSensorRole::Z_PROBE: return "z_probe";
        case SwitchSensorRole::DOCK_DETECT: return "dock_detect";
        default: return "none";
    }
}

/// @brief Parse role string to enum
[[nodiscard]] inline SwitchSensorRole switch_role_from_string(const std::string& str) {
    if (str == "filament_runout") return SwitchSensorRole::FILAMENT_RUNOUT;
    if (str == "filament_toolhead") return SwitchSensorRole::FILAMENT_TOOLHEAD;
    if (str == "filament_entry") return SwitchSensorRole::FILAMENT_ENTRY;
    if (str == "z_probe") return SwitchSensorRole::Z_PROBE;
    if (str == "dock_detect") return SwitchSensorRole::DOCK_DETECT;
    return SwitchSensorRole::NONE;
}

/// @brief Convert role to display string
[[nodiscard]] inline std::string switch_role_to_display_string(SwitchSensorRole role) {
    switch (role) {
        case SwitchSensorRole::NONE: return "None";
        case SwitchSensorRole::FILAMENT_RUNOUT: return "Runout";
        case SwitchSensorRole::FILAMENT_TOOLHEAD: return "Toolhead";
        case SwitchSensorRole::FILAMENT_ENTRY: return "Entry";
        case SwitchSensorRole::Z_PROBE: return "Z Probe";
        case SwitchSensorRole::DOCK_DETECT: return "Dock Detect";
        default: return "None";
    }
}

/// @brief Check if role is a filament-related role
[[nodiscard]] inline bool is_filament_role(SwitchSensorRole role) {
    return role == SwitchSensorRole::FILAMENT_RUNOUT ||
           role == SwitchSensorRole::FILAMENT_TOOLHEAD ||
           role == SwitchSensorRole::FILAMENT_ENTRY;
}

/// @brief Check if role is a probe-related role
[[nodiscard]] inline bool is_probe_role(SwitchSensorRole role) {
    return role == SwitchSensorRole::Z_PROBE;
}

}  // namespace helix::sensors
```

**Step 2: Commit**

```bash
git add include/switch_sensor_types.h
git commit -m "feat(sensors): add SwitchSensorRole and SwitchSensorConfig types"
```

---

## Task 5: Switch Sensor Manager - Failing Tests for Probe Role

**Files:**
- Modify: `tests/unit/test_filament_sensor_manager.cpp` → rename to `tests/unit/test_switch_sensor_manager.cpp`
- Create new tests for probe role

**Step 1: Rename test file**

```bash
git mv tests/unit/test_filament_sensor_manager.cpp tests/unit/test_switch_sensor_manager.cpp
```

**Step 2: Add failing tests for probe role**

Add to `tests/unit/test_switch_sensor_manager.cpp`:

```cpp
// Add these test cases at the end of the file

TEST_CASE("SwitchSensorManager handles Z_PROBE role", "[sensors][switch]") {
    // This test will fail until we implement the new role
    SwitchSensorManager manager;
    manager.set_sync_mode(true);

    // Discover a sensor
    std::vector<std::string> objects = {"filament_switch_sensor e1_sensor"};
    manager.discover_sensors(objects);

    // Assign probe role
    manager.set_sensor_role("filament_switch_sensor e1_sensor", SwitchSensorRole::Z_PROBE);

    auto sensors = manager.get_sensors();
    REQUIRE(sensors.size() == 1);
    REQUIRE(sensors[0].role == SwitchSensorRole::Z_PROBE);
}

TEST_CASE("SwitchSensorManager updates probe_switch_triggered subject", "[sensors][switch]") {
    SwitchSensorManager manager;
    manager.set_sync_mode(true);

    // Discover and assign probe role
    std::vector<std::string> objects = {"filament_switch_sensor probe"};
    manager.discover_sensors(objects);
    manager.set_sensor_role("filament_switch_sensor probe", SwitchSensorRole::Z_PROBE);

    // Initial state should be -1 (not triggered, sensor exists but no update yet)
    auto* subject = manager.get_probe_switch_triggered_subject();
    REQUIRE(subject != nullptr);

    // Send status update with triggered state
    nlohmann::json status = {
        {"filament_switch_sensor probe", {{"filament_detected", true}}}
    };
    manager.update_from_status(status);

    REQUIRE(lv_subject_get_int(subject) == 1);  // Triggered

    // Update to not triggered
    status["filament_switch_sensor probe"]["filament_detected"] = false;
    manager.update_from_status(status);

    REQUIRE(lv_subject_get_int(subject) == 0);  // Not triggered
}

TEST_CASE("SwitchSensorManager is_probe_triggered query", "[sensors][switch]") {
    SwitchSensorManager manager;
    manager.set_sync_mode(true);

    // No probe assigned
    REQUIRE_FALSE(manager.is_probe_triggered());

    // Discover and assign
    std::vector<std::string> objects = {"filament_switch_sensor e1"};
    manager.discover_sensors(objects);
    manager.set_sensor_role("filament_switch_sensor e1", SwitchSensorRole::Z_PROBE);

    // Update state
    nlohmann::json status = {{"filament_switch_sensor e1", {{"filament_detected", true}}}};
    manager.update_from_status(status);

    REQUIRE(manager.is_probe_triggered());
}
```

**Step 3: Run tests to verify they fail**

```bash
make test-run CATCH_ARGS="[switch]"
```

Expected: FAIL - SwitchSensorRole::Z_PROBE not defined, methods don't exist

**Step 4: Commit failing tests**

```bash
git add tests/unit/test_switch_sensor_manager.cpp
git commit -m "test(sensors): add failing tests for Z_PROBE role in switch sensors"
```

---

## Task 6: Implement Z_PROBE Role in Switch Sensor Manager

**Files:**
- Modify: `include/filament_sensor_manager.h` → rename to `include/switch_sensor_manager.h`
- Modify: `src/print/filament_sensor_manager.cpp` → move to `src/sensors/switch_sensor_manager.cpp`

**Step 1: Rename header file**

```bash
git mv include/filament_sensor_manager.h include/switch_sensor_manager.h
```

**Step 2: Update header with new types and methods**

Edit `include/switch_sensor_manager.h`:
- Include `switch_sensor_types.h`
- Replace `FilamentSensorRole` with `SwitchSensorRole`
- Add `probe_switch_triggered_` subject
- Add `get_probe_switch_triggered_subject()` method
- Add `is_probe_triggered()` method

(Detailed code changes depend on current file contents - implement until tests pass)

**Step 3: Move and rename implementation**

```bash
git mv src/print/filament_sensor_manager.cpp src/sensors/switch_sensor_manager.cpp
```

**Step 4: Update implementation**

Edit `src/sensors/switch_sensor_manager.cpp`:
- Update includes
- Replace `FilamentSensorRole` with `SwitchSensorRole`
- Add probe subject initialization
- Update `update_subjects()` to handle probe role
- Implement `is_probe_triggered()`

**Step 5: Update Makefile**

Update SOURCES to replace old path with new:
```makefile
# Remove: src/print/filament_sensor_manager.cpp
# Add: src/sensors/switch_sensor_manager.cpp
```

**Step 6: Update all includes**

Find and update all files that include the old header:
```bash
grep -r "filament_sensor_manager.h" --include="*.cpp" --include="*.h"
```

**Step 7: Run tests to verify they pass**

```bash
make test-run CATCH_ARGS="[switch]"
```

Expected: PASS - all switch sensor tests pass

**Step 8: Commit**

```bash
git add -A
git commit -m "feat(sensors): rename FilamentSensorManager to SwitchSensorManager with Z_PROBE role"
```

---

## Tasks 7-14: Remaining Sensor Managers (TDD Pattern)

Each sensor manager follows the same TDD pattern:

1. **Write failing tests** for discovery, state updates, subjects
2. **Create types header** with role enum, config struct, state struct
3. **Create manager header** with interface
4. **Implement manager** until tests pass
5. **Commit**

### Task 7: Width Sensor Manager
### Task 8: Color Sensor Manager
### Task 9: Humidity Sensor Manager
### Task 10: Probe Sensor Manager
### Task 11: Accelerometer Manager

(Each follows the same structure as Tasks 2-6)

---

## Task 15: Home Panel Indicators

**Files:**
- Create: `ui_xml/humidity_indicator.xml`
- Create: `ui_xml/probe_indicator.xml`
- Create: `ui_xml/width_indicator.xml`
- Modify: `ui_xml/home_panel.xml`

(Detailed steps for XML components)

---

## Task 16: Settings Overlay

**Files:**
- Rename: `ui_xml/filament_sensors_overlay.xml` → `ui_xml/sensors_overlay.xml`
- Rename: `src/ui/ui_settings_filament_sensors.cpp` → `src/ui/ui_settings_sensors.cpp`
- Modify with sections for each sensor category

---

## Task 17: Wizard Step for Probe Selection

**Files:**
- Create: `ui_xml/wizard_probe_sensor_select.xml`
- Create: `include/ui_wizard_probe_sensor_select.h`
- Create: `src/ui/ui_wizard_probe_sensor_select.cpp`
- Modify: `src/ui/ui_wizard.cpp`

---

## Task 18: Update AD5M Pro Preset

**Files:**
- Modify: `config/presets/adventurer-5m-pro.json`

**Step 1: Add e1_sensor as Z_PROBE**

Add to preset config:
```json
{
  "sensors": {
    "switch": {
      "sensors": [
        { "klipper_name": "filament_switch_sensor e1_sensor", "role": "z_probe", "enabled": true }
      ]
    }
  }
}
```

**Step 2: Commit**

```bash
git add config/presets/adventurer-5m-pro.json
git commit -m "feat(presets): configure e1_sensor as Z probe for AD5M Pro"
```

---

## Verification Checklist

- [ ] `make -j` builds successfully
- [ ] `make test-run` passes all sensor tests
- [ ] Manual test on AD5M Pro:
  - [ ] Wizard shows probe sensor step with e1_sensor
  - [ ] Can assign e1_sensor as Z_PROBE
  - [ ] Settings overlay shows all sensor sections
  - [ ] Home panel shows sensor indicators
  - [ ] `probe_switch_triggered` subject updates when sensor triggered
- [ ] Config migration works (old `filament_sensors` auto-converts)
