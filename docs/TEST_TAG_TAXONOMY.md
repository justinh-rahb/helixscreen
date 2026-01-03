# Test Tag Taxonomy

This document describes the test tagging system for HelixScreen's ~2000 unit tests.

## Philosophy

Tests are tagged by **feature/importance**, not by layer/speed. This enables:
- Running all tests for a specific feature during development
- Identifying critical tests that must pass before any commit
- Understanding test coverage by functional area

## Tag Categories

### Importance Tags

| Tag | Count | Purpose | Runtime |
|-----|-------|---------|---------|
| `[core]` | ~18 | **Critical tests** - if these fail, the app is fundamentally broken | <2s |
| `[slow]` | ~137 | Tests that take >500ms - excluded from fast iteration | varies |

### Feature Tags

| Tag | Count | Purpose |
|-----|-------|---------|
| `[connection]` | ~70 | Moonraker WebSocket connection lifecycle, retry logic, robustness |
| `[state]` | ~60 | PrinterState singleton, LVGL subjects, observer patterns |
| `[print]` | ~46 | Print workflow: start, pause, cancel, exclude object, progress |
| `[api]` | ~79 | Moonraker API infrastructure, request/response handling |
| `[calibration]` | ~27 | Bed mesh, input shaper, QGL, Z-tilt |
| `[printer]` | ~130 | Printer detection, capabilities, hardware discovery |
| `[ams]` | ~67 | AMS/MMU backends (includes `[afc]`, `[valgace]` sub-tags) |
| `[filament]` | ~32 | Spoolman integration, filament sensors |
| `[network]` | ~25 | WiFi scanning/connecting, Ethernet management |
| `[assets]` | ~28 | Thumbnail extraction, prerendered image handling |
| `[ui]` | ~138 | Theme, icons, widgets, notifications, panels |
| `[gcode]` | ~125 | G-code parsing, streaming, geometry |
| `[config]` | ~64 | Configuration loading, validation, defaults |
| `[wizard]` | ~29 | Setup wizard flow |
| `[history]` | ~24 | Print history, notification history |
| `[application]` | ~40 | Application lifecycle, initialization |

### Sub-Tags (Used with Parent Tags)

| Tag | Parent | Purpose |
|-----|--------|---------|
| `[afc]` | `[ams]` | AFC (Armored Filament Changer) backend |
| `[valgace]` | `[ams]` | Valgace AMS backend |
| `[ui_theme]` | `[ui]` | Theme colors, fonts, spacing |
| `[ui_icon]` | `[ui]` | Icon rendering, MDI icons |
| `[navigation]` | `[ui]` | Panel switching, overlay stack |

## Make Targets

```bash
# Critical tests (must always pass)
make test-core           # ~18 tests, <2s

# Feature-specific tests
make test-connection     # Connection lifecycle
make test-state          # PrinterState, observers
make test-print          # Print workflow
make test-calibration    # Bed mesh, input shaper
make test-printer        # Printer detection
make test-ams            # AMS backends
make test-filament       # Spoolman, sensors
make test-assets         # Thumbnails
make test-network        # WiFi, Ethernet
make test-config         # Configuration

# Speed-based tests
make test-fast           # All except [slow] and hidden
make test-slow           # Only [slow] tests
make test-smoke          # Minimal critical subset

# All tests
make test-run            # All except [slow] and hidden
make test-all            # Everything including [slow]
```

## Core Tests (Must Pass)

These 18 tests validate the most critical functionality:

### PrinterState (`test_printer_state.cpp`)
- Singleton returns same instance
- Singleton persists modifications  
- Singleton subjects have consistent addresses
- Observer fires when printer connection state changes

### Navigation (`test_navigation.cpp`)
- Navigation initialization
- Panel switching
- Invalid panel handling
- Repeated panel selection
- All panels are accessible

### Config (`test_config.cpp`)
- get() returns existing string value
- get() returns existing int value
- get() with missing key throws exception
- get() with default returns default when key missing

### Print Start (`test_print_start_collector.cpp`)
- PRINT_START marker detection
- Completion marker detection
- Homing phase detection
- Heating bed phase detection

### UI (`test_ui_temp_graph.cpp`)
- Create and destroy graph

## Migration History

### 2024-12-20: Initial Taxonomy

**Removed:**
- `[moonraker]` tag (was 189 tests covering everything) - too broad

**Added:**
- `[core]` tag for ~18 critical tests
- `[connection]` tag for connection lifecycle tests
- `[state]` tag for PrinterState/observer tests
- `[print]` tag for print workflow tests
- `[calibration]` tag for bed mesh/input shaper tests
- `[assets]` tag for thumbnail tests

**Migrated:**
- Connection tests: `test_moonraker_connection_retry.cpp`, `test_moonraker_client_*.cpp`
- State tests: `test_printer_state.cpp`, `test_mock_shared_state.cpp`
- Print tests: `test_print_start_collector.cpp`, `test_mock_print_simulation.cpp`
- API tests: `test_moonraker_api_*.cpp`
- Calibration tests: `test_bed_mesh_*.cpp`, `test_moonraker_api_input_shaper.cpp`

## Adding New Tests

When creating new tests:

1. **Always add a feature tag** - What functional area does this test?
2. **Add `[core]` if critical** - Would the app be broken without this?
3. **Add `[slow]` if >500ms** - Keeps fast iteration fast
4. **Consider sub-tags** - e.g., `[ams][afc]` for AFC-specific tests

Example:
```cpp
// Good: Feature + importance tags
TEST_CASE("PrinterState observer cleanup", "[core][state][observer]")

// Good: Feature + speed tag
TEST_CASE("Connection retry with 5s timeout", "[connection][slow]")

// Bad: No feature context
TEST_CASE("Some test", "[unit]")
```

## Disabled Tests

Tests prefixed with `[.]` are hidden/disabled. Currently ~68 disabled tests need triage:
- Some are integration tests requiring full environment
- Some are obsolete after refactoring
- Some are flaky and need fixing

Run `./build/bin/helix-tests "[.]" --list-tests` to see all disabled tests.
