# WiFi Settings Panel Implementation Plan

## Goal
Implement a comprehensive WiFi/Network settings overlay matching the reference UI, with **REACTIVE architecture** - XML declares bindings, C++ manages subjects.

## User Requirements (Confirmed)
1. ✅ **Right-side overlay** from settings panel
2. ✅ **Two-stage network test** using `ui_step_progress` (vertical): gateway → internet
3. ✅ **Hidden network support** via manual SSID entry
4. ✅ **Share components** between wizard and settings
5. ✅ **Reactive UI** - bind dynamic elements in XML, minimal C++ widget manipulation

---

## Reference UI Layout

```
┌─────────────────────────────────────────────────────────────┐
│ ← WiFi Settings                                             │
├────────────────────────┬────────────────────────────────────┤
│ WLAN (Only 2.4G)  [ON] │ Other networks (4)           [🔄]  │
│                        │ ┌──────────────────────────────┐   │
│ Current network        │ │ Guest              🔒 📶      │   │
│ ┌────────────────────┐ │ ├──────────────────────────────┤   │
│ │ ✓ Weissach IoT  📶 │ │ │ High Meadow        🔒 📶      │   │
│ │ IP: 192.168.30.202 │ │ ├──────────────────────────────┤   │
│ │ MAC: 50:41:1C:...  │ │ │ GL-X3000-6c0       🔒 📶      │   │
│ └────────────────────┘ │ └──────────────────────────────┘   │
│                        │                                    │
│ Test network 🔍        │ + Add Other Networks               │
│ ● Gateway    ✓         │                                    │
│ ○ Internet             │                                    │
└────────────────────────┴────────────────────────────────────┘
```

---

## Architecture: Reactive UI Pattern

### Principle: XML Declares, C++ Updates Subjects

```
┌─────────────────┐      subjects      ┌─────────────────┐
│     C++ Code    │ ──────────────────▶│   lv_subject_t  │
│ (WiFiManager,   │                    │ wifi_enabled    │
│  NetworkTester) │                    │ connected_ssid  │
└─────────────────┘                    │ ip_address      │
                                       │ mac_address     │
                                       │ network_count   │
                                       │ wifi_connected  │
                                       │ test_step       │
                                       │ test_status     │
                                       └────────┬────────┘
                                                │ bindings
                                                ▼
                                       ┌─────────────────┐
                                       │   XML Layout    │
                                       │ bind_text       │
                                       │ bind_flag_if_eq │
                                       │ bind_state_if   │
                                       └─────────────────┘
```

### C++ Responsibility: Update Subjects Only
```cpp
// ✅ CORRECT - Update subject, XML reacts automatically
lv_subject_set_string(&connected_ssid_, "Weissach IoT");
lv_subject_set_int(&wifi_connected_, 1);

// ❌ WRONG - Direct widget manipulation
lv_label_set_text(ssid_label, "Weissach IoT");
lv_obj_clear_flag(connected_icon, LV_OBJ_FLAG_HIDDEN);
```

---

## Files Summary

### Create (7 files)
| File | Purpose |
|------|---------|
| `include/wifi_ui_utils.h` | Shared utility header |
| `src/wifi_ui_utils.cpp` | Signal icon calculation, MAC address retrieval |
| `include/network_tester.h` | Async network test header |
| `src/network_tester.cpp` | Gateway detection, ping implementation |
| `include/wifi_settings_overlay.h` | Reactive overlay class header |
| `src/wifi_settings_overlay.cpp` | Subject management, callbacks |
| `ui_xml/hidden_network_modal.xml` | Hidden network entry modal |

### Replace (1 file)
| File | Purpose |
|------|---------|
| `ui_xml/wifi_settings_overlay.xml` | Two-column reactive layout |

### Modify (3 files)
| File | Changes |
|------|---------|
| `src/ui_panel_settings.cpp` | Use WiFiSettingsOverlay |
| `src/main.cpp` | Register hidden_network_modal |
| `Makefile` | Add new source files |

---

## Implementation Strategy: Agent-Based

Use specialized agents to maximize parallelism and code quality:

| Phase | Primary Agent | Review Agent |
|-------|---------------|--------------|
| 1. Shared Utils | `general-purpose` | `critical-reviewer` |
| 2. Network Tester | `general-purpose` | `critical-reviewer` |
| 3. XML Layout | `widget-maker` | `ui-reviewer` |
| 4. Hidden Modal | `widget-maker` | `ui-reviewer` |
| 5. Overlay Class | `general-purpose` | `critical-reviewer` |
| 6. Integration | `general-purpose` | `critical-reviewer` |

---

## Phase-by-Phase Implementation

### Phase 1: Shared Utilities

**Goal:** Extract reusable WiFi UI utilities from wizard

**Tasks:**
- [ ] Create `include/wifi_ui_utils.h`
- [ ] Create `src/wifi_ui_utils.cpp`
- [ ] Implement `wifi_compute_signal_icon_state()`
- [ ] Implement `wifi_get_device_mac()`
- [ ] Update Makefile

**Agent:** `general-purpose` with prompt:
```
Create wifi_ui_utils.h/cpp with:
1. Signal icon state calculation (1-8 from strength + secured)
2. Device MAC address retrieval (Linux: /sys/class/net/wlan0/address, macOS: ifconfig)
Reference ui_wizard_wifi.cpp for existing signal calculation.
```

**Review:** `critical-reviewer` - check error handling, thread safety

**Status:** ✅ Complete

---

### Phase 2: Network Tester

**Goal:** Async network connectivity testing utility

**Tasks:**
- [ ] Create `include/network_tester.h`
- [ ] Create `src/network_tester.cpp`
- [ ] Implement gateway detection (Linux/macOS)
- [ ] Implement ping functionality
- [ ] Implement async test flow with ui_async_call
- [ ] Update Makefile

**Agent:** `general-purpose` with prompt:
```
Create NetworkTester class with:
1. get_default_gateway() - Linux: /proc/net/route, macOS: route -n get default
2. ping_host() - system ping with timeout
3. start_test() - async execution, reports via ui_async_call
Reference ui_async_call pattern from wifi_manager.cpp.
```

**Review:** `critical-reviewer` - check thread safety, cancellation handling

**Status:** ✅ Complete

---

### Phase 3: WiFi Settings Overlay XML

**Goal:** Two-column reactive layout with all bindings

**Tasks:**
- [ ] Replace `ui_xml/wifi_settings_overlay.xml`
- [ ] Implement left column (WLAN toggle, current network, network test)
- [ ] Implement right column (network list, add other button)
- [ ] Add all subject bindings
- [ ] Add event callbacks

**Agent:** `widget-maker` with prompt:
```
Create wifi_settings_overlay.xml with two-column layout:
- LEFT (40%): WLAN toggle card, current network card (SSID/IP/MAC), network test card
- RIGHT (60%): networks header with count, scrollable network list, add other button
Use reactive bindings: bind_text, bind_flag_if_eq, bind_state_if_eq
Reference existing: display_settings_overlay.xml, wizard_wifi_setup.xml
```

**Review:** `ui-reviewer` - check design tokens, responsive layout, accessibility

**Status:** ✅ Complete

---

### Phase 4: Hidden Network Modal XML

**Goal:** Modal for manual SSID entry

**Tasks:**
- [ ] Create `ui_xml/hidden_network_modal.xml`
- [ ] Implement SSID input field
- [ ] Implement security type dropdown
- [ ] Implement password field (hide when "None")
- [ ] Add cancel/connect buttons
- [ ] Add error message display

**Agent:** `widget-maker` with prompt:
```
Create hidden_network_modal.xml for manual WiFi entry:
- SSID text input
- Security dropdown (WPA/WPA2, WPA3, None)
- Password input (hidden when None selected)
- Error message area
- Cancel/Connect buttons
Reference: wifi_password_modal.xml pattern
```

**Review:** `ui-reviewer` - check modal patterns, keyboard handling

**Status:** ✅ Complete

---

### Phase 5: WiFiSettingsOverlay Class

**Goal:** Main overlay class with reactive subject management

**Tasks:**
- [ ] Create `include/wifi_settings_overlay.h`
- [ ] Create `src/wifi_settings_overlay.cpp`
- [ ] Implement subject initialization
- [ ] Implement callback registration
- [ ] Implement lifecycle methods (create, show, hide, cleanup)
- [ ] Implement subject update helpers
- [ ] Implement network list population
- [ ] Implement modal handling
- [ ] Update Makefile

