# TODO Implementation Plan

**Status:** Tier 1 Complete (commit `1ba61f3`)
**Remaining:** Tier 2 (Medium) + Tier 3 (Complex)
**Total Remaining Items:** 20 TODO/FIXME items

---

## Overview

This document tracks the remaining TODO/FIXME items in the HelixScreen codebase, organized by implementation complexity. Each item includes file location, current state, implementation approach, and references to existing patterns.

### Progress Summary

| Tier | Status | Items | Est. Time |
|------|--------|-------|-----------|
| Tier 1: Quick Wins | ✅ Complete | 7 items | - |
| **Tier 2: Medium** | 🔲 Pending | **11 items** | **8-12 hours** |
| **Tier 3: Complex** | 🔲 Pending | **9 items** | **16-24 hours** |

---

## Tier 2: Medium Effort (30-90 min each)

These items require moderate implementation effort—typically adding new UI elements, integrating with existing systems, or implementing well-defined features with clear patterns to follow.

---

### 2.1 Print Status Panel - Temperature Overlays

**File:** `src/ui_panel_print_status.cpp`
**Lines:** 418, 423
**Priority:** High (frequently used feature)

#### Current State
```cpp
// Line 418
void PrintStatusPanel::handle_nozzle_temp_clicked() {
    spdlog::info("[PrintStatusPanel] Nozzle temp clicked");
    // TODO: Push nozzle temp overlay for adjustment
}

// Line 423
void PrintStatusPanel::handle_bed_temp_clicked() {
    spdlog::info("[PrintStatusPanel] Bed temp clicked");
    // TODO: Push bed temp overlay for adjustment
}
```

#### Implementation

1. **Push existing temperature overlay panels:**
```cpp
void PrintStatusPanel::handle_nozzle_temp_clicked() {
    spdlog::info("[PrintStatusPanel] Opening nozzle temp adjustment");

    // Use existing TempControlPanel infrastructure
    auto& temp_panel = get_global_temp_control_panel();
    temp_panel.set_mode(TempControlPanel::Mode::NOZZLE);

    // Create overlay if not exists
    if (!nozzle_temp_overlay_) {
        nozzle_temp_overlay_ = lv_xml_create(parent_screen_, "nozzle_temp_panel", NULL);
        temp_panel.setup(nozzle_temp_overlay_, parent_screen_);
    }

    ui_nav_push_overlay(nozzle_temp_overlay_);
}
```

2. **Alternative: Reuse Controls panel temp overlays:**
   - The ControlsPanel already has `handle_nozzle_temp_clicked()` and `handle_bed_temp_clicked()`
   - Pattern at `src/ui_panel_controls.cpp:178-205`
   - May be able to share the same overlay instances

#### Pattern Reference
- `src/ui_panel_controls.cpp:178-205` - Temperature overlay creation in Controls panel
- `src/ui_nav.cpp:ui_nav_push_overlay()` - Navigation stack management

#### Testing
```bash
./build/bin/helix-screen --test -p print-status
# Click on temperature display areas
# Verify overlay opens with current values
```

---

### 2.2 Print Status Panel - Tune Overlay

**File:** `src/ui_panel_print_status.cpp`
**Line:** 509
**Priority:** Medium (nice-to-have during prints)

#### Current State
```cpp
void PrintStatusPanel::handle_tune_clicked() {
    spdlog::info("[PrintStatusPanel] Tune clicked");
    // TODO: Push tune overlay with speed/flow/z-offset controls
}
```

#### Implementation

This requires creating a new panel from scratch:

1. **Create XML component** `ui_xml/tune_panel.xml`:
```xml
<component name="tune_panel">
    <lv_obj name="tune_panel" style_bg_color="const:bg_dark" ...>
        <overlay_header title="Tune Settings"/>
        <lv_obj name="overlay_content">
            <!-- Speed % slider -->
            <lv_obj name="speed_row" style_flex_flow="row">
                <lv_label text="Speed"/>
                <lv_slider name="speed_slider" value="100" min="10" max="300"/>
                <lv_label bind_text="tune_speed_text"/>
            </lv_obj>

            <!-- Flow % slider -->
            <lv_obj name="flow_row" style_flex_flow="row">
                <lv_label text="Flow"/>
                <lv_slider name="flow_slider" value="100" min="50" max="150"/>
                <lv_label bind_text="tune_flow_text"/>
            </lv_obj>

            <!-- Z-Offset adjustment -->
            <lv_obj name="z_offset_row" style_flex_flow="row">
                <lv_label text="Z Offset"/>
                <lv_button name="z_down"><lv_label text="-0.01"/></lv_button>
                <lv_label bind_text="tune_z_offset_text"/>
                <lv_button name="z_up"><lv_label text="+0.01"/></lv_button>
            </lv_obj>
        </lv_obj>
    </lv_obj>
</component>
```

