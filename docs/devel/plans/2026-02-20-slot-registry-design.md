# SlotRegistry Design: Unified Slot State Management

**Date:** 2026-02-20
**Status:** Design
**Scope:** AFC backend, Happy Hare backend, Mock backend, shared types

---

## Problem

The AMS backend layer manages 8+ parallel data structures indexed by slot/lane/gate
position. These structures are initialized at different times, rebuilt by different code
paths, and can silently get out of sync. A production bug confirmed this: `lane_names_[]`
was in AFC discovery order while global slot indices were in alphabetically-sorted unit
order, causing load/unload/eject/reset operations to target the wrong lane.

The bug class is structural: any time a new index-dependent structure is added or an
existing rebuild path is modified, the developer must remember to update ALL parallel
structures. This is fragile and has already caused real user-facing bugs.

### Parallel structures in AFC backend (before)

| Structure | Type | Indexed By | Rebuilt On Reorganize? |
|-----------|------|------------|----------------------|
| `lane_names_` | `vector<string>` | global slot index | Yes (after bug fix) |
| `lane_name_to_index_` | `unordered_map` | lane name → global index | Yes (after bug fix) |
| `lane_sensors_` | `array<LaneSensors, 16>` | global slot index | **NO** |
| `system_info_.units[].slots[]` | nested vectors | unit-local + global | Yes |
| `system_info_.tool_to_slot_map` | `vector<int>` | tool number | **NO** |
| `endless_spool_configs_` | `vector` | global slot index | **NO** |
| `unit_lane_map_` | `unordered_map` | unit name → lane names | Yes (input to reorganize) |
| `unit_infos_` | `vector<AfcUnitInfo>` | AFC JSON order | Rebuilt from Klipper objects |

### Parallel structures in Happy Hare backend (before)

| Structure | Type | Indexed By | Rebuilt? |
|-----------|------|------------|---------|
| `gate_sensors_` | `vector<GateSensorState>` | global gate index | Resized but never reindexed |
| `system_info_.units[].slots[]` | nested vectors | unit-local + global | Created once in `initialize_gates()` |
| `system_info_.tool_to_slot_map` | `vector<int>` | tool number | Rebuilt on every TTG map update |
| `errored_slot_` | `int` | global gate index | Never validated after init |

---

## Solution: SlotRegistry

A shared, backend-agnostic class that owns ALL per-slot state and provides the single
source of truth for slot identity, indexing, sensor data, tool mapping, and endless spool
configuration. All backends (AFC, Happy Hare, Mock) use the same class, ensuring that
mock tests exercise the same index management code as production.

**Key invariant:** After any call to `reorganize()`, ALL internal structures are
consistent. There is no window where some structures are updated and others are stale.

### What SlotRegistry owns (per slot)

- Backend-specific name (e.g., "lane4" for AFC, "0" for Happy Hare)
- `SlotInfo` data (color, material, status, weight, mapped_tool, error, etc.)
- `SlotSensors` data (prep, load, hub, pre-gate, buffer status, etc.)
- Endless spool backup mapping
- Unit membership (which unit this slot belongs to)

### What stays on the backend

- Hub sensors (`hub_sensors_` map — keyed by hub name, not slot-indexed)
- Unit-level info (`unit_infos_` for AFC, `num_units_` for HH — discovery metadata)
- Protocol-specific state (`current_lane_name_` for AFC, `filament_pos_` for HH)
- Action/error state (`system_info_.action`, `error_segment_`, etc.)
- Extruder state, toolhead sensors (not per-slot)

### What stays on `AmsSystemInfo`

`AmsSystemInfo` remains the public API snapshot returned by `get_system_info()`. It
becomes a **derived view** built from the registry via `build_system_info()`, plus
non-slot fields filled in by the backend. It is no longer the primary store for slot data.

Fields that move to SlotRegistry ownership:
- `units[].slots[]` → derived from `SlotRegistry::slots_[]`
- `total_slots` → derived from `SlotRegistry::slot_count()`
- `tool_to_slot_map` → derived from `SlotRegistry::tool_to_slot_`

Fields that stay on `AmsSystemInfo` (non-slot):
- `action`, `operation_detail`, `current_slot`, `current_tool`
- `filament_loaded`, `supports_bypass`, `bypass_active`
- Capability flags
- `type`, `type_name`, `firmware_version`

