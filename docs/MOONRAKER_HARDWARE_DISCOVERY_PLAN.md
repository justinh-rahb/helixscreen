# Moonraker Hardware Discovery Implementation Plan

**Created:** 2025-11-02
**Purpose:** Complete implementation guide for integrating Moonraker hardware discovery into the wizard and main application

## Current State Summary

### ✅ Completed (2025-11-02)
- Moonraker client is initialized early in main.cpp (before wizard) so it's available for connection testing
- Initialization code has been refactored into `initialize_moonraker_client()` function for cleanliness
- Wizard can test connections and save configuration
- Main.cpp properly instantiates MoonrakerClient and MoonrakerAPI
- **Phase 1: Hardware discovery triggered after successful wizard connection** ✅
- **Phase 4.1: Mock backend created with 7 printer profiles** ✅
- **Factory pattern in initialize_moonraker_client() selects mock vs real based on TestConfig** ✅

### ❌ Missing
- Wizard steps 4-7 still use hardcoded values instead of discovered hardware (Phase 2)
- Printer auto-detection not implemented (Phase 3)
- Real-time updates not implemented (Phase 5)

## Phase 1: Trigger Hardware Discovery After Wizard Connection ✅ COMPLETE

### 1.1 Modify ui_wizard_connection.cpp
**Location:** `src/ui_wizard_connection.cpp:201-217`

**Current Issue:** After successful connection test, the wizard immediately disconnects. This prevents hardware discovery.

**Implementation:**
```cpp
// After config save (line 209)
// Trigger hardware discovery
client->discover_printer([]() {
    spdlog::info("[Wizard Connection] Hardware discovery complete!");

    // Log discovered hardware counts
    MoonrakerClient* client = get_moonraker_client();
    if (client) {
        auto heaters = client->get_heaters();
        auto sensors = client->get_sensors();
        auto fans = client->get_fans();
        auto leds = client->get_leds();

        spdlog::info("[Wizard Connection] Discovered {} heaters, {} sensors, {} fans, {} LEDs",
                    heaters.size(), sensors.size(), fans.size(), leds.size());
    }
});
// DO NOT close connection here - keep it for hardware config steps
// client->close();  // REMOVE THIS LINE
```

### 1.2 Add Hardware Discovery State Management
**New file needed:** `include/wizard_hardware_state.h`

```cpp
class WizardHardwareState {
private:
    static std::vector<std::string> discovered_heaters;
    static std::vector<std::string> discovered_sensors;
    static std::vector<std::string> discovered_fans;
    static std::vector<std::string> discovered_leds;

public:
    static void set_hardware(
        const std::vector<std::string>& heaters,
        const std::vector<std::string>& sensors,
        const std::vector<std::string>& fans,
        const std::vector<std::string>& leds
    );

    static std::vector<std::string> get_heaters();
    static std::vector<std::string> get_sensors();
    static std::vector<std::string> get_fans();
    static std::vector<std::string> get_leds();
    static void clear();
};
```

## Phase 2: Update Wizard Hardware Selection Steps (4-7)

### 2.1 Step 4: Bed Select (ui_wizard_bed_select.cpp)
**Current:** Hardcoded "heater_bed\nNone" options
**Location:** `src/ui_wizard_bed_select.cpp:70-85`

**Implementation:**
```cpp
void ui_wizard_bed_select_setup(lv_obj_t* screen_root) {
    // Get discovered hardware
    MoonrakerClient* client = get_moonraker_client();
    if (!client) return;

    auto heaters = client->get_heaters();
    auto sensors = client->get_sensors();

    // Filter for bed heaters
    std::vector<std::string> bed_heaters;
    for (const auto& h : heaters) {
        if (h.find("bed") != std::string::npos) {
            bed_heaters.push_back(h);
        }
    }

    // Filter for bed sensors
    std::vector<std::string> bed_sensors;
    for (const auto& s : sensors) {
        if (s.find("bed") != std::string::npos ||
            s.find("chamber") != std::string::npos) {
            bed_sensors.push_back(s);
        }
    }

    // Build options strings
    std::string heater_options = bed_heaters.empty() ? "None" : "";
    for (size_t i = 0; i < bed_heaters.size(); i++) {
        if (i > 0) heater_options += "\n";
        heater_options += bed_heaters[i];
    }
    heater_options += "\nNone";

    // Update dropdowns
    lv_obj_t* heater_dropdown = lv_obj_find_by_name(screen_root, "bed_heater_dropdown");
    if (heater_dropdown) {
        lv_dropdown_set_options(heater_dropdown, heater_options.c_str());
    }
}
```

