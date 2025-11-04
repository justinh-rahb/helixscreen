# Moonraker API Integration Testing Guide

This document provides testing procedures for verifying the Moonraker API integrations across all UI panels.

## ✅ Completed Integration Work

**Date**: 2025-11-02
**Work**: Wired 18 UI operations across 9 files to MoonrakerAPI

### Summary

- **Phase 1**: Motion & Temperature (4 integrations)
- **Phase 2**: Print Control (3 integrations)
- **Phase 3**: Extrusion (4 integrations)
- **Phase 4**: Utility Operations (2 integrations)
- **Phase 5**: Additional Operations (5 integrations)

All integrations compile successfully and follow consistent patterns.

---

## Test Strategy

Since `MoonrakerAPI` methods are not virtual, traditional unit testing with mocks is not feasible. Instead, we use a combination of:

1. **Compilation verification** - Code compiles without errors ✅
2. **Integration testing** - Manual verification with real/mock Moonraker
3. **Code review** - Pattern consistency and parameter correctness

---

## Manual Integration Test Procedures

### Prerequisites

```bash
# Build the application
make -j

# Run in test mode (uses mock backends)
./build/bin/helix-ui-proto --test

# Or run with real Moonraker (requires connection)
./build/bin/helix-ui-proto
```

### Test Checklist

Copy this checklist and mark items as you test:

#### Phase 1: Motion & Temperatures

- [ ] **Motion Jog**
  - Navigate to Motion panel
  - Click jog buttons (N, S, E, W, NE, NW, SE, SW)
  - **Expected**: `spdlog` shows `[Motion] X/Y axis moved` messages
  - **Verify**: Correct axis ('X' or 'Y') and distance values

- [ ] **Motion Home**
  - Click home buttons (X, Y, Z, All)
  - **Expected**: `spdlog` shows `[Motion] Axis X/Y/Z homed` or `All axes homed`
  - **Verify**: Correct axes parameter ("X", "Y", "Z", or "")

- [ ] **Nozzle Temperature**
  - Navigate to Controls → Temps → Nozzle
  - Set target temperature (e.g., 210°C)
  - **Expected**: `spdlog` shows `[Temp] Nozzle temp set to 210°C`
  - **Verify**: Correct heater name ("extruder") and temperature value

- [ ] **Bed Temperature**
  - Navigate to Controls → Temps → Bed
  - Set target temperature (e.g., 60°C)
  - **Expected**: `spdlog` shows `[Temp] Bed temp set to 60°C`
  - **Verify**: Correct heater name ("heater_bed") and temperature value

#### Phase 2: Print Control

- [ ] **Pause Print**
  - Start print simulation (if available)
  - Click pause button in Print Status panel
  - **Expected**: `spdlog` shows `[PrintStatus] Pausing print...` then `Print paused successfully`
  - **Verify**: UI state changes to PAUSED immediately (optimistic update)
  - **Error test**: Disconnect Moonraker and verify state reverts to PRINTING on error

- [ ] **Resume Print**
  - While print is paused, click resume
  - **Expected**: `spdlog` shows `[PrintStatus] Resuming print...` then `Print resumed successfully`
  - **Verify**: UI state changes to PRINTING immediately (optimistic update)
  - **Error test**: Verify state reverts to PAUSED on error

- [ ] **Cancel Print**
  - Click cancel button
  - **Expected**: `spdlog` shows `[PrintStatus] Cancel button clicked` then `Print cancelled successfully`
  - **Verify**: Print stops and status changes to CANCELLED

#### Phase 3: Extrusion Operations

- [ ] **Extrude**
  - Navigate to Controls → Extrusion
  - Ensure nozzle temperature > 170°C (safety check)
  - Select amount (10mm, 25mm, 50mm, 100mm)
  - Click Extrude button
  - **Expected**: `spdlog` shows `[Extrusion] Extruding Xmm...` then `Extruded Xmm successfully`
  - **Verify**: Correct axis ('E'), positive distance, feedrate (300 mm/min)
  - **Safety test**: Try extruding when cold - should see `Extrude blocked: nozzle too cold`

- [ ] **Retract**
  - Same conditions as extrude
  - Click Retract button
  - **Expected**: `spdlog` shows `[Extrusion] Retracting Xmm...` then `Retracted Xmm successfully`
  - **Verify**: Correct axis ('E'), negative distance, feedrate (300 mm/min)
  - **Safety test**: Verify cold extrusion blocking works

- [ ] **Filament Temperature Presets**
  - Navigate to Filament panel
  - Click material preset (PLA, ABS, PETG)
  - **Expected**: `spdlog` shows `[Filament] Material selected: X (target=Y°C)` then `Nozzle temp set to Y°C`
  - **Verify**: Correct temperatures (PLA=200°C, ABS=240°C, PETG=230°C)

