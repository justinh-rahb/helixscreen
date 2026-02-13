# Mainsail Feature Research - Inspiration for HelixScreen

## Research Summary

Deep analysis of the Mainsail codebase (Vue 2.7 + Vuetify + Vuex) covering ~12 months of development (Feb 2025 - Feb 2026). 137+ commits, 5 major releases (v2.12 through v2.17).

---

## HIGH-VALUE Features We DON'T Have Yet

### Tier 1: Big Impact, Clearly Relevant to Touchscreen

| Feature | What Mainsail Does | HelixScreen Opportunity |
|---------|-------------------|------------------------|
| **Job Queue** | Drag-reorder print queue, count duplicates, sum ETA, add from history | Touch-friendly queue panel - swipe to reorder, tap to adjust count. Huge for batch printing. |
| **Exclude Objects** | Cancel individual objects mid-print via dialog | Perfect for touchscreen - tap object thumbnail to exclude. Moonraker already supports this. |
| **Maintenance Reminders** | Track by filament used, print hours, or calendar date. Snooze support. | "Nozzle change due in 50hrs" notification overlay. Touchscreen-native reminder cards. |
| **Print History + Stats** | Full job history, success/fail charts, filament usage trends, CSV export | History panel with stats dashboard - charts showing success rate, filament consumption, print hours. |
| **Timelapse Controls** | Layer macro vs hyperlapse mode, park head strategies, render settings | Timelapse settings in print panel, preview rendered timelapses on screen |
| **LED/Neopixel Control** | Light groups, color presets, neopixel RGBW picker, LED effects panel | Color wheel picker for chamber lights, preset buttons ("Printing", "Done", "Error") |
| **Multi-Color G-code** | Render multi-color file thumbnails/metadata | Show filament color breakdown on print-select cards |
| **Console with History** | Multi-line input, command history (up/down), autocomplete, RAW debug output | Touch console with swipe-through command history, tap-to-repeat |

### Tier 2: Medium Impact, Worth Considering

| Feature | What Mainsail Does | HelixScreen Opportunity |
|---------|-------------------|------------------------|
| **Spoolman Integration** | Active spool display, weight tracking, spool selection dialog, QR search | Spool info card on filament panel, "scan QR to load spool" if camera available |
| **Happy Hare / AFC MMU** | Full multi-material UI - gate mapping, clog detection meter, flowguard | MMU status panel with gate visualization (relevant for multi-material printers) |
| **Power Device Control** | Toggle TP-Link, Tasmota, relays on/off | Power panel with big toggle buttons for lights, enclosure fan, etc. |
| **Announcements** | RSS-like feed with priority levels, dismissal, snooze | Update/announcement cards on home panel |
| **G-code Viewer** | 3D layer-by-layer visualization with scrubbing | Could embed simplified 2D layer view for print preview |
| **Preheat Presets** | Named presets (PLA, PETG, ABS, etc.) with one-tap apply | We have temp presets - but could add chamber temp (M141) support |
| **Temperature Combined Sensors** | temperature_combined, SHT3X, AHT sensors, hall filament width | Support more sensor types in temp panel |

### Tier 3: Nice to Have, Lower Priority for Touchscreen

| Feature | What Mainsail Does | Notes |
|---------|-------------------|-------|
| **Farm Mode** | Multi-printer dashboard | Not relevant for single-printer touchscreen |
| **Config File Editor** | In-browser editor with CodeMirror | Too complex for touchscreen UX |
| **Built-in Themes** | Voron, Prusa, LDO, BTT, VzBot branded themes | Could do printer-brand themes |
| **Webcam in UI** | 10+ streaming backends | We could show webcam feed on a panel |
| **Update Manager** | Software update UI with git history | Could show "updates available" notification |

---

## Features Mainsail Added in Past 12 Months (Chronological)

### v2.12.0 (mid-2025)
- Built-in theme system (8 manufacturer themes: Voron, Prusa, LDO, BTT, VzBot, YUMI, Multec, Klipper)
- Moonraker sensors in history/statistics
- SGP40 sensor support
- TMC overheating warnings
- Fullscreen mode for files/viewer/webcam
- Favicon progress circle

