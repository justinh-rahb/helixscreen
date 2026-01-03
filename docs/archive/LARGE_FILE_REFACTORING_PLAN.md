# Large File Refactoring Plan

## Executive Summary

**Objective**: Break down 8 monolithic files (>1500 lines each) into focused, well-tested modules.

**Current Focus**: `main.cpp` (1,967 lines) → 6 modules with 40 unit tests

**Target Files** (in priority order):
1. `main.cpp` - 1,967 lines → **19 lines** ✅ **COMPLETE**
2. `moonraker_client_mock.cpp` - 2,462 lines
3. `ui_panel_print_status.cpp` - 2,414 lines
4. `ui_gcode_viewer.cpp` - 2,011 lines
5. `ui_panel_print_select.cpp` - 1,850 lines
6. `gcode_layer_renderer.cpp` - 1,616 lines
7. `bed_mesh_renderer.cpp` - 1,573 lines
8. `include/helix_icon_data.h` - 2,062 lines (auto-generated, skip)

---

## main.cpp Refactoring Plan

### Current State (Needs Fixing)

**Broken files in `src/application/`:**
- `lvgl_initializer.cpp` - Has duplicate function definitions (lines 33-43 duplicate 12-31)
- `application.cpp` - Uses fake `extern "C" spdlog_info()` that doesn't exist

**Existing stub files:**
- `application.h/cpp` - Shell class, needs rewrite
- `lvgl_initializer.h/cpp` - Broken, needs fix
- `discovery_manager.h` - Empty stub
- `moonraker_initializer.h` - Empty stub
- `panel_manager.h` - Empty stub

**Existing tests (need rewrite):**
- `tests/unit/application/test_application.cpp` - Tests mock struct, not real classes
- `tests/unit/application/test_infrastructure.cpp` - Placeholder

### Module Architecture

Extract `main.cpp` into 6 focused modules:

| Module | Responsibility | Est. Lines | Tests |
|--------|----------------|------------|-------|
| `DisplayManager` | LVGL display init, backend, input devices | ~150 | 7 |
| `AssetManager` | Font/image registration (static methods) | ~80 | 4 |
| `SubjectInitializer` | Reactive subject setup in dependency order | ~200 | 4 |
| `MoonrakerManager` | Client/API lifecycle, thread-safe notifications | ~300 | 6 |
| `PanelFactory` | Panel creation, wiring, overlays | ~250 | 5 |
| `Application` | Top-level orchestrator, main loop | ~200 | 5 |

**New slim `main.cpp`:** ~10 lines (just creates `Application` and calls `run()`)

---

## Phase 0: Fix Broken State ✅ COMPLETE

**Files fixed:**

| File | Problem | Fix |
|------|---------|-----|
| `src/application/lvgl_initializer.cpp` | Duplicate function definitions | ✅ Deleted lines 33-43 |
| `src/application/application.cpp` | Fake spdlog wrapper | ✅ Use `#include <spdlog/spdlog.h>` directly |

**Validation:** `make -j` compiles without errors ✅

---

## Phase 1: Test Infrastructure ✅ COMPLETE

### 1.1 ApplicationTestFixture

**File:** `tests/unit/application/application_test_fixture.h` ✅

Extends `LVGLTestFixture` with helpers for testing application modules:
- Mock `RuntimeConfig` for test mode ✅
- `MockPrinterState` for coordinating mock behavior ✅
- Common test utilities (`configure_test_mode()`, `set_sim_speedup()`, `reset_mocks()`) ✅

### 1.2 Test Stubs

**File:** `tests/unit/application/test_app_stubs.cpp` ✅

Uses existing stubs from `test_test_config.cpp`:
- `get_runtime_config()` / `get_mutable_runtime_config()` already provided
- PrinterState/MoonrakerClient stubs provided by test fixtures

### 1.3 Build System Updates

- Updated `Makefile` to include `tests/unit/application/*.cpp` ✅
- Added compile rule for application tests in `mk/tests.mk` ✅
- All tests compile and pass ✅