2. **Create C++ handler** `include/ui_panel_tune.h` and `src/ui_panel_tune.cpp`:
   - Inherit from `PanelBase`
   - Subscribe to PrinterState for current values
   - Wire slider change events to Moonraker API calls:
     - Speed: `M220 S{percent}`
     - Flow: `M221 S{percent}`
     - Z-Offset: `SET_GCODE_OFFSET Z_ADJUST={delta} MOVE=1`

3. **Wire into PrintStatusPanel:**
```cpp
void PrintStatusPanel::handle_tune_clicked() {
    if (!tune_panel_) {
        tune_panel_ = lv_xml_create(parent_screen_, "tune_panel", NULL);
        get_global_tune_panel().setup(tune_panel_, parent_screen_);
    }
    ui_nav_push_overlay(tune_panel_);
}
```

#### Pattern Reference
- `src/ui_panel_fan.cpp` - Slider-based control panel
- `src/ui_panel_motion.cpp` - Button-based increment/decrement pattern

#### G-code Commands
```gcode
M220 S100    ; Set speed to 100%
M221 S100    ; Set flow to 100%
SET_GCODE_OFFSET Z_ADJUST=0.01 MOVE=1  ; Adjust Z offset by +0.01mm
```

#### Testing
```bash
./build/bin/helix-screen --test -p print-status
# Click Tune button
# Verify sliders update values
# Verify changes sent to mock printer
```

---

### 2.3 Print Status Panel - Cancel Confirmation

**File:** `src/ui_panel_print_status.cpp`
**Line:** 515
**Priority:** High (destructive action needs confirmation)

#### Current State
```cpp
void PrintStatusPanel::handle_cancel_clicked() {
    spdlog::info("[PrintStatusPanel] Cancel clicked");
    // TODO: Show confirmation dialog before canceling
    if (api_) {
        api_->execute_gcode("CANCEL_PRINT", ...);
    }
}
```

#### Implementation

1. **Add confirmation dialog using existing pattern:**
```cpp
void PrintStatusPanel::handle_cancel_clicked() {
    spdlog::info("[PrintStatusPanel] Cancel clicked - showing confirmation");

    // Create confirmation dialog (same pattern as motor disable)
    if (!cancel_confirmation_dialog_) {
        cancel_confirmation_dialog_ = ui_dialog_create_confirmation(
            parent_screen_,
            "Cancel Print?",
            "This will stop the current print. The print cannot be resumed.",
            "Cancel Print",  // Confirm button text
            "Keep Printing", // Cancel button text
            on_cancel_confirm,
            on_cancel_dismiss,
            this
        );
    }

    lv_obj_remove_flag(cancel_confirmation_dialog_, LV_OBJ_FLAG_HIDDEN);
}

void PrintStatusPanel::handle_cancel_confirm() {
    spdlog::info("[PrintStatusPanel] Cancel confirmed");
    lv_obj_add_flag(cancel_confirmation_dialog_, LV_OBJ_FLAG_HIDDEN);

    if (api_) {
        api_->execute_gcode(
            "CANCEL_PRINT",
            []() { NOTIFY_SUCCESS("Print cancelled"); },
            [](const MoonrakerError& err) {
                NOTIFY_ERROR("Failed to cancel: {}", err.user_message());
            });
    }
}
```

2. **Add member variables:**
```cpp
// In header
lv_obj_t* cancel_confirmation_dialog_ = nullptr;

static void on_cancel_confirm(lv_event_t* e);
static void on_cancel_dismiss(lv_event_t* e);
void handle_cancel_confirm();
void handle_cancel_dismiss();
```

