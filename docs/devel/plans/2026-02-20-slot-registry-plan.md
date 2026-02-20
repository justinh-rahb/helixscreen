# SlotRegistry Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace 8+ parallel data structures across AFC, Happy Hare, and Mock backends with a single shared `SlotRegistry` class that atomically manages all slot-indexed state.

**Architecture:** `SlotRegistry` is a standalone C++ class (no LVGL/Moonraker deps) that owns per-slot identity, sensor data, tool mapping, and endless spool config. Each backend holds a `SlotRegistry slots_` member and delegates all slot lookups/mutations to it. `AmsSystemInfo` becomes a derived snapshot built from the registry.

**Tech Stack:** C++17, Catch2 (testing), spdlog (logging in backends only — registry itself is pure C++)

**Design doc:** `docs/devel/plans/2026-02-20-slot-registry-design.md`

**Process:** TDD throughout. Code review after each phase. No "good enough" — fix every medium+ issue found in review.

---

## Phase 1: SlotRegistry Class (Test-First)

Build the registry as a standalone, fully tested class before touching any backend.

### Task 1.1: SlotSensors struct + SlotEntry struct + header skeleton

**Files:**
- Create: `include/slot_registry.h`

**Step 1: Write the header with all types and the SlotRegistry class declaration**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ams_types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <map>
#include <utility>

namespace helix::printer {

/// Unified per-slot sensor state. Replaces AFC's LaneSensors and
/// Happy Hare's GateSensorState with a single struct usable by all backends.
struct SlotSensors {
    // AFC binary sensors
    bool prep = false;
    bool load = false;
    bool loaded_to_hub = false;

    // Happy Hare pre-gate sensor
    bool has_pre_gate_sensor = false;
    bool pre_gate_triggered = false;

    // AFC buffer/readiness
    std::string buffer_status;
    std::string filament_status;
    float dist_hub = 0.0f;
};

/// A single slot in the registry. Owns all per-slot state.
struct SlotEntry {
    int global_index = -1;
    int unit_index = -1;
    std::string backend_name;  // "lane4" (AFC), "0" (HH) — for G-code

    SlotInfo info;
    SlotSensors sensors;
    int endless_spool_backup = -1;
};

/// Unit metadata in the registry.
struct RegistryUnit {
    std::string name;
    int first_slot = 0;
    int slot_count = 0;
};

/// Single source of truth for all slot-indexed state.
///
/// NOT thread-safe — callers must hold their own mutex.
/// No LVGL or Moonraker dependencies.
class SlotRegistry {
public:
    // === Initialization ===
    void initialize(const std::string& unit_name,
                    const std::vector<std::string>& slot_names);
    void initialize_units(
        const std::vector<std::pair<std::string, std::vector<std::string>>>& units);

    // === Reorganization (atomic) ===
    void reorganize(
        const std::map<std::string, std::vector<std::string>>& unit_slot_map);
    bool matches_layout(
        const std::map<std::string, std::vector<std::string>>& unit_slot_map) const;

    // === Slot access ===
    int slot_count() const;
    bool is_valid_index(int global_index) const;
    const SlotEntry* get(int global_index) const;
    SlotEntry* get_mut(int global_index);
    const SlotEntry* find_by_name(const std::string& backend_name) const;
    SlotEntry* find_by_name_mut(const std::string& backend_name);
    int index_of(const std::string& backend_name) const;
    std::string name_of(int global_index) const;

    // === Unit access ===
    int unit_count() const;
    const RegistryUnit& unit(int unit_index) const;
    std::pair<int, int> unit_slot_range(int unit_index) const;
    int unit_for_slot(int global_index) const;

    // === Tool mapping ===
    int tool_for_slot(int global_index) const;
    int slot_for_tool(int tool_number) const;
    void set_tool_mapping(int global_index, int tool_number);
    void set_tool_map(const std::vector<int>& tool_to_slot);

    // === Endless spool ===
    int backup_for_slot(int global_index) const;
    void set_backup(int global_index, int backup_slot);

    // === Snapshot ===
    AmsSystemInfo build_system_info() const;

    // === Lifecycle ===
    bool is_initialized() const;
    void clear();

private:
    std::vector<SlotEntry> slots_;
    std::unordered_map<std::string, int> name_to_index_;
    std::vector<int> tool_to_slot_;
    std::vector<RegistryUnit> units_;
    bool initialized_ = false;