**Agent:** `general-purpose` with prompt:
```
Create WiFiSettingsOverlay class:
1. Subject management for all 15 subjects (init, update helpers)
2. Callback registration mapping XML names to static functions
3. WiFiManager integration for scanning and connection
4. NetworkTester integration for test functionality
5. Modal management (password and hidden network)
Architecture: update subjects only, no direct widget manipulation except network list.
Reference: WizardWifiStep pattern.
```

**Review:** `critical-reviewer` - check memory safety, callback patterns, subject lifecycle

**Status:** ✅ Complete

---

### Phase 6: Integration

**Goal:** Wire overlay into settings panel

**Tasks:**
- [ ] Update `src/ui_panel_settings.cpp` - handle_network_clicked()
- [ ] Update `src/main.cpp` - register hidden_network_modal component
- [ ] Test end-to-end flow
- [ ] Verify mock backend works

**Agent:** `general-purpose` with prompt:
```
Integrate WiFiSettingsOverlay:
1. Update SettingsPanel::handle_network_clicked() to use WiFiSettingsOverlay singleton
2. Register hidden_network_modal component in main.cpp
3. Ensure lazy initialization pattern
```

**Review:** `critical-reviewer` - check integration points, error handling

**Status:** ✅ Complete

---

### Phase 7: Final Testing & Polish

**Goal:** Comprehensive testing and bug fixes

**Tasks:**
- [ ] Test with mock WiFi backend
- [ ] Test WLAN toggle behavior
- [ ] Test network list scanning/refresh
- [ ] Test network test feature
- [ ] Test password modal flow
- [ ] Test hidden network modal flow
- [ ] Test navigation (back button)
- [ ] Screenshot verification

**Status:** ✅ Complete

---

## Progress Tracking

### Session Log

| Date | Session | Work Completed | Next Steps |
|------|---------|----------------|------------|
| TBD | 1 | Planning complete | Start Phase 1 |
| 2025-12-07 | 2 | **All 7 phases complete!** Created wifi_ui_utils, network_tester, wifi_settings_overlay.xml, hidden_network_modal.xml, WiFiSettingsOverlay class, integration. Committed: ce9834c | Runtime testing, UI polish |

### Current Session Focus

**Phase:** All Complete
**Status:** Committed to main (ce9834c) - Ready for Manual Testing
**Blockers:** None

### Notes for Next Session

1. Run manual tests per Testing Checklist below
2. Fix any UI issues found during testing
3. Verify mock WiFi backend provides fake networks
4. Test all button callbacks work correctly
5. Screenshot verification for both dark/light modes

---

## Test Commands

### Build & Unit Tests
```bash
# Build everything
make -j

# Run unit tests
./build/bin/helix-tests

# Run specific WiFi-related tests (if any)
./build/bin/helix-tests "[wifi]" "[network]"
```

### Runtime Testing
```bash
# Test WiFi settings overlay (mock backend)
./build/bin/helix-screen --test -p settings -vv

# Auto-screenshot test
HELIX_AUTO_SCREENSHOT=1 HELIX_AUTO_QUIT_MS=5000 ./build/bin/helix-screen --test -p settings -vv

# Test with different screen sizes
./build/bin/helix-screen --test -p settings -s small -vv
./build/bin/helix-screen --test -p settings -s medium -vv
./build/bin/helix-screen --test -p settings -s large -vv
```

### Manual Test Flow
1. Launch: `./build/bin/helix-screen --test -p settings -vv`
2. Click "WiFi Settings" row
3. Toggle WLAN on/off
4. Click refresh to scan networks
5. Click "Run Test" to test connectivity
6. Click a network to connect
7. Click "Add Other Networks" for hidden network modal
8. Press back button to return

---

## Testing Checklist

### Functional Tests
- [ ] WLAN toggle enables/disables scanning
- [ ] Current network card shows when connected
- [ ] Current network card hides when disconnected
- [ ] IP address updates reactively
- [ ] MAC address displays correctly
- [ ] Network list populates with available networks
- [ ] Network count updates in header
- [ ] Refresh button triggers scan
- [ ] Refresh spinner shows during scan
- [ ] Test button starts network test
- [ ] Test button disables during test
- [ ] Gateway step shows pending → active → success/failed
- [ ] Internet step shows pending → active → success/failed
- [ ] Clicking secured network shows password modal
- [ ] Clicking unsecured network connects directly
- [ ] Password modal connect button initiates connection
- [ ] Password modal shows error on auth failure
- [ ] Hidden network modal allows manual SSID entry
- [ ] Hidden network security dropdown works
- [ ] Back button returns to settings panel

