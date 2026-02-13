# Architectural Debt Register

This document tracks known architectural issues identified during codebase audits.
These are not urgent but should be addressed when touching related code.

> **Last Updated:** 2026-02-06
> **Audit Method:** Multi-agent codebase analysis (5 concurrent agents: class hierarchy, duplication, threading, dead code, coupling)

---

## Priority Legend
- **CRITICAL** - Significant maintainability/correctness risk
- **HIGH** - Should address soon
- **MEDIUM** - Address when touching related code
- **LOW** - Nice to have

## Refactoring Ease Legend
- **Easy** - Contained scope, low risk, can be done in a single session
- **Moderate** - Touches multiple files, needs testing, but well-understood
- **Hard** - Wide blast radius, needs careful incremental migration

---

## 1. God Classes

### 1.1 PrinterState - Internal: RESOLVED, API Surface: HIGH

**File:** `include/printer_state.h` (1,428 lines)
**Impl:** `src/printer/printer_state.cpp` (~627 lines)

**Internal decomposition (RESOLVED 2026-01-12):**
Decomposed into 13 focused, testable domain state classes with 300+ characterization tests:

```
PrinterState (facade - delegates to all components)
+-- PrinterTemperatureState    - 4 subjects: extruder/bed temps and targets
+-- PrinterMotionState         - 8 subjects: position, homed axes, speed/flow, Z-offset
+-- PrinterLedState            - 6 subjects: LED state, RGBW, brightness
+-- PrinterFanState            - 2+ dynamic: fan speeds, per-fan subjects
+-- PrinterPrintState          - 17 subjects: progress, filename, layers, time, phases
+-- PrinterCapabilitiesState   - 14 subjects: hardware capability flags
+-- PrinterPluginStatusState   - 2 subjects: helix plugin, phase tracking
+-- PrinterCalibrationState    - 7 subjects: retraction, manual probe, motors
+-- PrinterHardwareValidationState - 11 subjects: issues, severity, status
+-- PrinterCompositeVisibilityState - 5 subjects: derived can_show_* flags
+-- PrinterNetworkState        - 5 subjects: connection, klippy, nav buttons
+-- PrinterVersionsState       - 2 subjects: klipper/moonraker versions
+-- PrinterExcludedObjectsState - 2 subjects: excluded objects version + set
```

**Remaining problem - PUBLIC API SURFACE (HIGH):**
Despite good internal decomposition, the facade still exposes **68 individual subject getter methods** and **~98 other public methods** (166 total). Every UI panel includes `printer_state.h`, which transitively pulls in all 13 component headers + LVGL + spdlog + JSON. A change to any component header triggers recompilation of 30+ UI files.

**Suggested improvement:**
Bundle subject getters by component to reduce API surface from 68 to ~13:
```cpp
// Instead of 68 individual getters:
lv_subject_t* get_extruder_temp_subject();
lv_subject_t* get_bed_temp_subject();
// ... 66 more

// 13 struct bundles:
struct TemperatureSubjects {
    lv_subject_t* extruder_temp;
    lv_subject_t* extruder_target;
    lv_subject_t* bed_temp;
    lv_subject_t* bed_target;
};
const TemperatureSubjects& get_temperature_subjects();
const MotionSubjects& get_motion_subjects();
// etc.
```

**Ease:** Moderate - mechanical refactoring but touches many call sites
**Impact:** High - reduces API surface 5x, improves compile times

**Archived Handoff:** `docs/archive/PRINTERSTATE_DECOMPOSITION_HANDOFF.md`

---

### 1.2 PrintStatusPanel (RESOLVED - 2026-01-12)

**File:** `src/ui/ui_panel_print_status.cpp` (reduced from 2983 to ~1700 lines)

**Resolution:**
Decomposed into focused, testable components with 83 characterization tests (304 assertions):

```
PrintStatusPanel (UI orchestration only)
+-- PrintCancelModal              - Cancel print confirmation
+-- SaveZOffsetModal              - Z-offset save warning
+-- ExcludeObjectModal            - Object exclusion confirmation
+-- RunoutGuidanceModal           - 6-action filament runout response
+-- PrintTuneOverlay              - Speed/flow/Z-offset tuning (~280 lines)
+-- PrintLightTimelapseControls   - LED/timelapse toggles (~90 lines)
+-- PrintExcludeObjectManager     - Long-press -> modal -> undo -> API (~200 lines)
+-- FilamentRunoutHandler         - Runout detection -> modal -> macros (~160 lines)
```

**Commits:** 1070db09, 5175942d, 6b9466cb, 5266aed6