    void rebuild_reverse_maps();
};

}  // namespace helix::printer
```

**Step 2: Commit**

```bash
git add include/slot_registry.h
git commit -m "feat(ams): add SlotRegistry header — types and API declaration"
```

---

### Task 1.2: Write failing tests for initialization

**Files:**
- Create: `tests/unit/test_slot_registry.cpp`

**Step 1: Write tests for single-unit initialization**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include "slot_registry.h"

using namespace helix::printer;

TEST_CASE("SlotRegistry single-unit initialization", "[slot_registry][init]") {
    SlotRegistry reg;

    REQUIRE_FALSE(reg.is_initialized());
    REQUIRE(reg.slot_count() == 0);

    std::vector<std::string> names = {"lane0", "lane1", "lane2", "lane3"};
    reg.initialize("Turtle_1", names);

    SECTION("basic state") {
        REQUIRE(reg.is_initialized());
        REQUIRE(reg.slot_count() == 4);
        REQUIRE(reg.unit_count() == 1);
    }

    SECTION("slot access by index") {
        for (int i = 0; i < 4; ++i) {
            const auto* entry = reg.get(i);
            REQUIRE(entry != nullptr);
            REQUIRE(entry->global_index == i);
            REQUIRE(entry->unit_index == 0);
            REQUIRE(entry->backend_name == names[i]);
        }
    }

    SECTION("slot access by name") {
        REQUIRE(reg.index_of("lane2") == 2);
        REQUIRE(reg.name_of(3) == "lane3");
        REQUIRE(reg.index_of("nonexistent") == -1);
        REQUIRE(reg.name_of(99) == "");
        REQUIRE(reg.name_of(-1) == "");
    }

    SECTION("find_by_name") {
        const auto* entry = reg.find_by_name("lane1");
        REQUIRE(entry != nullptr);
        REQUIRE(entry->global_index == 1);
        REQUIRE(reg.find_by_name("nope") == nullptr);
    }

    SECTION("unit info") {
        const auto& u = reg.unit(0);
        REQUIRE(u.name == "Turtle_1");
        REQUIRE(u.first_slot == 0);
        REQUIRE(u.slot_count == 4);

        auto [first, end] = reg.unit_slot_range(0);
        REQUIRE(first == 0);
        REQUIRE(end == 4);
    }

    SECTION("unit_for_slot") {
        for (int i = 0; i < 4; ++i) {
            REQUIRE(reg.unit_for_slot(i) == 0);
        }
        REQUIRE(reg.unit_for_slot(-1) == -1);
        REQUIRE(reg.unit_for_slot(4) == -1);
    }

    SECTION("is_valid_index") {
        REQUIRE(reg.is_valid_index(0));
        REQUIRE(reg.is_valid_index(3));
        REQUIRE_FALSE(reg.is_valid_index(-1));
        REQUIRE_FALSE(reg.is_valid_index(4));
    }

    SECTION("default slot info") {
        const auto* entry = reg.get(0);
        REQUIRE(entry->info.global_index == 0);
        REQUIRE(entry->info.slot_index == 0);  // unit-local
        REQUIRE(entry->info.mapped_tool == -1);
        REQUIRE(entry->info.status == SlotStatus::UNKNOWN);
    }
}

TEST_CASE("SlotRegistry multi-unit initialization", "[slot_registry][init]") {
    SlotRegistry reg;

    std::vector<std::pair<std::string, std::vector<std::string>>> units = {
        {"Turtle_1", {"lane0", "lane1", "lane2", "lane3"}},
        {"AMS_1", {"lane4", "lane5", "lane6", "lane7"}},
    };
    reg.initialize_units(units);

    REQUIRE(reg.slot_count() == 8);
    REQUIRE(reg.unit_count() == 2);

    SECTION("unit boundaries") {
        auto [f0, e0] = reg.unit_slot_range(0);
        REQUIRE(f0 == 0);
        REQUIRE(e0 == 4);
        auto [f1, e1] = reg.unit_slot_range(1);
        REQUIRE(f1 == 4);
        REQUIRE(e1 == 8);
    }

    SECTION("global index continuity") {
        for (int i = 0; i < 8; ++i) {
            REQUIRE(reg.get(i)->global_index == i);
        }
    }

    SECTION("unit-local indices") {
        REQUIRE(reg.get(0)->info.slot_index == 0);
        REQUIRE(reg.get(3)->info.slot_index == 3);
        REQUIRE(reg.get(4)->info.slot_index == 0);  // first slot in unit 1
        REQUIRE(reg.get(7)->info.slot_index == 3);
    }

    SECTION("unit_for_slot across units") {
        REQUIRE(reg.unit_for_slot(0) == 0);
        REQUIRE(reg.unit_for_slot(3) == 0);
        REQUIRE(reg.unit_for_slot(4) == 1);
        REQUIRE(reg.unit_for_slot(7) == 1);
    }

    SECTION("name lookup across units") {
        REQUIRE(reg.index_of("lane4") == 4);
        REQUIRE(reg.name_of(7) == "lane7");
    }
}
```