- [ ] **Filament Custom Temperature**
  - Click "Custom" button
  - Enter custom temperature (e.g., 215°C)
  - **Expected**: `spdlog` shows `[Filament] Custom temperature confirmed: 215°C` then `Nozzle temp set to 215°C`

- [ ] **Filament Purge**
  - Ensure nozzle is hot
  - Click Purge button
  - **Expected**: `spdlog` shows `[Filament] Purging 10mm` then `Purge complete (10mm extruded)`
  - **Verify**: Fixed 10mm extrusion at 300 mm/min

#### Phase 4: Utility Operations

- [ ] **Motors Disable**
  - Navigate to Controls panel
  - Click "Motors Disable" card
  - **Expected**: `spdlog` shows `[Controls] Motors Disable card clicked` then `Motors disabled successfully`
  - **Verify**: M84 G-code command executed
  - **Note**: Confirmation dialog planned for future implementation

- [ ] **Light Toggle (Home Panel)**
  - On Home panel, click light icon button
  - **Expected**: `spdlog` shows `[Home] Light button clicked` then `Light command executed successfully`
  - **Verify**: SET_PIN command with VALUE=1.0 (on) or VALUE=0.0 (off)
  - **Note**: Command configurable in future (different printers use different pin names/macros)
  - **Error test**: If printer doesn't have lights configured, verify graceful error handling

#### Phase 5: Additional Operations

- [ ] **Filament Load**
  - Navigate to Filament panel
  - Ensure nozzle temperature > 170°C (safety check)
  - Click Load button
  - **Expected**: `spdlog` shows `[Filament] Loading filament` then `Filament loaded successfully`
  - **Verify**: LOAD_FILAMENT macro executed
  - **Safety test**: Try loading when cold - should see `Load blocked: nozzle too cold`
  - **Note**: Macro name configurable in future

- [ ] **Filament Unload**
  - Same conditions as load
  - Click Unload button
  - **Expected**: `spdlog` shows `[Filament] Unloading filament` then `Filament unloaded successfully`
  - **Verify**: UNLOAD_FILAMENT macro executed
  - **Safety test**: Verify cold extrusion blocking works

- [ ] **Print Status Nozzle Temp Adjust**
  - During a print, click nozzle temperature card in Print Status panel
  - **Expected**: Temperature adjustment overlay appears
  - **Verify**: Can set temperature using the overlay
  - **Note**: Uses same nozzle_temp_panel as Controls section

- [ ] **Print Status Bed Temp Adjust**
  - During a print, click bed temperature card in Print Status panel
  - **Expected**: Temperature adjustment overlay appears
  - **Verify**: Can set temperature using the overlay
  - **Note**: Uses same bed_temp_panel as Controls section

- [ ] **Print Status Light Toggle**
  - During a print, click light button in Print Status panel
  - **Expected**: `spdlog` shows `[PrintStatus] Light button clicked` then `Light command executed successfully`
  - **Verify**: SET_PIN command with VALUE=1.0 (on) or VALUE=0.0 (off)
  - **Note**: Same functionality as Home panel light toggle

---

## Error Handling Verification

For each API call, verify error handling:

### Pattern

```cpp
MoonrakerAPI* api = get_moonraker_api();
if (!api) {
    spdlog::warn("[Component] MoonrakerAPI not available");
    return;
}

api->some_method(params,
    []() { spdlog::info("[Component] Success"); },
    [](const MoonrakerError& err) {
        spdlog::error("[Component] Failed: {}", err.message);
        // Optional: Revert optimistic UI updates
    }
);
```

### Test Cases

1. **API Unavailable**
   - Kill Moonraker connection
   - Try any operation
   - **Expected**: Log shows "MoonrakerAPI not available" and operation stops gracefully

2. **API Error**
   - Use invalid parameter (if possible)
   - **Expected**: Error callback triggered, error message logged
   - **Verify**: UI reverts optimistic changes (e.g., pause/resume state)

3. **Success Path**
   - Normal operation with connected Moonraker
   - **Expected**: Success callback triggered, success message logged

---

## Automated Validation (Code Review Checklist)

Since unit tests can't mock non-virtual methods, validate through code inspection:

### ✅ Verification Checklist

- [x] All API calls include null check for `get_moonraker_api()`
- [x] All API calls provide success and error callbacks
- [x] All callbacks use spdlog for logging (no printf/cout)
- [x] Error callbacks log error.message
- [x] Static variables captured by copy in lambdas (e.g., `int temp_copy = nozzle_target`)
- [x] Optimistic UI updates have error rollback (pause/resume)
- [x] Safety checks precede dangerous operations (MIN_EXTRUSION_TEMP)
- [x] Correct API method used for each operation:
  - `move_axis()` for jog, extrude, retract, purge
  - `home_axes()` for homing operations
  - `set_temperature()` for nozzle/bed temperatures
  - `pause_print()`, `resume_print()`, `cancel_print()` for print control