---

## SlotSensors Design

Unified sensor struct that accommodates both AFC and Happy Hare sensor models:

```cpp
// include/slot_sensors.h (new file, or in ams_types.h)
struct SlotSensors {
    // AFC binary sensors
    bool prep = false;            // Prep sensor triggered
    bool load = false;            // Load sensor triggered
    bool loaded_to_hub = false;   // Filament reached hub

    // Happy Hare pre-gate sensor
    bool has_pre_gate_sensor = false;
    bool pre_gate_triggered = false;

    // AFC buffer/readiness
    std::string buffer_status;    // e.g., "Advancing"
    std::string filament_status;  // e.g., "Ready", "Not Ready"
    float dist_hub = 0.0f;       // Distance to hub in mm
};
```

Each backend populates only its relevant fields. The unused fields remain at their
zero-initialized defaults and are never read by code paths for the other backend.

### Naming: LaneSensors → SlotSensors

`LaneSensors` (currently in `ams_backend_afc.h`) and `GateSensorState` (currently in
`ams_backend_happy_hare.h`) are both replaced by `SlotSensors`. The new struct lives in
shared space (`ams_types.h` or its own header).

---

## SlotRegistry API

```cpp
// include/slot_registry.h (new file)

struct SlotEntry {
    int global_index = -1;        // Position in registry (== vector index)
    int unit_index = -1;          // Which unit this slot belongs to
    std::string backend_name;     // Backend-specific name for G-code ("lane4", "0")

    SlotInfo info;                // Color, material, status, mapped_tool, error, etc.
    SlotSensors sensors;          // All sensor state
    int endless_spool_backup = -1; // Backup slot index (-1 = none)
};

struct RegistryUnit {
    std::string name;
    int first_slot = 0;
    int slot_count = 0;
};

class SlotRegistry {
public:
    // === Initialization ===

    /// Set up slots from a flat list of backend names. Creates one unit with
    /// the given name. All slots get default SlotInfo values.
    void initialize(const std::string& unit_name,
                    const std::vector<std::string>& slot_names);

    /// Set up slots with explicit unit boundaries. Each unit gets its own
    /// contiguous range of slots. No alphabetical sorting — caller controls order.
    /// Used by Happy Hare where unit structure is determined by gate_count / num_units.
    void initialize_units(const std::vector<std::pair<std::string,
                          std::vector<std::string>>>& units);

    // === Reorganization (atomic) ===

    /// Rebuild from unit→slot_names map. Sorts units alphabetically by name,
    /// assigns global indices sequentially, preserves all SlotEntry data by
    /// matching on backend_name. ALL internal structures (name_to_index_,
    /// tool_to_slot_, units_) are rebuilt atomically.
    void reorganize(const std::map<std::string, std::vector<std::string>>& unit_slot_map);

    // === Slot access ===

    int slot_count() const;
    bool is_valid_index(int global_index) const;

    const SlotEntry* get(int global_index) const;
    SlotEntry* get_mut(int global_index);

    /// Find slot by backend-specific name. Returns nullptr if not found.
    const SlotEntry* find_by_name(const std::string& backend_name) const;
    SlotEntry* find_by_name_mut(const std::string& backend_name);

    /// Get global index for a backend name. Returns -1 if not found.
    int index_of(const std::string& backend_name) const;

    /// Get backend name for a global index. Returns "" if invalid.
    std::string name_of(int global_index) const;

    // === Unit access ===

    int unit_count() const;
    const RegistryUnit& unit(int unit_index) const;

    /// Get the [first, first+count) range of global slot indices for a unit.
    std::pair<int, int> unit_slot_range(int unit_index) const;

    /// Find which unit owns a given global slot index. Returns -1 if invalid.
    int unit_for_slot(int global_index) const;

    // === Tool mapping ===

    /// Get tool number mapped to a slot (-1 if unmapped).
    /// Reads from SlotEntry::info.mapped_tool.
    int tool_for_slot(int global_index) const;

    /// Get slot index mapped to a tool number (-1 if unmapped).
    int slot_for_tool(int tool_number) const;

    /// Set tool mapping for a slot. Updates both the SlotEntry and the
    /// reverse lookup. Clears any previous mapping for that tool number.
    void set_tool_mapping(int global_index, int tool_number);

    /// Bulk-set tool-to-slot map (e.g., from Happy Hare TTG map array).
    /// Clears all existing mappings first.
    void set_tool_map(const std::vector<int>& tool_to_slot);

    // === Endless spool ===

    int backup_for_slot(int global_index) const;
    void set_backup(int global_index, int backup_slot);

    // === Snapshot for public API ===

    /// Build the units/slots portion of AmsSystemInfo from current registry state.
    /// Caller fills in non-slot fields (action, current_slot, capabilities, etc.).
    AmsSystemInfo build_system_info() const;

    // === Lifecycle ===

    bool is_initialized() const;
    void clear();

private:
    std::vector<SlotEntry> slots_;
    std::unordered_map<std::string, int> name_to_index_;  // backend_name → global index
    std::vector<int> tool_to_slot_;                        // tool_number → global index
    std::vector<RegistryUnit> units_;
    bool initialized_ = false;

    /// Internal: rebuild name_to_index_ and tool_to_slot_ from slots_.
    void rebuild_reverse_maps();
};
```

