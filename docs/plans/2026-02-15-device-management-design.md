# Device Management System Design

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement the corresponding implementation plan.

**Goal:** Redesign the device section/action defaults so both AFC and Happy Hare backends have complete, accurate defaults — and the mock backend faithfully represents each.

**Architecture:** Per-backend defaults files (`afc_defaults`, `hh_defaults`) define sections and actions. Three shared section IDs (`setup`, `speed`, `maintenance`) are a naming convention. Backends own their full definitions. Mock picks from the right defaults based on its mode.

---

## Data Model

No changes to existing types:

- `DeviceSection` — id, label, display_order, description
- `DeviceAction` — id, label, section, type (BUTTON/TOGGLE/SLIDER/DROPDOWN/INFO), current_value, min/max/unit, enabled/disable_reason
- `ActionType` enum — BUTTON, TOGGLE, SLIDER, DROPDOWN, INFO

## Section ID Convention

Three shared section IDs used by both backends:

| ID | Label | Order | Purpose |
|----|-------|-------|---------|
| `setup` | Setup | 0 | Calibration wizards, system config, LED, modes |
| `speed` | Speed | 1 | Speed multipliers, motion settings |
| `maintenance` | Maintenance | 2 | Test commands, servo/motor ops, diagnostics |

AFC adds backend-specific sections at higher display_order values:

| ID | Label | Order | Purpose |
|----|-------|-------|---------|
| `hub` | Hub & Cutter | 4 | Cutter enable, cut distance, hub bowden |
| `tip_forming` | Tip Forming | 5 | Ramming, cooling tube, retraction |
| `purge` | Purge & Wipe | 6 | Purge enable/length, brush wipe |
| `config` | Configuration | 7 | Save & Restart |

## Per-Backend Defaults

### `afc_defaults.h/.cpp` (existing, modified)

Changes from current state:
- Rename section `"calibration"` → `"setup"`, label "Setup"
- Fold LED section actions (`led_toggle`, `quiet_mode`) into `"setup"` section
- Remove `"led"` section entirely
- Add sections: `hub`, `tip_forming`, `purge`, `config`
- Add actions for new sections (currently hardcoded in AFC backend):
  - **Hub:** hub_cut_enabled (TOGGLE), hub_cut_dist (SLIDER 0-100mm), hub_bowden_length (SLIDER 100-2000mm), assisted_retract (TOGGLE)
  - **Tip Forming:** ramming_volume (SLIDER 0-100), unloading_speed_start (SLIDER 0-200 mm/s), cooling_tube_length (SLIDER 0-500mm), cooling_tube_retraction (SLIDER 0-100mm)
  - **Purge:** purge_enabled (TOGGLE), purge_length (SLIDER 0-200mm), brush_enabled (TOGGLE)
  - **Config:** save_restart (BUTTON)

Capabilities struct stays AFC-only — no changes.

### `hh_defaults.h/.cpp` (new)

Three sections, core essential actions:

**Setup:**
| Action ID | Type | Label | Details |
|-----------|------|-------|---------|
| `calibrate_bowden` | BUTTON | Calibrate Bowden | Runs MMU_CALIBRATE_BOWDEN |
| `calibrate_encoder` | BUTTON | Calibrate Encoder | Runs MMU_CALIBRATE_ENCODER |
| `calibrate_gear` | BUTTON | Calibrate Gear | Runs MMU_CALIBRATE_GEAR |
| `calibrate_gates` | BUTTON | Calibrate Gates | Runs MMU_CALIBRATE_GATES |
| `led_mode` | DROPDOWN | LED Mode | Options: off, gate_status, filament_color, on |
| `calibrate_servo` | BUTTON | Calibrate Servo | Runs servo angle calibration |

**Speed:**
| Action ID | Type | Label | Details |
|-----------|------|-------|---------|
| `gear_load_speed` | SLIDER | Gear Load Speed | 10-300 mm/s |
| `gear_unload_speed` | SLIDER | Gear Unload Speed | 10-300 mm/s |
| `selector_speed` | SLIDER | Selector Speed | 10-300 mm/s |

**Maintenance:**
| Action ID | Type | Label | Details |
|-----------|------|-------|---------|
| `test_grip` | BUTTON | Test Grip | Runs MMU_TEST_GRIP |
| `test_load` | BUTTON | Test Load | Runs MMU_TEST_LOAD |
| `motors_toggle` | TOGGLE | Motors | Enable/disable MMU motors |
| `servo_buzz` | BUTTON | Buzz Servo | Runs servo engagement test |
| `reset_servo_counter` | BUTTON | Reset Servo Counter | Resets servo down count |
| `reset_blade_counter` | BUTTON | Reset Blade Counter | Resets cutter blade count |

No capabilities struct for v1 — add when real HH backend implements device management.

## Mock Backend Changes

### AFC mode (`set_afc_mode(true)`)
- Use ALL sections from `afc_defaults` — remove section ID filtering
- Hub/tip/purge actions enabled with mock default values (not gated on config loading)
- Config section's `save_restart` disabled with reason "Not available in mock mode"

### HH mode (default, `set_afc_mode(false)`)
- Use `hh_defaults` sections and actions instead of cherry-picked AFC data
- Calibration buttons return success with log message
- Speed sliders accept and store values (no-op beyond logging)
- Motor toggle tracks state

### Tool changer mode
- No device sections (unchanged)

## Real Backend Changes

### AFC backend (`ams_backend_afc.cpp`)
- Move hub/tip_forming/purge/config section definitions from hardcoded → `afc_defaults`
- Backend overlays dynamic values from config files onto defaults
- When config files not loaded, actions present but disabled with "AFC config not loaded"
- Net reduction in backend code (same pattern as the earlier dedup refactor)

### Happy Hare backend
- No changes in this implementation. Returns empty sections/actions.
- When HH device management is implemented, `hh_defaults` provides the starting point.
- **Acknowledged as near-term priority.**

### Base class
- No changes. Virtual methods with empty defaults stay as-is.

## Test Coverage

### `test_afc_defaults.cpp` (existing, updated)
- Update for renamed sections (calibration → setup, LED folded in)
- Add tests for hub/tip/purge/config sections and their actions
- Verify all 7 sections present with correct IDs and ordering

### `test_hh_defaults.cpp` (new)
- Section count, ordering, field validation
- Action uniqueness, section validity
- BUTTON/SLIDER defaults, slider ranges
- Verify 3 shared section IDs match convention (setup, speed, maintenance)

No new UI tests — detail overlay already renders any action type.

## UI Changes

### Section icon mapping (`section_icon_for_id()`)
- Rename `"calibration"` → `"setup"` mapping (icon: `wrench` or `cog`)
- Remove `"led"` mapping
- Add any missing section icons

### No structural UI changes
- Drill-down navigation stays (operations overlay → section detail)
- All action types already supported (BUTTON, TOGGLE, SLIDER, DROPDOWN, INFO)

## Migration Notes

- Section ID `"calibration"` → `"setup"` affects: afc_defaults, mock backend, UI icon mapping, tests
- Section `"led"` removed — its actions move to `"setup"`
- Mock HH mode stops using AFC defaults entirely — uses `hh_defaults`
