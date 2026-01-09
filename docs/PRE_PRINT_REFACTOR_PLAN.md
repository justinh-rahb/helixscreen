# Pre-Print Toggle Control: Refactor Plan

> **Last Updated**: 2026-01-08
> **Status**: Core functionality complete, technical debt remains

## Overview

The pre-print subsystem enables users to control operations like bed mesh calibration, QGL, Z-tilt adjustment, and nozzle cleaning before a print starts. These operations can be embedded in three places:

1. **G-code file** - Slicer-generated commands embedded directly in the file
2. **PRINT_START macro** - Printer-side macro that runs at print start
3. **Printer capability database** - Known capabilities for specific printer models

The subsystem detects these operations and presents unified checkboxes to the user. When the user unchecks an operation, the system either:
- Comments out the command in the G-code file (for file-embedded ops)
- Passes skip/perform parameters to PRINT_START (for macro-embedded ops)

### Current State

The core functionality works correctly after recent fixes. The system uses a priority order of **Database > Macro > File** for determining which parameter to use when controlling an operation. Key components are stable but have accumulated technical debt that makes maintenance difficult.

---

## Architecture Diagram

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                    UI Layer                             │
                    │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌───────┐ │
                    │  │  Bed Mesh  │ │    QGL     │ │  Z-Tilt    │ │ Clean │ │
                    │  │ Checkbox   │ │ Checkbox   │ │ Checkbox   │ │ CB    │ │
                    │  └─────┬──────┘ └─────┬──────┘ └─────┬──────┘ └───┬───┘ │
                    └────────┼──────────────┼──────────────┼────────────┼─────┘
                             │              │              │            │
                             └──────────────┴──────────────┴────────────┘
                                            │
                                            ▼
                    ┌─────────────────────────────────────────────────────────┐
                    │             PrintPreparationManager                     │
                    │                   (Orchestrator)                        │
                    │                                                         │
                    │  start_print() ───┬─── collect_ops_to_disable()        │
                    │                   │                                     │
                    │                   └─── collect_macro_skip_params()     │
                    │                             │                           │
                    │                             ▼                           │
                    │                   PRIORITY ORDER:                       │
                    │                   1. Database (cached)                  │
                    │                   2. Macro analysis                     │
                    │                   3. File scan                          │
                    └─────────────────────────┬───────────────────────────────┘
                                              │
                    ┌─────────────────────────┼───────────────────────────────┐
                    │                         │                               │
          ┌─────────▼─────────┐    ┌─────────▼─────────┐    ┌────────▼───────┐
          │  GCodeOpsDetector │    │ PrintStartAnalyzer│    │ PrinterDetector│
          │  (File Scanning)  │    │ (Macro Analysis)  │    │ (Database)     │
          │                   │    │                   │    │                │
          │ • Scans G-code    │    │ • Fetches macro   │    │ • JSON lookup  │
          │   file content    │    │   from printer    │    │   by printer   │
          │ • Finds embedded  │    │ • Detects skip/   │    │   model name   │
          │   operations      │    │   perform params  │    │                │
          │ • Returns line #s │    │ • Determines      │    │ • Returns      │
          │   for commenting  │    │   controllability │    │   native param │
          │                   │    │                   │    │   names        │
          └───────────────────┘    └───────────────────┘    └────────────────┘
                    │                        │                       │
                    │                        │                       │
                    ▼                        ▼                       ▼
          ┌───────────────────────────────────────────────────────────────────┐
          │                      operation_patterns.h                         │
          │                    (Shared Pattern Definitions)                   │
          │                                                                   │
          │  • OPERATION_KEYWORDS[] - Commands to detect                      │
          │  • SKIP_PARAM_VARIATIONS[] - SKIP_* patterns                      │
          │  • PERFORM_PARAM_VARIATIONS[] - PERFORM_*/DO_* patterns           │
          └───────────────────────────────────────────────────────────────────┘
