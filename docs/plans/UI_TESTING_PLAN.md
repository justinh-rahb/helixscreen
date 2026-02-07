# UI Testing Plan

## Status & Context

**Status**: Stalled - Infrastructure exists, coverage is minimal
**Last Updated**: 2026-02-06
**Previous Attempt**: Jan 2026 built XMLTestFixture and 5 temp_display binding tests, 38 scaffolded tests never implemented

### The Problem

We have ~145 XML components, 31 panels, 25+ modals, 13 wizard steps, and 30+ event callbacks. Our UI test coverage:

| Area | Coverage | Tests |
|------|----------|-------|
| Panel navigation | **0%** | Zero tests |
| User interactions (click/type) | **~5%** | A few wizard connection tests |
| Subject bindings | **~3%** | 5 temp_display tests out of 144+ bindings |
| Event callbacks | **~10%** | A handful of settings callbacks |
| Workflow end-to-end | **0%** | Zero tests |
| Panel rendering/structure | **~5%** | AMS slot structure only |

Every new panel and XML change is untested until someone sees it on screen. The longer this goes, the worse it gets.

### What We're NOT Doing

**No screenshot-based visual regression.** Pixel comparison is too brittle for active UI development - any color tweak, font change, or wording edit breaks every test. We use structural/behavioral testing instead: verify widgets exist, bindings propagate, clicks trigger actions, visibility states are correct.

---

## Infrastructure (Already Built)

The test infrastructure is solid. The gap is test coverage, not tooling.

### Fixture Hierarchy

```
LVGLTestFixture                 ← Virtual 800x480 display, thread-safe LVGL init
├── XMLTestFixture              ← + Theme, fonts, XML component registration, subject bindings
├── UITestFixture               ← + Click/type simulation via UITest helpers
├── MoonrakerTestFixture        ← + PrinterState, MoonrakerClient, MoonrakerAPI
├── FullMoonrakerTestFixture    ← MoonrakerTestFixture + UITestFixture combined
└── LVGLUITestFixture           ← Full production-like init (ALL components, ALL subjects, ALL callbacks)
```

**Use `LVGLUITestFixture` for most new tests.** It mirrors production startup and handles all registration automatically. Only drop to XMLTestFixture for isolated component tests.

### UITest Helpers (`tests/ui_test_utils.h`)

```cpp
UITest::click(widget)                    // Click widget center
UITest::click_at(x, y)                   // Click coordinates
UITest::type_text(textarea, "text")      // Type into input
UITest::send_key(LV_KEY_ENTER)           // Send key event
UITest::wait_ms(ms)                      // Wait with LVGL processing
UITest::wait_until(condition, timeout)   // Wait for condition
UITest::wait_for_visible(widget, ms)     // Wait for show
UITest::wait_for_hidden(widget, ms)      // Wait for hide
UITest::is_visible(widget)               // Check visibility
UITest::is_checked(widget)               // Check state
UITest::get_text(widget)                 // Get label text
UITest::find_by_name(parent, "name")     // Find child by name
```

### Key Technical Facts

1. **Subject binding is synchronous** - `lv_subject_set_int()` fires observers immediately, no `process_lvgl()` needed for binding tests
2. **Temperatures in centidegrees** - `20000` = 200.00C
3. **Widget attrs are null-terminated** - `const char* attrs[] = {"key", "value", nullptr}`
4. **`process_lvgl()` can hang** with async updates - avoid for simple binding tests
5. **Panels need dependency registration** - complex panels use sub-components that must be registered first (LVGLUITestFixture handles this)

---

## Approach: Structural/Behavioral Testing

We test three things, in order of importance:

### 1. Binding Correctness (Does the UI show the right data?)

Set a subject value, verify the UI widget reflects it. This catches:
- Broken bindings (typo in subject name, wrong binding type)
- Wrong transforms (centidegrees not converted, missing formatting)
- Visibility logic errors (wrong ref_value in bind_flag_if_eq)

```cpp
TEST_CASE_METHOD(LVGLUITestFixture, "home: extruder temp binding", "[ui][home]") {
    state().set_extruder_temp(20000);
    auto* panel = ui_nav_get_current_panel();
    auto* temp = UITest::find_by_name(panel, "extruder_temp");
    REQUIRE(ui_temp_display_get_current(temp) == 200);
}
```

### 2. Interaction Correctness (Do clicks do the right thing?)

Click a button, verify the expected state change or navigation happens.

```cpp
TEST_CASE_METHOD(LVGLUITestFixture, "controls: home all button", "[ui][controls]") {
    ui_nav_go_to("controls_panel");
    auto* panel = ui_nav_get_current_panel();
    auto* btn = UITest::find_by_name(panel, "btn_home_all");
    UITest::click(btn);
    // Verify G28 was sent via mock client
    REQUIRE(client().last_gcode() == "G28");
}
```

### 3. State-Driven Visibility (Does the right stuff show/hide?)