**Step 2: Run tests to verify they fail (won't link — no implementation yet)**

```bash
make test 2>&1 | tail -5
```

Expected: Linker errors for SlotRegistry methods.

**Step 3: Commit the failing tests**

```bash
git add tests/unit/test_slot_registry.cpp
git commit -m "test(ams): add SlotRegistry initialization tests (red)"
```

---

### Task 1.3: Implement initialization methods

**Files:**
- Create: `src/printer/slot_registry.cpp`

**Step 1: Implement `initialize()`, `initialize_units()`, accessors, `clear()`**

Implement all methods needed to pass the Task 1.2 tests. Key points:
- `initialize()` creates one unit, populates `slots_` with `SlotEntry` for each name
- `initialize_units()` creates multiple units, assigns contiguous global indices
- Both call `rebuild_reverse_maps()` at the end
- `rebuild_reverse_maps()` clears and rebuilds `name_to_index_` from `slots_`
- `get()` bounds-checks index against `slots_.size()`
- `find_by_name()` uses `name_to_index_` lookup
- `unit_for_slot()` searches `units_` for the one containing the index
- Default `SlotInfo` has `mapped_tool = -1`, `status = UNKNOWN`

**Step 2: Build and run tests**

```bash
make test && ./build/bin/helix-tests "[slot_registry][init]"
```

Expected: All PASS.

**Step 3: Commit**

```bash
git add src/printer/slot_registry.cpp
git commit -m "feat(ams): implement SlotRegistry initialization"
```

---

### Task 1.4: Write failing tests for reorganize

**Files:**
- Modify: `tests/unit/test_slot_registry.cpp`

**Step 1: Add reorganize tests**

```cpp
TEST_CASE("SlotRegistry reorganize sorts units alphabetically", "[slot_registry][reorganize]") {
    SlotRegistry reg;

    // Initialize in non-alphabetical order
    std::vector<std::pair<std::string, std::vector<std::string>>> units = {
        {"Zebra", {"z0", "z1"}},
        {"Alpha", {"a0", "a1"}},
    };
    reg.initialize_units(units);

    // Verify initial order (as given)
    REQUIRE(reg.unit(0).name == "Zebra");
    REQUIRE(reg.get(0)->backend_name == "z0");

    // Reorganize with same data — should sort alphabetically
    std::map<std::string, std::vector<std::string>> unit_map = {
        {"Zebra", {"z0", "z1"}},
        {"Alpha", {"a0", "a1"}},
    };
    reg.reorganize(unit_map);

    SECTION("units sorted alphabetically") {
        REQUIRE(reg.unit(0).name == "Alpha");
        REQUIRE(reg.unit(1).name == "Zebra");
    }

    SECTION("slots reindexed to match") {
        REQUIRE(reg.get(0)->backend_name == "a0");
        REQUIRE(reg.get(1)->backend_name == "a1");
        REQUIRE(reg.get(2)->backend_name == "z0");
        REQUIRE(reg.get(3)->backend_name == "z1");
    }

    SECTION("reverse maps rebuilt") {
        REQUIRE(reg.index_of("a0") == 0);
        REQUIRE(reg.index_of("z0") == 2);
        REQUIRE(reg.name_of(0) == "a0");
    }
}

TEST_CASE("SlotRegistry reorganize preserves slot data", "[slot_registry][reorganize]") {
    SlotRegistry reg;

    // Initialize, then set some slot state
    reg.initialize("Unit_A", {"s0", "s1", "s2"});
    reg.get_mut(1)->info.color_rgb = 0xFF0000;
    reg.get_mut(1)->info.material = "PLA";
    reg.get_mut(1)->info.status = SlotStatus::AVAILABLE;
    reg.get_mut(1)->sensors.prep = true;
    reg.get_mut(1)->sensors.load = true;
    reg.get_mut(1)->endless_spool_backup = 2;

    // Reorganize into 2 units — s1 moves from index 1 to a new position
    std::map<std::string, std::vector<std::string>> unit_map = {
        {"Unit_B", {"s1"}},         // s1 now at global 0 (Unit_B sorts before Unit_Z)
        {"Unit_Z", {"s0", "s2"}},   // s0 at global 1, s2 at global 2
    };
    reg.reorganize(unit_map);

    SECTION("s1 data preserved at new position") {
        const auto* entry = reg.find_by_name("s1");
        REQUIRE(entry != nullptr);
        REQUIRE(entry->global_index == 0);  // moved from 1 to 0
        REQUIRE(entry->info.color_rgb == 0xFF0000);
        REQUIRE(entry->info.material == "PLA");
        REQUIRE(entry->info.status == SlotStatus::AVAILABLE);
        REQUIRE(entry->sensors.prep == true);
        REQUIRE(entry->sensors.load == true);
        REQUIRE(entry->endless_spool_backup == 2);  // Note: backup index is stale — see Task 1.7
    }

    SECTION("indices and unit membership updated") {
        const auto* s1 = reg.find_by_name("s1");
        REQUIRE(s1->unit_index == 0);
        REQUIRE(s1->info.slot_index == 0);  // unit-local

        const auto* s0 = reg.find_by_name("s0");
        REQUIRE(s0->global_index == 1);
        REQUIRE(s0->unit_index == 1);
        REQUIRE(s0->info.slot_index == 0);  // first in Unit_Z
    }
}

TEST_CASE("SlotRegistry reorganize with new and removed slots", "[slot_registry][reorganize]") {
    SlotRegistry reg;

    reg.initialize("Unit", {"s0", "s1", "s2"});
    reg.get_mut(0)->info.color_rgb = 0xAAAAAA;

    // Reorganize: s1 removed, s3 added
    std::map<std::string, std::vector<std::string>> unit_map = {
        {"Unit", {"s0", "s2", "s3"}},
    };
    reg.reorganize(unit_map);

    SECTION("s0 preserved") {
        REQUIRE(reg.find_by_name("s0")->info.color_rgb == 0xAAAAAA);
    }

    SECTION("s1 removed") {
        REQUIRE(reg.find_by_name("s1") == nullptr);
        REQUIRE(reg.index_of("s1") == -1);
    }

    SECTION("s3 added with defaults") {
        const auto* s3 = reg.find_by_name("s3");
        REQUIRE(s3 != nullptr);
        REQUIRE(s3->info.status == SlotStatus::UNKNOWN);
    }

    SECTION("slot count updated") {
        REQUIRE(reg.slot_count() == 3);
    }
}

TEST_CASE("SlotRegistry idempotent reorganize", "[slot_registry][reorganize]") {
    SlotRegistry reg;

    std::map<std::string, std::vector<std::string>> unit_map = {
        {"Alpha", {"a0", "a1"}},
        {"Beta", {"b0", "b1"}},
    };

    reg.initialize("temp", {"a0", "a1", "b0", "b1"});
    reg.get_mut(0)->info.color_rgb = 0x112233;
    reg.reorganize(unit_map);

    // Capture state
    auto color_before = reg.get(0)->info.color_rgb;
    auto name_before = reg.get(0)->backend_name;

    // Reorganize again with same layout
    reg.reorganize(unit_map);

    REQUIRE(reg.get(0)->info.color_rgb == color_before);
    REQUIRE(reg.get(0)->backend_name == name_before);
}

TEST_CASE("SlotRegistry matches_layout", "[slot_registry][reorganize]") {
    SlotRegistry reg;

    std::map<std::string, std::vector<std::string>> layout = {
        {"A", {"s0", "s1"}},
        {"B", {"s2", "s3"}},
    };

    reg.initialize("temp", {"s0", "s1", "s2", "s3"});
    reg.reorganize(layout);

    REQUIRE(reg.matches_layout(layout));

    // Different slot in a unit
    std::map<std::string, std::vector<std::string>> different = {
        {"A", {"s0", "s1"}},
        {"B", {"s2", "s99"}},
    };
    REQUIRE_FALSE(reg.matches_layout(different));

    // Different unit count
    std::map<std::string, std::vector<std::string>> fewer_units = {
        {"A", {"s0", "s1", "s2", "s3"}},
    };
    REQUIRE_FALSE(reg.matches_layout(fewer_units));
}
```

**Step 2: Run to verify failure**

```bash
make test && ./build/bin/helix-tests "[slot_registry][reorganize]"
```

Expected: FAIL (reorganize not implemented).

**Step 3: Commit**

```bash
git add tests/unit/test_slot_registry.cpp
git commit -m "test(ams): add SlotRegistry reorganize tests (red)"
```

---

### Task 1.5: Implement reorganize

**Files:**
- Modify: `src/printer/slot_registry.cpp`

**Step 1: Implement `reorganize()` and `matches_layout()`**

Key algorithm for `reorganize()`:
1. Stash existing `SlotEntry` data by `backend_name`
2. Sort unit names alphabetically
3. Clear `slots_`, `units_`
4. Iterate sorted units, for each slot name:
   - If in stash: restore entry, fix up `global_index`, `unit_index`, `info.slot_index`, `info.global_index`
   - If new: create default entry
   - Push to `slots_`
5. Call `rebuild_reverse_maps()`
6. Set `initialized_ = true`

`matches_layout()`:
1. Check unit count matches
2. For each unit in sorted order, check unit name and slot names match current state

**Step 2: Run tests**

```bash
make test && ./build/bin/helix-tests "[slot_registry]"
```

Expected: All PASS.

**Step 3: Commit**

```bash
git add src/printer/slot_registry.cpp
git commit -m "feat(ams): implement SlotRegistry reorganize with data preservation"
```

---

### Task 1.6: Write failing tests for tool mapping

**Files:**
- Modify: `tests/unit/test_slot_registry.cpp`

**Step 1: Add tool mapping tests**

```cpp
TEST_CASE("SlotRegistry tool mapping", "[slot_registry][tool_mapping]") {
    SlotRegistry reg;
    reg.initialize("Unit", {"s0", "s1", "s2", "s3"});

    SECTION("no mapping by default") {
        REQUIRE(reg.tool_for_slot(0) == -1);
        REQUIRE(reg.slot_for_tool(0) == -1);
    }

    SECTION("set and get single mapping") {
        reg.set_tool_mapping(2, 5);
        REQUIRE(reg.tool_for_slot(2) == 5);
        REQUIRE(reg.slot_for_tool(5) == 2);
        REQUIRE(reg.get(2)->info.mapped_tool == 5);
    }

    SECTION("remapping a tool clears previous") {
        reg.set_tool_mapping(0, 3);
        reg.set_tool_mapping(1, 3);  // T3 moves from slot 0 to slot 1
        REQUIRE(reg.slot_for_tool(3) == 1);
        REQUIRE(reg.tool_for_slot(0) == -1);  // cleared
        REQUIRE(reg.tool_for_slot(1) == 3);
    }

    SECTION("bulk set_tool_map") {
        // TTG-style: tool_to_slot[0]=2, tool_to_slot[1]=0, tool_to_slot[2]=3, tool_to_slot[3]=1
        reg.set_tool_map({2, 0, 3, 1});
        REQUIRE(reg.slot_for_tool(0) == 2);
        REQUIRE(reg.slot_for_tool(1) == 0);
        REQUIRE(reg.slot_for_tool(2) == 3);
        REQUIRE(reg.slot_for_tool(3) == 1);
        REQUIRE(reg.tool_for_slot(2) == 0);
        REQUIRE(reg.tool_for_slot(0) == 1);
    }

    SECTION("tool mapping survives reorganize") {
        reg.set_tool_mapping(1, 7);

        std::map<std::string, std::vector<std::string>> unit_map = {
            {"B", {"s2", "s3"}},
            {"A", {"s0", "s1"}},
        };
        reg.reorganize(unit_map);

        // s1 moved from index 1 to index 1 (A sorts first, s1 is second in A)
        // But let's verify via name lookup
        const auto* s1 = reg.find_by_name("s1");
        REQUIRE(s1->info.mapped_tool == 7);
        REQUIRE(reg.slot_for_tool(7) == s1->global_index);
    }

    SECTION("invalid indices") {
        REQUIRE(reg.tool_for_slot(-1) == -1);
        REQUIRE(reg.tool_for_slot(99) == -1);
        REQUIRE(reg.slot_for_tool(-1) == -1);
        REQUIRE(reg.slot_for_tool(99) == -1);
    }
}
```

**Step 2: Run to verify failure**

```bash
make test && ./build/bin/helix-tests "[slot_registry][tool_mapping]"
```

**Step 3: Commit**

```bash
git add tests/unit/test_slot_registry.cpp
git commit -m "test(ams): add SlotRegistry tool mapping tests (red)"
```

---

### Task 1.7: Implement tool mapping + endless spool

**Files:**
- Modify: `src/printer/slot_registry.cpp`

**Step 1: Implement tool mapping methods**

- `set_tool_mapping(global, tool)`: set `slots_[global].info.mapped_tool = tool`, update `tool_to_slot_` reverse map, clear previous holder of that tool number
- `set_tool_map(vec)`: clear all mapped_tool, then set each from the vector
- `tool_for_slot(global)`: return `slots_[global].info.mapped_tool`
- `slot_for_tool(tool)`: return `tool_to_slot_[tool]` if in range, else -1
- `rebuild_reverse_maps()` must also rebuild `tool_to_slot_` from `slots_[].info.mapped_tool`
- Endless spool: `backup_for_slot(global)` → `slots_[global].endless_spool_backup`, `set_backup(global, backup)` → set it

**Step 2: Run tests**

```bash
make test && ./build/bin/helix-tests "[slot_registry]"
```

Expected: All PASS.

**Step 3: Commit**

```bash
git add src/printer/slot_registry.cpp
git commit -m "feat(ams): implement SlotRegistry tool mapping and endless spool"
```

---

### Task 1.8: Write failing tests for build_system_info

**Files:**
- Modify: `tests/unit/test_slot_registry.cpp`

**Step 1: Add snapshot tests**

```cpp
TEST_CASE("SlotRegistry build_system_info", "[slot_registry][snapshot]") {
    SlotRegistry reg;

    std::vector<std::pair<std::string, std::vector<std::string>>> units = {
        {"Unit_A", {"s0", "s1"}},
        {"Unit_B", {"s2", "s3", "s4"}},
    };
    reg.initialize_units(units);

    // Set some state
    reg.get_mut(0)->info.color_rgb = 0xFF0000;
    reg.get_mut(0)->info.material = "PLA";
    reg.get_mut(0)->info.status = SlotStatus::AVAILABLE;
    reg.set_tool_mapping(0, 0);
    reg.set_tool_mapping(2, 1);

    auto info = reg.build_system_info();

    SECTION("total slots") {
        REQUIRE(info.total_slots == 5);
    }

    SECTION("unit structure") {
        REQUIRE(info.units.size() == 2);
        REQUIRE(info.units[0].name == "Unit_A");
        REQUIRE(info.units[0].slot_count == 2);
        REQUIRE(info.units[0].first_slot_global_index == 0);
        REQUIRE(info.units[1].name == "Unit_B");
        REQUIRE(info.units[1].slot_count == 3);
        REQUIRE(info.units[1].first_slot_global_index == 2);
    }

    SECTION("slot data in units") {
        REQUIRE(info.units[0].slots[0].color_rgb == 0xFF0000);
        REQUIRE(info.units[0].slots[0].material == "PLA");
        REQUIRE(info.units[0].slots[0].status == SlotStatus::AVAILABLE);
        REQUIRE(info.units[0].slots[0].global_index == 0);
        REQUIRE(info.units[0].slots[0].slot_index == 0);
    }

    SECTION("tool_to_slot_map") {
        REQUIRE(info.tool_to_slot_map.size() >= 2);
        REQUIRE(info.tool_to_slot_map[0] == 0);
        REQUIRE(info.tool_to_slot_map[1] == 2);
    }
}

TEST_CASE("SlotRegistry endless spool", "[slot_registry][endless_spool]") {
    SlotRegistry reg;
    reg.initialize("Unit", {"s0", "s1", "s2"});

    REQUIRE(reg.backup_for_slot(0) == -1);  // default

    reg.set_backup(0, 2);
    REQUIRE(reg.backup_for_slot(0) == 2);

    reg.set_backup(0, -1);  // clear
    REQUIRE(reg.backup_for_slot(0) == -1);

    SECTION("invalid index") {
        REQUIRE(reg.backup_for_slot(-1) == -1);
        REQUIRE(reg.backup_for_slot(99) == -1);
    }
}
```

**Step 2: Run to verify failure, commit**

```bash
make test && ./build/bin/helix-tests "[slot_registry][snapshot]"
git add tests/unit/test_slot_registry.cpp
git commit -m "test(ams): add SlotRegistry snapshot and endless spool tests (red)"
```

---

### Task 1.9: Implement build_system_info + endless spool

**Files:**
- Modify: `src/printer/slot_registry.cpp`

**Step 1: Implement `build_system_info()`**

Build `AmsSystemInfo` from registry state:
- Create `AmsUnit` for each `RegistryUnit`, populate with `SlotInfo` copies from `slots_`
- Set `total_slots`, `tool_to_slot_map`
- Do NOT set action, current_slot, capabilities — backend fills those in

**Step 2: Implement `backup_for_slot()`, `set_backup()`**

Simple delegation to `slots_[index].endless_spool_backup`.

**Step 3: Run ALL slot_registry tests**

```bash
make test && ./build/bin/helix-tests "[slot_registry]"
```

Expected: All PASS.

**Step 4: Commit**

```bash
git add src/printer/slot_registry.cpp
git commit -m "feat(ams): implement SlotRegistry build_system_info and endless spool"
```

---

### Task 1.10: Write regression test for the original mixed-topology bug

**Files:**
- Modify: `tests/unit/test_slot_registry.cpp`

**Step 1: Add the exact scenario from the user report**

```cpp
TEST_CASE("SlotRegistry mixed topology slot index correctness", "[slot_registry][regression]") {
    // Reproduces the production bug: 6-toolhead mixed system
    // Box Turtle (4 lanes PARALLEL) + 2 OpenAMS (4 lanes HUB each)
    // AFC discovery order may differ from alphabetical sort order

    SlotRegistry reg;

    // Simulate AFC discovery order (may NOT be alphabetical)
    std::vector<std::pair<std::string, std::vector<std::string>>> discovery_order = {
        {"OpenAMS AMS_1", {"lane4", "lane5", "lane6", "lane7"}},
        {"OpenAMS AMS_2", {"lane8", "lane9", "lane10", "lane11"}},
        {"Box_Turtle Turtle_1", {"lane0", "lane1", "lane2", "lane3"}},
    };
    reg.initialize_units(discovery_order);

    // Now reorganize (sorts alphabetically)
    std::map<std::string, std::vector<std::string>> unit_map = {
        {"Box_Turtle Turtle_1", {"lane0", "lane1", "lane2", "lane3"}},
        {"OpenAMS AMS_1", {"lane4", "lane5", "lane6", "lane7"}},
        {"OpenAMS AMS_2", {"lane8", "lane9", "lane10", "lane11"}},
    };
    reg.reorganize(unit_map);

    SECTION("Box Turtle sorts first") {
        REQUIRE(reg.unit(0).name == "Box_Turtle Turtle_1");
        REQUIRE(reg.unit(0).first_slot == 0);
    }

    SECTION("AMS_1 starts at global index 4") {
        REQUIRE(reg.unit(1).name == "OpenAMS AMS_1");
        REQUIRE(reg.unit(1).first_slot == 4);
        REQUIRE(reg.name_of(4) == "lane4");  // THE BUG: was returning wrong lane
    }

    SECTION("AMS_2 starts at global index 8") {
        REQUIRE(reg.unit(2).name == "OpenAMS AMS_2");
        REQUIRE(reg.unit(2).first_slot == 8);
        REQUIRE(reg.name_of(11) == "lane11");  // THE BUG: was returning wrong lane
    }

    SECTION("every slot resolves to correct lane name") {
        // This is the critical invariant that was violated
        for (int i = 0; i < 12; ++i) {
            std::string expected = "lane" + std::to_string(i);
            REQUIRE(reg.name_of(i) == expected);
        }
    }

    SECTION("reverse lookup also correct") {
        for (int i = 0; i < 12; ++i) {
            std::string name = "lane" + std::to_string(i);
            REQUIRE(reg.index_of(name) == i);
        }
    }
}
```

**Step 2: Run to verify it passes (should pass with existing implementation)**

```bash
make test && ./build/bin/helix-tests "[slot_registry][regression]"
```

**Step 3: Commit**

```bash
git add tests/unit/test_slot_registry.cpp
git commit -m "test(ams): add regression test for mixed-topology slot index bug"
```

---

### Task 1.11: Phase 1 Code Review

**STOP. Run full slot_registry test suite and review.**

```bash
./build/bin/helix-tests "[slot_registry]" -v
```

Review checklist:
- [ ] All tests pass
- [ ] No raw `slots_[]` access that could go out of bounds
- [ ] `rebuild_reverse_maps()` is called in every path that modifies `slots_`
- [ ] `reorganize()` preserves ALL fields of `SlotEntry` (sensors, endless_spool_backup, not just info)
- [ ] `matches_layout()` correctly detects structural changes
- [ ] No memory leaks (vectors properly cleared before rebuild)
- [ ] No UB (no dangling pointers from stash move)
- [ ] Code style matches project conventions (SPDX headers, namespace)
- [ ] `build_system_info()` produces valid `AmsUnit` with correct `first_slot_global_index`

Fix ALL medium+ issues found.

---

## Phase 2: AFC Backend Migration

### Task 2.1: Add `SlotRegistry slots_` to AFC backend, keep old members

**Files:**
- Modify: `include/ams_backend_afc.h`
- Modify: `src/printer/ams_backend_afc.cpp`

**Step 1: Add the new member alongside old ones**

In the header, add `#include "slot_registry.h"` and `SlotRegistry slots_;` member.
Do NOT remove old members yet — this is a parallel-run phase to verify correctness.

**Step 2: In `initialize_lanes()` (now renamed to `initialize_slots()`), also initialize the registry**

After the existing `lane_names_ = lane_names;` code, add:
```cpp
slots_.initialize("AFC Box Turtle", lane_names);
```

**Step 3: In `reorganize_units_from_map()`, also call registry reorganize**

After the existing rebuild logic, add:
```cpp
slots_.reorganize(unit_slot_map_converted);
```

Where `unit_slot_map_converted` is the same map but as `std::map` (sorted). Log if the two produce different results.

**Step 4: Verify existing tests still pass**

```bash
make test && ./build/bin/helix-tests "[ams][afc]" "[ams][mock][mixed]"
```

**Step 5: Commit**

```bash
git add include/ams_backend_afc.h src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams/afc): add SlotRegistry alongside legacy data structures"
```

---

### Task 2.2: Migrate AFC slot lookup to registry

**Files:**
- Modify: `src/printer/ams_backend_afc.cpp`

**Step 1: Replace `get_lane_name()` with `slots_.name_of()`**

Remove the `get_lane_name()` method entirely. Replace ALL callers:
- `load_filament()` (line ~1885)
- `select_slot()` (line ~1933)
- `eject_lane()` (line ~2046)
- `reset_lane()` (line ~2019)
- `set_slot_info()` (line ~2129)
- `set_endless_spool_backup()` (line ~2345-2347)

**Step 2: Replace `lane_name_to_index_` lookups with `slots_.index_of()`**

In `parse_afc_state()`, `parse_afc_stepper()`, `parse_afc_extruder()`, `parse_afc_buffer()`, `compute_filament_segment_unlocked()`.

**Step 3: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 4: Commit**

```bash
git add src/printer/ams_backend_afc.cpp include/ams_backend_afc.h
git commit -m "refactor(ams/afc): migrate slot lookups to SlotRegistry"
```

---

### Task 2.3: Migrate AFC sensor state to registry

**Files:**
- Modify: `src/printer/ams_backend_afc.cpp`
- Modify: `include/ams_backend_afc.h`

**Step 1: Replace all `lane_sensors_[slot_index]` access**

In `parse_afc_stepper()` (line ~887): use `slots_.get_mut(slot_index)->sensors` instead of `lane_sensors_[slot_index]`.

In `compute_filament_segment_unlocked()` and `get_slot_filament_segment()`: use `slots_.get(index)->sensors`.

**Step 2: Remove `lane_sensors_` member and `LaneSensors` struct from header**

The `LaneSensors` struct is replaced by `SlotSensors` in `slot_registry.h`.

**Step 3: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 4: Commit**

```bash
git add include/ams_backend_afc.h src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams/afc): migrate sensor state to SlotRegistry"
```

---

### Task 2.4: Migrate AFC slot info + endless spool to registry

**Files:**
- Modify: `src/printer/ams_backend_afc.cpp`
- Modify: `include/ams_backend_afc.h`

**Step 1: Replace `system_info_.get_slot_global(slot_index)` with `slots_.get_mut(slot_index)->info`**

In `parse_afc_stepper()`, `parse_afc_state()`, `parse_lane_data()`, `parse_afc_buffer()`.

**Step 2: Replace `endless_spool_configs_[slot_index]` with `slots_.backup_for_slot()` / `slots_.set_backup()`**

In `parse_afc_stepper()` (line ~1014-1026) and `set_endless_spool_backup()`.

**Step 3: Replace `get_system_info()` to use `slots_.build_system_info()`**

```cpp
AmsSystemInfo AmsBackendAfc::get_system_info() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto info = slots_.build_system_info();
    // Fill in non-slot fields
    info.action = system_info_.action;
    info.operation_detail = system_info_.operation_detail;
    info.current_slot = system_info_.current_slot;
    info.current_tool = system_info_.current_tool;
    info.filament_loaded = system_info_.filament_loaded;
    info.supports_bypass = system_info_.supports_bypass;
    info.bypass_active = system_info_.bypass_active;
    info.type = system_info_.type;
    info.type_name = system_info_.type_name;
    info.firmware_version = system_info_.firmware_version;
    // ... all capability flags
    return info;
}
```

**Step 4: Remove `endless_spool_configs_` member from header**

**Step 5: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 6: Commit**

```bash
git add include/ams_backend_afc.h src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams/afc): migrate slot info and endless spool to SlotRegistry"
```

---

### Task 2.5: Migrate AFC tool mapping to registry

**Files:**
- Modify: `src/printer/ams_backend_afc.cpp`

**Step 1: Replace `system_info_.tool_to_slot_map` updates in `parse_afc_stepper()`**

Where the code currently does:
```cpp
system_info_.tool_to_slot_map.resize(tool_num + 1, -1);
system_info_.tool_to_slot_map[tool_num] = slot_index;
```

Replace with:
```cpp
slots_.set_tool_mapping(slot_index, tool_num);
```

**Step 2: Replace `get_tool_mapping()` to read from registry**

**Step 3: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams][tool_mapping]" "[ams][afc]"
```

**Step 4: Commit**

```bash
git add src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams/afc): migrate tool mapping to SlotRegistry"
```

---

### Task 2.6: Remove legacy parallel structures from AFC

**Files:**
- Modify: `include/ams_backend_afc.h`
- Modify: `src/printer/ams_backend_afc.cpp`

**Step 1: Remove from header:**
- `lane_names_` (line 524)
- `lane_name_to_index_` (line 527)
- `lane_sensors_` (line 546) and `LaneSensors` struct (lines 538-545)
- `lanes_initialized_` (line 523)
- `endless_spool_configs_` (lines 584-585)

**Step 2: Replace `lanes_initialized_` with `slots_.is_initialized()` everywhere**

**Step 3: Replace `initialize_lanes()` with `initialize_slots()` that only calls `slots_.initialize()`**

The old method's `system_info_` population is no longer needed — `build_system_info()` handles it.

**Step 4: Simplify `reorganize_units_from_map()` to just call `slots_.reorganize()`**

The entire ~100-line method body reduces to:
1. Convert `unit_lane_map_` to `std::map`
2. Check `slots_.matches_layout(map)` — skip if unchanged
3. Call `slots_.reorganize(map)`
4. Log the result

**Step 5: Remove `get_lane_name()` method declaration from header**

**Step 6: Run ALL tests**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 7: Commit**

```bash
git add include/ams_backend_afc.h src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams/afc): remove legacy parallel data structures"
```

---

### Task 2.7: Phase 2 Code Review

**STOP. Full review of AFC backend.**

```bash
./build/bin/helix-tests "[ams][afc]" "[ams][mock][mixed]" "[ams][tool_mapping]" "[ams][endless_spool]" -v
```

Review checklist:
- [ ] All AMS tests pass
- [ ] No remaining references to `lane_names_`, `lane_name_to_index_`, `lane_sensors_`, `endless_spool_configs_`
- [ ] No direct `system_info_.units[].slots[]` mutation (all goes through registry)
- [ ] `system_info_` is only READ from (via `build_system_info()`), never mutated for slot data
- [ ] Non-slot fields on `system_info_` (action, current_slot, capabilities) still properly maintained
- [ ] `parse_afc_stepper()` uses `slots_.find_by_name_mut()` — no index arithmetic
- [ ] `reorganize_units_from_map()` uses `matches_layout()` to skip no-op rebuilds
- [ ] `reorganize_units_from_unit_info()` correctly builds map and delegates to registry
- [ ] Thread safety: all `slots_` access under `mutex_`
- [ ] Hub sensor propagation still works (uses `unit_infos_`, not registry)
- [ ] Validation: all slot index checks use `slots_.is_valid_index()`

Fix ALL medium+ issues.

---

## Phase 3: Happy Hare Backend Migration

### Task 3.1: Add `SlotRegistry slots_` to Happy Hare backend

**Files:**
- Modify: `include/ams_backend_happy_hare.h`
- Modify: `src/printer/ams_backend_happy_hare.cpp`

**Step 1: Add `#include "slot_registry.h"` and `SlotRegistry slots_;` member**

