# Pre-Print Options Testing Guide

This document provides comprehensive testing procedures for the Bambu-style pre-print options feature, covering both manual testing scenarios and automated test coverage.

## Feature Overview

The pre-print options feature allows users to select preparatory operations before starting a print:

| Option | Description | Klipper Command |
|--------|-------------|-----------------|
| **Bed Leveling** | Probe bed and create mesh | `BED_MESH_CALIBRATE` |
| **Quad Gantry Level** | Level CoreXY gantry (Voron-style) | `QUAD_GANTRY_LEVEL` |
| **Z Tilt Adjust** | Level bed via Z steppers | `Z_TILT_ADJUST` |
| **Nozzle Clean** | Wipe nozzle before print | `CLEAN_NOZZLE` (macro) |

Options are shown/hidden based on printer capabilities detected via Moonraker.

---

## Manual Test Plan

### Prerequisites

```bash
# Build the application
make -j

# For mock testing (no real printer)
./build/bin/helix-screen --test -p print-select

# For live testing (requires Moonraker connection)
./build/bin/helix-screen -p print-select
```

---

### Test Category 1: UI Display & Layout

#### TC1.1: Detail View Opens Correctly
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Navigate to Print Select panel | File list displays |
| 2 | Click on any G-code file | Detail view overlay appears |
| 3 | Verify layout | Thumbnail, filename, metadata, options card, and buttons visible |
| 4 | Verify options card | All visible checkboxes fit without truncation |

**CLI shortcut:** `./build/bin/helix-screen --test -p print-select --select-file "3DBenchy.gcode"`

#### TC1.2: Options Card Visibility by Printer Type
| Printer Type | Expected Visible Options |
|--------------|-------------------------|
| **Voron 2.4** (CoreXY + QGL) | Bed Leveling ✓, QGL ✓, Z Tilt ✗, Nozzle Clean ✓ |
| **Voron Trident** (CoreXY + Z Tilt) | Bed Leveling ✓, QGL ✗, Z Tilt ✓, Nozzle Clean ✓ |
| **Prusa MK4** (bed mesh only) | Bed Leveling ✓, QGL ✗, Z Tilt ✗, Nozzle Clean ✗ |
| **Generic** (no special macros) | Bed Leveling ✓, QGL ✗, Z Tilt ✗, Nozzle Clean ✗ |

**Verification:** Options should be hidden (not just disabled) when printer lacks capability.

#### TC1.3: Responsive Layout
| Screen Size | Expected Behavior |
|-------------|-------------------|
| Small (480×800) | Options fit, may need scroll |
| Medium (800×480) | All options visible without scroll |
| Large (1024×600+) | All options visible with comfortable spacing |

```bash
./build/bin/helix-screen --test -s small -p print-select --select-file "test.gcode"
./build/bin/helix-screen --test -s medium -p print-select --select-file "test.gcode"
./build/bin/helix-screen --test -s large -p print-select --select-file "test.gcode"
```

---

### Test Category 2: Checkbox Behavior

#### TC2.1: Default Checkbox States
| Option | Expected Default |
|--------|-----------------|
| Bed Leveling | Unchecked |
| QGL | Unchecked |
| Z Tilt | Unchecked |
| Nozzle Clean | Unchecked |

#### TC2.2: Checkbox Toggle
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Click on unchecked "Bed Leveling" | Checkbox becomes checked |
| 2 | Click on checked "Bed Leveling" | Checkbox becomes unchecked |
| 3 | Click on checkbox label text | Checkbox toggles (label is clickable) |

#### TC2.3: Multiple Selection
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Check "Bed Leveling" | Only Bed Leveling checked |
| 2 | Check "QGL" | Both Bed Leveling and QGL checked |
| 3 | Check "Nozzle Clean" | All three checked |
| 4 | Uncheck "QGL" | Bed Leveling and Nozzle Clean remain checked |

---

### Test Category 3: Print Execution Flow

#### TC3.1: Print Without Options
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open file detail view | Options card visible |
| 2 | Leave all checkboxes unchecked | - |
| 3 | Click "Print" button | Print starts immediately (no sequencer) |
| 4 | Check logs | `start_print()` called directly, no CommandSequencer |

**Log verification:**
```bash
./build/bin/helix-screen --test -vv 2>&1 | grep -E "start_print|CommandSequencer"
# Expected: "start_print() called" without sequencer logs
```