### Thread Safety

`SlotRegistry` itself is NOT thread-safe. It is always accessed under the backend's
existing `mutex_`. This avoids double-locking and keeps the threading model unchanged.

### No LVGL or Moonraker Dependencies

`SlotRegistry` is a pure C++ data structure. It depends only on `ams_types.h` for
`SlotInfo`, `AmsUnit`, `AmsSystemInfo`. No LVGL subjects, no Moonraker API, no spdlog.
This makes it trivially testable.

---

## Backend Integration

### AFC Backend Changes

**Removed members:**
- `lane_names_` → `slots_.name_of()` / `slots_.index_of()`
- `lane_name_to_index_` → `slots_.index_of()` / `slots_.find_by_name()`
- `lane_sensors_` → `slots_.get(i)->sensors` / `slots_.find_by_name_mut(name)->sensors`
- `endless_spool_configs_` → `slots_.backup_for_slot()` / `slots_.set_backup()`
- `lanes_initialized_` → `slots_.is_initialized()`

**Added member:**
- `SlotRegistry slots_;`

**Modified methods:**

| Method | Change |
|--------|--------|
| `initialize_lanes()` | Calls `slots_.initialize(unit_name, lane_names)` |
| `reorganize_units_from_map()` | Calls `slots_.reorganize(unit_lane_map_)` — one call replaces entire method body |
| `get_lane_name()` | Removed. Callers use `slots_.name_of(index)` |
| `get_system_info()` | Calls `slots_.build_system_info()`, fills in non-slot fields |
| `get_slot_info()` | Delegates to `slots_.get(index)->info` |
| `load_filament()` | `slots_.name_of(slot_index)` for lane name |
| `unload_filament()` | No slot index needed (unloads current) |
| `eject_lane()` | `slots_.name_of(slot_index)` — no more direct `lane_names_[]` access |
| `reset_lane()` | `slots_.name_of(slot_index)` |
| `parse_afc_stepper()` | `slots_.find_by_name_mut(lane_name)` for sensor + slot updates |
| `parse_afc_state()` | `slots_.index_of(current_lane)` for current_slot resolution |
| `set_tool_mapping()` | `slots_.set_tool_mapping()` after G-code |
| `set_endless_spool_backup()` | `slots_.set_backup()` after G-code |
| `parse_lane_data()` | Works through `slots_.find_by_name_mut()` |

**`reorganize_units_from_unit_info()` simplification:**
This method rebuilds `unit_lane_map_` from `unit_infos_` then calls
`reorganize_units_from_map()`. With the registry, it becomes:
1. Build `unit_slot_map` from `unit_infos_`
2. Call `slots_.reorganize(unit_slot_map)`
3. Done. No separate rebuild of lane_names_, lane_name_to_index_, etc.

**Eliminated reorganize hazard:** Today, two reorganize paths can fire in the same
`handle_status_update()` call. With the registry, both paths call `slots_.reorganize()`,
which is idempotent for the same input. If the input differs, the last call wins with
a consistent state (no partial rebuild).

### Happy Hare Backend Changes