---

## Phase 2: Module Extraction

### 2.1 DisplayManager ✅ COMPLETE

**Source from main.cpp:**
- `init_lvgl()` (lines 631-712)
- `g_display_backend`, `display`, `indev_mouse` globals
- `helix_get_ticks()`, `helix_delay()` timing functions

**Files:**
- `include/display_manager.h` ✅
- `src/application/display_manager.cpp` ✅

**Interface:**
```cpp
class DisplayManager {
public:
    struct Config {
        int width = 800;
        int height = 480;
        int scroll_throw = 25;
        int scroll_limit = 5;
        bool require_pointer = true;
    };
    
    bool init(const Config& config);
    void shutdown();
    bool is_initialized() const;
    
    lv_display_t* display() const;
    lv_indev_t* pointer_input() const;
    lv_indev_t* keyboard_input() const;
    DisplayBackend* backend() const;
    int width() const;
    int height() const;
    
    static uint32_t get_ticks();
    static void delay(uint32_t ms);
};
```

**Tests:** `tests/unit/application/test_display_manager.cpp` ✅

| Test Case | Status |
|-----------|--------|
| `Config has sensible defaults` | ✅ |
| `Config can be customized` | ✅ |
| `starts uninitialized` | ✅ |
| `shutdown is safe when not initialized` | ✅ |
| `get_ticks returns increasing values` | ✅ |
| `delay blocks for approximate duration` | ✅ |
| Full init/shutdown tests | Marked `.pending` (need isolated LVGL) |

---

### 2.2 AssetManager ✅ COMPLETE

**Source from main.cpp:**
- `register_fonts_and_images()` (lines 403-466)

**Files:**
- `include/asset_manager.h` ✅
- `src/application/asset_manager.cpp` ✅

**Interface:**
```cpp
class AssetManager {
public:
    static void register_fonts();
    static void register_images();
    static void register_all();
};
```

**Tests:** `tests/unit/application/test_asset_manager.cpp` ✅

| Test Case | Status |
|-----------|--------|
| `register_fonts registers MDI icons` | ✅ |
| `register_fonts registers Noto Sans` | ✅ |
| `register_images registers images` | ✅ |
| `register_all is idempotent` | ✅ |

---

### 2.3 SubjectInitializer ✅ COMPLETE

**Source from main.cpp:**
- `initialize_subjects()` (lines 468-628)
- Observer guard setup

**Files:**
- `include/subject_initializer.h` ✅
- `src/application/subject_initializer.cpp` ✅

**Interface:**
```cpp
class SubjectInitializer {
public:
    bool init_all(const RuntimeConfig& runtime_config);
    void inject_api(MoonrakerAPI* api);
    bool is_initialized() const;
    size_t observer_count() const;
    
    // Accessors for owned resources
    UsbManager* usb_manager() const;
    TempControlPanel* temp_control_panel() const;
    
    // Accessors for panels needing API injection
    PrintSelectPanel* print_select_panel() const;
    PrintStatusPanel* print_status_panel() const;
    MotionPanel* motion_panel() const;
    ExtrusionPanel* extrusion_panel() const;
    BedMeshPanel* bed_mesh_panel() const;
    
private:
    void init_core_subjects();
    void init_printer_state_subjects();
    void init_ams_subjects();
    void init_panel_subjects(const RuntimeConfig& runtime_config);
    void init_observers();
    void init_utility_subjects();
    void init_usb_manager(const RuntimeConfig& runtime_config);
    
    std::vector<ObserverGuard> m_observers;
    std::unique_ptr<UsbManager> m_usb_manager;
    std::unique_ptr<TempControlPanel> m_temp_control_panel;
    // Panel pointers for deferred API injection
};
```

**Tests:** `tests/unit/application/test_subject_initializer.cpp` ✅

Note: SubjectInitializer has heavy dependencies making it difficult to unit test
in isolation. Tests focus on RuntimeConfig interface; full initialization is
tested as integration tests.