---

### 1.3 SettingsPanel (RESOLVED - 2026-01-12)

**File:** `src/ui/ui_panel_settings.cpp` (reduced from 1976 to 935 lines - 53% reduction)

**Resolution:**
Decomposed into focused overlay components with 92 characterization tests (528 assertions):

```
SettingsPanel (UI orchestration only)
+-- MachineLimitsOverlay            - Velocity/accel/jerk limits (~220 lines)
+-- MacroButtonsOverlay             - Configurable macro buttons (~250 lines)
+-- FilamentSensorSettingsOverlay   - Filament sensor list/toggle (~180 lines)
+-- HardwareHealthOverlay           - Hardware issue display/actions (~360 lines)
+-- DisplaySettingsOverlay          - Brightness, sleep, render modes (~240 lines)
```

**Commits:** 3cea3e6b, b0b3c8c1, bec9de47, ff9235fe, 235d9ada

---

### 1.4 Application (HIGH)

**File:** `src/application/application.cpp` (1,894 lines), `include/application.h` (160 lines)

**Problem:**
- Orchestrates 12+ initialization phases
- Owns 8+ manager instances (DisplayManager, SubjectInitializer, MoonrakerManager, PrintHistoryManager, TemperatureHistoryManager, PanelFactory, PluginManager, ActionPromptManager)
- **60+ includes** in .cpp file - massive compile dependency
- Mixes startup orchestration with runtime operations
- Hardcoded overlay creation
- All UI panel headers directly included (could go through PanelFactory registry instead)

**Suggested Improvement:**
- Extract `ApplicationBootstrapper` for initialization sequence
- Move runtime operations to `ApplicationRuntime`
- Use PanelFactory registry to avoid 40+ direct panel includes
- Use dependency injection instead of direct manager creation

**Ease:** Hard - central orchestrator, high blast radius
**Impact:** Medium - compile times, testability

---

### 1.5 MoonrakerAPI (HIGH) - NEW

**File:** `include/moonraker_api.h` (1,360 lines)
**Impl:** Split across `moonraker_api.cpp`, `moonraker_api_advanced.cpp` (1,627 lines), and others

**Problem:**
- **117 public methods** spanning 7+ domains
- **8+ callback type definitions** (SuccessCallback, ErrorCallback, FileListCallback, BoolCallback, StringCallback, HistoryListCallback, etc.)
- Mixes: file operations, motion control, heating, history tracking, timelapse management, spoolman integration, LED control, plugin management
- 39+ UI files depend directly on this header

**Current structure (too monolithic):**
```cpp
class MoonrakerAPI {
    // File operations (~15 methods)
    void list_files(...);
    void get_file_metadata(...);
    void delete_file(...);

    // Motion control (~10 methods)
    void home_axes(...);
    void move_axis(...);
    void set_speed_factor(...);

    // Temperature/heating (~8 methods)
    void set_extruder_temp(...);
    void set_bed_temp(...);

    // History (~5 methods)
    void get_print_history(...);

    // Timelapse (~5 methods)
    void get_timelapse_frames(...);

    // Spoolman, LED, plugins, etc...
};
```

**Suggested Improvement:**
Split into domain-specific sub-facades:
```cpp
class MoonrakerFileAPI;        // list_files, get_metadata, delete, upload
class MoonrakerMotionAPI;      // home, move, speed_factor, flow_factor
class MoonrakerHeatingAPI;     // set_temp, pid_calibrate
class MoonrakerHistoryAPI;     // get_history, delete_job
class MoonrakerTimelapsseAPI;  // frames, render, download
```
Each sub-facade can hold a reference to the shared MoonrakerClient. The main MoonrakerAPI class becomes a thin accessor: `api.files().list_files(...)`.

**Ease:** Moderate - mechanical but needs method-by-method migration
**Impact:** High - reduces coupling, improves testability (mock only what you need)

---

### 1.6 AmsState (MEDIUM) - NEW

**File:** `include/ams_state.h` (~600+ lines)

**Problem:**
- Singleton exposing 50+ getters and 16 slot-based subjects
- Per-slot subjects don't scale well (MAX_SLOTS = 16 is arbitrary)
- Duplicates the PrinterState pattern without the same decomposition

**Ease:** Moderate
**Impact:** Low-Medium - mostly self-contained

---

## 2. Singleton Cascade Pattern

**Severity:** MEDIUM

**Scope:** 23+ singletons across the codebase

