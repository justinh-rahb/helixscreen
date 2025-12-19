# Filament Panel Rebuild Plan

## Summary
Comprehensive rebuild of the filament panel to fix critical code violations, improve layout efficiency, and implement real multi-filament management.

## Current Issues

### Critical Code Violations
1. **`lv_obj_add_event_cb()` for preset buttons** (lines 116-121) - violates Rules #12, #13
2. **Imperative state management** duplicating XML bindings (lines 206-228)
3. **Imperative icon updates** instead of conditional visibility (lines 159-166)
4. **Imperative checked state** on preset buttons (lines 237-249)
5. **`lv_obj_add_event_cb()` for modal dialog buttons** (lines 564-573, 604-614)

### Layout Problems
1. Temperature status card is oversized for its content
2. Operations section is cramped at bottom of screen
3. 40%/58% column split = 98%, leaving gaps
4. Buttons use unnecessary flex layout when they could size to content
5. AMS section is a useless stub ("coming soon")

### Design Token Violations
1. Hardcoded `height="40"` for AMS container
2. Hardcoded temperatures in XML ("210°C", "60°C", "170°C")
3. Redundant `clickable="true"` attributes

## Proposed New Layout

```
┌─────────────────────────────────────────────────────────────┐
│  FULL-SCREEN FILAMENT PANEL                                 │
├────────────────────────────┬────────────────────────────────┤
│  LEFT COLUMN (45%)         │  RIGHT COLUMN (55%)            │
├────────────────────────────┼────────────────────────────────┤
│  ┌────────────────────┐    │  ┌────────────────────────┐    │
│  │ TEMPERATURE STATUS │    │  │ MATERIAL PRESETS       │    │
│  │ [Icon] 25 / 210°C  │    │  │ ┌─────┐ ┌─────┐       │    │
│  │ Heating to 210°C...│    │  │ │ PLA │ │PETG │       │    │
│  └────────────────────┘    │  │ └─────┘ └─────┘       │    │
│                            │  │ ┌─────┐ ┌─────┐       │    │
│  ┌────────────────────┐    │  │ │ ABS │ │ ⚙️  │       │    │
│  │ FILAMENT SENSOR    │    │  │ └─────┘ └─────┘       │    │
│  │ (compact indicator)│    │  └────────────────────────┘    │
│  └────────────────────┘    │                                │
│                            │  Selected: PLA                 │
│  ┌────────────────────┐    │  Nozzle: 210°C  Bed: 60°C     │
│  │ MULTI-FILAMENT     │    │                                │
│  │ (if AMS detected)  │    │  ┌────────────────────────┐    │
│  │ ┌──┐┌──┐┌──┐┌──┐  │    │  │ OPERATIONS             │    │
│  │ │1 ││2 ││3 ││4 │  │    │  │ ┌─────────┐┌─────────┐ │    │
│  │ └──┘└──┘└──┘└──┘  │    │  │ │  LOAD   ││ UNLOAD  │ │    │
│  │ [Manage Slots]    │    │  │ └─────────┘└─────────┘ │    │
│  └────────────────────┘    │  │ ┌─────────────────────┐│    │
│                            │  │ │      PURGE 10mm    ││    │
│                            │  │ └─────────────────────┘│    │
│                            │  │ ⚠️ Heat to 170°C...    │    │
│                            │  └────────────────────────┘    │
└────────────────────────────┴────────────────────────────────┘
```

**Key changes:**
- Temperature status is compact (single row with icon + temps + status)
- Filament sensor indicator shown when sensors configured
- Operations section has generous vertical space
- Multi-filament section shows mini slot swatches when AMS detected
- Column split: 45%/55% (sums to 100%)

## Implementation Phases

### Phase 1: Fix Critical Code Violations (C++)

**File:** `src/ui_panel_filament.cpp`

1. **Move preset button events to XML** (lines 111-124)
   - Add `<event_cb trigger="clicked" callback="on_filament_preset_X"/>` to each preset button in XML
   - Register callbacks in constructor: `lv_xml_register_event_cb(nullptr, "on_filament_preset_pla", ...)`
   - Delete the `lv_obj_add_event_cb()` loop in `setup()`

