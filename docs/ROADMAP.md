# HelixScreen Development Roadmap

**Last Updated:** 2025-12-16

---

## Project Status: Beta → Feature Parity Complete (Core)

HelixScreen is a production-quality Klipper touchscreen UI. **Core feature parity is largely achieved** - all critical TIER 1 features are now implemented.

| Area | Status | Details |
|------|--------|---------|
| **UI Panels** | ✅ Complete | 20+ production panels + 10 overlays |
| **Settings** | ✅ Complete | 18 settings across 8 categories |
| **First-Run Wizard** | ✅ Complete | 7-step guided setup |
| **Moonraker API** | ✅ Comprehensive | 40+ methods, print history, timelapse, power |
| **Build System** | ✅ Complete | macOS, Linux, Pi, AD5M |
| **Test Suite** | ✅ Complete | 51+ unit tests |
| **Feature Parity (TIER 1)** | ✅ Complete | All critical features implemented |

---

## Current Priorities

### 1. AMS/Multi-Material Support
**Status:** In Progress (merged to main)

Support for Happy Hare and AFC-Klipper multi-filament systems with Bambu-inspired UI:
- [x] Phase 0: Foundation - Detection, state management, mock backend
- [x] Phase 1: Core UI - AMS panel with slot grid visualization
- [x] Phase 2: Basic Operations - Load/unload/select with real backends
- [x] Phase 2.5: Spool Visualization - Pseudo-3D spool canvas with gradients
- [x] Phase 2.7: Filament Path Data Model
- [x] Phase 4: Rich Feedback - Filament path canvas with animations
- [x] Phase 4.5: Real AFC Backend Integration - Sensor parsing, unit tests
- [x] Phase 4.6: External Spool Bypass Support - UI and backend
- [ ] Phase 2.6: Configurable visualization (deferred)
- [ ] Phase 3: Spoolman integration for material/color info
- [ ] Phase 5: Print integration - Color requirements display
- [ ] Phase 6: Error recovery wizard
- [ ] Phase 7: Advanced features - Mapping, endless spool, calibration
- [ ] Phase 8: Polish - Documentation, stress testing

See `docs/AMS_IMPLEMENTATION_PLAN.md` for detailed specification.

### 2. Production Hardening
**Status:** In Progress

- [x] **Connection-aware navigation** - Disable Controls/Filament when disconnected, auto-navigate to home
- [x] **Reconnection flow UX** - Toast notifications for disconnect/reconnect/klippy states
- [x] **Print cancel confirmation** - Modal confirmation before canceling prints
- [x] **Memory profiling tools** - Development overlay for memory monitoring
- [ ] **Structured logging** - Log levels, rotation, remote debugging
- [ ] **Edge case testing** - Print failures, filesystem errors

---

## Feature Parity Status (TIER 1 - Critical)

All TIER 1 features are now implemented:

| Feature | Status | Implementation |
|---------|--------|----------------|
| **Macro Panel** | ✅ Complete | Full implementation with prettified names, system filtering, single-tap execution |
| **Console Panel** | ✅ Complete | G-code history with color coding (commands, responses, errors) |
| **Screws Tilt Adjust** | ✅ Complete | Visual bed diagram, animated rotation indicators, multi-probe workflow |
| **Power Device Control** | ✅ Complete | Device list, on/off toggles, lock during print, friendly names |
| **Print History** | ✅ Complete | Dashboard + list views, statistics, filtering, search, reprint/delete |
| **Timelapse Settings** | ✅ Complete | Enable/disable, mode selection, framerate, auto-render |
| **Temperature Presets** | ✅ Complete | Off/PLA/PETG/ABS presets in temp panels |
| **Layer Display** | ✅ Complete | Current/total layers on print status |
| **Camera/Webcam** | 🚧 Stub | Coming Soon overlay (awaiting MJPEG implementation) |
| **Input Shaper** | 🚧 Stub | Coming Soon overlay |

### Remaining Stubs (Lower Priority)
- **Camera Panel** - MJPEG viewer, multi-camera, PiP
- **Input Shaper Panel** - Resonance calibration UI

---

## Feature Parity Status (TIER 2 - High Priority)

| Feature | Status | Notes |
|---------|--------|-------|
| **Firmware Retraction** | ⬜ | View/adjust retraction settings |
| **Spoolman Integration** | ⬜ | Filament tracking, QR scanner |
| **Job Queue** | ⬜ | Batch printing queue |
| **Update Manager** | ⬜ | Software updates via Moonraker |

---

## Feature Details

### Screws Tilt Adjust Panel
**Files:** `ui_xml/screws_tilt_panel.xml`, `src/ui_panel_screws_tilt.cpp`