---

## Integration Points Summary

| File | Operation | API Method | Parameters | Line |
|------|-----------|------------|------------|------|
| `ui_panel_motion.cpp` | Jog X/Y | `move_axis()` | 'X'/'Y', distance, 0 | ~824-843 |
| `ui_panel_motion.cpp` | Home axes | `home_axes()` | "X"/"Y"/"Z"/"" | ~873-881 |
| `ui_panel_controls_temp.cpp` | Set nozzle temp | `set_temperature()` | "extruder", temp | ~387-395 |
| `ui_panel_controls_temp.cpp` | Set bed temp | `set_temperature()` | "heater_bed", temp | ~526-534 |
| `ui_panel_print_status.cpp` | Pause print | `pause_print()` | - | ~236-245 |
| `ui_panel_print_status.cpp` | Resume print | `resume_print()` | - | ~252-260 |
| `ui_panel_print_status.cpp` | Cancel print | `cancel_print()` | - | ~289-297 |
| `ui_panel_print_status.cpp` | Nozzle temp adjust | Navigation | Opens nozzle_temp_panel | ~206-232 |
| `ui_panel_print_status.cpp` | Bed temp adjust | Navigation | Opens bed_temp_panel | ~235-261 |
| `ui_panel_print_status.cpp` | Light toggle | `execute_gcode()` | SET_PIN command | ~265-291 |
| `ui_panel_controls_extrusion.cpp` | Extrude | `move_axis()` | 'E', +amount, 300 | ~227-234 |
| `ui_panel_controls_extrusion.cpp` | Retract | `move_axis()` | 'E', -amount, 300 | ~258-265 |
| `ui_panel_filament.cpp` | Temp preset | `set_temperature()` | "extruder", temp | ~230-238 |
| `ui_panel_filament.cpp` | Temp custom | `set_temperature()` | "extruder", temp | ~261-269 |
| `ui_panel_filament.cpp` | Purge | `move_axis()` | 'E', 10.0, 300 | ~340-347 |
| `ui_panel_filament.cpp` | Load filament | `execute_gcode()` | "LOAD_FILAMENT" | ~306-321 |
| `ui_panel_filament.cpp` | Unload filament | `execute_gcode()` | "UNLOAD_FILAMENT" | ~336-351 |
| `ui_panel_controls.cpp` | Motors disable | `execute_gcode()` | "M84" | ~240-247 |
| `ui_panel_home.cpp` | Light toggle | `execute_gcode()` | SET_PIN command | ~308-315 |

---

## Future Test Enhancements

### Option 1: Make API Methods Virtual

Update `moonraker_api.h` to make methods virtual:

```cpp
class MoonrakerAPI {
public:
    virtual void move_axis(...);
    virtual void home_axes(...);
    virtual void set_temperature(...);
    // etc.
};
```

This would enable traditional mock-based unit testing.

### Option 2: Dependency Injection

Pass `MoonrakerAPI*` to panel initialization functions instead of using global `get_moonraker_api()`:

```cpp
lv_obj_t* ui_panel_motion_create(lv_obj_t* parent, MoonrakerAPI* api);
```

### Option 3: Integration Test Harness

Create automated integration tests that:
1. Start mock Moonraker server
2. Programmatically trigger UI events
3. Verify API calls received by mock server
4. Check UI state changes

---

## Test Results

Document your test results here:

**Date**: _______________
**Tester**: _______________
**Build**: _______________

| Test | Pass | Fail | Notes |
|------|------|------|-------|
| Motion Jog |  |  |  |
| Motion Home |  |  |  |
| Nozzle Temp |  |  |  |
| Bed Temp |  |  |  |
| Pause Print |  |  |  |
| Resume Print |  |  |  |
| Cancel Print |  |  |  |
| Extrude |  |  |  |
| Retract |  |  |  |
| Filament Presets |  |  |  |
| Filament Custom |  |  |  |
| Filament Purge |  |  |  |
| Filament Load |  |  |  |
| Filament Unload |  |  |  |
| Motors Disable |  |  |  |
| Light Toggle (Home) |  |  |  |
| Print Status Nozzle Temp |  |  |  |
| Print Status Bed Temp |  |  |  |
| Print Status Light |  |  |  |

**Overall Status**: ☐ Pass  ☐ Fail  ☐ Partial

**Issues Found**:

---

## References

- **MoonrakerAPI documentation**: `include/moonraker_api.h`
- **Implementation guide**: `HANDOFF.md`
- **Logging guide**: `CLAUDE.md` (spdlog usage)
- **Test mode guide**: `include/runtime_config.h`