**Removed members:**
- `gate_sensors_` → `slots_.get(i)->sensors`
- `errored_slot_` → iterate `slots_` to find/clear error on recovery
- `gates_initialized_` → `slots_.is_initialized()`

**Added member:**
- `SlotRegistry slots_;`

**Modified methods:**

| Method | Change |
|--------|--------|
| `initialize_gates()` | Builds unit→slot_names pairs, calls `slots_.initialize_units()` |
| `get_system_info()` | Calls `slots_.build_system_info()`, fills in non-slot fields |
| `get_slot_info()` | Delegates to `slots_.get(index)->info` |
| `parse_mmu_state()` | Gate arrays iterate `slots_.get_mut(i)` instead of `get_slot_global()` |
| `parse_mmu_state()` sensor block | `slots_.get_mut(gate_idx)->sensors.pre_gate_triggered = ...` |
| `parse_mmu_state()` TTG map | `slots_.set_tool_map(ttg_array)` |
| `get_slot_filament_segment()` | `slots_.get(index)->sensors` for pre-gate check |

**Simpler than AFC** because Happy Hare has no reorganize path — units are created once
in `initialize_gates()` and never change.

### Mock Backend Changes

**Added member:**
- `SlotRegistry slots_;`

**Key benefit:** Mock backend uses the SAME `SlotRegistry` class. Any index management
bug that would manifest in AFC or Happy Hare also manifests in mock. Test coverage of
the registry directly tests the production code path.

Mock's `set_mixed_topology_mode()` calls `slots_.initialize_units()` with the multi-unit
structure, then `slots_.reorganize()` if needed. The slot data setup (colors, materials,
tool mappings) goes through registry methods.

---

## Naming Cleanup

Non-backend-internal "lane" references are renamed to "slot":

| Before | After | Location |
|--------|-------|----------|
| `LaneSensors` | `SlotSensors` | `ams_backend_afc.h` → `ams_types.h` or `slot_registry.h` |
| `GateSensorState` | Absorbed into `SlotSensors` | `ams_backend_happy_hare.h` → removed |
| `get_lane_name()` | Removed (use `slots_.name_of()`) | `ams_backend_afc.cpp` |
| `lane_name_to_index_` | Removed (use `slots_.index_of()`) | `ams_backend_afc.h` |
| `lane_names_` | Removed (use `slots_.name_of()`) | `ams_backend_afc.h` |
| `lane_sensors_` | Removed (use `slots_.get()->sensors`) | `ams_backend_afc.h` |
| `lanes_initialized_` | Removed (use `slots_.is_initialized()`) | `ams_backend_afc.h` |
| `gates_initialized_` | Removed (use `slots_.is_initialized()`) | `ams_backend_happy_hare.h` |
| `gate_sensors_` | Removed (use `slots_.get()->sensors`) | `ams_backend_happy_hare.h` |
| `initialize_lanes()` | `initialize_slots()` | `ams_backend_afc.cpp` |
| `initialize_gates()` | `initialize_slots()` | `ams_backend_happy_hare.cpp` |
| `reset_lane()` | Keep — AFC-protocol-specific (sends `AFC_RESET LANE=`) | `ams_backend_afc.cpp` |
| `eject_lane()` | Keep — AFC-protocol-specific (sends G-code with lane name) | `ams_backend_afc.cpp` |

**Rule:** If it appears in a G-code command string or Klipper object name, keep the
original terminology (lane, gate). If it's in our abstraction layer, public API, or
shared code, use "slot."

---

## Additional Code Smells Fixed

These issues from the code review are resolved as part of this refactor:

### C1: `lane_sensors_` fixed array → dynamic, rebuilt on reorganize
The `std::array<LaneSensors, 16>` becomes part of `SlotEntry` in the registry. It's
always the right size and always in the right order.

### C2: `reorganize_units_from_map()` runs on every status update
Add a dirty check: compare the incoming `unit_slot_map` against the current registry
state. Only call `reorganize()` if the structure actually changed (different units or
different slot assignments). The registry can expose a `matches_layout()` method for this.

### C3: Two reorganize paths can conflict
Both paths now call `slots_.reorganize()`. The registry handles this idempotently.

### C4: Direct `lane_names_[]` access bypasses `get_lane_name()`
All access goes through registry methods. `lane_names_` no longer exists.