Set state, verify correct elements are visible/hidden.

```cpp
TEST_CASE_METHOD(LVGLUITestFixture, "home: disconnected overlay", "[ui][home]") {
    state().set_connection_state(0);  // disconnected
    auto* overlay = UITest::find_by_name(panel, "disconnected_overlay");
    REQUIRE(UITest::is_visible(overlay));

    state().set_connection_state(1);  // connected
    REQUIRE_FALSE(UITest::is_visible(overlay));
}
```

---

## Phases

### Phase 1: Panel Binding Smoke Tests

**Goal**: Every main panel has a test that creates it, sets representative state, and verifies key bindings work.

**Constraint**: These tests must be FAST (<100ms each). No sleeps, no timers. Set state → check widget → done.

| Panel | Key Bindings to Test | Priority |
|-------|---------------------|----------|
| `home_panel` | extruder/bed temp, status text, connection state, print progress, notification badge | P1 |
| `controls_panel` | X/Y/Z position, homed indicators, speed/flow %, fan slider | P1 |
| `print_status_panel` | filename, progress, layers, elapsed/remaining, pause button state | P1 |
| `print_select_panel` | view mode toggle | P1 |
| `filament_panel` | sensor states, AMS slot visibility | P2 |
| `settings_panel` | toggle states (dark mode, animations, sounds) | P2 |
| `nozzle_temp_panel` | current temp, target temp, presets | P2 |
| `bed_temp_panel` | current temp, target temp, presets | P2 |
| `motion_panel` | position readouts, step size | P2 |
| `extrusion_panel` | extrude length, speed | P3 |

**Estimated**: ~50 tests, ~10 per P1 panel. All fast.

**File**: `tests/unit/test_ui_panel_smoke.cpp` (one file, organized by panel in SECTIONs)

### Phase 2: Visibility & Conditional UI

**Goal**: Test bind_flag_if_eq / bind_flag_if_not_eq logic for critical UI states.

| Scenario | What to Test |
|----------|-------------|
| Disconnected | disconnected_overlay visible, nav buttons disabled |
| Klippy shutdown | shutdown_overlay visible, restart button shown |
| Printing | print status overlay shown, idle elements hidden |
| AMS present | AMS button visible on home, AMS panel accessible |
| No AMS | AMS button hidden |
| Notification | Badge visible with count > 0, hidden at 0 |
| E-stop enabled | E-stop button visible |
| LED present | Light button visible |

**Estimated**: ~20 tests. All fast (just set subject, check visibility).

**File**: `tests/unit/test_ui_visibility_states.cpp`

### Phase 3: Navigation & Interaction

**Goal**: Test that clicking buttons does the right thing.

| Test | Action | Verification |
|------|--------|-------------|
| Tab navigation | Click each tab | Correct panel shown |
| Overlay push/pop | Click nozzle temp → opens overlay → back button → returns | Panel stack correct |
| Home all button | Click home all on controls | G28 sent to mock client |
| Speed +/- buttons | Click speed up/down | speed_factor subject changes |
| Flow +/- buttons | Click flow up/down | flow_factor subject changes |
| Fan slider | Drag fan slider | Fan speed command sent |
| Temp presets | Click PLA preset | Target temp set |
| Print cancel | Click cancel → confirm | Cancel command sent |
| Pause/resume | Click pause button | Pause command sent |

**Estimated**: ~20 tests. Some may need `wait_ms()` for animation/debounce but should be <500ms each.

**File**: `tests/unit/test_ui_interactions.cpp`

### Phase 4: Modal Flows

**Goal**: Test modal show/dismiss, form input, confirm/cancel patterns.

| Modal | Flow |
|-------|------|
| `print_cancel_confirm` | Show → click confirm → verify cancel sent, modal dismissed |
| `wifi_password_modal` | Show → type password → click connect → verify |
| `save_z_offset_modal` | Show → verify offset displayed → click save |
| `exclude_object_modal` | Show → select object → verify exclusion → undo timer |
| `abort_progress_modal` | Show → track 7-state progression |

**Estimated**: ~15 tests. May need `wait_for_hidden()` for dismiss animations.

**File**: `tests/unit/test_ui_modals.cpp`

### Phase 5: Wizard Flow

**Goal**: Test the setup wizard end-to-end.

| Test | What |
|------|------|
| Full wizard flow | Step through all 9 steps, verify progression |
| Conditional skipping | Skip fan/LED/sensor steps when hardware absent |
| Back navigation | Go back preserves entered data |
| Connection validation | Invalid IP shows error, valid IP connects |
| WiFi password entry | Type password, verify connection attempt |

**Estimated**: ~10 tests. Some will be slow (connection attempts) - tag `[slow]` if >1s.

**File**: `tests/unit/test_ui_wizard_flow.cpp`

### Phase 6: Error State UI

**Goal**: Test that error conditions show the right UI.