**Step 2: In `initialize_gates()` (rename to `initialize_slots()`), populate registry**

Build unit→slot_names pairs from the gate count and num_units_ division:
```cpp
std::vector<std::pair<std::string, std::vector<std::string>>> units;
int gates_per_unit = gate_count / num_units_;
int remainder = gate_count % num_units_;
int offset = 0;
for (int u = 0; u < num_units_; ++u) {
    int count = gates_per_unit + (u == num_units_ - 1 ? remainder : 0);
    std::vector<std::string> names;
    for (int g = 0; g < count; ++g) {
        names.push_back(std::to_string(offset + g));  // "0", "1", ...
    }
    std::string unit_name = "Unit " + std::to_string(u + 1);
    if (num_units_ == 1) unit_name = "MMU";
    units.push_back({unit_name, names});
}
slots_.initialize_units(units);
```

**Step 3: Run existing HH tests**

```bash
make test && ./build/bin/helix-tests "[ams][happy_hare]"
```

**Step 4: Commit**

```bash
git add include/ams_backend_happy_hare.h src/printer/ams_backend_happy_hare.cpp
git commit -m "refactor(ams/hh): add SlotRegistry alongside legacy state"
```

---

### Task 3.2: Migrate Happy Hare state access to registry

**Files:**
- Modify: `src/printer/ams_backend_happy_hare.cpp`