#### Pattern Reference
- `src/ui_panel_controls.cpp:247-280` - Motor disable confirmation dialog
- `src/ui_dialog.cpp` - Dialog creation utilities (if exists)

#### Testing
```bash
./build/bin/helix-screen --test -p print-status
# Click Cancel button
# Verify dialog appears
# Click "Keep Printing" - dialog closes, no action
# Click "Cancel Print" - dialog closes, CANCEL_PRINT sent
```

---

### 2.4 Printer State - Network Status Query

**File:** `src/printer_state.cpp`
**Line:** 192
**Priority:** Low (status display only)

#### Current State
```cpp
NetworkStatus PrinterState::get_network_status() const {
    // TODO: Get actual network status from WiFiManager/EthernetManager
    return NetworkStatus::CONNECTED; // Mock for now
}
```

#### Implementation

```cpp
NetworkStatus PrinterState::get_network_status() const {
    // Check WiFi first
    auto& wifi = WiFiManager::instance();
    if (wifi.is_enabled()) {
        switch (wifi.get_state()) {
            case WiFiState::CONNECTED:
                return NetworkStatus::WIFI_CONNECTED;
            case WiFiState::CONNECTING:
                return NetworkStatus::CONNECTING;
            case WiFiState::DISCONNECTED:
                return NetworkStatus::DISCONNECTED;
            default:
                break;
        }
    }

    // Check Ethernet
    auto& ethernet = EthernetManager::instance();
    if (ethernet.get_state() == EthernetState::CONNECTED) {
        return NetworkStatus::ETHERNET_CONNECTED;
    }

    return NetworkStatus::DISCONNECTED;
}
```

#### Dependencies
- `WiFiManager` instance availability
- `EthernetManager` instance availability
- May need to add `NetworkStatus::WIFI_CONNECTED` and `ETHERNET_CONNECTED` enum values

#### Pattern Reference
- `src/wifi_manager.cpp` - WiFi state management
- `src/ethernet_manager.cpp` - Ethernet state management

---

### 2.5 Macro Manager - Version Query

**File:** `src/helix_macro_manager.cpp`
**Line:** 495
**Priority:** Low (diagnostic feature)

#### Current State
```cpp
void HelixMacroManager::query_macro_version() {
    // TODO: Query macro version from printer
    spdlog::debug("[MacroManager] Version query not implemented");
}
```

#### Implementation

```cpp
void HelixMacroManager::query_macro_version() {
    if (!api_) {
        spdlog::warn("[MacroManager] No API available for version query");
        return;
    }

    // Query the HELIX_VERSION gcode_macro variable
    nlohmann::json params = {
        {"objects", {
            {"gcode_macro HELIX_VERSION", {"version"}}
        }}
    };

    api_->send_jsonrpc(
        "printer.objects.query",
        params,
        [this](const nlohmann::json& result) {
            try {
                auto version = result["status"]["gcode_macro HELIX_VERSION"]["version"];
                macro_version_ = version.get<std::string>();
                spdlog::info("[MacroManager] Detected macro version: {}", macro_version_);
            } catch (const std::exception& e) {
                spdlog::warn("[MacroManager] Failed to parse version: {}", e.what());
            }
        },
        [](const MoonrakerError& err) {
            spdlog::debug("[MacroManager] Version query failed (macros may not be installed): {}",
                         err.message);
        });
}
```

#### Moonraker API
```json
// Request
{"method": "printer.objects.query", "params": {"objects": {"gcode_macro HELIX_VERSION": ["version"]}}}

// Response (if macro exists)
{"status": {"gcode_macro HELIX_VERSION": {"version": "1.2.3"}}}
```

---

### 2.6 G-code Viewer - Empty State

**File:** `src/ui_gcode_viewer.cpp`
**Line:** 136
**Priority:** Medium (polish)

#### Current State
```cpp
void GCodeViewer::draw_empty_state(lv_layer_t* layer) {
    // TODO: Draw "No G-code loaded" message
}
```

#### Implementation