#### TC3.2: Print With Single Option
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Open file detail view | - |
| 2 | Check "Bed Leveling" only | - |
| 3 | Click "Print" button | Sequence starts: Home → Bed Mesh → Print |
| 4 | Check logs | CommandSequencer executes 3 operations |

**Log verification:**
```bash
./build/bin/helix-screen --test -vv 2>&1 | grep -E "Executing step|operation"
# Expected:
# Executing step 1/3: Home
# Executing step 2/3: Bed Mesh
# Executing step 3/3: Start Print
```

#### TC3.3: Print With All Options (Voron 2.4 printer)
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Connect to Voron 2.4 (or mock) | QGL visible |
| 2 | Check all available options | Bed Leveling, QGL, Nozzle Clean |
| 3 | Click "Print" button | Sequence: Home → QGL → Bed Mesh → Nozzle Clean → Print |
| 4 | Verify order | Operations execute in dependency order |

**Expected sequence order:**
1. `G28` (Home)
2. `QUAD_GANTRY_LEVEL`
3. `BED_MESH_CALIBRATE`
4. `CLEAN_NOZZLE`
5. `SDCARD_PRINT_FILE` or API print start

#### TC3.4: Operation Completion Detection
| Operation | Completion Condition |
|-----------|---------------------|
| Home (G28) | `toolhead.homed_axes` contains "xyz" |
| QGL | `quad_gantry_level.applied` == true |
| Z Tilt | `z_tilt.applied` == true |
| Bed Mesh | `bed_mesh.profile_name` is non-empty |
| Nozzle Clean | Immediate (no state to observe) |
| Start Print | `print_stats.state` == "printing" |

---

### Test Category 4: Error Handling

#### TC4.1: Operation Failure
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Start print with QGL checked | Sequence begins |
| 2 | Simulate QGL failure (e.g., probe error) | Sequence aborts |
| 3 | Check UI | Error notification displayed |
| 4 | Check printer state | Printer not in "printing" state |

#### TC4.2: Disconnection During Sequence
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Start print with options | Sequence begins |
| 2 | Disconnect from Moonraker mid-sequence | Sequence aborts |
| 3 | Check UI | "Connection lost" notification |
| 4 | Reconnect | Can attempt new print |

#### TC4.3: Cancel During Sequence
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Start print with multiple options | Sequence begins |
| 2 | Navigate away or cancel | Sequence cancelled |
| 3 | Check logs | `CommandSequencer::cancel()` called |

---

### Test Category 5: Capability Detection

#### TC5.1: Printer Capability Query
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Connect to printer | Capabilities queried |
| 2 | Check logs | `PrinterCapabilities` logged |
| 3 | Verify subjects | `printer_has_qgl`, `printer_has_z_tilt`, etc. updated |

**Log verification:**
```bash
./build/bin/helix-screen -vv 2>&1 | grep "PrinterCapabilities"
# Expected: PrinterCapabilities: QGL, bed_mesh | 2 macros
```

#### TC5.2: Mock Printer Types
| Mock Type | Expected Capabilities |
|-----------|----------------------|
| `--test` (default) | QGL + bed_mesh + CLEAN_NOZZLE |
| Future: `--mock-printer=trident` | Z_TILT + bed_mesh |
| Future: `--mock-printer=ender3` | bed_mesh only |

---

### Test Category 6: State Persistence

#### TC6.1: Checkbox State Across File Selection
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Select file A, check "Bed Leveling" | - |
| 2 | Close detail view | - |
| 3 | Select file B | Checkboxes reset to defaults |

**Note:** Currently checkboxes reset per-file. Future enhancement may remember last-used settings.

#### TC6.2: Checkbox State After Failed Print
| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Check options and start print | Sequence begins |
| 2 | Sequence fails mid-way | Error shown |
| 3 | Re-open same file | Checkboxes retain last selection |

---

## Automated Test Coverage

### Unit Tests (Catch2)

Run all pre-print related tests:
```bash
./build/bin/helix-tests "[sequencer]" "[capabilities]" "[gcode_ops]"
```

#### CommandSequencer Tests (`test_command_sequencer.cpp`)

