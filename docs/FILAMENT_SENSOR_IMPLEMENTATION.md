# Filament Sensor Support Implementation

## Overview

This document tracks the implementation of comprehensive standalone filament sensor support (runout, toolhead, entry sensors - NOT AMS-related) for HelixScreen. The feature enables auto-discovery from Moonraker, user configuration, status display, and behavioral integration.

**Branch:** `feat/filament-sensors`
**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-filament-sensors`
**Started:** 2025-12-15
**Status:** ✅ COMPLETE (Phase 1-4 done, Phase 5.3 done, wizard deferred)

---

## Requirements Summary

| Category | Decision |
|----------|----------|
| **Discovery** | Auto-discover from Moonraker + user configures roles |
| **Roles** | Runout, Toolhead, Entry (fixed list, one sensor per role max) |
| **UI Naming** | Role names only ("Runout Sensor", not Klipper names) |
| **Setup** | Wizard step (skippable, auto-skip if no sensors) + Settings panel |
| **Settings Location** | Under Printer settings |
| **Per-Sensor Config** | Enable/Disable + Role assignment |
| **Master Switch** | Global toggle to disable all sensing |
| **Status Display** | Home, Print, Filament, Controls panels (subtle dot indicator) |
| **Motion Sensors** | Show motion activity indicator (encoder moving) |
| **Load/Unload** | Warn but allow (both directions) |
| **Auto-detect completion** | No (manual only) |
| **Pre-print check** | Warn if no filament, allow proceed |
| **Extrusion controls** | Unrestricted |
| **Print runout** | Toast + guidance modal with quick actions |
| **State change toasts** | Always (insert and remove) |
| **Data method** | WebSocket subscription (real-time) |
| **Config storage** | helixconfig.json |
| **AMS coexistence** | Allow both |
| **Missing sensor** | Show warning, keep config |
| **Mock mode** | Simulated sensors via env vars |

---

## Implementation Phases

### Phase 1: Backend Foundation ✅ COMPLETE
**Commit:** `61aa6ab` (2025-12-15)

#### 1.1 Data Types ✅
**File:** `include/filament_sensor_types.h`

Created core data structures:
- `FilamentSensorRole` enum: NONE, RUNOUT, TOOLHEAD, ENTRY
- `FilamentSensorType` enum: SWITCH, MOTION
- `FilamentSensorConfig` struct: klipper_name, sensor_name, role, type, enabled
- `FilamentSensorState` struct: filament_detected, enabled, detection_count, available
- Helper functions: `role_to_display_string()`, `role_to_config_string()`, `role_from_config_string()`

#### 1.2 PrinterCapabilities Extension ✅
**Files:** `include/printer_capabilities.h`, `src/printer_capabilities.cpp`

- Added `std::vector<std::string> filament_sensor_names_` member
- Added `has_filament_sensors()` and `get_filament_sensor_names()` methods
- Detection in `parse_objects()` for `filament_switch_sensor *` and `filament_motion_sensor *`
- Summary output includes sensor count

#### 1.3 FilamentSensorManager Class ✅
**Files:** `include/filament_sensor_manager.h`, `src/filament_sensor_manager.cpp`

Central manager singleton providing:
- Thread-safe state access (mutex-protected)
- Sensor discovery from Klipper object names
- Config load/save to helixconfig.json
- Role assignment and enable/disable per sensor
- Master enable switch
- State queries by role
- LVGL subjects for reactive UI binding:
  - `runout_detected_` (int: -1=no sensor, 0=empty, 1=detected)
  - `toolhead_detected_`
  - `entry_detected_`
  - `any_runout_` (1 if any sensor shows runout)
  - `motion_active_` (1 if motion sensor encoder active)
  - `master_enabled_subject_`
  - `sensor_count_`
- Toast notifications on state changes
- State change callback for external observers

#### 1.4 PrinterState Integration ✅
**File:** `src/printer_state.cpp`

- Added `#include "filament_sensor_manager.h"`
- Forward status updates to FilamentSensorManager at end of `update_from_status()`

#### 1.5 Moonraker Subscription ✅
**Files:** `include/moonraker_client.h`, `src/moonraker_client.cpp`

- Added `std::vector<std::string> filament_sensors_` member
- Parse filament sensors in `parse_objects()`
- Subscribe to all discovered filament sensors in `complete_discovery_subscription()`
- Debug logging for discovered sensors

#### 1.6 Config Schema ✅
**File:** `config/helixconfig.json.template`

Added `filament_sensors` section:
```json
{
  "filament_sensors": {
    "master_enabled": true,
    "_master_enabled_comment": "Global toggle...",
    "sensors": [],
    "_sensors_comment": "Array of sensor configurations..."
  }
}
```

#### 1.7 Mock Support ✅
**File:** `src/moonraker_client_mock.cpp`