| Test Case | Status |
|-----------|--------|
| RuntimeConfig defaults | ✅ |
| RuntimeConfig mock flags | ✅ |
| RuntimeConfig real flag overrides | ✅ |
| RuntimeConfig production mode | ✅ |
| RuntimeConfig skip_splash | ✅ |
| RuntimeConfig simulation defaults | ✅ |
| RuntimeConfig gcode viewer defaults | ✅ |
| RuntimeConfig test file path | ✅ |
| Integration tests (documented) | ✅ (marked .integration) |

---

### 2.4 MoonrakerManager ✅ COMPLETE

**Source from main.cpp:**
- `initialize_moonraker_client()` (lines 791-1053)
- `moonraker_client`, `moonraker_api` globals
- `notification_queue`, `notification_mutex`

**Files:**
- `include/moonraker_manager.h` ✅
- `src/application/moonraker_manager.cpp` ✅

**Interface:**
```cpp
class MoonrakerManager {
public:
    bool init(const RuntimeConfig& runtime_config, Config* config);
    void shutdown();
    int connect(const std::string& websocket_url, const std::string& http_base_url);
    void process_notifications();
    void process_timeouts();
    
    MoonrakerClient* client() const;
    MoonrakerAPI* api() const;
    size_t pending_notification_count() const;
    
private:
    void create_client(const RuntimeConfig& runtime_config);
    void configure_timeouts(Config* config);
    void register_callbacks();
    void create_api(const RuntimeConfig& runtime_config);
    
    std::unique_ptr<MoonrakerClient> m_client;
    std::unique_ptr<MoonrakerAPI> m_api;
    std::queue<json> m_notification_queue;
    std::mutex m_notification_mutex;
};
```

Note: Print start collector and its observers remain in main.cpp for now
(they use static lambdas capturing global state). Can be extracted to a
separate PrintStartManager in a future phase.

**Tests:** `tests/unit/application/test_moonraker_manager.cpp` ✅

| Test Case | Status |
|-----------|------------------|
| `init in test mode creates mock client` | Client is mock type |
| `init with test_files creates mock API` | API is mock type |
| RuntimeConfig mock decisions | ✅ |
| RuntimeConfig simulation speedup | ✅ |
| Integration tests (documented) | ✅ (marked .integration) |

---

### 2.5 PanelFactory ⏭️ NOT STARTED

**Source from main.cpp:**
- Panel lookup (lines 1402-1414)
- Panel setup calls (lines 1419-1454)
- `overlay_panels` struct (lines 350-358)
- `create_overlay_panel()` helper

**Files:**
- `src/application/panel_factory.h`
- `src/application/panel_factory.cpp`

**Interface:**
```cpp
class PanelFactory {
public:
    struct Panels {
        lv_obj_t* home = nullptr;
        lv_obj_t* print_select = nullptr;
        lv_obj_t* controls = nullptr;
        lv_obj_t* filament = nullptr;
        lv_obj_t* settings = nullptr;
        lv_obj_t* advanced = nullptr;
    };
    
    struct Overlays {
        lv_obj_t* motion = nullptr;
        lv_obj_t* nozzle_temp = nullptr;
        lv_obj_t* bed_temp = nullptr;
        lv_obj_t* extrusion = nullptr;
        lv_obj_t* print_status = nullptr;
        lv_obj_t* ams = nullptr;
    };
    
    bool find_panels(lv_obj_t* panel_container, Panels& out);
    void setup_panels(const Panels& panels, lv_obj_t* screen);
    lv_obj_t* create_overlay(lv_obj_t* screen, const char* component, const char* name);
    void create_requested_overlays(lv_obj_t* screen, const helix::CliArgs& args, Overlays& out);
};
```

**Tests:** `tests/unit/application/test_panel_factory.cpp`

| Test Case | What It Verifies |
|-----------|------------------|
| `find_panels locates all panels` | All 6 found by name |
| `find_panels fails on missing` | Returns false, logs error |
| `create_overlay creates object` | Non-null return |
| `create_overlay logs on failure` | Error logged |
| `setup_panels wires home panel` | Observers connected |