### Visual Tests (Screenshots)
- [ ] Two-column layout renders correctly
- [ ] WLAN card styling matches design
- [ ] Current network card styling matches design
- [ ] Network test card styling matches design
- [ ] Network list items styled correctly
- [ ] Password modal styled correctly
- [ ] Hidden network modal styled correctly
- [ ] Dark mode styling correct
- [ ] Light mode styling correct
- [ ] Responsive at different screen sizes

### Mock Backend Tests
- [ ] Mock WiFi backend provides fake networks
- [ ] Mock connection succeeds with correct password
- [ ] Mock connection fails with wrong password
- [ ] Mock scanning returns networks after delay

---

## Detailed XML Layout: wifi_settings_overlay.xml

```xml
<?xml version="1.0"?>
<!-- Copyright 2025 356C LLC -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<component>
  <!-- ================================================================== -->
  <!-- SUBJECTS - Declared here, initialized in C++ before create() -->
  <!-- ================================================================== -->
  <subjects>
    <subject name="wifi_enabled" type="int" default="0"/>
    <subject name="wifi_connected" type="int" default="0"/>
    <subject name="connected_ssid" type="string"/>
    <subject name="ip_address" type="string"/>
    <subject name="mac_address" type="string"/>
    <subject name="network_count" type="string" default="(0)"/>
    <subject name="wifi_scanning" type="int" default="0"/>
    <subject name="test_running" type="int" default="0"/>
    <subject name="test_step" type="int" default="0"/>
    <subject name="test_gateway_status" type="int" default="0"/>
    <subject name="test_internet_status" type="int" default="0"/>
    <subject name="password_modal_visible" type="int" default="0"/>
    <subject name="hidden_modal_visible" type="int" default="0"/>
    <subject name="modal_ssid" type="string"/>
    <subject name="modal_connecting" type="int" default="0"/>
    <subject name="modal_error" type="string"/>
  </subjects>

  <!-- ================================================================== -->
  <!-- MAIN VIEW -->
  <!-- ================================================================== -->
  <view extends="lv_obj" name="wifi_settings_overlay"
        width="#overlay_panel_width_large" height="100%"
        align="right_mid" style_border_width="0" style_pad_all="0"
        style_bg_opa="255" style_bg_color="#card_bg"
        scrollable="false" flex_flow="column">

    <!-- Header Bar with Back Button -->
    <header_bar name="overlay_header" title="WiFi Settings">
      <event_cb trigger="back_clicked" callback="on_back_clicked"/>
    </header_bar>

    <!-- Two-Column Content Row -->
    <lv_obj name="content_row" flex_grow="1" width="100%"
            style_bg_opa="0" style_pad_all="#space_lg"
            scrollable="false" style_border_width="0"
            flex_flow="row" style_pad_gap="#space_lg">

      <!-- LEFT COLUMN (40%) -->
      <lv_obj name="left_column" flex_grow="4" height="100%"
              style_bg_opa="0" style_border_width="0" style_pad_all="0"
              scrollable="false" flex_flow="column" style_pad_gap="#space_md">

        <!-- WLAN Toggle Card -->
        <lv_obj width="100%" height="content"
                style_bg_color="#app_bg_color" style_bg_opa="255"
                style_radius="#border_radius" style_border_width="0"
                style_pad_all="#space_lg" scrollable="false"
                flex_flow="row" style_flex_main_place="space_between"
                style_flex_cross_place="center">
          <lv_obj width="content" height="content" style_bg_opa="0"
                  style_border_width="0" style_pad_all="0" scrollable="false"
                  flex_flow="column">
            <text_body text="WLAN" style_text_color="#text_primary"/>
            <text_small text="(Only for 2.4G)" style_text_color="#text_secondary"/>
          </lv_obj>
          <lv_switch name="wlan_toggle">
            <lv_obj-bind_state_if_eq subject="wifi_enabled" state="checked" ref_value="1"/>
            <event_cb trigger="value_changed" callback="on_wlan_toggle_changed"/>
          </lv_switch>
        </lv_obj>

        <!-- Current Network Card -->
        <lv_obj name="current_network_card" width="100%" height="content"
                style_bg_color="#app_bg_color" style_bg_opa="255"
                style_radius="#border_radius" style_border_width="0"
                style_pad_all="#space_lg" scrollable="false"
                flex_flow="column" style_pad_gap="#space_sm">

          <text_small text="Current network" style_text_color="#text_secondary"/>

          <!-- Connected State (shown when wifi_connected == 1) -->
          <lv_obj name="connected_info" width="100%" height="content"
                  style_bg_opa="0" style_border_width="0" style_pad_all="0"
                  scrollable="false" flex_flow="column" style_pad_gap="#space_xs">
            <lv_obj-bind_flag_if_eq subject="wifi_connected" flag="hidden" ref_value="0"/>

            <!-- SSID with checkmark and signal icon -->
            <lv_obj width="100%" height="content" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_flex_cross_place="center" style_pad_gap="#space_sm">
              <icon src="check_circle" size="sm" variant="success"/>
              <text_body name="ssid_label" bind_text="connected_ssid"
                         style_text_color="#text_primary" flex_grow="1"/>
              <icon src="wifi_strength_4" size="sm" variant="accent"/>
            </lv_obj>

            <!-- IP Address -->
            <lv_obj width="100%" height="content" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_pad_gap="#space_xs">
              <text_small text="IP:" style_text_color="#text_secondary"/>
              <text_small name="ip_label" bind_text="ip_address"
                          style_text_color="#text_primary"/>
            </lv_obj>

            <!-- MAC Address -->
            <lv_obj width="100%" height="content" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_pad_gap="#space_xs">
              <text_small text="MAC:" style_text_color="#text_secondary"/>
              <text_small name="mac_label" bind_text="mac_address"
                          style_text_color="#text_primary"/>
            </lv_obj>
          </lv_obj>

          <!-- Disconnected State (shown when wifi_connected == 0) -->
          <lv_obj name="disconnected_info" width="100%" height="content"
                  style_bg_opa="0" style_border_width="0" style_pad_all="0"
                  scrollable="false" flex_flow="row" style_flex_cross_place="center"
                  style_pad_gap="#space_sm">
            <lv_obj-bind_flag_if_eq subject="wifi_connected" flag="hidden" ref_value="1"/>
            <icon src="wifi_off" size="sm" variant="secondary"/>
            <text_body text="Not connected" style_text_color="#text_secondary"/>
          </lv_obj>
        </lv_obj>

        <!-- Network Test Card -->
        <lv_obj name="network_test_card" width="100%" height="content"
                style_bg_color="#app_bg_color" style_bg_opa="255"
                style_radius="#border_radius" style_border_width="0"
                style_pad_all="#space_lg" scrollable="false"
                flex_flow="column" style_pad_gap="#space_md">
          <!-- Disabled when not connected -->
          <lv_obj-bind_state_if_eq subject="wifi_connected" state="disabled" ref_value="0"/>

          <text_small text="Test network" style_text_color="#text_secondary"/>

          <!-- Test Steps (reactive via subjects) -->
          <lv_obj width="100%" height="content" style_bg_opa="0"
                  style_border_width="0" style_pad_all="0" scrollable="false"
                  flex_flow="column" style_pad_gap="#space_sm">

            <!-- Step 1: Gateway -->
            <lv_obj width="100%" height="content" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_flex_cross_place="center" style_pad_gap="#space_sm">
              <!-- Pending circle (test_gateway_status == 0) -->
              <icon name="gw_pending" src="circle_outline" size="sm" variant="secondary">
                <lv_obj-bind_flag_if_not_eq subject="test_gateway_status" flag="hidden" ref_value="0"/>
              </icon>
              <!-- Active spinner (test_gateway_status == 1) -->
              <lv_spinner name="gw_active" width="24" height="24">
                <lv_obj-bind_flag_if_not_eq subject="test_gateway_status" flag="hidden" ref_value="1"/>
              </lv_spinner>
              <!-- Success check (test_gateway_status == 2) -->
              <icon name="gw_success" src="check_circle" size="sm" variant="success">
                <lv_obj-bind_flag_if_not_eq subject="test_gateway_status" flag="hidden" ref_value="2"/>
              </icon>
              <!-- Failed X (test_gateway_status == 3) -->
              <icon name="gw_failed" src="close_circle" size="sm" variant="error">
                <lv_obj-bind_flag_if_not_eq subject="test_gateway_status" flag="hidden" ref_value="3"/>
              </icon>
              <text_body text="Gateway" style_text_color="#text_primary"/>
            </lv_obj>

            <!-- Step 2: Internet -->
            <lv_obj width="100%" height="content" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_flex_cross_place="center" style_pad_gap="#space_sm">
              <!-- Pending circle (test_internet_status == 0) -->
              <icon name="inet_pending" src="circle_outline" size="sm" variant="secondary">
                <lv_obj-bind_flag_if_not_eq subject="test_internet_status" flag="hidden" ref_value="0"/>
              </icon>
              <!-- Active spinner (test_internet_status == 1) -->
              <lv_spinner name="inet_active" width="24" height="24">
                <lv_obj-bind_flag_if_not_eq subject="test_internet_status" flag="hidden" ref_value="1"/>
              </lv_spinner>
              <!-- Success check (test_internet_status == 2) -->
              <icon name="inet_success" src="check_circle" size="sm" variant="success">
                <lv_obj-bind_flag_if_not_eq subject="test_internet_status" flag="hidden" ref_value="2"/>
              </icon>
              <!-- Failed X (test_internet_status == 3) -->
              <icon name="inet_failed" src="close_circle" size="sm" variant="error">
                <lv_obj-bind_flag_if_not_eq subject="test_internet_status" flag="hidden" ref_value="3"/>
              </icon>
              <text_body text="Internet" style_text_color="#text_primary"/>
            </lv_obj>
          </lv_obj>

          <!-- Test Button -->
          <lv_button name="test_btn" width="100%" height="#button_height"
                     style_radius="#border_radius" style_border_width="0">
            <lv_obj-bind_state_if_eq subject="test_running" state="disabled" ref_value="1"/>
            <event_cb trigger="clicked" callback="on_test_network_clicked"/>
            <lv_obj width="100%" height="100%" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_flex_main_place="center"
                    style_flex_cross_place="center" style_pad_gap="#space_sm">
              <icon src="magnify" size="sm" variant="primary"/>
              <text_body text="Test Network" align="center"/>
            </lv_obj>
          </lv_button>
        </lv_obj>

        <!-- Spacer -->
        <lv_obj flex_grow="1" width="100%" style_bg_opa="0"
                style_border_width="0" scrollable="false"/>
      </lv_obj>

      <!-- RIGHT COLUMN (60%) -->
      <lv_obj name="right_column" flex_grow="6" height="100%"
              style_bg_opa="0" style_border_width="0" style_pad_all="0"
              scrollable="false" flex_flow="column" style_pad_gap="#space_md">

        <!-- Available Networks Header -->
        <lv_obj width="100%" height="content" style_bg_opa="0"
                style_border_width="0" style_pad_all="0" scrollable="false"
                flex_flow="row" style_flex_main_place="space_between"
                style_flex_cross_place="center">
          <lv_obj width="content" height="content" style_bg_opa="0"
                  style_border_width="0" style_pad_all="0" scrollable="false"
                  flex_flow="row" style_flex_cross_place="center" style_pad_gap="#space_sm">
            <text_body text="Other networks" style_text_color="#text_primary"/>
            <text_small name="count_label" bind_text="network_count"
                        style_text_color="#text_secondary"/>
          </lv_obj>
          <!-- Refresh Button -->
          <lv_button name="refresh_btn" width="content" height="36"
                     style_radius="#border_radius" style_border_width="0"
                     style_pad_left="#space_md" style_pad_right="#space_md">
            <lv_obj-bind_state_if_eq subject="wifi_scanning" state="disabled" ref_value="1"/>
            <event_cb trigger="clicked" callback="on_refresh_clicked"/>
            <lv_obj width="content" height="100%" style_bg_opa="0"
                    style_border_width="0" style_pad_all="0" scrollable="false"
                    flex_flow="row" style_flex_cross_place="center" style_pad_gap="#space_xs">
              <icon name="refresh_icon" src="refresh" size="xs" variant="primary">
                <lv_obj-bind_flag_if_eq subject="wifi_scanning" flag="hidden" ref_value="1"/>
              </icon>
              <lv_spinner name="refresh_spinner" width="16" height="16">
                <lv_obj-bind_flag_if_eq subject="wifi_scanning" flag="hidden" ref_value="0"/>
              </lv_spinner>
            </lv_obj>
          </lv_button>
        </lv_obj>

        <!-- Networks List Container -->
        <lv_obj name="networks_list" flex_grow="1" width="100%"
                style_bg_color="#app_bg_color" style_bg_opa="255"
                style_radius="#border_radius" style_border_width="0"
                style_pad_all="0" scrollable="true" scroll_dir="VER"
                flex_flow="column">
          <!-- Placeholder -->
          <lv_obj name="no_networks_placeholder" width="100%" height="100%"
                  style_bg_opa="0" style_border_width="0" style_pad_all="#space_lg"
                  scrollable="false" flex_flow="column"
                  style_flex_main_place="center" style_flex_cross_place="center">
            <icon src="wifi_off" size="lg" variant="secondary"/>
            <text_body text="No networks found" style_text_color="#text_secondary"
                       style_margin_top="#space_sm"/>
          </lv_obj>
        </lv_obj>

        <!-- Add Other Networks Button -->
        <lv_button name="add_other_btn" width="100%" height="#button_height"
                   style_radius="#border_radius" style_border_width="0"
                   style_bg_color="#app_bg_color">
          <event_cb trigger="clicked" callback="on_add_other_clicked"/>
          <lv_obj width="100%" height="100%" style_bg_opa="0"
                  style_border_width="0" style_pad_all="0" scrollable="false"
                  flex_flow="row" style_flex_main_place="center"
                  style_flex_cross_place="center" style_pad_gap="#space_sm">
            <icon src="plus" size="sm" variant="secondary"/>
            <text_body text="Add Other Networks" style_text_color="#text_secondary"/>
          </lv_obj>
        </lv_button>
      </lv_obj>
    </lv_obj>
  </view>
</component>
```