```

---

## Status Summary

| Category | Status | Details |
|----------|--------|---------|
| **Core Feature** | ✅ Complete | Pre-print toggles work for AD5M and generic printers |
| **PERFORM_* Support** | ✅ Complete | Opt-in parameters detected alongside SKIP_* |
| **Race Condition Fix** | ✅ Complete | Print button disabled during macro analysis |
| **Priority Order** | ✅ Complete | Database → Macro → File (unified UI + execution) |
| **Capability Caching** | ✅ Complete | Avoids repeated database lookups |
| **Test Coverage** | ⚠️ Partial | Missing tests for async state transitions |
| **Technical Debt** | 🔴 Remaining | 3 enums, no retry logic, checkbox ambiguity |

---

## ✅ Completed Work

| Commit | Date | Description |
|--------|------|-------------|
| `653a84fc` | 2026-01-08 | Added this refactor plan document |
| `7a56b19d` | 2026-01-08 | Unified priority order (Database > Macro > File) and added capability caching |
| `95acc10e` | 2026-01-08 | Disabled Print button during macro analysis (race condition fix) |
| `6c0f3759` | 2026-01-07 | Fixed capability database key naming (bed_mesh not bed_leveling) |
| `9854ef19` | 2026-01-07 | Added PERFORM_* opt-in parameter support alongside SKIP_* |

### What's Working Now
- ✅ Bed mesh, QGL, Z-tilt, nozzle clean checkboxes in file detail view
- ✅ Checkboxes correctly disable operations in PRINT_START macro
- ✅ AD5M uses native `FORCE_LEVELING=false` parameter from database
- ✅ Generic printers use detected `SKIP_*` or `PERFORM_*` parameters
- ✅ Print button disabled until macro analysis completes (no race condition)
- ✅ File-embedded operations can be commented out

---

## 🔴 Remaining Issues

| Priority | Issue | Location | Effort |
|----------|-------|----------|--------|
| High | Three operation enums with redundant definitions | `gcode_ops_detector.h`, `print_start_analyzer.h`, `operation_patterns.h` | 3h |
| High | Mutable cache pattern lacks thread-safety documentation | `ui_print_preparation_manager.cpp` | 0.5h |
| Medium | No retry for macro analysis on network failure | `analyze_print_start_macro()` | 2h |
| Medium | No priming checkbox in UI | `print_detail_panel.xml` | 1h |
| Low | Redundant detection in both analyzers | `GCodeOpsDetector` + `PrintStartAnalyzer` | 4h |
| Low | Silent macro analysis failure (no user notification) | `analyze_print_start_macro()` | 1h |
| Low | PrinterState vs PrinterDetector capability divergence | Two independent capability sources | 6h |
| Low | Checkbox semantic ambiguity | `PrePrintOptions` struct | 1h |

---

## 🟡 Medium-Term Refactors (2-4 hours each)

> **Status**: Not started - these are optional improvements

### MT1: Consolidate Operation Enums

**Current State:**
Three separate enums define the same operations with slightly different names:

| File | Enum | Notes |
|------|------|-------|
| `gcode_ops_detector.h` | `gcode::OperationType` | Includes `START_PRINT` (not an operation) |
| `print_start_analyzer.h` | `helix::PrintStartOpCategory` | Includes `BED_LEVEL` parent category |
| `operation_patterns.h` | `helix::OperationCategory` | Most complete, includes all variations |

**Problem:**
Adding a new operation (e.g., input shaper calibration) requires changes in 3+ files with manual synchronization. The conversion logic between enums is error-prone.

**Proposed Solution:**
1. Make `helix::OperationCategory` from `operation_patterns.h` the single source of truth
2. Create conversion functions from/to the legacy enums (for backward compatibility)
3. Gradually migrate callers to use `OperationCategory` directly
4. Remove legacy enums when all callers are migrated

**Files to Change:**
- `include/operation_patterns.h` - Already has the most complete enum
- `include/gcode_ops_detector.h` - Add using alias or deprecation
- `include/print_start_analyzer.h` - Add using alias or deprecation
- `src/print/print_start_analyzer.cpp` - Update detection logic
- `src/rendering/gcode_ops_detector.cpp` - Update detection logic
- `src/ui/ui_print_preparation_manager.cpp` - Update switch statements

**Effort:** 3-4 hours

---

### MT2: Create Capability Matrix Struct

**Current State:**
Three sources provide overlapping capability information with no unified view. The orchestrator queries each source separately and must reconcile differences.

**Proposed Design:**
```cpp
struct CapabilitySource {
    enum class Origin { DATABASE, MACRO_ANALYSIS, FILE_SCAN };
    Origin origin;
    std::string param_name;      // e.g., "FORCE_LEVELING"
    std::string skip_value;      // e.g., "false"
    ParameterSemantic semantic;  // OPT_IN or OPT_OUT
};

