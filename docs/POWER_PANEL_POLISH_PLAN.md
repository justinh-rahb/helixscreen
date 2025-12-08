# Power Panel UI Polish Plan

**Created:** 2025-12-08
**Status:** In Progress
**Goal:** Elevate Power Control panel to first-in-class touchscreen UI quality

---

## Overview

The Power Device Control panel is functionally complete but needs UI polish to meet our quality standards. A critical UI review identified 5 critical, 4 major, and 3 minor issues.

---

## Stage 1: Quick Wins (Design Token Compliance)
**Goal:** Fix hardcoded values and document exceptions
**Status:** Not Started
**Estimated Time:** 10 minutes

### Tasks

- [ ] **1.1** Document Rule 12 exception for dynamic widgets
  - File: `src/ui_panel_power.cpp:182`
  - Add comment explaining why `lv_obj_add_event_cb()` is used

- [ ] **1.2** Replace hardcoded pixel values with design tokens
  - File: `src/ui_panel_power.cpp:133-136`
  - Change `pad_all=12` → `ui_theme_get_px("#space_md")`
  - Change `radius=8` → `ui_theme_get_px("#border_radius")`

- [ ] **1.3** Enlarge touch targets to minimum 48px height
  - File: `src/ui_panel_power.cpp:150`
  - Set switch size: `lv_obj_set_size(toggle, 60, 48)`

### Success Criteria
- [ ] No hardcoded pixel values in `create_device_row()`
- [ ] Touch targets meet 48px minimum
- [ ] Build passes

---

## Stage 2: Create Reusable XML Component
**Goal:** Replace manual C++ widget creation with XML component
**Status:** Not Started
**Estimated Time:** 30 minutes

### Tasks

- [ ] **2.1** Create `ui_xml/power_device_row.xml` component
  ```xml
  <component>
    <api>
      <prop type="string" name="device_name" default="Device"/>
    </api>
    <view extends="lv_obj" name="power_device_row" ...>
      <lv_obj flex_grow="1" flex_flow="column">
        <text_body name="device_label" text="$device_name"/>
        <text_small name="device_status" text="" style_text_color="#text_secondary"/>
      </lv_obj>
      <lv_switch name="device_toggle" width="60" height="48">
        <event_cb trigger="value_changed" callback="on_power_toggle"/>
      </lv_switch>
    </view>
  </component>
  ```

- [ ] **2.2** Register component in `main.cpp`

- [ ] **2.3** Register event callback `on_power_toggle` globally

- [ ] **2.4** Update `create_device_row()` to use `lv_xml_create()`
  - Pass device_name as prop
  - Find toggle by name, set state and user_data
  - Remove manual widget creation code

- [ ] **2.5** Add long text handling
  - Set `lv_label_set_long_mode(label, LV_LABEL_LONG_DOT)`
  - Limit label width to prevent overflow

### Success Criteria
- [ ] Device rows created via XML component
- [ ] Consistent with `setting_toggle_row` pattern
- [ ] Long device names truncate with "..."
- [ ] Build and test pass

---

## Stage 3: Lock Indicator
**Goal:** Clear visual feedback for locked devices during printing
**Status:** Not Started
**Estimated Time:** 15 minutes

### Tasks

- [ ] **3.1** Add lock icon to XML component
  ```xml
  <lv_label name="lock_icon" text="#icon_lock"
            style_text_font="mdi_icons_24" hidden="true"/>
  ```

- [ ] **3.2** Add "Locked during print" secondary text
  - Show in `device_status` label when locked

- [ ] **3.3** Update C++ to show/hide lock indicator
  ```cpp
  if (is_locked) {
      lv_obj_clear_flag(lock_icon, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(status_label, "Locked during print");
  }
  ```

- [ ] **3.4** Verify lock icon is in `codepoints.h`
  - If not, add and run `make regen-fonts`

### Success Criteria
- [ ] Lock icon visible on locked devices
- [ ] Explanation text shown
- [ ] Visual distinction beyond just opacity