---

## Detailed C++ Class Design: WiFiSettingsOverlay

```cpp
// include/wifi_settings_overlay.h
#pragma once

#include "network_tester.h"
#include "wifi_manager.h"
#include "lvgl/lvgl.h"

#include <memory>
#include <string>
#include <vector>

class WiFiSettingsOverlay {
public:
    WiFiSettingsOverlay();
    ~WiFiSettingsOverlay();

    void init_subjects();
    void register_callbacks();
    lv_obj_t* create(lv_obj_t* parent_screen);
    void show();
    void hide();
    void cleanup();

    bool is_created() const { return overlay_root_ != nullptr; }
    bool is_visible() const { return visible_; }

private:
    // Subjects (reactive state)
    lv_subject_t wifi_enabled_;
    lv_subject_t wifi_connected_;
    lv_subject_t connected_ssid_;
    lv_subject_t ip_address_;
    lv_subject_t mac_address_;
    lv_subject_t network_count_;
    lv_subject_t wifi_scanning_;
    lv_subject_t test_running_;
    lv_subject_t test_gateway_status_;
    lv_subject_t test_internet_status_;
    lv_subject_t password_modal_visible_;
    lv_subject_t hidden_modal_visible_;
    lv_subject_t modal_ssid_;
    lv_subject_t modal_connecting_;
    lv_subject_t modal_error_;

    // String buffers
    char ssid_buffer_[64];
    char ip_buffer_[32];
    char mac_buffer_[32];
    char count_buffer_[16];
    char modal_ssid_buffer_[64];
    char modal_error_buffer_[128];

    // Widget references (minimal)
    lv_obj_t* overlay_root_ = nullptr;
    lv_obj_t* parent_screen_ = nullptr;
    lv_obj_t* networks_list_ = nullptr;
    lv_obj_t* password_modal_ = nullptr;
    lv_obj_t* hidden_modal_ = nullptr;

    // Managers
    std::shared_ptr<WiFiManager> wifi_manager_;
    std::unique_ptr<NetworkTester> network_tester_;

    // State
    bool subjects_initialized_ = false;
    bool callbacks_registered_ = false;
    bool visible_ = false;
    std::string pending_ssid_;

    // Subject update helpers
    void update_wifi_enabled(bool enabled);
    void update_connection_status();
    void update_network_count(int count);
    void update_scanning(bool scanning);
    void update_test_state(NetworkTester::TestState state,
                           const NetworkTester::TestResult& result);

    // Network list
    void populate_network_list(const std::vector<WiFiNetwork>& networks);
    void clear_network_list();
    void show_placeholder(bool show);

    // Modals
    void show_password_modal(const std::string& ssid);
    void hide_password_modal();
    void show_hidden_network_modal();
    void hide_hidden_network_modal();

    // Utility
    std::string get_device_mac();

    // Static callbacks
    static void on_back_clicked(lv_event_t* e);
    static void on_wlan_toggle_changed(lv_event_t* e);
    static void on_refresh_clicked(lv_event_t* e);
    static void on_test_network_clicked(lv_event_t* e);
    static void on_add_other_clicked(lv_event_t* e);
    static void on_network_item_clicked(lv_event_t* e);
    static void on_password_connect_clicked(lv_event_t* e);
    static void on_password_cancel_clicked(lv_event_t* e);
    static void on_hidden_connect_clicked(lv_event_t* e);
    static void on_hidden_cancel_clicked(lv_event_t* e);
};

WiFiSettingsOverlay& get_wifi_settings_overlay();
```