Complete manual bed leveling workflow:
- **5 UI States:** IDLE → PROBING → RESULTS → LEVELED → ERROR
- **Visual bed diagram** with animated rotation indicators (CW/CCW)
- **Friendly adjustment text** ("Tighten 1/4 turn" instead of "CW 00:15")
- **Color-coded severity:** Green (level), Yellow (minor), Red (major), Primary (worst screw)
- **Iterative workflow:** Probe → Adjust → Re-probe → Repeat until level
- **Success detection:** Auto-detect when all screws within tolerance
- **Moonraker API:** `SCREWS_TILT_CALCULATE` with response parsing

### Power Device Control Panel
**Files:** `ui_xml/power_panel.xml`, `ui_xml/power_device_row.xml`, `src/ui_panel_power.cpp`

Complete Moonraker power device integration:
- **Dynamic device list** from `/machine/device_power/devices`
- **On/Off toggle switches** with immediate feedback
- **Friendly device names** via prettify heuristic (e.g., `printer_psu` → "Printer Power")
- **Lock during print** with visual lock icon and explanation text
- **Empty state** with guidance when no devices configured
- **Error handling** with automatic refresh on failure

### Macro Panel
**Files:** `ui_xml/macro_panel.xml`, `ui_xml/macro_card.xml`, `src/ui_panel_macros.cpp`

Execute Klipper macros from touchscreen:
- **Sorted alphabetical list** of all available macros
- **Prettified names** (e.g., `CLEAN_NOZZLE` → "Clean Nozzle")
- **System macro filtering** (hides `_*` prefixed macros by default)
- **Single-tap execution** via G-code script API
- **Dangerous macro detection** (SAVE_CONFIG, FIRMWARE_RESTART, etc.)
- **Empty state** with guidance when no macros defined

### Console Panel
**Files:** `ui_xml/console_panel.xml`, `src/ui_panel_console.cpp`

Read-only G-code command history:
- **Terminal-style display** with newest entries at bottom
- **Color-coded output:**
  - White: Commands sent
  - Green: Successful responses
  - Red: Errors (`!!` or `Error:` prefix)
- **Scrollable history** (100 entries max)
- **Auto-refresh** on panel activation
- **Empty state** when no history available

### Print History Feature
**Files:** `ui_xml/history_dashboard_panel.xml`, `ui_xml/history_list_panel.xml`, `src/ui_panel_history_*.cpp`

Comprehensive print history with statistics:
- **Dashboard view** with aggregated stats:
  - Total prints, success rate, print time, filament used
  - Filament by type horizontal bar chart
  - Prints trend sparkline
  - Time filtering (Day/Week/Month/Year/All)
- **List view** with search, filter, sort:
  - Case-insensitive filename search with debounce
  - Status filter (All/Completed/Failed/Cancelled)
  - Sort by date, duration, filename
- **Detail overlay** with:
  - Thumbnail display
  - Full job metadata
  - Reprint and Delete actions
- **Timelapse integration** in print history (Phase 5)

### Timelapse Settings Overlay
**Files:** `ui_xml/timelapse_settings_overlay.xml`, `src/ui_timelapse_settings.cpp`

Configure Moonraker-Timelapse plugin:
- **Enable/disable toggle** for recording
- **Mode selection:** Layer Macro vs Hyperlapse
  - Mode info text explains each option
- **Framerate dropdown:** 15/24/30/60 fps
- **Auto-render toggle** for automatic video creation
- **Moonraker API:** `get_timelapse_settings`, `set_timelapse_settings`

---

## Documentation

| Document | Purpose |
|----------|---------|
| `docs/FEATURE_PARITY_RESEARCH.md` | Complete competitive analysis, API reference |
| `docs/FEATURE_STATUS.md` | Live implementation tracking |
| `docs/AMS_IMPLEMENTATION_PLAN.md` | Multi-material support spec |

---

## Backlog (Lower Priority)

| Feature | Priority | Notes |
|---------|----------|-------|
| **mDNS discovery** | Low | Auto-find Moonraker (manual IP works) |
| **OTA updates** | Future | Currently requires manual binary update |
| **User manual** | Future | End-user documentation |

---

## Completed Features

### Core Architecture
- [x] LVGL 9.4 with declarative XML layouts
- [x] Reactive Subject-Observer data binding
- [x] Class-based panel architecture (PanelBase, ObserverGuard, SubscriptionGuard)
- [x] Theme system with dark/light modes
- [x] Responsive breakpoints (small/medium/large displays)
- [x] RAII lifecycle management throughout
- [x] Design token system (no hardcoded colors/spacing)