### 2.2 Step 5: Hotend Select (ui_wizard_hotend_select.cpp)
**Current:** Hardcoded "extruder\nNone" options
**Location:** `src/ui_wizard_hotend_select.cpp:70-85`

**Filter patterns:**
- Heaters: "extruder", "extruder1", "extruder2"
- Sensors: "extruder", "hotend", names with "extruder"

### 2.3 Step 6: Fan Select (ui_wizard_fan_select.cpp)
**Current:** Hardcoded "heater_fan hotend_fan\nNone" options
**Location:** `src/ui_wizard_fan_select.cpp:70-85`

**Categorization:**
- Hotend fans: "heater_fan", "hotend_fan", names with "hotend"
- Part cooling: "fan", "part_fan", "part_cooling_fan"

### 2.4 Step 7: LED Select (ui_wizard_led_select.cpp)
**Current:** Hardcoded "led strip\nNone" options
**Location:** `src/ui_wizard_led_select.cpp:70-85`

**Common patterns:**
- "led", "neopixel", "dotstar"
- Names containing "strip", "light"

## Phase 3: Printer Auto-Detection

### 3.1 Create PrinterDetector Class
**New files:** `include/printer_detector.h` and `src/printer_detector.cpp`

```cpp
class PrinterDetector {
public:
    struct PrinterInfo {
        std::string name;      // User-friendly name
        std::string type;      // One of the 33 types in wizard
        int confidence;        // 0-100 confidence score
    };

    static PrinterInfo detect_printer(
        const std::vector<std::string>& heaters,
        const std::vector<std::string>& sensors,
        const std::vector<std::string>& fans,
        const std::string& hostname,
        const json& printer_info
    );

private:
    static int check_voron_signatures(/* params */);
    static int check_creality_signatures(/* params */);
    static int check_flashforge_signatures(/* params */);
    static int check_generic_patterns(/* params */);
};
```

### 3.2 Detection Heuristics

**Voron Detection:**
- Hostname contains "voron"
- Has "heater_bed" + "extruder" + specific fan configuration
- MCU names like "octopus", "spider"
- Specific stepper configurations (A/B motors for CoreXY)

**Creality K1 Detection:**
- Hostname contains "k1" or "creality"
- Build volume ~220x220x250
- Specific extruder/bed temp limits
- Fan configuration patterns

**FlashForge Adventurer 5M/5M Pro:**
- Hostname patterns
- Specific heater configurations
- Build volume patterns
- Enclosed printer indicators

**Generic Patterns:**
- Bedslinger: Has Y-axis bed movement
- CoreXY: Has A/B motors instead of X/Y
- Delta: Has tower_a, tower_b, tower_c steppers

### 3.3 Integration with Step 3 (Printer Identify)
**File:** `src/ui_wizard_printer_identify.cpp`

After connection is established (step 2), auto-populate:
1. Call PrinterDetector with discovered hardware
2. If confidence > 70%, pre-select the printer type in roller
3. Show confidence indicator
4. Allow manual override

## Phase 4: Mock Backend Enhancement ✅ COMPLETE

### 4.1 MoonrakerClientMock
**New file:** `src/moonraker_client_mock.cpp`

```cpp
class MoonrakerClientMock : public MoonrakerClient {
public:
    void discover_printer(std::function<void()> callback) override {
        // Simulate different printer configurations based on test flags
        if (test_config.printer_type == "voron") {
            heaters_ = {"heater_bed", "extruder"};
            sensors_ = {"temperature_sensor chamber", "temperature_sensor mcu_temp"};
            fans_ = {"heater_fan hotend_fan", "fan"};
        } else if (test_config.printer_type == "k1") {
            heaters_ = {"heater_bed", "extruder"};
            sensors_ = {"temperature_sensor bed_temp", "temperature_sensor nozzle_temp"};
            fans_ = {"fan", "fan_generic chamber_fan"};
        }
        // etc...

        if (callback) callback();
    }
};
```

### 4.2 Factory Pattern
**Location:** `src/main.cpp:755` (initialize_moonraker_client function)

