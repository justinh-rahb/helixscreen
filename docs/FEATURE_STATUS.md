# Feature Implementation Status

**Last Updated:** 2025-12-08

This document tracks the implementation status of all features identified in the feature parity analysis. It serves as the **single source of truth** for what's done, in progress, and remaining.

---

## Status Legend

| Icon | Status | Description |
|------|--------|-------------|
| ✅ | Complete | Fully implemented and tested |
| 🟡 | In Progress | Partially implemented, work ongoing |
| 🚧 | Stub Only | UI exists with "Coming Soon" overlay, not functional |
| ⬜ | Not Started | No work done yet |
| ❌ | Blocked | Cannot proceed (dependency, decision needed) |
| 🔴 | Deprecated | Removed from scope |

---

## Quick Stats

| Category | Complete | In Progress | Stub | Not Started | Total |
|----------|----------|-------------|------|-------------|-------|
| CRITICAL (Tier 1) | 3 | 1 | 4 | 0 | 8 |
| HIGH (Tier 2) | 2 | 0 | 1 | 4 | 7 |
| MEDIUM (Tier 3) | 0 | 0 | 0 | 7 | 7 |
| DIFFERENTIATOR (Tier 4) | 0 | 0 | 0 | 5 | 5 |
| **TOTAL** | **5** | **1** | **5** | **16** | **27** |

---

## TIER 1: CRITICAL Features

These features ALL major competitors have. Required for feature parity.

| Feature | Status | Files | Notes |
|---------|--------|-------|-------|
| **Temperature Presets** | ✅ | `nozzle_temp_panel.xml`, `bed_temp_panel.xml` | Off/PLA/PETG/ABS presets implemented |
| **Macro Panel** | ✅ | `ui_xml/macro_panel.xml`, `src/ui_panel_macros.cpp` | List & execute Klipper macros, prettified names, system macro filtering |
| **Console Panel** | 🚧 | `ui_xml/console_panel.xml` | Stub with Coming Soon overlay |
| **Screws Tilt Adjust** | 🚧 | `ui_xml/screws_tilt_panel.xml` | Stub with Coming Soon overlay |
| **Camera/Webcam** | 🚧 | `ui_xml/camera_panel.xml` | Stub with Coming Soon overlay |
| **Print History** | 🟡 | `ui_xml/history_panel.xml` | **IN PROGRESS** in `helixscreen-print-history` worktree |
| **Power Device Control** | ✅ | `ui_xml/power_panel.xml`, `src/ui_panel_power.cpp` | Complete - polished with XML components, friendly names, empty state |

### Detailed Status

#### Temperature Presets
- **Status:** ✅ Complete
- **Priority:** CRITICAL
- **Complexity:** MEDIUM
- **Implementation:** Built into temp panels directly
- **Files:**
  - [x] `ui_xml/nozzle_temp_panel.xml` - Off/PLA(210°C)/PETG(240°C)/ABS(250°C)
  - [x] `ui_xml/bed_temp_panel.xml` - Off/PLA(60°C)/PETG(80°C)/ABS(100°C)
- **API:** Uses existing heater control
- **Checklist:**
  - [x] Default presets (Off, PLA, PETG, ABS)
  - [ ] Custom preset creation (future enhancement)
  - [ ] Preset editing/deletion (future enhancement)
  - [ ] Quick-apply from home screen (future enhancement)
  - [ ] Persist in config (future enhancement)

#### Macro Panel
- **Status:** ✅ Complete
- **Priority:** CRITICAL
- **Complexity:** MEDIUM
- **Depends On:** None
- **Files:**
  - [x] `ui_xml/macro_panel.xml` - Panel layout with scrollable list, empty state
  - [x] `ui_xml/macro_card.xml` - Reusable card component for each macro
  - [x] `include/ui_panel_macros.h` - Panel class with prettify/danger detection
  - [x] `src/ui_panel_macros.cpp` - Full implementation
  - [x] `src/main.cpp` - Registration and CLI support
- **API:** Uses `PrinterCapabilities::macros()` populated from `printer.objects.list`
- **Checklist:**
  - [x] List all macros from Klipper (sorted alphabetically)
  - [x] Prettify macro names (CLEAN_NOZZLE → "Clean Nozzle")
  - [x] Execute macro via G-code (single tap)
  - [x] Filter system macros (_* prefix) by default
  - [x] Empty state when no macros available
  - [ ] Show system macros toggle (future enhancement)
  - [ ] Execute macro with params (future enhancement)
  - [ ] Favorites/quick access (future enhancement)
  - [ ] Dangerous macro confirmation dialog (future enhancement)