### Navigation & Panels
- [x] **Home Panel** - Printer status, live temps, LED control, disconnect overlay
- [x] **Controls Panel** - 6-card launcher (Motion, Temps, Extrusion, Fan, Mesh, PID)
- [x] **Motion Panel** - Jog pad, Z-axis, distance selector, homing
- [x] **Temperature Panels** - Nozzle/bed presets, temp graphs, custom entry
- [x] **Extrusion Panel** - Extrude/retract, amount selector, safety checks
- [x] **Filament Panel** - Load/unload, filament profiles
- [x] **Print Select** - Card/list views, sorting, USB source tabs
- [x] **Print Status** - Progress, time remaining, pause/resume/cancel, exclude object, cancel confirmation
- [x] **Settings Panel** - 18 settings (theme, display, sound, network, safety, calibration)
- [x] **Advanced Panel** - Bed mesh visualization, access to calibration tools

### Feature Parity Panels (NEW)
- [x] **Screws Tilt Panel** - Visual bed leveling with rotation indicators, iterative workflow
- [x] **Power Panel** - Moonraker power device control with friendly names and lock during print
- [x] **Macro Panel** - Execute Klipper macros with prettified names and filtering
- [x] **Console Panel** - G-code history with color-coded output
- [x] **Print History** - Dashboard + list with stats, filtering, search, reprint/delete
- [x] **Timelapse Settings** - Configure Moonraker-timelapse (mode, framerate, auto-render)

### Settings Features (18 total)
- [x] Dark/Light theme toggle with restart dialog
- [x] Display brightness control (hardware sync)
- [x] Display sleep timeout
- [x] Scroll momentum and sensitivity
- [x] LED light toggle (capability-aware)
- [x] Sound toggle with M300 test beep
- [x] Print completion notifications (Off/Notification/Alert)
- [x] E-Stop confirmation toggle
- [x] Bed mesh 3D visualization
- [x] Z-offset calibration
- [x] PID tuning
- [x] WiFi settings overlay
- [x] Network settings
- [x] Factory reset

### First-Run Wizard (7 steps)
- [x] WiFi setup with scanning and hidden network support
- [x] Moonraker connection with validation
- [x] Printer identification with auto-detection (50+ printer database)
- [x] Heater selection (bed/hotend)
- [x] Fan selection (hotend/part cooling)
- [x] LED selection (optional)
- [x] Summary and confirmation

### Moonraker Integration
- [x] WebSocket client with auto-reconnection
- [x] JSON-RPC protocol with timeout management
- [x] File operations (list, metadata, delete, upload, start print)
- [x] Print control (pause, resume, cancel)
- [x] Motion control (homing, jog, positioning)
- [x] Heater/fan/LED control
- [x] System commands (E-stop, restart)
- [x] Exclude object with undo window
- [x] **Print History** - `/server/history/*` (list, totals, job details, delete)
- [x] **Power Devices** - `/machine/device_power/*` (list devices, on/off/toggle)
- [x] **G-code Store** - `/server/gcode_store` (command history)
- [x] **Timelapse** - Moonraker-timelapse API (settings get/set)
- [x] **Screws Tilt** - `SCREWS_TILT_CALCULATE` command parsing

### G-code Features
- [x] Pre-print operation toggles (bed level, QGL, Z-tilt, nozzle clean)
- [x] G-code file modification (comment out embedded operations)
- [x] Command sequencer for pre-print ops
- [x] Printer capabilities detection
- [x] Memory-safe streaming for large files

### Build System
- [x] Makefile with parallel builds (`make -j`)
- [x] macOS native build (SDL2)
- [x] Linux native build (SDL2)
- [x] Raspberry Pi cross-compile (Docker, aarch64)
- [x] Adventurer 5M cross-compile (Docker, armv7-a, static linking)
- [x] CI/CD with GitHub Actions
- [x] Icon font generation with validation
- [x] Pre-commit hooks (clang-format, quality checks)

### Testing
- [x] Catch2 test framework
- [x] 51 unit tests covering core functionality
- [x] Mock Moonraker client for offline testing
- [x] Test fixtures for printer configurations

---

## Recent Work

### December 2025 (Week 2)
| Feature | Commit |
|---------|--------|
| **Screws Tilt Adjust - Full Implementation** | `253497f` |
| Real `SCREWS_TILT_CALCULATE` response parsing | `253497f` |
| Visual bed diagram with animated rotation indicators | |
| Friendly adjustment text ("Tighten 1/4 turn") | |
| **Print Cancel Confirmation Modal** | `a9f285e` |
| **Memory Profiling Tools** | `2a2d0a2` |
| Development overlay for memory monitoring | |
| macOS + Linux cross-platform support | |
| **Timelapse Integration** | `beb4fa0` |
| Timelapse settings overlay (mode, framerate, autorender) | |
| Integration with print history | |
| **Refactoring & Polish** | |
| Phase 3 design token migration complete | `8c9f3bb` |
| RAII migration with SubscriptionGuard | `df72783` |
| Removed 120+ redundant GPL boilerplate files | `788d466` |
| Log noise reduction (widget registration → trace) | `30ac5e4` |
| Print complete overlay with thumbnail | `58fc321` |

