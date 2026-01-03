# Config Schema: Plural Naming Cleanup

**Status:** Completed
**Priority:** Low
**Predecessor:** Commit `4748a55` (single-printer config simplification)
**Completed:** Jan 2026

---

## Context

In the single-printer config migration (Jan 2026), we simplified `printers.default_printer` to just `printer`. However, we kept the existing singular/role-based naming for hardware mappings, which is inconsistent.

## Current Structure (After Migration)

```json
{
  "printer": {
    "moonraker_host": "...",
    "moonraker_port": 7125,

    "heater": { "bed": "heater_bed", "hotend": "extruder" },
    "sensor": { "bed": "...", "hotend": "..." },
    "fan": { "part": "fan", "hotend": "heater_fan hotend_fan" },
    "led": { "strip": "neopixel chamber_light" },

    "fans": [],                    // Additional monitored fans
    "monitored_sensors": [],       // Additional monitored sensors

    "safety_limits": { ... },
    "capability_overrides": { ... }
  }
}
```

## Issues

1. **Inconsistent pluralization:** `heater` (singular) vs `fans` (plural array)
2. **Confusing duality:** `fan.part` (role mapping) AND `fans[]` (extra list)
3. **Missing arrays:** No `leds[]` or `heaters[]` for additional hardware

## Proposed Structure

```json
{
  "printer": {
    "moonraker_host": "...",
    "moonraker_port": 7125,

    "heaters": {
      "bed": "heater_bed",
      "hotend": "extruder"
    },
    "sensors": {
      "bed": "...",
      "hotend": "..."
    },
    "fans": {
      "part": "fan",
      "hotend": "heater_fan hotend_fan"
    },
    "leds": {
      "strip": "neopixel chamber_light"
    },

    "extra_fans": [],              // Renamed from "fans" to avoid confusion
    "extra_sensors": [],           // Renamed from "monitored_sensors"

    "safety_limits": { ... },
    "capability_overrides": { ... }
  }
}
```

## Files to Update

### Core Code
- [ ] `include/config.h` - Update docstrings
- [ ] `src/system/config.cpp` - Update `get_default_config()`, `get_default_printer_config()`
- [ ] `include/wizard_config_paths.h` - Change `/printer/heater/` to `/printer/heaters/`

### Wizard Screens (read/write hardware config)
- [ ] `src/ui/ui_wizard_heater_select.cpp`
- [ ] `src/ui/ui_wizard_fan_select.cpp`
- [ ] `src/ui/ui_wizard_led_select.cpp`
- [ ] `src/ui/ui_wizard_hardware_selector.cpp`

### Hardware Validators
- [ ] `src/printer/hardware_validator.cpp` - Reads heater/fan/sensor/led paths

### Other Consumers
- [ ] `src/print/filament_sensor_manager.cpp` - Uses monitored_sensors
- [ ] `src/application/application.cpp` - May reference hardware paths

### Config Files
- [ ] `config/helixconfig.json.template`
- [ ] `config/presets/adventurer-5m-pro.json`
- [ ] `lib/lvgl/helixconfig.json` (if tracked)

### Documentation
- [x] `docs/user/CONFIGURATION.md`
- [x] `docs/DEVELOPMENT.md`

### Tests
- [ ] `tests/unit/test_config.cpp`
- [ ] `tests/unit/test_hardware_validator.cpp`

## Implementation Notes

1. **Search pattern:** `grep -r "heater\|sensor\|fan\|led" --include="*.cpp" --include="*.h" src/ include/`
2. **Wizard paths are centralized** in `wizard_config_paths.h` - update once, affects all wizards
3. **`fans[]` array conflict:** Rename to `extra_fans[]` to disambiguate from `fans.part`/`fans.hotend`
4. **No migration needed** (pre-release) - users can delete config and re-run wizard

## Decision Points

1. **Keep role-based mapping inside pluralized keys?**
   - Yes: `heaters: { bed: "...", hotend: "..." }` (recommended)
   - No: `heaters: ["heater_bed", "extruder"]` (loses role context)

2. **Rename `monitored_sensors` → `extra_sensors`?**
   - Yes: Consistent with proposed `extra_fans`
   - No: Keep existing name, just pluralize role mappings

---

*Created: Jan 2026 | Related commit: 4748a55*