```cpp
void GCodeViewer::draw_empty_state(lv_layer_t* layer) {
    if (!empty_state_label_) {
        // Create label once, reuse
        empty_state_label_ = lv_label_create(viewer_obj_);
        lv_obj_set_style_text_color(empty_state_label_,
            ui_theme_parse_color(lv_xml_get_const("text_muted")), 0);
        lv_obj_set_style_text_font(empty_state_label_, &lv_font_montserrat_16, 0);
        lv_obj_center(empty_state_label_);
    }

    lv_label_set_text(empty_state_label_, "No G-code loaded");
    lv_obj_remove_flag(empty_state_label_, LV_OBJ_FLAG_HIDDEN);
}

void GCodeViewer::hide_empty_state() {
    if (empty_state_label_) {
        lv_obj_add_flag(empty_state_label_, LV_OBJ_FLAG_HIDDEN);
    }
}
```

Call `hide_empty_state()` when G-code data is loaded.

#### Pattern Reference
- Similar empty states in file browser panels

---

### 2.7 G-code Geometry - Configurable Travel Paths

**File:** `src/gcode_geometry_builder.cpp`
**Line:** 424
**Priority:** Low (advanced feature)

#### Current State
```cpp
void GCodeGeometryBuilder::process_travel_move(const GCodeLine& line) {
    // TODO: Make travel path visibility configurable
    // Currently always generates travel path geometry
}
```

#### Implementation

1. **Add config flag:**
```cpp
// In include/gcode_geometry_builder.h
struct GCodeViewerConfig {
    bool show_travel_moves = false;  // Default off (cleaner view)
    bool show_retractions = true;
    float line_width = 0.4f;
    // ... existing config
};
```

2. **Conditional geometry generation:**
```cpp
void GCodeGeometryBuilder::process_travel_move(const GCodeLine& line) {
    if (!config_.show_travel_moves) {
        // Just update position without generating geometry
        current_position_ = line.target_position;
        return;
    }

    // Existing travel path geometry code...
}
```

3. **Add UI toggle** in settings or viewer panel

---

### 2.8 Print Select - Multi-Drive USB

**File:** `src/ui_panel_print_select.cpp`
**Line:** 1556
**Priority:** Low (edge case)

#### Current State
```cpp
void PrintSelectPanel::handle_usb_button() {
    // TODO: Handle multiple USB drives
    // Currently assumes single drive
    browse_path("/media/usb0");
}
```

#### Implementation

```cpp
void PrintSelectPanel::handle_usb_button() {
    auto drives = enumerate_usb_drives();

    if (drives.empty()) {
        NOTIFY_WARNING("No USB drives detected");
        return;
    }

    if (drives.size() == 1) {
        browse_path(drives[0].mount_path);
        return;
    }

    // Multiple drives - show picker
    show_drive_picker(drives);
}

std::vector<USBDrive> PrintSelectPanel::enumerate_usb_drives() {
    std::vector<USBDrive> drives;

    // Check common mount points
    for (int i = 0; i < 4; i++) {
        std::string path = fmt::format("/media/usb{}", i);
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            drives.push_back({path, fmt::format("USB Drive {}", i + 1)});
        }
    }

    return drives;
}
```

---

### 2.9 Settings Panel - Network Settings Navigation

**File:** `src/ui_panel_settings.cpp`
**Line:** 378
**Priority:** Medium

#### Current State
```cpp
void SettingsPanel::handle_network_clicked() {
    spdlog::info("[SettingsPanel] Network clicked");
    // TODO: Navigate to network settings panel
}
```

#### Implementation

```cpp
void SettingsPanel::handle_network_clicked() {
    spdlog::info("[SettingsPanel] Opening network settings");

    // Create network settings overlay if not exists
    if (!network_panel_) {
        network_panel_ = lv_xml_create(parent_screen_, "network_panel", NULL);
        get_global_network_panel().setup(network_panel_, parent_screen_);
    }

    ui_nav_push_overlay(network_panel_);
}
```

**Note:** Requires `network_panel.xml` and `NetworkPanel` class to exist. If not, this becomes a Tier 3 item.

---

### 2.10 Settings Panel - Factory Reset

**File:** `src/ui_panel_settings.cpp`
**Line:** 383
**Priority:** Low (rarely used)

#### Current State
```cpp
void SettingsPanel::handle_factory_reset_clicked() {
    spdlog::info("[SettingsPanel] Factory reset clicked");
    // TODO: Show confirmation and perform reset
}
```