**Pattern Found:**
UI panels routinely call 3-5 singletons per method:
```cpp
SettingsManager::instance().get_dark_mode();
Config::instance().get_display_timeout();
NavigationManager::instance().get_active();
PrinterState::instance().get_printer_connection_state();
AmsState::instance().get_system_info();
```

**Files Affected:** All UI panels (34 singleton calls in ui_panel_settings.cpp alone)

**Testing impact:** Each singleton requires `reset_for_testing()` methods. Initialization order between singletons is fragile and undocumented.

**Known singletons:** PrinterState, AmsState, Config, DisplayManager, NavigationManager, SettingsManager, StaticPanelRegistry, ToastManager, TemperatureSensorManager, HumiditySensorManager, AccelSensorManager, ProbeSensorManager, ColorSensorManager, WidthSensorManager, FilamentSensorManager, PrintHistoryManager, + 7 more

**Suggested Improvement:**
- Create `UIPanelContext` value object passed to all panels
- Use factory pattern: `PanelFactory::create(context)`
- Consider service locator for testing

**Ease:** Hard - pervasive pattern, requires migration across all panels
**Impact:** Medium - testability, initialization safety

---

## 3. Inappropriate Intimacy

### 3.1 NavigationManager & UI Panels (MEDIUM)

**File:** `src/ui/ui_nav_manager.cpp` (1,072 lines)

**Problem:**
- Holds array of panel pointers, directly manipulates visibility
- Controls animation state through direct method calls
- Bidirectional coupling with panels

**Suggested Improvement:**
- Create `INavigable` interface with minimal contract
- Use event system instead of direct calls
- Extract `PanelAnimator` for animation logic

### 3.2 AmsBackend & AmsState (LOW-MEDIUM)

**Problem:**
- AmsState directly accesses backend internals via `get_backend()`
- Event callbacks create implicit contracts
- Stringly-typed events ("SLOT_CHANGED")

**Suggested Improvement:**
- Use structured event objects instead of strings
- Create `IAmsBackendObserver` interface

---

## 4. Feature Envy / Scattered Business Logic

**Severity:** MEDIUM

**Problem:**
Business logic scattered across UI panels:
- Temperature formatting (divide by 10) in multiple places
- Print time estimation in PrintStatusPanel
- Z-offset conversion (microns <-> mm) in multiple panels
- Fan speed computation in multiple controllers

**Suggested Improvement:**
Create `PrinterDisplay` model with pre-computed display values:
```cpp
struct PrinterDisplay {
    float extruder_temp_c;    // Already formatted
    int print_eta_seconds;    // Already computed
    float z_offset_mm;        // Already converted
    std::string layer_text;   // "Layer 42 / 100"
};
```

**Ease:** Moderate
**Impact:** Medium - reduces scattered logic, fewer bugs from inconsistent formatting

---

## 5. Mixed Concerns

### 5.1 MoonrakerClient (PARTIALLY RESOLVED - 2026-01-11)

**File:** `src/api/moonraker_client.cpp` (~1,771 lines)

**Completed Improvements (2026-01-11):**
- MoonrakerClient is now pure transport layer
- Hardware data flows via callbacks to MoonrakerAPI
- `PrinterDiscovery` is the single source of truth for hardware capabilities

**Remaining:**
- File size still ~1,771 lines - could split connection management from message dispatch
- 6 separate mutexes in the header (callbacks_, requests_, state_callback_, reconnect_, event_handler_, suppress_) - high internal complexity
- 7 atomic variables for state tracking

### 5.2 PrinterDetector - Mixed Concerns (LOW) - NEW

**File:** `include/printer_detector.h` (290 lines)

**Problem:**
Mixes three concerns: hardware detection, printer type identification, and capability auto-detection.

**Suggested:** Split into `HardwareDetector`, `PrinterProfileMatcher`, `CapabilityDetector`

**Ease:** Easy - contained scope
**Impact:** Low - works fine as-is

---

## 6. Code Duplication Patterns

> **2026-02-06 Audit:** Comprehensive duplication analysis across entire codebase.
> Sorted by impact (duplication count x code volume).

### 6.1 Sensor Manager Boilerplate (HIGH) - NEW

**Severity:** HIGH | **Ease:** Easy | **6 managers, ~95% identical**

**Files affected:**
- `src/sensors/temperature_sensor_manager.cpp`
- `src/sensors/humidity_sensor_manager.cpp`
- `src/sensors/accel_sensor_manager.cpp`
- `src/sensors/probe_sensor_manager.cpp`
- `src/sensors/color_sensor_manager.cpp`
- `src/sensors/width_sensor_manager.cpp`