| Error | Expected UI |
|-------|------------|
| Connection lost | Reconnecting overlay, retry countdown |
| Klippy shutdown | Shutdown overlay, firmware restart button |
| Print failed | Error message, return to file select |
| Filament runout | Guidance modal with load/resume options |
| Thermal runaway | Emergency message, heater status |

**Estimated**: ~10 tests.

**File**: `tests/unit/test_ui_error_states.cpp`

---

## Timing Budget

**Constraint**: `make test-run` must complete in <2 minutes (includes build).

All UI tests in Phases 1-4 should be <100ms each (no sleeps, synchronous bindings). Phase 5-6 may have some that need `[slow]` tagging.

| Phase | Tests | Est. Time |
|-------|-------|-----------|
| 1. Binding smoke | ~50 | <2s total |
| 2. Visibility | ~20 | <1s total |
| 3. Interactions | ~20 | <5s total |
| 4. Modals | ~15 | <3s total |
| 5. Wizard | ~10 | <10s total (some slow) |
| 6. Error states | ~10 | <2s total |
| **Total** | **~125** | **<25s** |

These are fast tests. The structural approach (set state, check widgets) doesn't need timers.

---

## Implementation Notes

### Test Organization

One file per phase, SECTIONs per panel/component within each file. Keeps related tests together and makes it easy to run subsets:

```bash
./build/bin/helix-tests "[ui][home]"       # All home panel UI tests
./build/bin/helix-tests "[ui][binding]"    # All binding tests
./build/bin/helix-tests "[ui][interaction]" # All interaction tests
```

### Tagging Convention

```
[ui]                 - All UI tests
[ui][binding]        - Subject binding verification
[ui][visibility]     - Show/hide state tests
[ui][interaction]    - Click/type simulation
[ui][modal]          - Modal flow tests
[ui][wizard]         - Wizard flow tests
[ui][error]          - Error state UI tests
[ui][home]           - Home panel specific
[ui][controls]       - Controls panel specific
[ui][print_status]   - Print status specific
... etc per panel
```

### When a Test Fails

A binding test failure means one of:
- **Subject name typo in XML** - binding silently fails, widget shows default
- **Wrong binding type** - e.g., bind_text where bind_value was needed
- **Transform error** - centidegrees not converted, wrong format string
- **Registration missing** - component or subject not registered

These are real bugs that would otherwise only be caught by visual inspection on hardware.

### What NOT to Test

- Pixel positions or exact layout (changes with theme/font updates)
- Animation timing or visual effects
- LVGL internals (subject pointer validity, observer mechanics)
- Things already tested by characterization tests (initial values, state transitions)

---

## Appendix: Binding Inventory (from XML analysis)

### home_panel (58 bindings)

| Widget | Subject | Type |
|--------|---------|------|
| `status_text_label` | status_text | bind_text |
| `printer_type_text` | printer_type_text | bind_text |
| `print_display_filename` | print_display_filename | bind_text |
| `print_progress_bar` | print_progress | bind_value |
| `disconnected_overlay` | printer_connection_state | bind_flag_if_eq |
| `shutdown_overlay` | klippy_state | bind_flag_if_not_eq |
| `notification_badge` | notification_count | bind_flag_if_eq |
| `extruder_temp` | extruder_temp | bind_current |
| `network_label` | network_label | bind_text |
| `estop_button_container` | estop_visible | bind_flag_if_eq |
| `light_button` | printer_has_led | bind_flag_if_eq |
| `ams_button` | ams_slot_count | bind_flag_if_eq |
| ... | (46 more) | ... |

### controls_panel (49 bindings)

| Widget | Subject | Type |
|--------|---------|------|
| `pos_x` | controls_pos_x | bind_text |
| `pos_y` | controls_pos_y | bind_text |
| `pos_z` | controls_pos_z | bind_text |
| `speed_pct` | controls_speed_pct | bind_text |
| `flow_pct` | controls_flow_pct | bind_text |
| `x_homed_indicator` | x_homed | bind_style |
| `part_fan_slider` | controls_fan_pct | bind_value |
| `nozzle_temp_display` | extruder_temp | bind_current |
| `bed_temp_display` | bed_temp | bind_current |
| `btn_macro_1-4` | macro_N_visible | bind_flag_if_eq |
| ... | (39 more) | ... |

### print_status_panel (29 bindings)

| Widget | Subject | Type |
|--------|---------|------|
| `print_display_filename` | print_display_filename | bind_text |
| `print_elapsed` | print_elapsed | bind_text |
| `print_remaining` | print_remaining | bind_text |
| `print_progress` | print_progress | bind_value |
| `print_layer_text` | print_layer_text | bind_text |
| `preparing_overlay` | preparing_visible | bind_flag_if_eq |
| `extruder_temp` | extruder_temp | bind_current |
| `bed_temp` | bed_temp | bind_current |
| `pause_button_icon` | pause_button_icon | bind_text |
| ... | (20 more) | ... |