### I1: `endless_spool_configs_` not rebuilt on reorganize
Now part of `SlotEntry`, rebuilt atomically in `reorganize()`.

### I2: `unit_lane_map_` cleared unconditionally
The registry's `reorganize()` takes a complete snapshot. The backend can build the map
from whatever source without worrying about clearing intermediate state.

### I5: `parse_lane_data` sort vs reorganize sort disagreement
`parse_lane_data()` no longer sorts — it uses `slots_.find_by_name_mut()` to update
slots by name, regardless of their current position in the registry.

### I6: Inconsistent slot validation
All validation goes through `slots_.is_valid_index()`. One check, one truth.

---

## Testing Strategy

### Unit tests for SlotRegistry (test-first)

New test file: `tests/unit/test_slot_registry.cpp`

| Test | What it verifies |
|------|-----------------|
| Initialize with flat slot list | `slot_count()`, `name_of()`, `index_of()`, `get()` all consistent |
| Initialize with multiple units | Unit boundaries, `unit_for_slot()`, `unit_slot_range()` |
| Reorganize preserves slot data | Colors, materials, sensors survive reorganization |
| Reorganize reorders correctly | Alphabetical unit sort, global indices reassigned |
| Reorganize rebuilds reverse maps | `index_of()` and `slot_for_tool()` correct after reorganize |
| Reorganize with new slots | Slots not in stash get defaults |
| Reorganize with removed slots | Slots not in new layout are dropped |
| Tool mapping set/get | Both directions, clearing previous mapping |
| Tool mapping survives reorganize | Tool→slot and slot→tool correct after reorganize |
| Bulk tool map (`set_tool_map`) | Happy Hare TTG-style bulk set |
| Endless spool backup | Set, get, survives reorganize |
| `build_system_info()` | Units, slots, tool_to_slot_map all populated correctly |
| Invalid index handling | `get(-1)`, `get(999)`, `name_of(-1)` return null/empty safely |
| Idempotent reorganize | Calling `reorganize()` twice with same input is a no-op |
| `matches_layout()` | Detects structural changes vs no-ops |
| Clear and reinitialize | Full lifecycle test |

### Backend integration tests

Existing test suites (`test_ams_backend_happy_hare.cpp`, `test_ams_tool_mapping.cpp`,
`test_ams_endless_spool.cpp`, `test_ams_device_actions.cpp`,
`test_ams_mock_mixed_topology.cpp`) must continue to pass. These validate the end-to-end
behavior through the backend API.

### Regression test for the original bug

New test case specifically for the mixed-topology slot index bug: create a mixed topology,
verify that `load_filament(global_index)` for each unit's slots resolves to the correct
backend name.

---

## File Changes Summary

### New files
- `include/slot_registry.h` — `SlotRegistry`, `SlotEntry`, `RegistryUnit`, `SlotSensors`
- `src/printer/slot_registry.cpp` — Implementation
- `tests/unit/test_slot_registry.cpp` — Comprehensive unit tests

### Modified files
- `include/ams_types.h` — Remove `tool_to_slot_map` from `AmsSystemInfo` (now in registry, but still populated in `build_system_info()` for backward compat)
- `include/ams_backend_afc.h` — Remove 6 parallel members, add `SlotRegistry slots_`
- `src/printer/ams_backend_afc.cpp` — Rewrite state management to use registry
- `include/ams_backend_happy_hare.h` — Remove `gate_sensors_`, `errored_slot_`, add `SlotRegistry slots_`
- `src/printer/ams_backend_happy_hare.cpp` — Rewrite state management to use registry
- `include/ams_backend_mock.h` — Add `SlotRegistry slots_`
- `src/printer/ams_backend_mock.cpp` — Use registry for slot setup
- `tests/unit/test_ams_mock_mixed_topology.cpp` — Verify through registry
- Other test files — Adapt to any API changes

### Unchanged files
- `include/ams_backend.h` — Base class interface unchanged (methods still take/return same types)
- `include/ams_state.h` / `src/printer/ams_state.cpp` — Unchanged (consumes `AmsSystemInfo` snapshots)
- UI layer — Unchanged (binds to subjects from `AmsState`, never touches backends directly)