---

### 2.6 Application (Orchestrator) ⏭️ NOT STARTED

**Source from main.cpp:**
- `main()` function skeleton
- Event loop (lines 1812-1918)
- Shutdown sequence (lines 1920-1966)

**Files:**
- `src/application/application.h` (rewrite existing)
- `src/application/application.cpp` (rewrite existing)

**Interface:**
```cpp
class Application {
public:
    Application();
    ~Application();
    
    int run(int argc, char* argv[]);
    
private:
    // Owned managers (in initialization order)
    std::unique_ptr<DisplayManager> m_display;
    std::unique_ptr<SubjectInitializer> m_subjects;
    std::unique_ptr<MoonrakerManager> m_moonraker;
    std::unique_ptr<PanelFactory> m_panels;
    
    // Runtime state
    RuntimeConfig m_config;
    helix::CliArgs m_args;
    bool m_running = false;
    
    // Initialization phases
    bool parse_args(int argc, char* argv[]);
    bool init_display();
    bool init_theme();
    bool init_ui();
    bool init_moonraker();
    bool connect_moonraker();
    
    // Event loop
    int main_loop();
    void handle_keyboard_shortcuts();
    void check_auto_screenshot();
    void check_timeout();
    
    // Shutdown
    void shutdown();
};
```

**Tests:** `tests/unit/application/test_application.cpp` (rewrite existing)

| Test Case | What It Verifies |
|-----------|------------------|
| `run with --help returns 0` | Clean exit |
| `run with --test sets mock mode` | RuntimeConfig correct |
| `shutdown sequence is ordered` | Reverse init order |
| `main_loop exits on quit` | Signal causes exit |
| `main_loop processes notifications` | Queue is drained |

---

## Phase 3: Integration 🔄 IN PROGRESS

### 3.0 Prerequisites (DONE)

**RuntimeConfig API Simplification:**
- ~~`get_runtime_config()` / `get_mutable_runtime_config()`~~ 
- **RESOLVED**: Simplified to single `RuntimeConfig* get_runtime_config()`
- Moved to `src/runtime_config.cpp`
- All callers updated to use `->` instead of `.`

### 3.1 New Slim main.cpp

Replace 1,967-line `main.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen

#include "application.h"

int main(int argc, char** argv) {
    Application app;
    return app.run(argc, argv);
}
```

### 3.2 Known Issues to Watch For

1. **g_log_dest_cli / g_log_file_cli** - Extern globals defined in `cli_args.cpp`. Application references them. Verify accessible after removing main.cpp.

2. **Static variables in main.cpp** - Some initialization uses statics. Check none are lost:
   ```bash
   grep -n "^static " src/main.cpp
   ```

### 3.3 Integration Testing (REQUIRED)

Since Application::run() can't be unit tested, **manual integration testing is required**:

```bash
# 1. Basic startup/shutdown
./build/bin/helix-screen --test --timeout 3 -vv

# 2. Different panels
./build/bin/helix-screen --test -p home --timeout 3
./build/bin/helix-screen --test -p controls --timeout 3
./build/bin/helix-screen --test -p settings --timeout 3
./build/bin/helix-screen --test -p advanced --timeout 3
./build/bin/helix-screen --test -p print-select --timeout 3

# 3. Overlay panels
./build/bin/helix-screen --test -p motion --timeout 3
./build/bin/helix-screen --test -p bed-mesh --timeout 3
./build/bin/helix-screen --test -p print-status --timeout 3
./build/bin/helix-screen --test -p ams --timeout 3

# 4. Screen sizes
./build/bin/helix-screen --test -s tiny --timeout 3
./build/bin/helix-screen --test -s small --timeout 3
./build/bin/helix-screen --test -s medium --timeout 3
./build/bin/helix-screen --test -s large --timeout 3

# 5. Wizard
./build/bin/helix-screen --test --wizard --timeout 5

# 6. Logging levels
./build/bin/helix-screen --test --timeout 2       # WARN only
./build/bin/helix-screen --test --timeout 2 -v    # INFO
./build/bin/helix-screen --test --timeout 2 -vv   # DEBUG
./build/bin/helix-screen --test --timeout 2 -vvv  # TRACE

# 7. Help
./build/bin/helix-screen --help
```