#### Implementation

```cpp
void SettingsPanel::handle_factory_reset_clicked() {
    spdlog::info("[SettingsPanel] Factory reset clicked - showing confirmation");

    if (!reset_confirmation_dialog_) {
        reset_confirmation_dialog_ = ui_dialog_create_confirmation(
            parent_screen_,
            "Factory Reset",
            "This will reset all settings to defaults.\n"
            "WiFi networks, printer configs, and preferences will be cleared.",
            "Reset",
            "Cancel",
            on_reset_confirm,
            on_reset_dismiss,
            this
        );
    }

    lv_obj_remove_flag(reset_confirmation_dialog_, LV_OBJ_FLAG_HIDDEN);
}

void SettingsPanel::handle_reset_confirm() {
    spdlog::warn("[SettingsPanel] Performing factory reset");
    lv_obj_add_flag(reset_confirmation_dialog_, LV_OBJ_FLAG_HIDDEN);

    // Clear config files
    RuntimeConfig::instance().reset_to_defaults();

    // Clear stored WiFi networks
    WiFiManager::instance().forget_all_networks();

    NOTIFY_SUCCESS("Settings reset to defaults");

    // Optional: restart application
    // std::exit(0);
}
```

---

### 2.11 Test Fix - Mutex Race Condition

**File:** `tests/unit/test_moonraker_client_robustness.cpp`
**Line:** 91
**Priority:** Medium (test reliability)

#### Current State
```cpp
// FIXME: Disabled - mutex already locked assertion
// Test sometimes fails due to callback executing during fixture teardown
TEST_CASE_METHOD(RobustnessFixture, "...", "[.disabled]") {
```

#### Root Cause
- Test fixture destructor runs while async callbacks are still pending
- Callbacks try to acquire mutex that's being destroyed
- Race between callback execution and fixture cleanup

#### Implementation

```cpp
class RobustnessFixture {
protected:
    std::atomic<int> pending_callbacks_{0};
    std::mutex callback_mutex_;
    std::condition_variable callback_cv_;

    void wait_for_callbacks() {
        std::unique_lock<std::mutex> lock(callback_mutex_);
        callback_cv_.wait_for(lock, std::chrono::seconds(5), [this] {
            return pending_callbacks_.load() == 0;
        });
    }

    ~RobustnessFixture() {
        // Wait for all pending callbacks before destruction
        wait_for_callbacks();
    }

    // Callback wrapper
    auto make_callback(std::function<void()> fn) {
        pending_callbacks_++;
        return [this, fn = std::move(fn)]() {
            fn();
            pending_callbacks_--;
            callback_cv_.notify_one();
        };
    }
};
```

Then update tests to use `make_callback()` wrapper.

---

## Tier 3: Complex (2-8 hours each)

These items require significant implementation effort, architectural changes, or deep understanding of external systems (LVGL 9, async patterns, etc.).

---

### 3.1 Temperature Graph - LVGL 9 Gradient Fill

**File:** `src/ui_temp_graph.cpp`
**Lines:** 110, 262
**Priority:** Medium (visual polish)
**Estimated Time:** 8-14 hours

#### Current State
```cpp
// Line 110
static void draw_gradient_fill(lv_event_t* e) {
    // TODO: Rewrite for LVGL 9 draw task system
    // The old approach using lv_event_get_draw_ctx() doesn't work
    // Need to use draw task events instead

    /* Commented out gradient code... */
}

// Line 262
// TODO: Re-enable gradient fills once LVGL 9 draw task system is understood
// lv_obj_add_event_cb(chart, draw_gradient_fill, LV_EVENT_DRAW_MAIN_END, this);
```

#### Background

LVGL 9 changed the draw architecture:
- **Old (v8):** `lv_event_get_draw_ctx()` returned direct draw context
- **New (v9):** Draw tasks are queued, must use `LV_EVENT_DRAW_TASK_ADDED` + layer access

#### Implementation Steps

1. **Enable draw task events:**
```cpp
lv_obj_add_flag(chart_, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
```

2. **Register for draw task event:**
```cpp
lv_obj_add_event_cb(chart_, on_draw_task_added, LV_EVENT_DRAW_TASK_ADDED, this);
```

