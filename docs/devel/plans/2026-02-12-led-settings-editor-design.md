# LED Settings Editor Design

Auto-state mapping editor and macro device configuration UI for the LED Settings overlay.

## Overview

Expand the LED Settings overlay with two interactive editors:
1. **Auto-state mapping editor** — configure what LEDs do at each printer state
2. **Macro device configuration** — create/edit/delete macro-based LED devices

All editing is inline within the existing scrollable overlay. No sub-overlays.

## Overlay Structure (top to bottom)

1. **LED SELECTION** — chip selector for which strips the light button controls (existing)
2. **STARTUP** — "LED on at Start" toggle (existing)
3. **AUTOMATIC LED CONTROL** — enable toggle + 6 expandable state mapping rows
4. **MACRO DEVICES** — add button + editable device cards

## Auto-State Mapping Rows

Six states: Idle, Heating, Printing, Paused, Error, Complete.

### Collapsed row

```
[icon] State Name          Action Summary    [swatch/icon]
```

- Icon per state: power-sleep (idle), bed (heating), printer-3d (printing), pause (paused), alert (error), check (complete)
- Summary text: "White 50%", "Off", "Breathing", etc.
- Color swatch or action-type icon on the right

### Expanded row (tap to toggle)

Shows a dropdown for action type, then contextual controls below it.

**Action type dropdown — capability-filtered:**

| Action Type  | Available When                                      |
|-------------|-----------------------------------------------------|
| Off         | Always                                               |
| Brightness  | Always (white intensity only)                        |
| Color       | At least one selected strip has `supports_color`     |
| Effect      | `LedController::effects().is_available()`            |
| WLED Preset | `LedController::wled().is_available()`               |
| Macro       | `LedController::macro().is_available()`              |

**Contextual controls per action type:**

- **Off**: no extra controls
- **Brightness**: slider (0–100%)
- **Color**: preset swatch row (from `LedController::color_presets()`) + brightness slider
- **Effect**: dropdown of effects from `effects().effects()`
- **WLED Preset**: number input (preset ID)
- **Macro**: dropdown of configured macro device actions

Changes auto-save: `LedAutoState::set_mapping()` + `save_config()` + `evaluate()`.

State rows are hidden when auto-state toggle is disabled.

## Non-Color LED Handling

`apply_action()` checks `supports_color` per strip:

- **Color-capable strip**: full RGBW — `set_color(strip, r*scale, g*scale, b*scale, 0.0)`
- **Non-color strip**: brightness only — `set_color(strip, scale, scale, scale, 0.0)`, ignoring color value

"Brightness" action type: always white intensity, works for all strip types.

"Off" action: `turn_off(strip)` — works for all strip types.

## Macro Device Configuration

### Empty state

Info message: "No macro devices configured. Use macro devices to control LEDs via custom Klipper macros." Plus "Add Macro Device" button.

### Add flow

Tapping "Add Macro Device" inserts a new card in edit mode.

1. **Type dropdown**: On/Off, Toggle, Preset
2. **Display Name**: text input, auto-suggested from first assigned macro
3. **Macro assignment** — dropdowns populated from `discovered_led_macros_`:
   - On/Off: "On Macro" + "Off Macro" dropdowns
   - Toggle: "Toggle Macro" dropdown
   - Preset: repeating rows of "Preset Name" (text) + "Macro" (dropdown), plus "Add Preset" button

### Card display (collapsed)

```
┌──────────────────────────────────────────────┐
│ 🔌 Lights                    On/Off  [✏️][🗑️] │
│    ON: LIGHTS_ON  ·  OFF: LIGHTS_OFF         │
└──────────────────────────────────────────────┘
```

Edit expands to edit mode. Delete with confirmation (tap again to confirm).

### Macro dropdown source

Only discovered macros from the printer (`discovered_led_macros_`). No manual entry. Macros are not filtered out when assigned elsewhere (same macro can appear in multiple devices).

### Persistence

`LedController::set_configured_macros()` + `save_config()` after every add/edit/delete.

### Auto-creation removal

Remove the auto-creation logic in `discover_from_hardware()` (lines 198–256). Keep candidate discovery (`discovered_led_macros_`). Macro devices are user-configured only.

## Implementation

### Files to modify

| File | Change |
|------|--------|
| `ui_xml/led_settings_overlay.xml` | Add state mapping rows, redesign macro section |
| `include/ui_settings_led.h` | Add editor methods and state tracking members |
| `src/ui/ui_settings_led.cpp` | Implement editors (dynamic rows based on capabilities) |
| `src/led/led_auto_state.cpp` | Fix `apply_action()` for non-color LEDs, add "brightness" type |
| `include/led/led_auto_state.h` | Document "brightness" action type |
| `src/led/led_controller.cpp` | Remove macro auto-creation logic |

### What's imperative vs declarative

- Static structure (section headers, toggles, add button): XML
- Dynamic content (state rows, macro cards): C++ — capabilities vary at runtime
- Event callbacks: XML `<event_cb>` + `lv_xml_register_event_cb()`

### Testing

- Non-color LED fallback in `test_led_auto_state.cpp`
- Capability filtering logic tests
- Macro device CRUD tests
- "brightness" action type round-trip