2. **Remove imperative button state management** (lines 206-228)
   - XML already has `<lv_obj-bind_state_if_eq subject="filament_safety_warning_visible" state="disabled" ref_value="1"/>`
   - Delete all `lv_obj_add_state()` and `lv_obj_remove_state()` calls for buttons

3. **Replace imperative icon updates** (lines 159-166)
   - Use 3 icon elements with conditional visibility bindings
   - Add new subject `filament_status_state` (0=cold, 1=heating, 2=ready)
   - XML: `<icon src="check"><bind_flag_if_ne subject="filament_status_state" flag="hidden" ref_value="2"/></icon>`

4. **Replace imperative preset checked state** (lines 237-249)
   - Add `<lv_obj-bind_state_if_eq subject="filament_material_selected" state="checked" ref_value="N"/>` to each button
   - Delete `update_preset_buttons_visual()` function entirely

5. **Fix modal dialog button wiring** (lines 564-614)
   - Create a dedicated modal component with proper XML callbacks, OR
   - Use existing `modal_dialog` component and register callbacks globally

### Phase 2: Redesign XML Layout

**File:** `ui_xml/filament_panel.xml`

1. **Fix column widths**
   ```xml
   <lv_obj name="left_column" width="45%" .../>
   <lv_obj name="right_column" width="55%" .../>
   ```

2. **Compact temperature status**
   - Single row: icon + "25 / 210°C" + status text
   - Remove the oversized card layout

3. **Add filament sensor indicator**
   - Reuse `filament_sensor_indicator.xml` component
   - Show when sensors are configured

4. **Improve operations section**
   - Give it `flex_grow="1"` to take remaining space
   - Remove the spacer that was pushing it down