**Duplicated patterns (repeated verbatim 6x):**
1. Anonymous namespace async callback wrapper (`async_update_XXX_subjects_callback`)
2. Singleton `instance()` method (Meyer's singleton)
3. Default constructor/destructor
4. `category_name()` returning a string literal
5. `discover()` method with identical klipper object iteration, state map management, and stale entry cleanup
6. `update_subjects_on_main_thread()` pattern

**Suggested:** Create `SensorManagerBase<Derived>` CRTP template:
```cpp
template <typename Derived, typename SensorConfig, typename SensorState>
class SensorManagerBase : public ISensorManager {
public:
    static Derived& instance();
    void discover(const std::vector<std::string>& klipper_objects) override;
    // Common lifecycle, async callback, state management
protected:
    virtual bool parse_klipper_name(const std::string& name, ...) = 0;
    virtual std::string category_name() const = 0;
};
```
Each sensor manager would only implement `parse_klipper_name()`, `category_name()`, and sensor-specific update logic.

**Estimated reduction:** ~800 lines of duplicated code eliminated

---

### 6.2 AMS Backend Initialization (HIGH) - NEW

**Severity:** HIGH | **Ease:** Easy | **4 backends, ~95% identical lifecycle**

**Files affected:**
- `src/printer/ams_backend_afc.cpp` (1,617 lines)
- `src/printer/ams_backend_happy_hare.cpp`
- `src/printer/ams_backend_valgace.cpp`
- `src/printer/ams_backend_toolchanger.cpp`

**Duplicated patterns:**
1. Constructor: identical `system_info_` initialization (type, version, current_tool, current_slot, filament_loaded, action, total_slots, supports_*) - differs only in AmsType enum and 2-3 boolean flags
2. Destructor: identical `subscription_.release()`
3. `start()`: identical mutex lock, running_ check, client_/api_ null checks, subscription registration, running_ = true
4. `stop()`: identical mutex lock, subscription release, running_ = false

**Suggested:** Extract `AmsBackendBase` with common lifecycle:
```cpp
class AmsBackendBase : public AmsBackend {
protected:
    AmsBackendBase(MoonrakerAPI* api, MoonrakerClient* client, AmsType type, const char* type_name);
    AmsError start() override;  // Common: lock, check, subscribe, set running
    void stop() override;       // Common: lock, release, clear running
    virtual void handle_status_update(const nlohmann::json& notification) = 0;
    virtual void configure_capabilities(AmsSystemInfo& info) = 0;  // Per-backend flags
};
```

**Estimated reduction:** ~600 lines of duplicated lifecycle code

---

### 6.3 Wizard Step Boilerplate (MEDIUM) - NEW

**Severity:** MEDIUM | **Ease:** Moderate | **8+ wizards, 47 global instances**

**Files affected:** All `src/ui/ui_wizard_*.cpp` files

**Duplicated patterns (repeated 8+ times):**
1. Global singleton pattern:
   ```cpp
   static std::unique_ptr<WizardXxxStep> g_wizard_xxx_step;
   WizardXxxStep* get_wizard_xxx_step() {
       if (!g_wizard_xxx_step) {
           g_wizard_xxx_step = std::make_unique<WizardXxxStep>();
           StaticPanelRegistry::instance().register_destroy("WizardXxxStep", []() { g_wizard_xxx_step.reset(); });
       }
       return g_wizard_xxx_step.get();
   }
   ```
2. Move constructor/assignment (copy-paste with member names changed)
3. `init_subjects()` with logging bookends
4. `subjects_initialized_` guard pattern

**Suggested:** CRTP base class or `DEFINE_WIZARD_STEP(ClassName)` macro for the singleton pattern. Move semantics can use `= default` if members are movable.

**Estimated reduction:** ~400 lines across wizard files

---

### 6.4 Panel Singleton Getters (MEDIUM)

**Scope:** 27 panels, 162-216 lines of boilerplate

**Current:**
```cpp
static std::unique_ptr<MotionPanel> g_motion_panel;
MotionPanel& get_global_motion_panel() {
    if (!g_motion_panel) {
        g_motion_panel = std::make_unique<MotionPanel>();
        StaticPanelRegistry::instance().register_destroy(...);
    }
    return *g_motion_panel;
}
```

**Suggested:** `DEFINE_GLOBAL_PANEL(ClassName)` macro

---

### 6.5 Event Callback Registration Chains (MEDIUM)

**Scope:** 50+ files, 150+ calls

**Current (from ui_panel_controls.cpp, 40 lines of this):**
```cpp
lv_xml_register_event_cb(nullptr, "on_calibration_bed_mesh", on_calibration_bed_mesh);
lv_xml_register_event_cb(nullptr, "on_calibration_zoffset", on_calibration_zoffset);
lv_xml_register_event_cb(nullptr, "on_calibration_screws", on_calibration_screws);
// ... 20+ more
```

**Suggested:** Registration table approach:
```cpp
static const EventCallbackEntry callbacks[] = {
    {"on_calibration_bed_mesh", on_calibration_bed_mesh},
    {"on_calibration_zoffset",  on_calibration_zoffset},
    // ...
};
register_event_callbacks(callbacks, std::size(callbacks));
```

Or even simpler - a macro: `REGISTER_CB(on_calibration_bed_mesh)` that derives the string name from the function name.

---

### 6.6 Subject Deinit Boilerplate (MEDIUM)

**Scope:** 135-216 lines across 27 panels

**Current:**
```cpp
void Panel::deinit_subjects() {
    if (!subjects_initialized_) return;
    lv_subject_deinit(&subject1_);
    lv_subject_deinit(&subject2_);
    // ... 5-15 more calls
    subjects_initialized_ = false;
}
```

**Suggested:** SubjectManagedPanel base class (previously identified, still pending)

---

### 6.7 Static Callback Trampolines (LOW-MEDIUM)

**Scope:** 180-720 lines across 18 panels

**Pattern:** Every panel declares identical static callback signatures.

**Suggested:** Preprocessor macro to generate trampolines

---

### 6.8 Observer Setup Patterns (LOW-MEDIUM) - NEW

**Scope:** 18 files, 83 instances

**Pattern (repeated per observer):**
```cpp
observe_int_sync<ClassName>(printer_state.get_xxx_subject(), "subject_name",
    [this](int value) { /* handle */ }, subjects_);
observe_string<ClassName>(printer_state.get_yyy_subject(), "subject_name",
    [this](const std::string& value) { /* handle */ }, subjects_);
```

Not strongly duplicated per-instance (each handler is different), but the setup boilerplate is verbose. Would benefit from subject bundle approach (see 1.1).

---

### 6.9 JSON Parse + Fallback Pattern (LOW) - NEW

**Scope:** 20+ files

**Pattern:**
```cpp
std::ifstream file(path);
if (!file.is_open()) { spdlog::warn("..."); return fallback(); }
json j;
try { j = json::parse(file); }
catch (const json::parse_error& e) { spdlog::warn("..."); return fallback(); }
auto obj = std::make_shared<Type>();
if (!obj->parse_json(j, path)) { spdlog::warn("..."); return fallback(); }
return obj;
```

**Suggested:** Template helper `load_json_with_fallback<T>(path, fallback_fn)`

**Ease:** Easy | **Impact:** Low - mostly readability

---

## 7. Threading & Async Complexity - NEW

> **2026-02-06 Audit:** Dedicated threading analysis.
> Overall threading complexity score: 5.5/10. Fundamentals are solid, but inconsistency adds maintainer burden.

### 7.1 Three Async Patterns, No Guidance (HIGH)

**Problem:** Three different patterns for deferring work to the main thread, with no documentation on when to use which:

| Pattern | Usage | Pros | Cons |
|---------|-------|------|------|
| `helix::async::call_method_ref()` | ~60% of uses (printer_state.cpp) | Type-safe templates, clear intent | Template overhead |
| `ui_queue_update<T>()` | ~20% (ui_print_preparation_manager, ui_panel_settings) | Clear data ownership via unique_ptr | Verbose (10-15 lines per use, needs struct def) |
| Raw `ui_async_call()` with lambdas | ~20% (abort_manager, various UI) | Compact | Less type-safe, manual memory management |

**Suggested:**
1. Document in CLAUDE.md/ARCHITECTURE.md when to use each pattern
2. Prefer `helix::async::call_method_ref()` for simple member function calls
3. Use `ui_queue_update<T>()` only for complex payloads needing explicit ownership
4. Discourage raw `ui_async_call()` in new code

**Ease:** Easy (documentation only) to Moderate (migrating existing raw calls)
**Impact:** High - prevents future divergence, reduces confusion

### 7.2 Callback Bypass (MEDIUM) - NEW

**Problem:** `PrintStartCollector` directly calls `client_.register_method_callback("notify_gcode_response", ...)`, bypassing the `MoonrakerManager`'s notification queue. Most other callbacks route through `m_notification_queue`. This means some callbacks are queued (thread-safe) and some fire directly on the WebSocket thread (different safety guarantees).

**Affected:** `src/print/print_start_collector.cpp`

**Suggested:** Route through MoonrakerManager's notification queue for consistency.

**Ease:** Easy - single file change
**Impact:** Medium - consistency, thread safety predictability

### 7.3 Redundant Synchronization (MEDIUM) - NEW

**Problem areas:**

1. **MoonrakerClient `is_destroying_` flag** - redundant with `lifetime_guard_` (weak_ptr). Both serve the same purpose: preventing callbacks after destruction. The weak_ptr approach (`lifetime_guard_.lock()` returns nullptr) is sufficient.

2. **MoonrakerClient destructor `try_lock()`** - Uses `try_lock()` on `requests_mutex_` to handle potentially corrupted mutexes during static destruction. If `try_lock()` fails, cleanup callbacks are silently skipped. Fragile but intentional safety valve.
   - File: `src/api/moonraker_client.cpp:85-137` (52-line destructor)

3. **PrinterState `state_mutex_`** - Defined at `printer_state.h:1376` but barely used (only for excluded objects update). Most state changes already deferred via `helix::async`. Could be removed if all updates are properly async.

4. **File-scoped atomic rate-limiting flags** in `moonraker_client.cpp:32-33`:
   ```cpp
   std::atomic<bool> g_already_notified_max_attempts{false};
   std::atomic<bool> g_already_notified_disconnect{false};
   ```
   Used together in `reset_notification_flags()` but separate atomics = potential inconsistency window.

**Ease:** Easy (remove redundant flag) to Moderate (audit all paths)
**Impact:** Low-Medium - code clarity, reduced cognitive load

### 7.4 Mixed Atomic/Mutex in AbortManager (LOW) - NEW

**File:** `include/abort_manager.h`

**Problem:** Uses `atomic<State>` for lock-free state reads (good), but also separate atomics for `escalation_level_` and `commands_sent_` (logically related), plus a mutex protecting only `last_result_message_` (one std::string).

**Suggested:** Either consolidate related fields under single mutex, or document why mixed approach is intentional.

**Ease:** Easy
**Impact:** Low - works correctly, just confusing

### 7.5 Synchronization Inventory (Reference)

| Component | Mutexes | Atomics | Pattern |
|-----------|---------|---------|---------|
| MoonrakerClient | 6 | 7 | Per-concern mutex, atomics for flags |
| MoonrakerAPI | 2 | 3 | bed_mesh_, http_threads_, shutdown flag |
| MoonrakerManager | 1 | 1 (shared_ptr<atomic>) | Notification queue |
| AbortManager | 1 | 6 | Mixed atomic state machine + mutex for string |
| PrintStartCollector | 1 | 4 | State mutex + boolean atomics |
| UIUpdateQueue | 1 | 0 | Single queue mutex (textbook correct) |
| PrinterState | 1 | 0 | Barely used (see 7.3) |
| GcodeLayerCache | 1 | 0 | Cache access |
| ThumbnailProcessor | 1 | 0 | Processing queue |

**Total:** 14 mutexes, 19+ atomics across the codebase

---

## 8. API Surface & Coupling - NEW

> **2026-02-06 Audit:** Coupling analysis across all subsystems.

### 8.1 PrinterState Subject Getter Proliferation (HIGH)

**Scope:** 68 individual `get_*_subject()` methods on PrinterState

Already detailed in section 1.1. The key issue: UI panels need to know about individual subject names spread across 13 internal components. A bundle approach would let panels work with coherent groups.

### 8.2 Compile Dependency Fan-Out (MEDIUM) - NEW

**Problem:**
- `printer_state.h` includes 26 headers, cascading to ~150+ indirect includes
- 30+ UI files depend on `printer_state.h`
- 39+ UI files depend on `moonraker_api.h`
- `application.cpp` has 60+ includes

**Impact:** Changing a state component header triggers recompilation of 30+ source files.

**Suggested:**
1. Use forward declarations more aggressively in `printer_state.h`
2. Create `printer_state_fwd.h` with forward declarations only
3. Move subject bundle structs to lightweight headers
4. Use PanelFactory registry in application.cpp to avoid direct panel includes

**Ease:** Moderate - incremental, can do one header at a time
**Impact:** Medium - faster iteration during development

### 8.3 No Subject Registry / Type System (LOW) - NEW

**Problem:** Subject names are strings (`"extruder_temp"`, `"bed_temp"`, etc.) with no compile-time registry. Impossible to statically track which subjects exist, who creates them, who observes them, or detect typos.

**Current:** ~135 subjects created across various init_subjects() methods.

**Suggested:** Create a subject name registry (enum or constexpr strings) so typos are caught at compile time. Not urgent - the system works, but makes auditing harder.

**Ease:** Moderate
**Impact:** Low - correctness during refactoring

---

## 9. Oversized Types & Headers - NEW

### 9.1 ams_types.h (LOW)

**File:** `include/ams_types.h` (953 lines)

**Problem:** Unusually large for a types-only header. Mixes type definitions (enums, structs) with conversion helper functions (`ams_type_to_string`, `ams_type_from_string`, `is_tool_changer`, `is_filament_system`).

**Suggested:** Extract conversion functions to `ams_conversion.h`

**Ease:** Easy
**Impact:** Low

### 9.2 theme_manager.h (LOW)

**File:** `include/theme_manager.h` (740 lines), `src/ui/theme_manager.cpp` (2,429 lines)

**Problem:** Table-driven style system that has grown organically. 40+ StyleRole entries, large palette structs.

**Suggested:** Separate style definition data from management code. The data (token values, color palettes) could be in a separate header or even a JSON file.

**Ease:** Moderate
**Impact:** Low - works fine, just large

---

## 10. Dead Code & Incomplete Features - NEW

### 10.1 Deprecated API (MEDIUM) - NEW

**File:** `include/moonraker_api.h:293`

`MoonrakerAPI::has_helix_plugin()` is marked `[[deprecated]]` - should use `PrinterState::service_has_helix_plugin()` instead. Need to audit all call sites and migrate.

**Ease:** Easy
**Impact:** Medium - prevents confusion about canonical API

### 10.2 Incomplete Features - Known TODOs (LOW) - NEW

27 TODO/FIXME comments across the codebase. Notable ones:

| File | TODO | Priority |
|------|------|----------|
| `ui_panel_macros.cpp:327` | Add confirmation modal for dangerous macros | Medium |
| `ui_overlay_network_settings.cpp:1000-1027` | WiFi connect flow incomplete (3 TODOs) | Medium |
| `ui_panel_advanced.cpp:269` | Implement uninstall functionality | Low |
| `ui_panel_history_list.cpp:1271` | Timelapse viewer/player | Low |
| `plugin_api.cpp:209,456` | LVGL XML unregistration not implemented | Low |
| `ams_backend_afc.cpp:34` | Detect hardware bypass sensor config | Low |
| `ams_backend_happy_hare.cpp:33` | Detect hardware bypass sensor config | Low |
| `gcode_renderer.cpp:318` | Proper clipping for partially visible lines | Low |
| `ui_print_select_usb_source.cpp:183` | Multiple USB drive selector | Low |

### 10.3 Unused Parameters (LOW) - NEW

~15 instances of unused callback parameters (marked with `/* unused */`). Most are legitimate - required by interface signatures. Consider using `[[maybe_unused]]` attribute for clarity.

---

## 11. Missing Abstractions

**Severity:** LOW

**Patterns:**
- ~~Concrete `PrinterCapabilities` struct instead of interface~~ - **RESOLVED:** PrinterCapabilities deleted, replaced by `PrinterDiscovery` (2026-01-11)
- Concrete `MoonrakerAPI*` parameters instead of interface
- No `IPanel` or `IPanelFactory` interfaces

**Impact:** Hinders testing and extensibility

---

## 12. Architecture Strengths (Reference)

Identified during audit - patterns that are working well and should be preserved:

| Pattern | Where | Why It Works |
|---------|-------|-------------|
| Component composition | PrinterState -> 13 state classes | Avoids deep inheritance, each testable |
| Backend factory pattern | Display (3), WiFi (4), Ethernet (3), AMS (5) | Clean abstract interfaces, no deep chains |
| Two-level UI hierarchy | PanelBase/OverlayBase -> concrete panels | Flat, no over-engineering |
| RAII everywhere | ObserverGuard, SubjectManager, SubscriptionGuard, LvglTimerGuard | Prevents resource leaks |
| ui_async_call() | All WebSocket -> UI thread transitions | Prevents cross-thread LVGL access |
| Forward declarations | printer_state.h, moonraker_api.h | Prevents circular includes effectively |
| Two-phase initialization | init_subjects() -> create() -> register_callbacks() | Proper lifecycle management |
| Modal hierarchy | Modal base -> 8 implementations | Flat, each focused |

---

## Appendix: File Size Reference (Updated 2026-02-06)

### Largest Implementation Files

| File | Lines | Status | Notes |
|------|-------|--------|-------|
| moonraker_client_mock.cpp | 3,242 | Expected | Comprehensive mock for testing |
| theme_manager.cpp | 2,429 | Monitor | Table-driven data heavy |
| ui_panel_ams.cpp | 2,239 | Needs split | Complex state machine, see P5 |
| ui_panel_print_select.cpp | 2,143 | Revised target | Orchestration layer, 8+ modules, <2200 OK |
| ui_gcode_viewer.cpp | 2,076 | Monitor | Complex visualization |
| ui_panel_print_status.cpp | 1,963 | Needs split | See P5 |
| application.cpp | 1,894 | HIGH debt | See 1.4 |
| bed_mesh_renderer.cpp | 1,777 | OK | Complex visualization |
| moonraker_client.cpp | 1,771 | Monitor | Transport layer |
| moonraker_api_advanced.cpp | 1,627 | See 1.5 | Part of MoonrakerAPI split candidate |
| ams_backend_afc.cpp | 1,617 | HIGH dup | See 6.2 |
| ui_panel_controls.cpp | 1,571 | Monitor | |
| gcode_layer_renderer.cpp | 1,560 | OK | Rendering engine |
| ams_backend_mock.cpp | 1,537 | Expected | Mock |
| ui_print_preparation_manager.cpp | 1,535 | Monitor | |
| ui_filament_path_canvas.cpp | 1,532 | Monitor | Visualization |
| gcode_parser.cpp | 1,502 | OK | Parser |

### Largest Headers

| Header | Lines | Notes |
|--------|-------|-------|
| printer_state.h | 1,428 | 166 public methods (see 1.1) |
| moonraker_api.h | 1,360 | 117 public methods (see 1.5) |
| helix_icon_data.h | 2,062 | Auto-generated, OK |
| ams_types.h | 953 | Oversized for types (see 9.1) |
| theme_manager.h | 740 | Large singleton (see 9.2) |
| moonraker_client.h | 689 | Transport layer |
| ams_backend.h | 687 | Backend interface |

---

## Appendix: Prioritized Refactoring Roadmap

Recommended order based on impact/effort ratio:

| # | Item | Effort | Impact | Risk | Section |
|---|------|--------|--------|------|---------|
| 1 | Sensor manager template base | Easy | High (800 LOC) | Low | 6.1 |
| 2 | AMS backend base class | Easy | High (600 LOC) | Low | 6.2 |
| 3 | Document async pattern guidance | Easy | High (prevention) | None | 7.1 |
| 4 | Deprecated API migration | Easy | Medium | Low | 10.1 |
| 5 | Callback bypass fix | Easy | Medium | Low | 7.2 |
| 6 | PrinterState subject bundles | Moderate | High | Medium | 1.1 |
| 7 | Wizard step boilerplate macro | Moderate | Medium (400 LOC) | Low | 6.3 |
| 8 | Panel singleton macro | Moderate | Medium | Low | 6.4 |
| 9 | Event callback registration tables | Moderate | Medium | Low | 6.5 |
| 10 | MoonrakerAPI domain split | Moderate | High | Medium | 1.5 |
| 11 | Compile dependency reduction | Moderate | Medium | Low | 8.2 |
| 12 | Application bootstrapper extract | Hard | Medium | High | 1.4 |
| 13 | Singleton cascade -> context | Hard | Medium | High | 2 |

---

## Change Log

| Date | Change |
|------|--------|
| 2026-02-06 | **Major audit update** - 5-agent deep dive. Added: MoonrakerAPI god class (1.5), AmsState (1.6), PrinterState API surface issue (1.1 update), Threading & Async Complexity (7), API Surface & Coupling (8), Oversized Types (9), Dead Code & TODOs (10), Architecture Strengths (12), Prioritized Roadmap. Expanded Code Duplication (6) with sensor managers, AMS backends, wizard boilerplate, JSON parsing, observer patterns. Updated file size reference with all files >1500 LOC. Updated singleton count to 23+. |
| 2026-01-12 | **PrinterState god class RESOLVED** - All 13 domains extracted (~90+ subjects), PrinterState now a thin facade |
| 2026-01-12 | **SettingsPanel god class RESOLVED** - 5 overlay components extracted (1976->935 lines, 53% reduction), 92 tests with 528 assertions |
| 2026-01-11 | Updated for Hardware Discovery Refactor: PrinterCapabilities deleted, MoonrakerClient mixed concerns partially resolved |
| 2026-01-01 | Initial audit - documented all findings from multi-agent analysis |