struct CapabilityMatrix {
    std::map<OperationCategory, std::vector<CapabilitySource>> capabilities;

    // Returns highest-priority source for an operation
    std::optional<CapabilitySource> get_best_source(OperationCategory op) const;

    // Check if an operation is controllable from any source
    bool is_controllable(OperationCategory op) const;
};
```

**Benefit:** Single place to query "can I control bed mesh and how?" Eliminates scattered priority logic.

**Effort:** 3 hours

---

### MT3: Add Retry Logic for Macro Analysis

**Current State:**
If macro analysis fails (network hiccup, timeout), the system silently falls back to file-only mode. The user loses the ability to control macro-embedded operations with no notification.

**Proposed Solution:**
1. Add retry counter (max 2 retries with exponential backoff)
2. On persistent failure, show toast notification
3. Add manual retry button in file detail view
4. Cache successful results longer (current: session only)

**Implementation:**
```cpp
// In PrintPreparationManager:
void analyze_print_start_macro() {
    if (macro_analysis_retry_count_ >= MAX_RETRIES) {
        // Show user notification about degraded functionality
        return;
    }

    // ... existing analysis code ...

    // On error callback:
    if (++macro_analysis_retry_count_ < MAX_RETRIES) {
        schedule_retry(std::chrono::seconds(2 << macro_analysis_retry_count_));
    }
}
```

**Effort:** 2 hours

---

## 🟠 Long-Term Refactors (6+ hours)

> **Status**: Not started - architectural improvements for future consideration

### LT1: Move Capabilities to PrinterState

**Current State:**
`PrinterDetector` is a standalone JSON lookup that reads `printer_database.json`. `PrinterState` has separate Moonraker-sourced capabilities. These two sources can diverge.

**Target Architecture:**
- `PrinterState` owns ALL capability information
- On connection: loads database entry for printer type
- Merges with Moonraker-reported capabilities
- Exposes unified `PrinterCapabilities` object
- `PrinterDetector` becomes a utility for database lookup only

**Benefit:** Single source of truth eliminates divergence risk. Capabilities are reactive (update when printer state changes).

**Effort:** 8 hours

---

### LT2: Observer Pattern for Checkbox State

**Current State:**
`read_options_from_checkboxes()` reads LVGL widget state synchronously at print time. This requires the checkbox widgets to exist and be in correct state.

**Target Architecture:**
- Add `current_selections_` subject that updates when any checkbox changes
- Bind checkboxes to update subject on change (XML `event_cb`)
- `start_print()` reads from subject instead of widgets
- Subject persists across view transitions

**Benefit:** No async widget reads, state always consistent, easier testing.

**Effort:** 4 hours

---

### LT3: Unify GCodeOpsDetector and PrintStartAnalyzer

**Current State:**
Both classes scan for the same operations using similar patterns. `GCodeOpsDetector` scans file content; `PrintStartAnalyzer` scans macro content.

**Target Architecture:**
- Extract common scanning logic into `OperationScanner` class
- `GCodeOpsDetector` becomes thin wrapper: `scan_file()` -> `OperationScanner::scan()`
- `PrintStartAnalyzer` becomes thin wrapper: `parse_macro()` -> `OperationScanner::scan()`
- Add context flag to differentiate file vs macro scanning

**Benefit:** Single implementation of pattern matching, easier to add new operations.

**Effort:** 6 hours

---

## ⚠️ Test Coverage Gaps

> **Status**: Tests needed before major refactors

### Missing Tests

| Test | File | Description |
|------|------|-------------|
| `is_macro_analysis_in_progress()` state transitions | `test_print_preparation_manager.cpp` | Verify flag is set/cleared correctly during async analysis |
| Capability cache invalidation on printer change | `test_print_preparation_manager.cpp` | Verify cache clears when printer type changes |
| Thread-safety of mutable cache access | `test_print_preparation_manager.cpp` | Verify concurrent reads don't cause races |
| Macro analysis retry behavior | `test_print_preparation_manager.cpp` | Verify retry count and backoff timing |
| Format consistency between UI and execution | `test_print_preparation_manager.cpp` | Verify `format_preprint_steps()` matches `collect_macro_skip_params()` order |

### Recommended Test Additions

```cpp
// Test: is_macro_analysis_in_progress() state machine
TEST_CASE("PrintPreparationManager: macro analysis progress tracking",
          "[print_preparation][async]") {
    PrintPreparationManager manager;
    MockMoonrakerAPI mock_api;
    MockPrinterState mock_state;

    manager.set_dependencies(&mock_api, &mock_state);

    SECTION("In-progress flag set during analysis") {
        REQUIRE_FALSE(manager.is_macro_analysis_in_progress());

        manager.analyze_print_start_macro();
        REQUIRE(manager.is_macro_analysis_in_progress());

        // Simulate completion
        mock_api.complete_pending_requests();
        drain_async_queue();

        REQUIRE_FALSE(manager.is_macro_analysis_in_progress());
    }
}