3. **Handle draw tasks:**
```cpp
static void on_draw_task_added(lv_event_t* e) {
    auto* self = static_cast<TempGraph*>(lv_event_get_user_data(e));
    lv_draw_task_t* task = lv_event_get_draw_task(e);

    // Only handle line drawing tasks for the chart
    if (task->type != LV_DRAW_TASK_TYPE_LINE) return;

    // Get the layer for drawing
    lv_layer_t* layer = task->target_layer;

    // Now we can draw our gradient triangles
    self->draw_gradient_under_series(layer, task);
}

void TempGraph::draw_gradient_under_series(lv_layer_t* layer, lv_draw_task_t* task) {
    // Get chart data
    lv_chart_series_t* ser = lv_chart_get_series_next(chart_, NULL);
    if (!ser) return;

    // Get series points
    uint32_t point_cnt = lv_chart_get_point_count(chart_);
    lv_area_t chart_area;
    lv_obj_get_coords(chart_, &chart_area);

    // For each pair of adjacent points, draw a gradient quad from line to bottom
    for (uint32_t i = 0; i < point_cnt - 1; i++) {
        lv_point_t p1 = get_chart_point_pos(chart_, ser, i);
        lv_point_t p2 = get_chart_point_pos(chart_, ser, i + 1);

        // Draw gradient-filled triangle/quad
        lv_draw_triangle_dsc_t tri_dsc;
        lv_draw_triangle_dsc_init(&tri_dsc);

        // Top color (series color with alpha)
        tri_dsc.bg_color = ser->color;
        tri_dsc.bg_opa = LV_OPA_30;

        // Two triangles form a quad: line to bottom
        lv_point_t tri1[3] = {p1, p2, {p2.x, chart_area.y2}};
        lv_point_t tri2[3] = {p1, {p2.x, chart_area.y2}, {p1.x, chart_area.y2}};

        tri_dsc.p[0] = tri1[0]; tri_dsc.p[1] = tri1[1]; tri_dsc.p[2] = tri1[2];
        lv_draw_triangle(layer, &tri_dsc);

        tri_dsc.p[0] = tri2[0]; tri_dsc.p[1] = tri2[1]; tri_dsc.p[2] = tri2[2];
        lv_draw_triangle(layer, &tri_dsc);
    }
}
```

#### Key LVGL 9 APIs
- `lv_obj_add_flag(obj, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS)` - Enable task events
- `lv_event_get_draw_task(e)` - Get draw task from event
- `task->target_layer` - Layer to draw on
- `lv_draw_triangle(layer, &dsc)` - Draw triangle primitive

#### Reference Implementation
```
lib/lvgl/src/widgets/calendar/lv_calendar.c:369-402
```
This shows the LVGL 9 pattern for custom drawing in widget events.

#### Testing
```bash
./build/bin/helix-screen --test -p home
# Navigate to temperature panel
# Verify gradient appears under temperature lines
# Resize window - verify gradient scales correctly
# Toggle series visibility - verify gradient updates
```

---

### 3.2 Test Fix - Use-After-Free (Security Test)

**Files:**
- `tests/unit/test_moonraker_client_security.cpp` (Line 806)
- `src/moonraker_client.cpp` (Production fix needed)

**Priority:** High (production bug)
**Estimated Time:** 4-6 hours

#### Current State
```cpp
// test_moonraker_client_security.cpp:806
// FIXME: SIGSEGV - callback executes after client destruction
TEST_CASE_METHOD(SecurityFixture, "...", "[.disabled]") {
    MoonrakerClient client(...);

    client.send_request(..., [&client](auto result) {
        // BUG: This lambda captures &client
        // If callback fires after client destructor, SIGSEGV
        client.some_method();  // Use-after-free!
    });

    // client destructor runs, but callback may still be pending
}
```

#### Root Cause Analysis

1. **Async callback queued** → callback captures `&client`
2. **Test ends** → `client` destructor called
3. **libhv event loop** still has pending callback
4. **Callback fires** → accesses destroyed `client` → SIGSEGV

#### Production Code Fix

In `src/moonraker_client.cpp`, the `cleanup_pending_requests()` method needs a destruction guard:

```cpp
// Add member variable
class MoonrakerClient {
private:
    std::atomic<bool> is_destroying_{false};
    // ...
};

// In destructor
MoonrakerClient::~MoonrakerClient() {
    is_destroying_.store(true);
    cleanup_pending_requests();
    // ... existing cleanup
}

// In cleanup_pending_requests()
void MoonrakerClient::cleanup_pending_requests() {
    std::lock_guard<std::mutex> lock(pending_mutex_);

    for (auto& [id, request] : pending_requests_) {
        // Don't invoke callbacks during destruction - they may reference destroyed objects
        if (!is_destroying_.load()) {
            if (request.error_callback) {
                request.error_callback(MoonrakerError{
                    MoonrakerError::Code::CANCELLED,
                    "Request cancelled due to client shutdown"
                });
            }
        }
    }

    pending_requests_.clear();
}
```

#### Test Fix

Change tests to not capture `&client` in callbacks:

```cpp
TEST_CASE_METHOD(SecurityFixture, "...", "[security]") {
    std::atomic<int> callback_count{0};

    {
        MoonrakerClient client(...);

        client.send_request(...,
            [&callback_count](auto result) {
                // Safe: only captures atomic counter, not client
                callback_count++;
            },
            [&callback_count](auto error) {
                callback_count++;
            });

        // Wait for callbacks to complete before client destruction
        wait_for_event_loop_drain();
    }

    CHECK(callback_count > 0);
}
```

#### Testing
```bash
# Re-enable the disabled test
./build/bin/helix-tests "[security]"
# Run under ASan/TSan to verify no use-after-free
```

---

### 3.3 Test Fix - Wrong Return Value Assertion

**File:** `tests/unit/test_moonraker_client_robustness.cpp`
**Line:** 629
**Priority:** Low (test correctness)
**Estimated Time:** 15 minutes

#### Current State
```cpp
// FIXME: Returns -1 not 0
TEST_CASE_METHOD(RobustnessFixture, "error returns correct code") {
    auto result = client.some_error_operation();
    CHECK(result == 0);  // Wrong! Should be -1
}
```

#### Implementation
```cpp
CHECK(result == -1);  // Correct expectation
```

---

### 3.4 G-code Renderer - Line Clipping

**File:** `src/gcode_renderer.cpp`
**Line:** 300
**Priority:** Low (edge case optimization)
**Estimated Time:** 2-4 hours

#### Current State
```cpp
void GCodeRenderer::draw_line(const Line& line) {
    // TODO: Implement proper line clipping for lines extending outside viewport
    // Currently just draws the full line, relying on GPU clipping
}
```

#### Implementation

Implement Cohen-Sutherland line clipping algorithm:

```cpp
// Outcode bits
constexpr int INSIDE = 0;
constexpr int LEFT = 1;
constexpr int RIGHT = 2;
constexpr int BOTTOM = 4;
constexpr int TOP = 8;

int compute_outcode(float x, float y, const Rect& clip) {
    int code = INSIDE;
    if (x < clip.left) code |= LEFT;
    else if (x > clip.right) code |= RIGHT;
    if (y < clip.bottom) code |= BOTTOM;
    else if (y > clip.top) code |= TOP;
    return code;
}

bool clip_line(float& x0, float& y0, float& x1, float& y1, const Rect& clip) {
    int outcode0 = compute_outcode(x0, y0, clip);
    int outcode1 = compute_outcode(x1, y1, clip);

    while (true) {
        if (!(outcode0 | outcode1)) {
            // Both inside
            return true;
        }
        if (outcode0 & outcode1) {
            // Both outside same region
            return false;
        }

        // Calculate intersection
        int outcodeOut = outcode0 ? outcode0 : outcode1;
        float x, y;

        if (outcodeOut & TOP) {
            x = x0 + (x1 - x0) * (clip.top - y0) / (y1 - y0);
            y = clip.top;
        } else if (outcodeOut & BOTTOM) {
            x = x0 + (x1 - x0) * (clip.bottom - y0) / (y1 - y0);
            y = clip.bottom;
        } else if (outcodeOut & RIGHT) {
            y = y0 + (y1 - y0) * (clip.right - x0) / (x1 - x0);
            x = clip.right;
        } else {
            y = y0 + (y1 - y0) * (clip.left - x0) / (x1 - x0);
            x = clip.left;
        }

        if (outcodeOut == outcode0) {
            x0 = x; y0 = y;
            outcode0 = compute_outcode(x0, y0, clip);
        } else {
            x1 = x; y1 = y;
            outcode1 = compute_outcode(x1, y1, clip);
        }
    }
}
```