- Default mock sensor: `filament_switch_sensor fsensor`
- `HELIX_MOCK_FILAMENT_SENSORS` env var for custom sensors:
  - `none` - disable sensors
  - `switch:name,motion:name` - custom sensor list
- `HELIX_MOCK_FILAMENT_STATE` env var for sensor states:
  - `fsensor:empty` - set sensor as empty
  - `fsensor:detected` - set sensor as detected
- Filament sensor state included in initial state dispatch
- `filament_sensors_` populated for subscription

#### 1.8 Main Integration ✅
**File:** `src/main.cpp`

- Added `#include "filament_sensor_manager.h"`
- FilamentSensorManager initialization in `on_hardware_discovered` callback:
  - `init_subjects()`
  - `discover_sensors()`
  - `load_config()`

---

### Phase 2: UI Status Indicators ✅ COMPLETE

#### 2.1 Filament Sensor Indicator Component ✅
**New file:** `ui_xml/filament_sensor_indicator.xml`

Small reusable component showing sensor status as a colored dot:
- Green when filament detected (success_color)
- Red when no filament (error_color)
- Hidden when master disabled or no sensor assigned (value = -1)
- Uses bind_style for reactive color changes

#### 2.2 Panel Integrations ✅
Added dot indicators to:
- **Home Panel** (`ui_xml/home_panel.xml`) - In status card bottom row with "Filament" label
- **Controls Panel** (`ui_xml/controls_panel.xml`) - Next to filament card icon
- **Print Panel** (`ui_xml/print_status_panel.xml`) - Below bed temp, above control buttons

Note: Filament Panel integration deferred to Phase 3 (settings panel will show all sensors)

#### 2.3 Motion Sensor Activity Indicator 🔲 DEFERRED
Pulsing animation for motion sensors deferred to Phase 5 polish.

---

### Phase 3: Configuration UI ✅ COMPLETE
**Commit:** `8b99f64` (2025-12-16)

#### 3.1 Filament Sensors Settings Overlay ✅
**New files:**
- `ui_xml/filament_sensors_overlay.xml` - Settings overlay panel
- `ui_xml/filament_sensor_row.xml` - Reusable sensor row component

Implemented as overlay panel (not standalone panel) under Settings:
- Master enable toggle with reactive binding to `filament_master_enabled` subject
- Discovered sensors list with:
  - Sensor name + type badge (switch/motion)
  - Role dropdown (None, Runout, Toolhead, Entry)
  - Enable toggle per sensor
- "No sensors detected" placeholder with conditional visibility
- Memory cleanup handler for dynamically allocated sensor names
- Auto-save on change

#### 3.2 Settings Navigation ✅
**Modified:** `src/ui_panel_settings.cpp`, `ui_xml/settings_panel.xml`

- Added "Filament Sensors" item under PRINTER section
- Conditional visibility based on `filament_sensor_count` subject
- Handler creates overlay lazily on first click

#### 3.3 Wizard Step 🔲 DEFERRED
Deferred - wizard step can be added later if needed. Current implementation via Settings is sufficient for initial release.

#### 3.4 Wizard Integration 🔲 DEFERRED
Deferred along with wizard step.

---

### Phase 4: Behavioral Integration ✅ COMPLETE

#### 4.1 Load/Unload Warnings ✅
**File:** `src/ui_panel_filament.cpp`

Implemented:
- `handle_load_button()` checks toolhead sensor, warns if filament already present
- `handle_unload_button()` checks toolhead sensor, warns if no filament detected
- `show_load_warning()` / `show_unload_warning()` create modal dialogs
- Proceed/Cancel handlers with proper cleanup

#### 4.2 Pre-Print Warning ✅
**File:** `src/ui_panel_print_select.cpp`

Implemented at line 1430:
- Checks runout sensor before starting print
- Shows warning modal if no filament detected
- Allows user to proceed anyway or cancel

#### 4.3 State Change Toasts ✅
**File:** `src/filament_sensor_manager.cpp`

Implemented in `update_from_status()`:
- Toast on insert: "Runout Sensor: Filament inserted"
- Toast on remove: "Runout Sensor: Filament removed" (warning)
- Only fires for enabled sensors with assigned roles

#### 4.4 Runout Guidance Modal ✅
**Files:** `ui_xml/runout_guidance_modal.xml`, `src/ui_panel_print_status.cpp`

Implemented:
- Modal with warning icon, customizable title/message
- "Load Filament" button → navigates to filament panel
- "Resume Print" button → sends resume command
- "Cancel Print" button → sends cancel command
- `check_and_show_runout_guidance()` triggered when print pauses
- Proper cleanup on panel deactivation

---

### Phase 5: Testing & Polish (Partial)

#### 5.1 Mock Testing
- Test with `HELIX_MOCK_FILAMENT_SENSORS=switch:runout,switch:toolhead`
- Test with `HELIX_MOCK_FILAMENT_STATE=runout:empty`
- Verify UI indicators respond to state changes

