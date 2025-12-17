# AMS Mock Backend: Phase 2 & 3 Implementation Plan

**Status:** NOT STARTED (lower priority polish work)
**Related:** `docs/AMS_MOCK_REALISM_PLAN.md` (overlapping content)
**Last Updated:** 2025-12-16

## Overview

This document captures the remaining work for AMS mock backend realism improvements. Phase 1 (multi-phase operations + timing) is complete. This plan covers error injection, recovery UI, and advanced features.

> **Note:** This work is lower priority. The core AMS functionality (Phases 0-4.6) is complete and working. These mock enhancements would improve automated testing and demo scenarios but are not blocking any user-facing features.

---

## Phase 2: Error Testing & Recovery UI

### 2.1 Random Failure Injection

**Goal:** Enable automatic testing of error recovery flows without manual intervention.

#### Failure Types

| Failure | Trigger Point | UI Response | Recovery Action |
|---------|---------------|-------------|-----------------|
| Filament jam | LOADING at HUB/TOOLHEAD segment | Error modal | "Retry" or "Cancel" |
| Sensor timeout | CHECKING phase | Error modal | Auto-retry (3x) then manual |
| Tip forming fail | FORMING_TIP phase | Error modal | "Retry with higher temp" |
| Communication error | Any phase | Toast + modal | "Reconnect" |

#### Configuration API

```cpp
// include/ams_backend_mock.h
struct FailureConfig {
    float jam_probability = 0.0f;           // 0.0-1.0, triggers during LOADING
    float sensor_fail_probability = 0.0f;   // 0.0-1.0, triggers during CHECKING
    float tip_fail_probability = 0.0f;      // 0.0-1.0, triggers during FORMING_TIP
    float comm_error_probability = 0.0f;    // 0.0-1.0, triggers randomly
    int max_auto_retries = 3;               // Auto-retry before showing modal
};

void AmsBackendMock::set_failure_config(const FailureConfig& config);
FailureConfig AmsBackendMock::get_failure_config() const;
```

#### Environment Variables

```bash
# Global failure rate (applies to all failure types equally)
HELIX_MOCK_AMS_FAIL_RATE=0.1  # 10% failure rate

# Per-type rates (override global)
HELIX_MOCK_AMS_JAM_RATE=0.05
HELIX_MOCK_AMS_SENSOR_FAIL_RATE=0.02
HELIX_MOCK_AMS_TIP_FAIL_RATE=0.03
```

#### Implementation Notes

1. Check failure probability at start of each phase in `execute_load_operation()` / `execute_unload_operation()`
2. On failure: set `AmsAction::ERROR`, populate `operation_detail` with error type
3. Emit `EVENT_ERROR` with JSON payload containing error code and recovery options
4. Backend stays in ERROR state until `recover()` or `cancel()` called

### 2.2 Error Recovery Modal

**Goal:** Show actionable error UI when failures occur.

#### Modal Design

```
┌─────────────────────────────────────┐
│  ⚠️  Filament Jam Detected          │
│                                     │
│  Filament stuck at toolhead.        │
│  Check for obstructions and retry.  │
│                                     │
│  [Retry]  [Cancel]  [Help]          │
└─────────────────────────────────────┘
```

#### Error Types & Actions

| Error | Primary Action | Secondary | Tertiary |
|-------|----------------|-----------|----------|
| JAM | Retry | Cancel | Help (link to wiki) |
| SENSOR_TIMEOUT | Retry | Skip Check | Cancel |
| TIP_FORM_FAIL | Retry (hotter) | Retry (normal) | Cancel |
| COMM_ERROR | Reconnect | Cancel | - |

#### Files to Create/Modify

| File | Changes |
|------|---------|
| `ui_xml/ams_error_modal.xml` | New modal component |
| `src/ui_panel_ams.cpp` | Handle ERROR state, show modal |
| `include/ams_types.h` | Add `AmsErrorType` enum |

### 2.3 Pause/Resume Support

**Goal:** Allow operations to be paused for user intervention.

#### API

```cpp
AmsError AmsBackendMock::pause_operation();   // Sets action to PAUSED
AmsError AmsBackendMock::resume_operation();  // Resumes from where paused

bool AmsBackendMock::is_pausable() const;     // True if operation in progress
bool AmsBackendMock::is_paused() const;       // True if action == PAUSED
```

#### Implementation Notes

- Store pause point (which phase, how much time remaining)
- `resume_operation()` continues from stored point
- Pause only valid during LOADING/UNLOADING phases (not HEATING/CHECKING)

---

## Phase 3: Advanced Features

### 3.1 Multi-Unit Support

**Goal:** Simulate multiple AMS units (e.g., 2x Box Turtle = 8 slots).

#### Configuration

