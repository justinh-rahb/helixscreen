# Output Pin LED Support Design

## Problem

HelixScreen detects `[output_pin]` configurations with LED-related names but cannot control them. These are single-channel PWM (or digital) pins used for chamber lights, enclosure LEDs, etc. Users must currently set up macro-based workarounds.

## Solution

Add a 5th LED backend (`OutputPinBackend`) that sends `SET_PIN` gcode commands and subscribes to Moonraker for real-time state. Auto-detect PWM capability from Klipper config to determine slider vs toggle UI.

## Backend: OutputPinBackend

**Discovery:**
- Auto-detect: output_pins with "light"/"led"/"lamp" in name (existing `moonraker_client.cpp` logic)
- Manual add: users can add any output_pin as an LED from settings (like macro device config)
- PWM detection: check Klipper config object for `pwm: true`

**Control:**
- PWM pins: `SET_PIN PIN=<name> VALUE=<0.0-1.0>`
- Non-PWM pins: `SET_PIN PIN=<name> VALUE=0` / `SET_PIN PIN=<name> VALUE=1`

**State tracking:**
- Subscribe to `output_pin <name>` objects via Moonraker status subscription
- Reported `value` (0.0-1.0) maps to brightness percentage
- Real-time updates reflect external changes (gcode macros, other UIs)

**Strip info flags:**
- `supports_color = false`
- `supports_white = false`
- `is_pwm` flag determines brightness slider vs on/off toggle

## UI Adaptation

When selected strip has `supports_color == false`:
- Hide color presets and custom color picker
- Show brightness slider (if PWM) or just on/off toggle (if non-PWM)
- This also fixes the existing issue where macro strips show unusable color controls

## Config Persistence

- Auto-detected output_pin strips: discovered automatically, no config needed
- Manually added output_pin strips: saved in LED config JSON (like macro devices)
- PWM capability cached after first discovery

## Files to Create/Modify

| File | Change |
|------|--------|
| `include/led/led_backend.h` | Add `OUTPUT_PIN` to `LedBackendType`, add `is_pwm` to `LedStripInfo` |
| `include/led/led_controller.h` | `OutputPinBackend` class declaration |
| `src/led/led_controller.cpp` | `OutputPinBackend` implementation, discover output_pin strips, subscribe to Moonraker |
| `src/ui/ui_led_control_overlay.cpp` | Hide color controls when `supports_color == false`, brightness-only mode |
| `src/ui/ui_settings_led.cpp` | Add "Add output pin" option in LED settings |
| `src/printer/moonraker_client.cpp` | Subscribe to output_pin objects, parse PWM config from Klipper config |
| `tests/unit/test_led_controller.cpp` | Tests for OutputPinBackend discovery, PWM detection, SET_PIN commands |