#### Console Panel
- **Status:** ⬜ Not Started
- **Priority:** CRITICAL
- **Complexity:** HIGH
- **Depends On:** On-screen keyboard (exists)
- **Files to Create:**
  - [ ] `ui_xml/console_panel.xml`
  - [ ] `include/ui_panel_console.h`
  - [ ] `src/ui_panel_console.cpp`
- **Files to Modify:**
  - [ ] `ui_xml/navigation_bar.xml`
  - [ ] `src/main.cpp`
  - [ ] `include/moonraker_api.h`
  - [ ] `src/moonraker_api.cpp`
- **API:** `/server/gcode_store`, `/printer/gcode/script`, `notify_gcode_response`
- **Checklist:**
  - [ ] Scrollable command history
  - [ ] G-code input with keyboard
  - [ ] Color-coded output (errors red)
  - [ ] Temperature message filtering
  - [ ] Command history (up/down)
  - [ ] Clear button

#### Screws Tilt Adjust
- **Status:** ⬜ Not Started
- **Priority:** CRITICAL
- **Complexity:** HIGH
- **Depends On:** None
- **Files to Create:**
  - [ ] `ui_xml/screws_tilt_panel.xml`
  - [ ] `ui_xml/screw_indicator.xml`
  - [ ] `include/ui_panel_screws_tilt.h`
  - [ ] `src/ui_panel_screws_tilt.cpp`
- **Files to Modify:**
  - [ ] `ui_xml/controls_panel.xml` (add card)
- **API:** `SCREWS_TILT_CALCULATE` command, parse response
- **Checklist:**
  - [ ] Visual bed diagram with screw positions
  - [ ] 3x3, 4-corner support
  - [ ] Rotation indicators ("CW 1/4 turn")
  - [ ] Re-probe button
  - [ ] Different bed shapes

#### Camera/Webcam
- **Status:** ⬜ Not Started
- **Priority:** CRITICAL
- **Complexity:** HIGH
- **Depends On:** Crowsnest/webcam configured
- **Files to Create:**
  - [ ] `ui_xml/camera_panel.xml`
  - [ ] `ui_xml/camera_pip.xml`
  - [ ] `include/ui_panel_camera.h`
  - [ ] `src/ui_panel_camera.cpp`
  - [ ] `include/webcam_client.h`
  - [ ] `src/webcam_client.cpp`
- **API:** `/server/webcams/list`, `/server/webcams/item`
- **Checklist:**
  - [ ] Single MJPEG stream display
  - [ ] Multi-camera selector
  - [ ] PiP during print
  - [ ] Snapshot button
  - [ ] Rotation/flip settings

#### Print History
- **Status:** ⬜ Not Started
- **Priority:** CRITICAL
- **Complexity:** MEDIUM
- **Depends On:** None
- **Files to Create:**
  - [ ] `ui_xml/history_panel.xml`
  - [ ] `ui_xml/history_item.xml`
  - [ ] `include/ui_panel_history.h`
  - [ ] `src/ui_panel_history.cpp`
- **Files to Modify:**
  - [ ] `ui_xml/navigation_bar.xml` or settings
- **API:** `/server/history/list`, `/server/history/totals`, `/server/history/job`
- **Checklist:**
  - [ ] List past print jobs
  - [ ] Success/failure indicators
  - [ ] Print time, filament used
  - [ ] Reprint from history
  - [ ] Statistics dashboard
  - [ ] Delete entries

#### Power Device Control
- **Status:** ✅ Complete
- **Priority:** HIGH
- **Complexity:** LOW
- **Depends On:** Power devices configured in Moonraker
- **Files Created:**
  - [x] `ui_xml/power_panel.xml`
  - [x] `ui_xml/power_device_row.xml`
  - [x] `include/ui_panel_power.h`
  - [x] `src/ui_panel_power.cpp`
- **API:** `/machine/device_power/devices`, `/machine/device_power/device`
- **Checklist:**
  - [x] List all power devices
  - [x] On/Off/Toggle controls
  - [x] Status indicators
  - [x] Lock critical devices during print (with lock icon + status text)
  - [ ] Quick access from home (optional, not implemented)
- **UI Polish (Stages 1-4 complete):**
  - [x] Design tokens (no hardcoded values)
  - [x] Reusable XML component (`power_device_row.xml`)
  - [x] Lock indicator with explanation text
  - [x] Empty state UI with icon + guidance
  - [x] Friendly device names (prettify heuristic)
  - [x] Responsive `ui_switch` component