#### 5.2 Integration Testing
- Test with real Klipper printer with filament sensor
- Verify discovery, subscription, state updates
- Test wizard flow and settings panel

#### 5.3 Unit Tests ✅ COMPLETE
**New file:** `tests/unit/test_filament_sensor_manager.cpp`
**Commit:** Added 2025-12-18

Comprehensive test coverage with 11 test cases and 115 assertions:
- Type helper tests (role/type string conversion)
- Sensor discovery (switch, motion, multiple, invalid names)
- Role assignment (uniqueness enforcement, clearing roles)
- Enable/disable (per-sensor and master switch)
- State updates from Moonraker JSON
- State change callbacks
- State queries (is_filament_detected, has_any_runout, etc.)
- Subject value correctness for UI binding
- Motion sensor specific tests
- Edge cases (spaces in names, unknown sensors, rapid changes)
- Thread safety basics (copy semantics)

---

## File Summary

### New Files Created (Phase 1)
| File | Purpose |
|------|---------|
| `include/filament_sensor_types.h` | Core data types and enums |
| `include/filament_sensor_manager.h` | Manager class header |
| `src/filament_sensor_manager.cpp` | Manager implementation |

### Modified Files (Phase 1)
| File | Changes |
|------|---------|
| `include/printer_capabilities.h` | Added sensor detection methods |
| `src/printer_capabilities.cpp` | Implemented sensor detection |
| `include/moonraker_client.h` | Added `filament_sensors_` member |
| `src/moonraker_client.cpp` | Parse and subscribe to sensors |
| `src/moonraker_client_mock.cpp` | Mock sensor support |
| `src/printer_state.cpp` | Forward updates to manager |
| `src/main.cpp` | Initialize manager on discovery |
| `config/helixconfig.json.template` | Added config schema |

### Files To Create (Phase 2-5)
| File | Purpose |
|------|---------|
| `ui_xml/filament_sensor_indicator.xml` | Dot indicator component |
| `ui_xml/motion_sensor_indicator.xml` | Motion activity indicator |
| `ui_xml/filament_sensor_settings_panel.xml` | Settings panel layout |
| `include/ui_panel_filament_sensor_settings.h` | Settings panel header |
| `src/ui_panel_filament_sensor_settings.cpp` | Settings panel implementation |
| `ui_xml/wizard_filament_sensor_step.xml` | Wizard step layout |
| `include/ui_wizard_filament_sensor.h` | Wizard step header |
| `src/ui_wizard_filament_sensor.cpp` | Wizard step implementation |
| `ui_xml/runout_guidance_modal.xml` | Runout guidance modal |
| `tests/unit/test_filament_sensor_manager.cpp` | Unit tests |

---

## Environment Variables for Testing

| Variable | Example | Description |
|----------|---------|-------------|
| `HELIX_MOCK_FILAMENT_SENSORS` | `switch:fsensor,motion:encoder` | Custom mock sensor list |
| `HELIX_MOCK_FILAMENT_SENSORS` | `none` | Disable mock sensors |
| `HELIX_MOCK_FILAMENT_STATE` | `fsensor:empty` | Set sensor state (empty/detected) |

---

## Architecture Notes

### Data Flow
```
Moonraker notify_status_update
    ↓
PrinterState::update_from_status()
    ↓
FilamentSensorManager::update_from_status()
    ↓
Parse sensor states → Update subjects → Fire callbacks → Show toasts
    ↓
UI components react via subject binding
```

### Thread Safety
- `FilamentSensorManager` uses `std::mutex` for all state access
- Subject updates happen on main LVGL thread (via dispatch)
- Toast manager is thread-safe

### Config Persistence
- Config stored in `helixconfig.json` under `filament_sensors` key
- Uses `Config::get_instance()` for access
- Auto-saves on role/enable changes

### Klipper Object Names
- Switch sensor: `filament_switch_sensor <name>`
- Motion sensor: `filament_motion_sensor <name>`
- State fields: `filament_detected` (bool), `enabled` (bool)

---

## Testing Checklist

- [ ] Mock mode shows simulated sensors
- [ ] Auto-discovery finds sensors from Moonraker
- [ ] Settings panel shows discovered sensors
- [ ] Role assignment persists to config
- [ ] Master switch disables all indicators
- [ ] Dot indicators appear on all 4 panels
- [ ] Load warning shows when toolhead has filament
- [ ] Unload warning shows when no filament
- [ ] Pre-print warning shows when no filament
- [ ] Toast on filament insert/remove
- [ ] Runout modal shows when print pauses + no filament
- [ ] Wizard step skips when no sensors
- [ ] Motion sensor shows activity indicator
- [ ] Config survives app restart
- [ ] Missing sensor shows warning in UI

---

## Revision History

| Date | Author | Changes |
|------|--------|---------|
| 2025-12-15 | Claude | Initial document, Phase 1 complete |