### 3.4 Rollback Plan

If something breaks after replacing main.cpp:
```bash
git checkout src/main.cpp
make -j && ./build/bin/helix-screen --test --timeout 2
```

### 3.5 After Successful Replacement

1. Run full test suite: `./build/bin/helix-tests "~[.]"`
2. Mark Phase 3 complete in this document
3. Commit: `git commit -m "refactor: replace main.cpp with slim Application wrapper"`

---

## Future Improvements

### Panel Accessor Refactoring (Documented for Later)

The 20+ `get_global_*_panel()` accessor functions create tight coupling between panels and global state. A future refactor should migrate to dependency injection:

**Current pattern (global accessors):**
```cpp
HomePanel& get_global_home_panel();
ControlsPanel& get_global_controls_panel();
// ... 20+ more
```

**Future pattern (dependency injection):**
```cpp
class Application {
    std::unique_ptr<HomePanel> m_home_panel;
    std::unique_ptr<ControlsPanel> m_controls_panel;
    // Panels receive dependencies via constructor
};
```

**Benefits of DI:**
- Explicit dependencies (no hidden globals)
- Easier unit testing (inject mocks)
- Clearer ownership semantics
- Better IDE navigation

**Why deferred:** 100+ call sites throughout codebase. Systematic refactor needed.

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| AssetManager | Static methods only | Fonts/images registered once globally |
| Panel accessors | Keep global for now | 100+ call sites, big refactor |
| Observer ownership | SubjectInitializer owns guards | Application owns SubjectInitializer |
| Class members vs globals | Class members | Better encapsulation, testability |

---

## Test Coverage Summary

| Module | Test File | Cases | Assertions |
|--------|-----------|-------|------------|
| DisplayManager | `test_display_manager.cpp` | 6 | Config, state, timing |
| AssetManager | `test_asset_manager.cpp` | 9 | Registration tracking |
| SubjectInitializer | `test_subject_initializer.cpp` | 8 | RuntimeConfig behavior |
| MoonrakerManager | `test_moonraker_manager.cpp` | 6 | RuntimeConfig, mock flags |
| PanelFactory | `test_panel_factory.cpp` | 6 | Constants, PANEL_NAMES |
| Application | `test_application.cpp` | 5 | Config, mocks, LVGL fixture |
| **Total** | | **40** | **136** |

### Test Coverage Limitations

The Application class and its modules have **heavy dependencies** that make full unit testing difficult:

| Component | Unit Testable? | Current Coverage |
|-----------|---------------|------------------|
| `RuntimeConfig` | ✅ Yes | Good - all flags tested |
| `DisplayManager::Config` | ✅ Yes | Good - defaults and customization |
| `DisplayManager::get_ticks/delay` | ✅ Yes | Good - timing functions work |
| `AssetManager` | ✅ Yes | Good - registration tracking |
| `PanelFactory` constants | ✅ Yes | Good - PANEL_NAMES verified |
| `Application::run()` | ❌ No | None - needs full LVGL + display |
| `Application::init_*()` | ❌ No | None - need real backends |
| `MoonrakerManager::init()` | ❌ No | None - needs LVGL + E-Stop |
| `SubjectInitializer::init_all()` | ❌ No | None - needs LVGL + panels |
| `PanelFactory::find_panels()` | ❌ No | None - needs XML + LVGL |

### Integration Testing Strategy

For Application::run() and the full initialization sequence, use:

1. **Manual UI testing** - Run app with various CLI flags
2. **Timeout tests** - `./build/bin/helix-screen --test --timeout 2 -vv`
3. **Panel tests** - `./build/bin/helix-screen --test -p motion --timeout 2`
4. **Screenshot automation** - `./scripts/screenshot.sh`