| Test Case | Tag | Assertions |
|-----------|-----|------------|
| Queue management | `[sequencer]` | add/clear operations |
| Start conditions | `[sequencer]` | requires operations to start |
| Completion conditions | `[sequencer]` | state-based detection |
| State update processing | `[sequencer]` | JSON state parsing |
| Multi-operation sequences | `[sequencer]` | correct ordering |
| Cancellation | `[sequencer]` | clean abort |
| G-code generation | `[sequencer]` | correct command output |
| State names | `[sequencer]` | human-readable labels |
| Edge cases | `[sequencer]` | empty sequence, duplicate ops |
| Real-world sequences | `[sequencer]` | Voron-style full sequence |

**Current coverage:** 10 test cases, 108 assertions

#### Printer Capabilities Tests (`test_printer_capabilities.cpp`)

| Test Case | Tag | What it tests |
|-----------|-----|---------------|
| QGL detection | `[capabilities]` | quad_gantry_level in config |
| Z tilt detection | `[capabilities]` | z_tilt in config |
| Bed mesh detection | `[capabilities]` | bed_mesh in config |
| Macro discovery | `[capabilities]` | gcode_macro objects |
| Multiple capabilities | `[capabilities]` | combined detection |

#### GCode Ops Detector Tests (`test_gcode_ops_detector.cpp`)

| Test Case | Tag | What it tests |
|-----------|-----|---------------|
| BED_MESH_CALIBRATE detection | `[gcode_ops]` | inline command |
| START_PRINT parameters | `[gcode_ops]` | macro with params |
| G29 (Marlin) detection | `[gcode_ops]` | legacy bed probe |
| CLEAN_NOZZLE detection | `[gcode_ops]` | custom macro |
| Case insensitivity | `[gcode_ops]` | mixed case handling |
| Early termination | `[gcode_ops]` | stops at first extrusion |

---

### Integration Test Scenarios

These require the full UI but can be semi-automated:

```bash
# Scenario 1: Mock printer, auto-select file, screenshot
HELIX_AUTO_SCREENSHOT=1 HELIX_AUTO_QUIT_MS=3000 \
  ./build/bin/helix-screen --test -p print-select --select-file "test.gcode"

# Scenario 2: Verify detail view with all options visible
HELIX_AUTO_QUIT_MS=5000 ./build/bin/helix-screen --test -vv \
  -p print-select --select-file "3DBenchy.gcode" 2>&1 | \
  grep -E "checkbox|capability|visible"
```

---

## Test Data Requirements

### Mock G-code Files

For testing, ensure these files exist in the mock file list:

| Filename | Purpose |
|----------|---------|
| `3DBenchy.gcode` | Standard test file with thumbnail |
| `test-print.gcode` | Basic file for quick tests |
| `large-print.gcode` | Long print time for testing estimates |
| `no-thumbnail.gcode` | Missing thumbnail handling |

### Mock Printer Configurations

The `MoonrakerClientMock` provides:

| Config | Capabilities |
|--------|-------------|
| Default | QGL, bed_mesh, CLEAN_NOZZLE macro |
| Future: Trident | Z_TILT, bed_mesh |
| Future: Basic | bed_mesh only |

---

## Regression Checklist

Before releasing, verify:

- [ ] **TC1.1**: Detail view opens without layout issues
- [ ] **TC1.2**: Correct options shown for printer type
- [ ] **TC2.1**: Checkboxes default to unchecked
- [ ] **TC3.1**: Print without options works
- [ ] **TC3.2**: Single option executes correctly
- [ ] **TC3.3**: Multiple options execute in correct order
- [ ] **TC3.4**: Completion detection works for all operation types
- [ ] **TC4.1**: Operation failures handled gracefully
- [ ] **TC5.1**: Capabilities detected on connection
- [ ] All `[sequencer]` unit tests pass
- [ ] All `[capabilities]` unit tests pass
- [ ] Build succeeds on macOS and Linux

---

## Known Limitations

1. **No operation timeout**: Long-running operations (e.g., chamber soak) have no UI timeout
2. **No progress indication**: Only "in progress" state, no percentage
3. **No retry mechanism**: Failed operations abort entire sequence
4. **Checkbox state not persisted**: Resets when detail view closes

---

## Future Test Enhancements

1. **Automated UI tests**: Headless LVGL rendering + screenshot comparison
2. **Fuzzing**: Random checkbox combinations + simulated failures
3. **Performance tests**: Large G-code file scanning times
4. **Network resilience**: Packet loss, high latency simulation

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - System design patterns
- [TESTING.md](TESTING.md) - General test infrastructure
- [LVGL9_XML_GUIDE.md](LVGL9_XML_GUIDE.md) - UI component reference