### December 2025 (Week 1)
| Feature | Commit |
|---------|--------|
| **Print History Feature Complete** | `2025-12-10` |
| Print History - Thumbnail caching + UI polish | `46889b1` |
| Print History - Dashboard with charts and reactive bindings | `9f0154b` |
| Print History - Detail overlay with Reprint/Delete | `2d1de9f` |
| Print History - Search, filter, sort for list | `0ba7937` |
| Print History - Dashboard and list panels | `8aef45e` |
| Print History - Moonraker API integration | `258c30a` |
| Reconnection flow UX (toast notifications) | `9844ead` |
| Connection-aware navigation gating | `a7eb28f` |
| Print completion notifications (Off/Notification/Alert) | `80a3199` |
| WiFi settings overlay with reactive architecture | `9037d81` |
| AD5M static build infrastructure (glibc 2.25) | `cdffc63` |
| Display sleep and hardware brightness sync | `74cb36f` |
| Sound toggle with speaker detection and M300 beep | `ccffb61` |
| Hardware backlight control with timeout highlighting | `62d7c99` |
| Animated heating progress indicator | `9d4b058` |
| Temperature icon clicks open temp panels | `7ee6c72` |
| Background temperature data collection | `065ddd5` |
| Motion panel responsive layout with kinematics | `0c96790` |
| Extrusion panel overhaul with new features | `a47f27c` |
| Icon font validation system | `49cc359` |
| MDI icon font unification | `7fc39c5` |

### November 2025 Highlights
| Feature | Date |
|---------|------|
| First-Run Wizard (all 7 steps) | 2025-11-30 |
| Fan Control sub-screen | 2025-11-30 |
| Exclude Object with undo | 2025-11-29 |
| ObserverGuard RAII pattern | 2025-11-29 |
| G-code memory-safe streaming | 2025-11-29 |
| Reactive UI refactoring | 2025-11-29 |
| Toast redesign (floating, top-right) | 2025-11-27 |
| Printer database v2.0 (50+ printers) | 2025-11-22 |
| Print Status with live Moonraker data | 2025-11-18 |
| Class-based panel architecture | 2025-11-17 |

---

## Architecture Principles

- **Reactive Pattern:** All UI state via Subject-Observer bindings
- **XML First:** Layout in XML, logic in C++
- **RAII Lifecycle:** PanelBase, ObserverGuard, ModalBase for safe cleanup
- **Design Tokens:** Colors, spacing, typography from globals.xml
- **Capability Detection:** Features shown/hidden based on printer capabilities
- **Mock Testing:** `--test` flag enables offline development

---

## Phase History

<details>
<summary>Expand to see original 15-phase development history</summary>

| Phase | Name | Status |
|-------|------|--------|
| 1 | Foundation (LVGL 9.3, XML, navigation) | ✅ Complete |
| 2 | Navigation & Blank Panels | ✅ Complete |
| 3 | Print Select Core | ✅ Complete |
| 4 | Print Select Polish | ✅ Complete |
| 5 | Controls Panel (6 sub-screens) | ✅ Complete |
| 6 | Additional Panel Content | ✅ Complete |
| 7 | Panel Transitions & Polish | ✅ Complete |
| 8 | Backend Integration (Moonraker) | ✅ Complete |
| 9 | Theming & Accessibility | ✅ Complete |
| 10 | Testing & Optimization | ✅ Complete |
| 11 | First-Run Wizard | ✅ Complete |
| 12 | Production Readiness | 🔄 In Progress |
| 13 | G-code Pre-Print Modification | ✅ Complete |
| 14 | Class-Based Panel Architecture | ✅ Complete |
| 15 | Reactive UI Architecture | ✅ Complete |

See `docs/archive/` for detailed phase documentation.

</details>

---

## Target Platforms

| Platform | Architecture | Status | Notes |
|----------|--------------|--------|-------|
| macOS | x86_64/ARM64 | ✅ Ready | Development with SDL2 |
| Linux | x86_64 | ✅ Ready | CI/CD tested |
| Raspberry Pi 4/5 | aarch64 | ✅ Ready | Docker cross-compile |
| BTT Pad | aarch64 | ✅ Ready | Same as Pi |
| Adventurer 5M | armv7-a | ✅ Ready | Static linking |

---

## Contributing

See `docs/CONTRIBUTING.md` for code standards and workflow.

Key files:
- `CLAUDE.md` - Project instructions and patterns
- `docs/LVGL9_XML_GUIDE.md` - XML layout reference
- `docs/ARCHITECTURE.md` - System design
- `docs/DEVELOPMENT.md` - Build and workflow