5. **Remove redundant attributes**
   - Delete all `clickable="true"` from buttons (they're clickable by default)

6. **Bind temperatures dynamically**
   - Replace `text="210°C"` with `bind_text="filament_selected_nozzle_temp"`
   - Add subjects for selected material's nozzle and bed temps

### Phase 3: Material Preset Editing

**Goal:** Allow customization of each preset's name and both temperature setpoints

1. **Create `filament_preset_edit_modal.xml`**
   - Reuse patterns from `ams_edit_modal.xml`
   - Fields: Name (text input), Nozzle temp (keypad), Bed temp (keypad)
   - Save/Cancel buttons

2. **Add preset configuration storage**
   - Store in `helixconfig.json` under `filament_presets`
   - Default values from `AppConstants::MaterialPresets`

3. **Implement preset long-press to edit**
   - Register `LV_EVENT_LONG_PRESSED` callback
   - Opens edit modal for that preset

### Phase 4: Multi-Filament Slot Management

**Goal:** "Manage Slots" opens a real slot editor (reusing AMS patterns)

1. **Create `filament_slots_overlay.xml`**
   - Side panel overlay (like `filament_sensors_overlay.xml`)
   - Shows slots 1-N with color swatches and material names
   - Tap slot to edit → opens `ams_edit_modal`

2. **Implement slot grid in filament panel**
   - When AMS detected, show mini slot swatches
   - Reuse `ams_slot.xml` component scaled down

3. **Connect to AMS backend**
   - Reuse existing `AmsState` and backend code
   - Changes sync to AMS system and optionally Spoolman

### Phase 5: Testing & Polish

1. Build and test all panels
2. Verify subject bindings work correctly
3. Check sensor indicator visibility logic
4. Test preset editing saves/loads correctly
5. Test slot management with mock AMS data

## Files to Modify

| File | Changes |
|------|---------|
| `ui_xml/filament_panel.xml` | Complete redesign of layout |
| `src/ui_panel_filament.cpp` | Remove imperative code, add new subjects |
| `include/ui_panel_filament.h` | Add new subject declarations |
| `ui_xml/filament_preset_edit_modal.xml` | NEW - preset editing modal |
| `ui_xml/filament_slots_overlay.xml` | NEW - slot management overlay |
| `src/ui_filament_preset_edit_modal.cpp` | NEW - modal logic |
| `src/ui_filament_slots_overlay.cpp` | NEW - overlay logic |

## New Subjects Needed

| Subject | Type | Purpose |
|---------|------|---------|
| `filament_status_state` | int | 0=cold, 1=heating, 2=ready (for icon visibility) |
| `filament_selected_nozzle_temp` | string | "210°C" for selected material |
| `filament_selected_bed_temp` | string | "60°C" for selected material |
| `filament_selected_name` | string | "PLA" for selected material |
| `filament_sensor_visible` | int | 1 if sensors configured, 0 otherwise |

## Existing Components to Reuse

**These already exist and should NOT be recreated:**

| Component | Location | Use For |
|-----------|----------|---------|
| `ams_edit_modal.xml` | `ui_xml/` | Slot editing - has vendor/material dropdowns, color picker, temp display, weight slider, Spoolman sync |
| `ams_slot.xml` | `ui_xml/` | Mini slot swatches for the multi-filament card |
| `filament_sensor_indicator.xml` | `ui_xml/` | Small colored dot showing sensor state (green/red/hidden) |
| `filament_sensors_overlay.xml` | `ui_xml/` | Already exists for sensor role configuration |
| `ams_color_picker` | `include/ui_ams_color_picker.h` | Color selection UI |
| `modal_dialog` | `ui_xml/` | Generic dialog component used by `ui_modal_show()` |

**Key C++ patterns already in codebase:**

```cpp
// Static trampoline pattern (from ui_panel_filament.cpp)
static void on_load_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[FilamentPanel] on_load_clicked");
    LV_UNUSED(e);
    get_global_filament_panel().handle_load_button();
    LVGL_SAFE_EVENT_CB_END();
}

// Modal dialog creation pattern (from show_load_warning)
ui_modal_config_t config = {
    .position = {.use_alignment = true, .alignment = LV_ALIGN_CENTER},
    .backdrop_opa = 180,
    .keyboard = nullptr,
    .persistent = false,
    .on_close = nullptr
};
const char* attrs[] = {"title", "Dialog Title", "message", "Dialog message", nullptr};
ui_modal_configure(UI_MODAL_SEVERITY_WARNING, true, "Proceed", "Cancel");
dialog_ = ui_modal_show("modal_dialog", &config, attrs);
```

**FilamentSensorManager subjects (already registered):**
- `filament_runout_detected` - int: -1=no sensor, 0=empty, 1=detected
- `filament_toolhead_detected` - int: same
- `filament_entry_detected` - int: same
- `filament_master_enabled` - int: 0=disabled, 1=enabled
- `filament_sensor_count` - int: number of discovered sensors

**Default temperatures from AppConstants:**
```cpp
AppConstants::MaterialPresets::PLA = 210    // Nozzle
AppConstants::MaterialPresets::PETG = 240
AppConstants::MaterialPresets::ABS = 250
AppConstants::MaterialPresets::CUSTOM_DEFAULT = 200
AppConstants::Temperature::MIN_EXTRUSION_TEMP = 170  // Safety threshold
```

**Reference implementations to study:**
- `src/ui_panel_bed_mesh.cpp` - Gold standard for subject-based panels (per CLAUDE.md)
- `ui_xml/nozzle_temp_panel.xml` - Temperature display patterns, button grids
- `ui_xml/extrusion_panel.xml` - Content-based button sizing
- `src/ui_ams_edit_modal.cpp` - How edit modals work with dropdowns and color picker

## Risk Assessment

- **Low risk:** Fixing code violations is mechanical - patterns are well-established
- **Medium risk:** Layout redesign may need iteration to look right
- **Medium risk:** Preset editing adds new persistence (helixconfig.json)
- **Low risk:** Slot management reuses proven AMS code

## Definition of Done

- [ ] No `lv_obj_add_event_cb()` calls except for dynamically-created dialogs
- [ ] No imperative state/icon management in panel code
- [ ] All colors from design tokens
- [ ] Temperature display is compact and bound to subjects
- [ ] Operations section has adequate space
- [ ] "Manage Slots" opens real slot editor when AMS detected
- [ ] Preset customization works (name + nozzle + bed temps)
- [ ] Screenshot tests pass