---

## Stage 4: Empty State UI
**Goal:** Clear feedback when no power devices configured
**Status:** Not Started
**Estimated Time:** 15 minutes

### Tasks

- [ ] **4.1** Add empty state container to `power_panel.xml`
  ```xml
  <lv_obj name="empty_state" width="100%" flex_grow="1"
          flex_flow="column" style_flex_main_place="center"
          style_flex_cross_place="center" hidden="true">
    <lv_label text="#icon_power_plug_off" style_text_font="mdi_icons_64"
              style_text_color="#text_secondary"/>
    <text_heading text="No Power Devices" style_text_color="#text_secondary"/>
    <text_body text="Configure devices in Moonraker"
               style_text_color="#text_secondary"/>
  </lv_obj>
  ```

- [ ] **4.2** Add subject for empty state visibility
  - `power_has_devices` subject (int: 0 or 1)

- [ ] **4.3** Toggle visibility in `populate_device_list()`
  ```cpp
  bool has_devices = !devices.empty();
  lv_subject_set_int(&has_devices_subject_, has_devices ? 1 : 0);
  ```

- [ ] **4.4** Verify icon exists or add to codepoints

### Success Criteria
- [ ] Empty state shown when 0 devices
- [ ] Icon + heading + guidance text
- [ ] Device list shown when devices exist

---

## Stage 5: Error Feedback Enhancement
**Goal:** Visual feedback on the specific row that failed
**Status:** Not Started
**Estimated Time:** 15 minutes

### Tasks

- [ ] **5.1** Add error highlight on failed toggle
  ```cpp
  // Flash row red briefly on error
  lv_obj_set_style_bg_color(row.container,
      ui_theme_parse_color("#error_color"), 0);
  ```

- [ ] **5.2** Create timer to restore color after 2 seconds

- [ ] **5.3** Add loading spinner during device fetch
  - Show spinner, hide device list
  - On complete: hide spinner, show list

### Success Criteria
- [ ] Failed row flashes red
- [ ] Color restores after timeout
- [ ] Loading state has spinner

---

## Stage 6: Minor Polish (Optional)
**Goal:** Additional refinements for excellence
**Status:** Not Started
**Estimated Time:** 20 minutes

### Tasks

- [ ] **6.1** Add refresh button to header
- [ ] **6.2** Add status icon next to status message
- [ ] **6.3** Consider pull-to-refresh gesture

---

## Progress Tracking

| Stage | Status | Completed |
|-------|--------|-----------|
| Stage 1: Quick Wins | ✅ Complete | 2025-12-08 |
| Stage 2: XML Component | ✅ Complete | 2025-12-08 |
| Stage 3: Lock Indicator | ✅ Complete | 2025-12-08 |
| Stage 4: Empty State | ✅ Complete | 2025-12-08 |
| Stage 5: Error Feedback | Not Started | |
| Stage 6: Minor Polish | Not Started | |

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/ui_panel_power.cpp` | Design tokens, XML component usage, lock/error handling |
| `ui_xml/power_panel.xml` | Empty state container |
| `ui_xml/power_device_row.xml` | **NEW** - Reusable row component |
| `src/main.cpp` | Register new component and event callback |
| `assets/fonts/codepoints.h` | Lock icon, power plug off icon (if missing) |

---

## Review Checklist (Before Completion)

- [ ] All design tokens used (no hardcoded pixels/colors)
- [ ] Touch targets >= 48px height
- [ ] Long text truncates properly
- [ ] Lock indicator visible with explanation
- [ ] Empty state shows icon + guidance
- [ ] Error feedback highlights specific row
- [ ] Consistent with other panels (fan_panel, settings_panel)
- [ ] Build passes
- [ ] Visual test on 800x480 screen size

---

## References

- UI Review: Critical assessment from ui-reviewer agent
- Pattern: `ui_xml/setting_toggle_row.xml`
- Pattern: `src/ui_panel_fan.cpp`
- Design tokens: `ui_xml/globals.xml`