---

## TIER 2: HIGH Priority Features

Most competitors have these. Should implement for competitive parity.

| Feature | Status | Files | Notes |
|---------|--------|-------|-------|
| **Input Shaper Panel** | 🚧 | `ui_xml/input_shaper_panel.xml` | Stub with Coming Soon overlay |
| **Firmware Retraction** | ⬜ | - | View/adjust retraction settings |
| **Spoolman Integration** | ⬜ | - | Filament tracking, QR scanner |
| **Job Queue** | ⬜ | - | Batch printing queue |
| **Update Manager** | ⬜ | - | Software updates |
| **Timelapse Controls** | ⬜ | - | Moonraker-timelapse settings |
| **Layer Display** | ✅ | `ui_xml/print_status_panel.xml:45-46` | Implemented: `bind_text="print_layer_text"` |

### Detailed Status

#### Input Shaper Panel
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** HIGH
- **API:** `SHAPER_CALIBRATE`, `MEASURE_AXES_NOISE`, result parsing
- **Checklist:**
  - [ ] Run calibration buttons
  - [ ] Progress indicator
  - [ ] Display recommended settings
  - [ ] Graph viewer for resonance results

#### Firmware Retraction
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** LOW
- **API:** `firmware_retraction` printer object
- **Checklist:**
  - [ ] View current settings
  - [ ] Adjust retract_length, retract_speed
  - [ ] Adjust unretract settings
  - [ ] Apply changes

#### Spoolman Integration
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** MEDIUM
- **API:** `/server/spoolman/*` endpoints
- **Checklist:**
  - [ ] Spoolman panel with spool list
  - [ ] Active spool display
  - [ ] Spool selection at print start
  - [ ] QR code scanner (killer feature!)
  - [ ] Remaining filament gauge
  - [ ] Low filament warnings

#### Job Queue
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** MEDIUM
- **API:** `/server/job_queue/*` endpoints
- **Checklist:**
  - [ ] View queued jobs
  - [ ] Add files to queue
  - [ ] Reorder queue
  - [ ] Remove from queue
  - [ ] Start/pause queue

#### Update Manager
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** MEDIUM
- **API:** `/machine/update/*` endpoints
- **Checklist:**
  - [ ] Show available updates
  - [ ] Update status indicators
  - [ ] One-click update
  - [ ] Rollback option

#### Timelapse Controls
- **Status:** ⬜ Not Started
- **Priority:** HIGH
- **Complexity:** MEDIUM
- **API:** Moonraker-timelapse API
- **Checklist:**
  - [ ] Enable/disable toggle
  - [ ] Mode selector
  - [ ] Frame rate setting
  - [ ] Video library browser

#### Layer Display
- **Status:** ✅ Complete
- **Priority:** HIGH
- **Complexity:** LOW
- **Implementation:** `print_status_panel.xml:45-46`
- **API:** `print_stats.info.current_layer`, `print_stats.info.total_layer`
- **Checklist:**
  - [x] Current/total layers on print status (`bind_text="print_layer_text"`)
  - [ ] Layer progress bar (future enhancement)

---

## TIER 3: MEDIUM Priority Features

Some competitors have these. Nice to have for completeness.

| Feature | Status | Notes |
|---------|--------|-------|
| **Limits Panel** | ⬜ | Velocity/acceleration limits |
| **LED Effects** | ⬜ | StealthBurner LED control |
| **Probe Calibration** | ⬜ | Beacon/Cartographer/Eddy |
| **Temperature Graphs** | ⬜ | Multi-sensor historical graphs |
| **Filament Sensors** | ⬜ | Runout/motion sensor status |
| **System Info** | ⬜ | CPU/memory/network stats |
| **Adaptive Mesh** | ⬜ | Native Klipper 0.12+ feature |

---

## TIER 4: DIFFERENTIATOR Features

NO competitor does these well. Opportunity to lead.

| Feature | Status | Notes |
|---------|--------|-------|
| **PID Tuning UI** | ⬜ | UNIQUE - touchscreen PID calibration |
| **Pressure Advance UI** | ⬜ | Live PA adjustment |
| **First-Layer Wizard** | ⬜ | Guided Z-offset + mesh flow |
| **Material Database** | ⬜ | Built-in material profiles |
| **Maintenance Tracker** | ⬜ | Nozzle/belt reminders |

---

## Infrastructure Improvements

