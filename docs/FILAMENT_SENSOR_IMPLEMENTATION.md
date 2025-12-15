# Filament Sensor Support Implementation

## Overview

This document tracks the implementation of comprehensive standalone filament sensor support (runout, toolhead, entry sensors - NOT AMS-related) for HelixScreen. The feature enables auto-discovery from Moonraker, user configuration, status display, and behavioral integration.

**Branch:** `feat/filament-sensors`
**Worktree:** `/Users/pbrown/Code/Printing/helixscreen-filament-sensors`
**Started:** 2025-12-15
**Status:** Phase 1 Complete, Phase 2-5 Pending

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

### Phase 2: UI Status Indicators 🔲 PENDING

#### 2.1 Filament Sensor Indicator Component
**New file:** `ui_xml/filament_sensor_indicator.xml`

Small reusable component showing sensor status as a colored dot:
- Green when filament detected
- Red when no filament
- Hidden when master disabled or no sensor assigned

#### 2.2 Panel Integrations
Add dot indicators to:
- **Home Panel** (`ui_xml/home_panel.xml`) - Near temperature/network icons
- **Filament Panel** (`ui_xml/filament_panel.xml`) - Status section with all configured sensors
- **Controls Panel** (`ui_xml/controls_panel.xml`) - On filament card header
- **Print Panel** (`ui_xml/print_status_panel.xml`) - Near temperature displays

#### 2.3 Motion Sensor Activity Indicator
**New file:** `ui_xml/motion_sensor_indicator.xml`

Pulsing dot when encoder active (for motion sensors during extrusion).

---

### Phase 3: Configuration UI 🔲 PENDING

#### 3.1 Filament Sensor Settings Panel
**New files:**
- `include/ui_panel_filament_sensor_settings.h`
- `src/ui_panel_filament_sensor_settings.cpp`
- `ui_xml/filament_sensor_settings_panel.xml`

Features:
- Master enable toggle
- Discovered sensors list with:
  - Klipper name display
  - Role dropdown (None, Runout, Toolhead, Entry)
  - Enable toggle per sensor
- "No sensors detected" message when empty
- Auto-save on change

#### 3.2 Settings Navigation
**Modify:** `src/ui_panel_settings.cpp`

Add "Filament Sensors" item under Printer category.

#### 3.3 Wizard Step
**New files:**
- `include/ui_wizard_filament_sensor.h`
- `src/ui_wizard_filament_sensor.cpp`
- `ui_xml/wizard_filament_sensor_step.xml`

Features:
- Auto-skip if no sensors detected
- List discovered sensors with role assignment
- Skip and Done buttons

#### 3.4 Wizard Integration
**Modify:** `src/ui_wizard.cpp`

Add filament sensor step after printer connection, before completion.

---

### Phase 4: Behavioral Integration 🔲 PENDING

#### 4.1 Load/Unload Warnings
**Modify:** `src/ui_panel_filament.cpp`

- Warn if toolhead already has filament on load
- Warn if no filament detected on unload
- Allow proceed with warning dialogs

#### 4.2 Pre-Print Warning
**Modify:** `src/ui_panel_print_select.cpp`

- Check runout sensor before starting print
- Show warning if no filament, allow proceed

#### 4.3 State Change Toasts
**Already implemented in:** `src/filament_sensor_manager.cpp`

Toast on insert/remove with role name (e.g., "Runout: Filament inserted").

#### 4.4 Runout Guidance Modal
**New file:** `ui_xml/runout_guidance_modal.xml`

Modal shown when print pauses + no filament detected:
- "Load Filament" button → Navigate to filament panel
- "Resume Print" button → Resume command
- "Cancel Print" button → Cancel command

**Modify:** `src/ui_panel_print_status.cpp`

Detect print paused + runout condition and show modal.

---

### Phase 5: Testing & Polish 🔲 PENDING

#### 5.1 Mock Testing
- Test with `HELIX_MOCK_FILAMENT_SENSORS=switch:runout,switch:toolhead`
- Test with `HELIX_MOCK_FILAMENT_STATE=runout:empty`
- Verify UI indicators respond to state changes

#### 5.2 Integration Testing
- Test with real Klipper printer with filament sensor
- Verify discovery, subscription, state updates
- Test wizard flow and settings panel

#### 5.3 Unit Tests
**New file:** `tests/unit/test_filament_sensor_manager.cpp`

Test cases:
- Discovery from Klipper objects list
- Config load/save
- Role assignment uniqueness
- State updates and subject notifications
- Missing sensor handling

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