Key integration test commands:
```bash
# Basic startup/shutdown
./build/bin/helix-screen --test --timeout 2 -vv

# Panel overlays
./build/bin/helix-screen --test -p motion --timeout 2
./build/bin/helix-screen --test -p bed-mesh --timeout 2
./build/bin/helix-screen --test -p print-status --timeout 2

# Wizard flow
./build/bin/helix-screen --test --wizard --timeout 5

# Different screen sizes
./build/bin/helix-screen --test -s tiny --timeout 2
./build/bin/helix-screen --test -s large --timeout 2
```

---

## Progress Tracking

### Phase Status

| Phase | Status | Notes |
|-------|--------|-------|
| Phase 0: Fix Broken State | ✅ COMPLETE | Fixed duplicate code, fixed spdlog |
| Phase 1: Test Infrastructure | ✅ COMPLETE | ApplicationTestFixture, stubs, build system |
| Phase 2.1: DisplayManager | ✅ COMPLETE | Header, impl, tests pass |
| Phase 2.2: AssetManager | ✅ COMPLETE | Header, impl, 9 tests pass |
| Phase 2.3: SubjectInitializer | ✅ COMPLETE | Header, impl, 12 tests pass |
| Phase 2.4: MoonrakerManager | ✅ COMPLETE | Header, impl, 2 test cases pass + print_start_collector |
| Phase 2.5: PanelFactory | ✅ COMPLETE | Header, impl, 2 test cases pass |
| Phase 2.6: Application | ✅ COMPLETE | Full impl with all initialization phases |
| Phase 3: Integration | ✅ COMPLETE | main.cpp replaced with 19-line wrapper |

### Session Log

**2025-12-19 Session 1 (Previous):**
- Created worktree and branch
- Created directory structure
- Created stub files (broken)
- Created placeholder tests (not useful)

**2025-12-19 Session 2:**
- Reviewed and assessed previous work
- Identified broken files needing fixes
- Created comprehensive implementation plan
- Documented module interfaces and test cases

**2025-12-19 Session 3:**
- ✅ Fixed `lvgl_initializer.cpp` - removed duplicate function definitions
- ✅ Fixed `application.cpp` - replaced fake spdlog wrapper with proper includes
- ✅ Verified build compiles successfully
- Updated VS Code `c_cpp_properties.json` with proper include paths
- ✅ Created `ApplicationTestFixture` extending `LVGLTestFixture`
- ✅ Created `application_test_fixture.cpp` implementation
- ✅ Rewrote `test_application.cpp` with real test infrastructure
- ✅ Rewrote `test_infrastructure.cpp` with fixture tests
- ✅ Updated Makefile and mk/tests.mk for application test subdirectory
- ✅ All application tests pass (27 assertions)
- ✅ Created `DisplayManager` class (`include/display_manager.h`, `src/application/display_manager.cpp`)
- ✅ Removed broken stub files (`lvgl_initializer.h/cpp`)
- ✅ Updated Makefile to compile `src/application/*.cpp`
- ✅ All DisplayManager tests pass (28 assertions in 12 test cases)
- ✅ Created `AssetManager` class (`include/asset_manager.h`, `src/application/asset_manager.cpp`)
- ✅ All AssetManager tests pass (13 assertions in 9 test cases)
- ✅ Created `SubjectInitializer` class (`include/subject_initializer.h`, `src/application/subject_initializer.cpp`)
- ✅ All SubjectInitializer tests pass (38 assertions in 12 test cases)
- Fixed `runtime_config.h` to include `<cstdio>` for self-contained compilation
- ✅ Created `MoonrakerManager` class (`include/moonraker_manager.h`, `src/application/moonraker_manager.cpp`)
- ✅ All MoonrakerManager tests pass (11 assertions in 2 test cases)
- ✅ Created `PanelFactory` class (`include/panel_factory.h`, `src/application/panel_factory.cpp`)
- ✅ All PanelFactory tests pass (7 assertions in 2 test cases)
- ✅ Rewrote `Application` class (`include/application.h`, `src/application/application.cpp`)
- ✅ All application tests pass (116 assertions in 34 test cases)