**Step 1: Replace `system_info_.get_slot_global(gate_index)` with `slots_.get_mut(gate_index)->info`**

In `parse_mmu_state()` — for gate_status, gate_color_rgb, gate_material, ttg_map, endless_spool_groups, sensors, error state.

**Step 2: Replace `gate_sensors_[idx]` with `slots_.get_mut(idx)->sensors`**

Map `GateSensorState` fields to `SlotSensors`:
- `has_pre_gate_sensor` → `sensors.has_pre_gate_sensor`
- `pre_gate_triggered` → `sensors.pre_gate_triggered`

**Step 3: Replace TTG map handling with `slots_.set_tool_map()`**

In the `ttg_map` parsing block, instead of manually updating `system_info_.tool_to_slot_map` and iterating units to set `mapped_tool`:
```cpp
slots_.set_tool_map(ttg_vec);
```

**Step 4: Replace `get_system_info()` to use `slots_.build_system_info()`**

Same pattern as AFC: build from registry, fill in non-slot fields.

**Step 5: Replace `errored_slot_` tracking**

Instead of `errored_slot_` int, find the errored slot by iterating:
```cpp
// On error clear:
for (int i = 0; i < slots_.slot_count(); ++i) {
    auto* entry = slots_.get_mut(i);
    if (entry->info.error.has_value()) {
        entry->info.error.reset();
    }
}
```