| Item | Status | Notes |
|------|--------|-------|
| **Coming Soon Component** | ✅ | `ui_xml/coming_soon_overlay.xml` - Complete |
| **Nav Bar Updates** | ⬜ | Icons for new panels |
| **Settings Reorganization** | ⬜ | Group new settings |
| **Moonraker API Additions** | 🟡 | ~25 new endpoints (Power API ✅ complete) |

---

## Implementation Log

### 2025-12-08
- Created FEATURE_PARITY_RESEARCH.md with comprehensive analysis
- Created FEATURE_STATUS.md (this file)
- Identified 47 feature gaps across 4 priority tiers

---

## Next Actions

### Immediate (Today)
1. [ ] Create "Coming Soon" component in globals.xml
2. [ ] Add nav icons for new panels
3. [ ] Create stub panels with Coming Soon overlays:
   - [ ] macro_panel.xml
   - [ ] console_panel.xml
   - [ ] camera_panel.xml
   - [ ] history_panel.xml
   - [ ] power_panel.xml
   - [ ] screws_tilt_panel.xml
   - [ ] input_shaper_panel.xml

### Quick Wins (Next Session)
1. [ ] Layer display in print_status_panel
2. [ ] Temperature presets (basic)
3. [ ] Power device control

### Core Features (Following Sessions)
1. [x] Macro panel - list and execute
2. [ ] Console panel - read-only history
3. [ ] Camera panel - single MJPEG stream
4. [ ] History panel - list past jobs

---

## Dependencies Map

```
Nothing depends on these (can start immediately):
├── Temperature Presets
├── Layer Display
├── Power Device Control
├── Firmware Retraction
└── Limits Panel

These depend on "Coming Soon" component:
├── Console Panel (stub)
├── Camera Panel (stub)
├── History Panel (stub)
├── Screws Tilt Panel (stub)
└── Input Shaper Panel (stub)

These depend on completed features:
├── Spoolman → needs Camera for QR scanner
├── First-Layer Wizard → needs working Z-offset
└── PID Tuning UI → needs console to show progress
```

---

## Files Created/Modified Tracking

### New Files (Planned)
```
ui_xml/
├── coming_soon_overlay.xml      [ ] Created  [ ] Tested
├── temp_preset_modal.xml        [ ] Created  [ ] Tested
├── macro_panel.xml              [ ] Created  [ ] Tested
├── macro_card.xml               [ ] Created  [ ] Tested
├── console_panel.xml            [ ] Created  [ ] Tested
├── screws_tilt_panel.xml        [ ] Created  [ ] Tested
├── screw_indicator.xml          [ ] Created  [ ] Tested
├── camera_panel.xml             [ ] Created  [ ] Tested
├── camera_pip.xml               [ ] Created  [ ] Tested
├── history_panel.xml            [ ] Created  [ ] Tested
├── history_item.xml             [ ] Created  [ ] Tested
├── power_panel.xml              [x] Created  [x] Tested
├── power_device_row.xml         [x] Created  [x] Tested
├── input_shaper_panel.xml       [ ] Created  [ ] Tested
├── retraction_panel.xml         [ ] Created  [ ] Tested
├── spoolman_panel.xml           [ ] Created  [ ] Tested
├── job_queue_panel.xml          [ ] Created  [ ] Tested
├── update_panel.xml             [ ] Created  [ ] Tested
└── timelapse_panel.xml          [ ] Created  [ ] Tested

include/
├── temperature_presets.h        [ ] Created  [ ] Tested
├── ui_panel_macros.h            [ ] Created  [ ] Tested
├── ui_panel_console.h           [ ] Created  [ ] Tested
├── ui_panel_screws_tilt.h       [ ] Created  [ ] Tested
├── ui_panel_camera.h            [ ] Created  [ ] Tested
├── webcam_client.h              [ ] Created  [ ] Tested
├── ui_panel_history.h           [ ] Created  [ ] Tested
├── ui_panel_power.h             [x] Created  [x] Tested
├── ui_panel_input_shaper.h      [ ] Created  [ ] Tested
├── ui_panel_retraction.h        [ ] Created  [ ] Tested
├── spoolman_client.h            [ ] Created  [ ] Tested
├── ui_panel_job_queue.h         [ ] Created  [ ] Tested
└── ui_panel_updates.h           [ ] Created  [ ] Tested

src/
├── temperature_presets.cpp      [ ] Created  [ ] Tested
├── ui_panel_macros.cpp          [ ] Created  [ ] Tested
├── ui_panel_console.cpp         [ ] Created  [ ] Tested
├── ui_panel_screws_tilt.cpp     [ ] Created  [ ] Tested
├── ui_panel_camera.cpp          [ ] Created  [ ] Tested
├── webcam_client.cpp            [ ] Created  [ ] Tested
├── ui_panel_history.cpp         [ ] Created  [ ] Tested
├── ui_panel_power.cpp           [x] Created  [x] Tested
├── ui_panel_input_shaper.cpp    [ ] Created  [ ] Tested
├── ui_panel_retraction.cpp      [ ] Created  [ ] Tested
├── spoolman_client.cpp          [ ] Created  [ ] Tested
├── ui_panel_job_queue.cpp       [ ] Created  [ ] Tested
└── ui_panel_updates.cpp         [ ] Created  [ ] Tested
```