---

### 3.5 G-code Viewer - Layer Slider Enhancement

**File:** `src/ui_gcode_viewer.cpp`
**Line:** (implied from feature)
**Priority:** Low (enhancement)
**Estimated Time:** 2-3 hours

#### Feature Description
Add a layer range slider to show only certain layers of the G-code preview.

#### Implementation
1. Add min/max layer slider to viewer UI
2. Filter rendered geometry by layer range
3. Update geometry when slider changes

---

### 3.6 USB Automount - Linux udev Rules

**File:** `src/usb_monitor_linux.cpp`
**Priority:** Low (platform-specific)
**Estimated Time:** 2-3 hours

#### Feature Description
Automatic USB drive mounting on Linux embedded systems.

---

### 3.7 Network Panel - Full Implementation

**File:** `src/ui_panel_network.cpp` (new)
**Priority:** Medium (depends on 2.9)
**Estimated Time:** 6-8 hours

#### Feature Description
Complete network settings panel with:
- WiFi network scanning/selection
- Ethernet status display
- IP address configuration
- Connection testing

---

### 3.8 Tune Panel - Full Implementation

**File:** `src/ui_panel_tune.cpp` (new)
**Priority:** Medium (depends on 2.2)
**Estimated Time:** 4-6 hours

#### Feature Description
As described in 2.2, but with additional features:
- Pressure advance adjustment
- Input shaper tuning
- Live feedback from printer

---

### 3.9 Test Suite - Re-enable Disabled Tests

**Files:** Multiple test files
**Priority:** High (test coverage)
**Estimated Time:** 4-6 hours (after fixing root causes)

After fixing the race conditions (2.11) and use-after-free (3.2):
1. Remove `[.disabled]` tags from tests
2. Verify all tests pass
3. Run under sanitizers (ASan, TSan) to confirm fixes

---

## Implementation Order

### Recommended Sequence

**Phase 1: High-Impact Items (8-10 hours)**
1. Print status cancel confirmation (2.3) - High priority, safety
2. Print status temperature overlays (2.1) - High usage
3. Settings network navigation (2.9) - Completes settings panel
4. Test wrong assertion fix (3.3) - Quick win

**Phase 2: Polish & Stability (10-12 hours)**
5. G-code viewer empty state (2.6)
6. Test mutex race fix (2.11)
7. Settings factory reset (2.10)
8. Printer state network query (2.4)

**Phase 3: Complex Features (16-20 hours)**
9. Use-after-free production fix (3.2) - Critical bug
10. Temperature graph gradient (3.1) - Visual polish
11. Tune overlay (2.2 + 3.8) - New panel
12. Re-enable disabled tests (3.9)

**Phase 4: Low Priority (8-10 hours)**
13. Macro version query (2.5)
14. Travel paths config (2.7)
15. Multi-drive USB (2.8)
16. Line clipping (3.4)

---

## Testing Strategy

### After Each Item
```bash
make -j && ./build/bin/helix-tests
```

### Full Regression
```bash
./build/bin/helix-tests
./build/bin/helix-screen --test  # Manual UI verification
```

### Sanitizer Runs (After Tier 3.2)
```bash
make clean && make SANITIZE=address -j
./build/bin/helix-tests

make clean && make SANITIZE=thread -j
./build/bin/helix-tests
```

---

## Commit Strategy

### Commit per Major Feature
Each item should be committed separately with descriptive messages:

```
feat(print-status): add cancel print confirmation dialog
fix(tests): resolve mutex race condition in robustness fixture
feat(temp-graph): implement LVGL 9 gradient fill using draw tasks
fix(moonraker): prevent use-after-free in cleanup_pending_requests
```

### Or Batch by Tier
```
feat: implement tier-2 TODO items (UI dialogs, state fixes)
feat: implement tier-3 TODO items (LVGL 9, production fixes)
```