// Test: Capability cache invalidation
TEST_CASE("PrintPreparationManager: capability cache invalidation",
          "[print_preparation][cache]") {
    PrintPreparationManager manager;

    SECTION("Cache invalidates when printer type changes") {
        // Set printer type to AD5M
        Config::get_instance()->set("printer/type", "FlashForge Adventurer 5M Pro");

        // Access capabilities (populates cache)
        auto caps1 = manager.get_cached_capabilities();
        REQUIRE_FALSE(caps1.empty());

        // Change printer type
        Config::get_instance()->set("printer/type", "Voron 2.4");

        // Access should return different capabilities
        auto caps2 = manager.get_cached_capabilities();
        // Voron doesn't have FORCE_LEVELING
        REQUIRE(caps1.params != caps2.params);
    }
}
```

---

## Decision Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2025-01 | Use Database > Macro > File priority | Database entries are curated and provide correct native parameter names. Macro analysis may detect wrong patterns. File operations are lowest priority since they're handled by commenting out. |
| 2025-01 | Add PERFORM_* alongside SKIP_* | Many printers use opt-in semantics (PERFORM_BED_MESH=1 means do it). Only supporting SKIP_* missed these configurations. |
| 2025-01 | Use mutable cache for capabilities | Avoid repeated JSON parsing. Thread-safety relies on LVGL's single-threaded model (all access from main thread). |
| 2025-01 | Disable Print button during macro analysis | Prevents race condition where user clicks Print before analysis completes, resulting in missing skip parameters. Better UX than post-print warning. |
| 2025-01 | Keep three detector classes separate | Each has different responsibilities: file modification (GCodeOps), macro parameters (PrintStart), static lookup (Printer). Consolidation deferred to LT3. |

---

## Threading Model Notes

The pre-print subsystem has specific threading requirements:

1. **Macro Analysis Callbacks**: Run on HTTP thread. Must use `ui_queue_update()` to defer shared state updates to main thread.

2. **Capability Cache**: Uses `mutable` for lazy initialization. Safe because all access occurs on main LVGL thread. If threading model changes, cache needs mutex protection.

3. **Print Start Flow**: `start_print()` must be called from main thread (reads checkbox widgets). Async file modification callbacks use `alive_guard_` pattern to detect if manager was destroyed.

```cpp
// Safe pattern for async callbacks:
auto alive = alive_guard_;  // Capture shared_ptr
api_->async_operation(
    [self, alive](...) {
        if (!alive || !*alive) {
            return;  // Manager destroyed, bail out
        }
        // Safe to access self->...
    }
);
```