### v2.13.0 (late summer 2025)
- Console refactor: debug prefix, RAW output, multi-line
- Moonraker heartbeat WebSocket keepalive
- Send-and-wait RPC helper
- Macro descriptions as tooltips
- Editor file structure sidebar
- Hide other Klipper instances option
- Status panel tabs → icons
- Second-layer cancel confirmation
- Default heightmap orientation option
- Multiple Nevermore filter support

### v2.14.0 (April 2025)
- Console multi-line input with key up/down history
- Macro settings search
- Count per page in history/timelapse panels
- SHT3X sensor support
- Hall filament width sensor
- Load cell gram scale display
- Dockable probe support in endstop panel
- Python package update support

### v2.15.0 (November 2025)
- **Happy Hare MMU integration** (huge - 15+ PRs)
- **AFC filament changer support** (10+ PRs)
- LED/neopixel panel refactor → dedicated LED effects panel
- Webcam rotation for all clients
- Multi-color G-code file support
- More date format options
- Light theme for CodeMirror
- Pressure advance for extruder_stepper
- Unprivileged Docker image

### v2.16.0 (December 2025)
- AHT1X/2X/3X sensor support
- Various AFC/HappyHare refinements
- ResizeObserver replacing vue-resize
- Sidebar template simplification

### v2.17.0 (January 2026)
- Temperature_combined sensor support
- Iframe-based webcam service
- Chamber temp (M141) in preheat
- LED effects panel (dedicated)
- Dialog modernization (v-model)
- RPC strict typing
- Vitest test environment
- Page title updates in inactive tabs
- Dynamic input width based on max temp
- Start print dialog on reprint from history
- ETA day calculation fix
- Natural sort for numeric file suffixes

---

## Patterns & Ideas Worth Stealing

### 1. Confirmation Escalation
Mainsail uses a "second layer" confirmation for dangerous actions (cancel print requires two taps). Good pattern for touchscreen where accidental taps are common.

### 2. Smart Input Locking
Features auto-disable based on printer state. Can't extrude when cold. Can't home during print. We do some of this but could be more thorough.

### 3. Macro Prompts
Dynamic parameter input dialogs generated from macro metadata. Macros can define buttons, text fields, button groups that Mainsail renders. We could support `RESPOND TYPE=command` prompts.

### 4. Maintenance Triggers
Three trigger types: filament consumed, print hours, calendar date. Each can snooze. Brilliant for touchscreen reminders.

### 5. History → Queue Pipeline
Can add a completed print directly back to the job queue. Good "reprint" workflow.

### 6. Exclude Objects Mid-Print
Moonraker supports this natively. Visual feedback showing which objects are excluded. Perfect for touchscreen tap-to-exclude.

### 7. Temperature Menu → Settings Bridge
Temperature panel has a menu to jump directly to settings or turn off all heaters. Nice navigation shortcut.

### 8. Webcam Nozzle Crosshair
Overlay showing nozzle position on webcam feed. Useful for alignment/leveling.

---

## What HelixScreen Already Does Better

For context - areas where our touchscreen-native approach wins:
- **Touch-optimized controls** (Mainsail is mouse-first)
- **XML declarative UI** (cleaner than Vue for embedded)
- **Subject-based reactivity** (lighter than Vuex)
- **Wizard flows** (Mainsail uses basic dialogs, we have proper wizards)
- **Real-time animation** (LVGL animations vs web CSS transitions)
- **Resource efficiency** (runs on Pi, Mainsail needs a browser)

---

## Recommended Feature Priorities for HelixScreen

Based on impact × feasibility for touchscreen:

1. **Exclude Objects** - High impact, Moonraker API exists, great touch UX
2. **Job Queue Panel** - Batch printing is huge for production users
3. **Print History + Stats** - Users love seeing their printer stats
4. **Maintenance Reminders** - Touchscreen notification cards are natural
5. **LED/Light Control** - Big colorful buttons, perfect for touch
6. **Preheat with Chamber Temp** - Quick add if we support M141
7. **Power Device Toggle** - Simple on/off buttons for peripherals
8. **Console Improvements** - Command history, better input