**Step 6: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams][happy_hare]"
```

**Step 7: Commit**

```bash
git add src/printer/ams_backend_happy_hare.cpp
git commit -m "refactor(ams/hh): migrate state access to SlotRegistry"
```

---

### Task 3.3: Remove legacy state from Happy Hare

**Files:**
- Modify: `include/ams_backend_happy_hare.h`
- Modify: `src/printer/ams_backend_happy_hare.cpp`

**Step 1: Remove from header:**
- `GateSensorState` struct (lines 270-273) and `gate_sensors_` (line 274)
- `gates_initialized_` (line 262) — replaced by `slots_.is_initialized()`
- `errored_slot_` (line 278) — replaced by iteration

**Step 2: Run tests**

```bash
make test && ./build/bin/helix-tests "[ams][happy_hare]"
```

**Step 3: Commit**

```bash
git add include/ams_backend_happy_hare.h src/printer/ams_backend_happy_hare.cpp
git commit -m "refactor(ams/hh): remove legacy parallel data structures"
```

---

### Task 3.4: Phase 3 Code Review

**STOP. Full review of Happy Hare backend.**

```bash
./build/bin/helix-tests "[ams][happy_hare]" -v
```

Review checklist:
- [ ] All HH tests pass
- [ ] No remaining references to `gate_sensors_`, `errored_slot_`, `gates_initialized_`
- [ ] Sensor access uses `slots_.get(i)->sensors.has_pre_gate_sensor` / `.pre_gate_triggered`
- [ ] TTG map uses `slots_.set_tool_map()` — no manual `tool_to_slot_map` manipulation
- [ ] `get_system_info()` uses `slots_.build_system_info()` + non-slot fields
- [ ] Error state management works without `errored_slot_` tracker
- [ ] `initialize_slots()` correctly handles `num_units_ > 1`
- [ ] `filament_pos_` path segment computation still works with registry sensor access

Fix ALL medium+ issues.

---

## Phase 4: Mock Backend Migration

### Task 4.1: Migrate mock backend to SlotRegistry

**Files:**
- Modify: `include/ams_backend_mock.h`
- Modify: `src/printer/ams_backend_mock.cpp`

**Step 1: Add `SlotRegistry slots_` member**

**Step 2: Migrate `init_mock_data()` to populate registry**

Instead of building `system_info_.units[].slots[]` directly, build the unit→slot_names map and call `slots_.initialize_units()` or `slots_.initialize()`. Then set colors/materials/status through `slots_.get_mut()`.

**Step 3: Migrate `set_mixed_topology_mode()` and `set_afc_mode()` etc.**

These currently build `system_info_` directly. Change to build through the registry.

**Step 4: Remove `endless_spool_configs_` from mock (use registry)**

**Step 5: Migrate `get_system_info()` to `slots_.build_system_info()` + non-slot fields**

**Step 6: Migrate all `system_info_.get_slot_global()` reads/writes to registry**

For `force_slot_status()`, `simulate_error()`, etc.

**Step 7: Run ALL tests (mock is used by almost everything)**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 8: Commit**

```bash
git add include/ams_backend_mock.h src/printer/ams_backend_mock.cpp
git commit -m "refactor(ams/mock): migrate to SlotRegistry"
```

---

### Task 4.2: Phase 4 Code Review

**STOP. Full review of mock backend + full test suite.**

```bash
make test && ./build/bin/helix-tests "[ams]" -v
```

Review checklist:
- [ ] ALL AMS tests pass (mock, AFC, HH, mixed, tool mapping, endless spool, etc.)
- [ ] Mock's `system_info_` is only used for non-slot fields
- [ ] `set_mixed_topology_mode()` uses registry for slot setup
- [ ] `force_slot_status()` goes through registry
- [ ] All 28 mixed topology tests still pass
- [ ] Mock test helpers (friend classes) still have access to what they need

Fix ALL medium+ issues.

---

## Phase 5: Naming Cleanup

### Task 5.1: Rename non-backend "lane" references to "slot"

**Files:**
- Modify: `include/ams_backend_afc.h` — rename `initialize_lanes()` → `initialize_slots()` (method name)
- Modify: `src/printer/ams_backend_afc.cpp` — update all callsites
- Modify: `include/ams_backend_happy_hare.h` — rename `initialize_gates()` → `initialize_slots()`
- Modify: `src/printer/ams_backend_happy_hare.cpp` — update all callsites
- Modify: any test files referencing these method names

**Rule:** Keep "lane" in G-code strings, Klipper object names, AFC protocol-level code. Rename in our abstraction layer.

**Specific renames:**
- `initialize_lanes()` → `initialize_slots()` (AFC)
- `initialize_gates()` → `initialize_slots()` (HH)
- `parse_lane_data()` → keep (AFC protocol-specific)
- `reorganize_units_from_map()` → simplify name to `reorganize_slots()` (it's just calling registry now)
- `reorganize_units_from_unit_info()` → `rebuild_unit_map_from_klipper()` or similar (it builds the map from Klipper objects, then calls reorganize)

**Step 1: Do the renames**

**Step 2: Run ALL tests**

```bash
make test && ./build/bin/helix-tests "[ams]"
```

**Step 3: Commit**

```bash
git add -A  # careful: only staged files
git commit -m "refactor(ams): rename lane/gate methods to slot in abstraction layer"
```

---

### Task 5.2: Remove dead code and update docs

**Files:**
- Modify: `docs/devel/FILAMENT_MANAGEMENT.md` — update architecture section, key files table, data flow, add SlotRegistry documentation
- Remove: any TODO/FIXME comments that reference the old parallel structures
- Clean up: unused includes, forward declarations

**Step 1: Update FILAMENT_MANAGEMENT.md**

Add a "SlotRegistry" section explaining the single-source-of-truth architecture. Update the Key Files table to include `slot_registry.h`. Update the Data Flow section.

**Step 2: Run tests one final time**

```bash
make test && ./build/bin/helix-tests "[ams]" -v
```

**Step 3: Commit**

```bash
git add docs/devel/FILAMENT_MANAGEMENT.md
git commit -m "docs(ams): update FILAMENT_MANAGEMENT.md for SlotRegistry architecture"
```

---

## Phase 6: Final Integration Review

### Task 6.1: Run full test suite

```bash
make test-run
```

ALL tests must pass, not just `[ams]` tagged ones. The mock backend is used by non-AMS tests too.

### Task 6.2: Final code review

Review the ENTIRE diff from start to finish:

```bash
git diff main..HEAD --stat
git diff main..HEAD
```

Review checklist:
- [ ] Full test suite passes
- [ ] No remaining parallel slot-indexed structures outside SlotRegistry
- [ ] All three backends (AFC, HH, Mock) use SlotRegistry consistently
- [ ] No "lane" naming in shared/abstraction code (only in protocol-specific code)
- [ ] SlotSensors properly replaces both LaneSensors and GateSensorState
- [ ] Design doc matches implementation
- [ ] FILAMENT_MANAGEMENT.md updated
- [ ] No accidental changes to unrelated code
- [ ] No debug logging left behind
- [ ] SPDX headers on all new files

### Task 6.3: Squash/organize commits for merge

Organize the commit history into logical chunks suitable for main branch:
1. `feat(ams): add SlotRegistry — unified slot state management`
2. `refactor(ams/afc): migrate AFC backend to SlotRegistry`
3. `refactor(ams/hh): migrate Happy Hare backend to SlotRegistry`
4. `refactor(ams/mock): migrate Mock backend to SlotRegistry`
5. `refactor(ams): rename lane/gate to slot in abstraction layer`
6. `docs(ams): update filament management docs for SlotRegistry`