```cpp
void initialize_moonraker_client(Config* config) {
    spdlog::info("Initializing Moonraker client...");

    // Create appropriate client based on test mode
    if (get_test_config().should_mock_moonraker()) {
        moonraker_client = new MoonrakerClientMock();
        spdlog::info("Using MOCK Moonraker client");
    } else {
        moonraker_client = new MoonrakerClient();
        spdlog::info("Using REAL Moonraker client");
    }

    // Rest of initialization...
}
```

## Phase 5: Real-time Updates and Commands

### 5.1 Temperature Updates
- Already plumbed via notification_queue in main.cpp
- Need to ensure PrinterState subjects are properly bound to UI elements
- Test with real Moonraker connection

### 5.2 Motion Commands
- Implement in moonraker_api.cpp if not already done
- G28 (home), G1 (move), etc.
- Wire to motion panel buttons

### 5.3 Print Control
- Start/pause/cancel already in MoonrakerAPI
- Ensure proper state management
- Test file upload and print start

## Testing Plan

### Test Scenarios

1. **Wizard Flow with Mock:**
   ```bash
   ./build/bin/helix-ui-proto --test --wizard
   ```
   - Verify mock hardware appears in dropdowns
   - Save config and verify persistence

2. **Wizard Flow with Real Printer:**
   ```bash
   ./build/bin/helix-ui-proto --test --real-moonraker --wizard
   ```
   - Connect to real printer
   - Verify discovered hardware populates dropdowns
   - Test printer auto-detection

3. **Connection Persistence:**
   - Verify connection stays alive between wizard steps
   - Verify proper cleanup on wizard completion

4. **Hardware Discovery:**
   - Test with various Klipper configs
   - Verify all heaters/sensors/fans are discovered
   - Test edge cases (no bed, multiple extruders)

## File Modification Summary

### Files to Modify
1. `src/ui_wizard_connection.cpp` - Keep connection alive, trigger discovery
2. `src/ui_wizard_bed_select.cpp` - Dynamic dropdown population
3. `src/ui_wizard_hotend_select.cpp` - Dynamic dropdown population
4. `src/ui_wizard_fan_select.cpp` - Dynamic dropdown population
5. `src/ui_wizard_led_select.cpp` - Dynamic dropdown population
6. `src/ui_wizard_printer_identify.cpp` - Auto-detection integration
7. `src/main.cpp` - Factory pattern for mock/real client

### New Files to Create
1. `include/wizard_hardware_state.h` - Hardware state management
2. `src/wizard_hardware_state.cpp` - Implementation
3. `include/printer_detector.h` - Auto-detection logic
4. `src/printer_detector.cpp` - Implementation
5. `src/moonraker_client_mock.cpp` - Mock implementation

## Critical Implementation Notes

1. **Thread Safety:** Remember that Moonraker callbacks run on background thread. Use proper synchronization when updating shared state.

2. **LVGL Updates:** UI updates must happen on main thread. Use the existing notification_queue pattern.

3. **Memory Management:** Discovered hardware strings need proper lifecycle management. Consider using shared_ptr or storing in singleton.

4. **Error Handling:** Gracefully handle:
   - Connection failures during wizard
   - Empty hardware lists
   - Discovery timeouts
   - Partial discovery (some categories empty)

5. **Dropdown Format:** LVGL rollers expect "\n" separated strings. Build carefully with proper escaping.

6. **Config Persistence:** Ensure selected hardware is saved to config and restored on next launch.

## Success Criteria

- ✅ Wizard step 2 triggers hardware discovery after successful connection
- ✅ Wizard steps 4-7 show real discovered hardware in dropdowns
- ✅ Printer auto-detection works for target printers (Voron, K1, FlashForge)
- ✅ Mock backend provides realistic test data
- ✅ Real-time temperature updates work
- ✅ Motion commands can be sent successfully
- ✅ No hardcoded hardware options remain in wizard

## Implementation Order

1. **Phase 1** - Trigger discovery (unblocks everything else)
2. **Phase 4.1** - Create mock backend (enables testing without real printer)
3. **Phase 2** - Update wizard steps 4-7 (core functionality)
4. **Phase 3** - Printer auto-detection (enhancement)
5. **Phase 5** - Real-time updates (final integration)

Start with Phase 1 as it's the foundation that enables all other work.