```cpp
// Constructor variant for multi-unit
AmsBackendMock(const std::vector<int>& slots_per_unit);

// Example: Two 4-slot units
auto mock = std::make_unique<AmsBackendMock>({4, 4});

// Environment variable
// HELIX_AMS_UNITS=4,4,4  (three 4-slot units)
```

#### Unit-Specific Behavior

| Feature | Behavior |
|---------|----------|
| Cross-unit changes | +2s delay (simulates physical distance) |
| Unit names | "Unit 1", "Unit 2", etc. (or custom) |
| Independent status | Each unit can be online/offline |
| Slot indexing | Global (0-7 for 2x4) or per-unit (0.0-0.3, 1.0-1.3) |

#### API Additions

```cpp
int AmsBackendMock::get_unit_count() const;
int AmsBackendMock::get_unit_for_slot(int global_slot) const;
std::string AmsBackendMock::get_unit_name(int unit_index) const;
void AmsBackendMock::set_unit_online(int unit_index, bool online);
```

### 3.2 Enhanced Dryer Simulation

**Goal:** Add humidity tracking and power consumption stats.

#### Extended DryerInfo

```cpp
struct DryerInfo {
    // ... existing fields ...

    // New fields
    float humidity_pct = 50.0f;      // Chamber humidity (decreases during drying)
    float power_watts = 0.0f;        // Current power draw (0 when off)
    int filter_hours_remaining = -1; // -1 = not tracked, 0 = replace now
};
```

#### Humidity Simulation

- Start: 50-70% (random)
- During drying: Decreases ~5%/hour (at 1x speed)
- Target: 10-15% after full cycle
- After stop: Slowly increases back toward ambient

#### Power Consumption

| State | Power (W) |
|-------|-----------|
| Off | 0 |
| Heating | 80-120 |
| Maintaining | 40-60 |
| Fan only | 15-25 |

### 3.3 Filament Runout Simulation

**Goal:** Test mid-print runout handling UI.

#### API

```cpp
// Schedule runout after delay
void AmsBackendMock::schedule_runout(int slot_index, int delay_ms);

// Trigger immediately
void AmsBackendMock::trigger_runout(int slot_index);

// Cancel scheduled runout
void AmsBackendMock::cancel_scheduled_runout(int slot_index);
```

#### Behavior

1. When triggered: Set slot status to `EMPTY`, emit `EVENT_RUNOUT`
2. If slot was active: Set `filament_loaded = false`, action stays IDLE
3. UI should show runout notification with options:
   - "Load from slot X" (if other slots have filament)
   - "Load manually" (bypass mode)
   - "Pause print"

---

## Testing Checklist

### Phase 2 Tests

- [ ] `HELIX_MOCK_AMS_FAIL_RATE=0.5` causes ~50% operation failures
- [ ] JAM error shows modal with Retry/Cancel
- [ ] Retry after JAM succeeds (with probability)
- [ ] SENSOR_TIMEOUT auto-retries 3x before showing modal
- [ ] Pause during LOADING works
- [ ] Resume after pause continues operation

### Phase 3 Tests

- [ ] `HELIX_AMS_UNITS=4,4` creates 8-slot mock with 2 units
- [ ] Cross-unit tool change takes longer
- [ ] `set_unit_online(1, false)` marks unit 2 slots as unavailable
- [ ] Dryer humidity decreases during drying cycle
- [ ] Power consumption reported correctly per state
- [ ] `trigger_runout(0)` emits EVENT_RUNOUT and clears slot

---

## File Change Summary

| Phase | File | Changes |
|-------|------|---------|
| 2 | `include/ams_backend_mock.h` | FailureConfig struct, pause/resume API |
| 2 | `src/ams_backend_mock.cpp` | Failure injection in phase executors |
| 2 | `include/ams_types.h` | AmsErrorType enum |
| 2 | `ui_xml/ams_error_modal.xml` | New error modal component |
| 2 | `src/ui_panel_ams.cpp` | ERROR state handling, modal integration |
| 3 | `include/ams_backend_mock.h` | Multi-unit constructor, unit API |
| 3 | `src/ams_backend_mock.cpp` | Multi-unit logic, enhanced dryer |
| 3 | `include/ams_types.h` | DryerInfo humidity/power fields |
| 3 | `src/ams_backend.cpp` | Parse HELIX_AMS_UNITS env var |

---

## Priority Notes

- **Phase 2.1 + 2.2** (failure injection + error modal) are the highest value - enables testing error recovery UI
- **Phase 2.3** (pause/resume) is nice-to-have, lower priority
- **Phase 3.1** (multi-unit) only needed if users with multiple AMS units report issues
- **Phase 3.2 + 3.3** (dryer + runout) are polish features