### Modified Files (Planned)
```
ui_xml/
├── globals.xml                  [ ] Updated (Coming Soon component)
├── navigation_bar.xml           [ ] Updated (new icons)
├── nozzle_temp_panel.xml        [ ] Updated (presets)
├── bed_temp_panel.xml           [ ] Updated (presets)
├── print_status_panel.xml       [ ] Updated (layer display)
├── controls_panel.xml           [ ] Updated (new cards)
└── home_panel.xml               [ ] Updated (quick access)

include/
├── moonraker_api.h              [x] Updated (Power API: get_power_devices, set_device_power)
├── moonraker_api_mock.h         [x] Updated (Power API mock methods)
└── moonraker_client.h           [ ] Updated (new subscriptions)

src/
├── main.cpp                     [x] Updated (power panel registration, power_device_row component)
├── moonraker_api.cpp            [x] Updated (Power API implementation)
├── moonraker_api_mock.cpp       [x] Updated (Power API mock with MOCK_EMPTY_POWER)
└── ui_panel_print_status.cpp    [ ] Updated (layer display)

config/
└── helixconfig.json.template    [ ] Updated (presets, settings)
```

---

## Session Notes

Use this section to track progress across sessions.

### Session 3 (2025-12-08)
- **Goal:** Power Device Control implementation
- **Completed:**
  - Power Device Control panel functionally complete
  - Added `get_power_devices()` and `set_device_power()` to MoonrakerAPI
  - Created `ui_panel_power.h/cpp` with dynamic device row creation
  - Updated `power_panel.xml` with functional layout
  - Added mock power device support to MoonrakerAPIMock (4 test devices)
  - Registered panel in `main.cpp` with `-p power` CLI option
- **UI Review Findings:** 5 critical, 4 major issues identified
  - Touch targets too small (26px vs 48px minimum)
  - Hardcoded pixel values instead of design tokens
  - No empty state UI, no lock indicator
  - Created `POWER_PANEL_POLISH_PLAN.md` with 6-stage fix plan
- **Status:** Power panel needs UI polish before marking complete

### Session 4 (2025-12-08)
- **Goal:** Power Panel UI Polish (Stages 1-4)
- **Completed:**
  - Fixed blue background bug (`ui_theme_parse_color` vs `ui_theme_get_color`)
  - Created `power_device_row.xml` reusable XML component
  - Replaced manual C++ widget creation with `lv_xml_create()` pattern
  - Used existing `ui_switch` component with responsive `size="medium"`
  - Added `prettify_device_name()` heuristic for friendly labels
  - Added lock icon + "Locked during print" status for locked devices
  - Added empty state UI with icon, heading, guidance text
  - Added `MOCK_EMPTY_POWER=1` env var for testing empty state
- **Key Fixes:**
  - Violated Rule 12 (XML event_cb) - refactored to proper pattern
  - Violated Rule 1 (design tokens) - removed all hardcoded values
  - Toggle switches now use `ui_switch` instead of raw `lv_switch`
- **Status:** Power Panel marked complete (Stages 5-6 optional)
- **Next:** Error feedback enhancement (Stage 5) is optional polish

### Session 2 (2025-12-08)
- **Goal:** Infrastructure setup
- **Completed:**
  - Created `coming_soon_overlay.xml` component
  - Created 7 panel stubs with Coming Soon overlays
  - Fixed worktree build issues (ccache, submodules)
  - Added `scripts/init-worktree.sh`
- **Reference:** See FEATURE_PARITY_HANDOFF.md for details

### Session 1 (2025-12-08)
- **Goal:** Research and documentation
- **Completed:**
  - Created FEATURE_PARITY_RESEARCH.md
  - Created FEATURE_STATUS.md
  - Updated ROADMAP.md
- **Next:** Create Coming Soon component, panel stubs