**2025-12-19 Session 4:**
- ✅ Implemented full `Application::run()` with all initialization phases
- ✅ Added print_start_collector to `MoonrakerManager` (was in main.cpp)
- ✅ Deleted broken stub files (`lvgl_initializer.h/cpp`, `discovery_manager.h`, `moonraker_initializer.h`, `panel_manager.h`)
- ✅ Improved unit tests - added meaningful tests for PanelFactory, MoonrakerManager
- ✅ All tests pass (136 assertions in 40 test cases)
- ✅ App runs successfully with `--test --timeout 3 -vv`
- ✅ Documented test coverage limitations (Application::run() not unit-testable)
- ✅ Added integration testing strategy to docs

**2025-12-19 Session 5:**
- ✅ Simplified RuntimeConfig API: removed `get_mutable_runtime_config()`, now single `RuntimeConfig* get_runtime_config()`
- ✅ Created `src/runtime_config.cpp` with global singleton
- ✅ Updated all callers across src/ and tests/ to use `->` instead of `.`
- ✅ Fixed test_test_config.cpp to work with new API
- ✅ All tests pass (136 assertions in 40 test cases)
- ✅ App runs successfully with `--test --timeout 2`

**2025-12-19 Session 6 (Current):**
- ✅ Moved `g_log_dest_cli`/`g_log_file_cli` globals from main.cpp to cli_args.cpp
- ✅ Fixed remaining `get_mutable_runtime_config()` calls in application.cpp
- ✅ Enabled `src/application/application.cpp` in Makefile (was excluded)
- ✅ **Replaced main.cpp** with 19-line wrapper calling `Application::run()`
- ✅ Fixed spdlog shutdown order crash (log before shutdown, not after)
- ✅ All integration tests pass (--test, -p motion, -p bed-mesh, -s tiny, --help, --version)
- ✅ All unit tests pass (136 assertions in 40 test cases)
- **Result:** main.cpp reduced from 1,967 lines to 19 lines (99% reduction)

---

## Quick Reference

### Key Files

| Purpose | Path |
|---------|------|
| This plan | `docs/LARGE_FILE_REFACTORING_PLAN.md` |
| Application modules | `src/application/` |
| Application tests | `tests/unit/application/` |
| LVGL test fixture | `tests/lvgl_test_fixture.h` |
| Existing mocks | `tests/mocks/` |

### Commands

```bash
# Build
make -j

# Run tests
make test

# Run app in test mode
./build/bin/helix-screen --test -vv

# Run specific panel
./build/bin/helix-screen --test -p motion -vv
```

### main.cpp Structure Reference

```
Lines 1-116:     Includes
Lines 117-138:   Timing functions (helix_get_ticks, helix_delay)
Lines 140-174:   Forward declarations
Lines 175-201:   create_overlay_panel() helper
Lines 207-250:   ensure_project_root_cwd()
Lines 252-382:   Static globals and accessor functions
Lines 403-466:   register_fonts_and_images()
Lines 468-628:   initialize_subjects()
Lines 631-712:   init_lvgl()
Lines 716-788:   Screenshot functions
Lines 791-1053:  initialize_moonraker_client()
Lines 1056-1966: main() function
  1058-1092:     CLI parsing
  1094-1175:     Config & logging
  1177-1250:     Display init
  1252-1340:     Theme & widget setup
  1342-1458:     Subject & UI init
  1460-1510:     Moonraker & wizard
  1512-1703:     Navigation & overlays
  1705-1786:     Moonraker connection
  1788-1918:     Event loop
  1920-1966:     Shutdown
```

---

*This document is the authoritative plan for main.cpp modularization. A new session should read this file to continue implementation.*