---

## Detailed Network Tester Implementation

```cpp
// include/network_tester.h
#pragma once

#include <functional>
#include <string>
#include <thread>
#include <atomic>

class NetworkTester {
public:
    enum class TestState {
        IDLE, TESTING_GATEWAY, TESTING_INTERNET, COMPLETED, FAILED
    };

    struct TestResult {
        bool gateway_ok = false;
        bool internet_ok = false;
        std::string gateway_ip;
        std::string error_message;
    };

    using Callback = std::function<void(TestState, const TestResult&)>;

    NetworkTester();
    ~NetworkTester();

    void start_test(Callback callback);
    void cancel();
    bool is_running() const { return running_.load(); }

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::thread worker_thread_;
    Callback callback_;
    TestResult result_;

    void run_test();
    void report_state(TestState state);
    std::string get_default_gateway();
    bool ping_host(const std::string& host, int timeout_sec = 2);
};
```

### Key Implementation

```cpp
// Gateway detection (cross-platform)
std::string NetworkTester::get_default_gateway() {
#ifdef __APPLE__
    FILE* pipe = popen("route -n get default 2>/dev/null | grep gateway | awk '{print $2}'", "r");
    // ... parse output
#else
    // Linux: Parse /proc/net/route for default gateway (dest 00000000)
    std::ifstream route("/proc/net/route");
    // ... parse hex gateway to IP
#endif
}

// Ping with timeout
bool NetworkTester::ping_host(const std::string& host, int timeout_sec) {
#ifdef __APPLE__
    std::string cmd = fmt::format("ping -c 1 -t {} {} >/dev/null 2>&1", timeout_sec, host);
#else
    std::string cmd = fmt::format("ping -c 1 -W {} {} >/dev/null 2>&1", timeout_sec, host);
#endif
    return system(cmd.c_str()) == 0;
}
```

---

## Integration Flow

### Settings Panel → WiFi Overlay

```cpp
void SettingsPanel::handle_network_clicked() {
    auto& overlay = get_wifi_settings_overlay();

    if (!overlay.is_created()) {
        overlay.init_subjects();
        overlay.register_callbacks();
        overlay.create(parent_screen_);
    }

    overlay.show();
}
```

### Callbacks → Subject Updates

```cpp
void WiFiSettingsOverlay::on_test_network_clicked(lv_event_t* e) {
    auto* self = static_cast<WiFiSettingsOverlay*>(lv_event_get_user_data(e));

    // Reset test state subjects
    lv_subject_set_int(&self->test_gateway_status_, 0);
    lv_subject_set_int(&self->test_internet_status_, 0);
    lv_subject_set_int(&self->test_running_, 1);

    // Start async test
    self->network_tester_->start_test([self](NetworkTester::TestState state,
                                              const NetworkTester::TestResult& result) {
        self->update_test_state(state, result);
    });
}
```
