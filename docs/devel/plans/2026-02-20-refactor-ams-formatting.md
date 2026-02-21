# AMS Backend Base Class & Formatting Consolidation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extract shared code from 3 AMS subscription-based backends into a base class, and consolidate duplicated temperature formatting into a single source of truth.

**Architecture:** Two independent refactorings executed sequentially. AMS refactoring extracts ~70% of identical code from AFC/HappyHare/ToolChanger into `AmsSubscriptionBackend` base class. Formatting consolidation merges duplicate temperature functions from `format_utils` into `ui_temperature_utils` as the single temperature formatting authority.

**Tech Stack:** C++17, LVGL 9, Catch2, spdlog

**Worktree:** `.worktrees/refactor-2026-02` (branch: `feature/refactor-2026-02`)

---

## Part 1: AMS Backend Base Class

### Background

AFC, HappyHare, and ToolChanger backends share identical implementations for: `emit_event()`, `execute_gcode()`, `check_preconditions()`, `stop()`, `release_subscriptions()`, `is_running()`, `set_event_callback()`, `get_current_tool()`, `get_current_slot()`, `is_filament_loaded()`, `get_current_action()`, and the subscription setup portion of `start()`.

**Mutex note:** AFC uses `std::recursive_mutex` (introduced in `8f726504` as a quick fix for callback re-entrancy). The proper fix — releasing the lock before `emit_event()` — was applied to HappyHare/ToolChanger in `f54d1ceb`. The base class will use `std::mutex` with structured lock scoping, and AFC will be migrated to this pattern.

**ValgACE:** Ignored (bit-rotten, polling-based). **Mock:** Ignored (completely different architecture).

### Task 1: Characterization Tests for Extractable Methods

**Files:**
- Create: `tests/unit/test_ams_backend_base_char.cpp`

**Step 1: Write characterization tests**

These tests capture current behavior of the methods we're extracting. They test via the concrete backends (AFC, HappyHare, ToolChanger) to verify behavior is preserved after extraction.

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include "ams_backend_afc.h"
#include "ams_backend_happy_hare.h"
#include "ams_backend_toolchanger.h"

using namespace helix::printer;

// --- emit_event ---

TEST_CASE("AMS backends: emit_event calls registered callback", "[ams][characterization]") {
    SECTION("AFC") {
        AmsBackendAfc backend(nullptr, nullptr);
        std::string received_event;
        std::string received_data;
        backend.set_event_callback([&](const std::string& e, const std::string& d) {
            received_event = e;
            received_data = d;
        });
        backend.emit_event(IAmsBackend::EVENT_STATE_CHANGED, "test_data");
        REQUIRE(received_event == IAmsBackend::EVENT_STATE_CHANGED);
        REQUIRE(received_data == "test_data");
    }
    SECTION("HappyHare") {
        AmsBackendHappyHare backend(nullptr, nullptr);
        std::string received_event;
        backend.set_event_callback([&](const std::string& e, const std::string&) {
            received_event = e;
        });
        backend.emit_event(IAmsBackend::EVENT_STATE_CHANGED, "");
        REQUIRE(received_event == IAmsBackend::EVENT_STATE_CHANGED);
    }
    SECTION("ToolChanger") {
        AmsBackendToolChanger backend(nullptr, nullptr);
        std::string received_event;
        backend.set_event_callback([&](const std::string& e, const std::string&) {
            received_event = e;
        });
        backend.emit_event(IAmsBackend::EVENT_STATE_CHANGED, "");
        REQUIRE(received_event == IAmsBackend::EVENT_STATE_CHANGED);
    }
}

TEST_CASE("AMS backends: emit_event with no callback is safe", "[ams][characterization]") {
    AmsBackendAfc backend(nullptr, nullptr);
    REQUIRE_NOTHROW(backend.emit_event(IAmsBackend::EVENT_STATE_CHANGED, ""));
}

// --- check_preconditions ---

TEST_CASE("AMS backends: check_preconditions when not running", "[ams][characterization]") {
    SECTION("AFC") {
        AmsBackendAfc backend(nullptr, nullptr);
        auto err = backend.check_preconditions();
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
    SECTION("HappyHare") {
        AmsBackendHappyHare backend(nullptr, nullptr);
        auto err = backend.check_preconditions();
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
    SECTION("ToolChanger") {
        AmsBackendToolChanger backend(nullptr, nullptr);
        auto err = backend.check_preconditions();
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
}

// --- execute_gcode ---

TEST_CASE("AMS backends: execute_gcode without API returns error", "[ams][characterization]") {
    SECTION("AFC") {
        AmsBackendAfc backend(nullptr, nullptr);
        auto err = backend.execute_gcode("G28");
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
    SECTION("HappyHare") {
        AmsBackendHappyHare backend(nullptr, nullptr);
        auto err = backend.execute_gcode("G28");
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
    SECTION("ToolChanger") {
        AmsBackendToolChanger backend(nullptr, nullptr);
        auto err = backend.execute_gcode("G28");
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
}

// --- State query defaults ---

TEST_CASE("AMS backends: default state after construction", "[ams][characterization]") {
    SECTION("AFC") {
        AmsBackendAfc backend(nullptr, nullptr);
        REQUIRE(backend.get_type() == AmsType::AFC);
        REQUIRE(backend.get_current_tool() == -1);
        REQUIRE(backend.get_current_slot() == -1);
        REQUIRE_FALSE(backend.is_filament_loaded());
        REQUIRE(backend.get_current_action() == AmsAction::IDLE);
        REQUIRE_FALSE(backend.is_running());
    }
    SECTION("HappyHare") {
        AmsBackendHappyHare backend(nullptr, nullptr);
        REQUIRE(backend.get_type() == AmsType::HAPPY_HARE);
        REQUIRE(backend.get_current_tool() == -1);
        REQUIRE(backend.get_current_slot() == -1);
        REQUIRE_FALSE(backend.is_filament_loaded());
        REQUIRE(backend.get_current_action() == AmsAction::IDLE);
        REQUIRE_FALSE(backend.is_running());
    }
    SECTION("ToolChanger") {
        AmsBackendToolChanger backend(nullptr, nullptr);
        REQUIRE(backend.get_type() == AmsType::TOOL_CHANGER);
        REQUIRE(backend.get_current_tool() == -1);
        REQUIRE(backend.get_current_slot() == -1);
        REQUIRE_FALSE(backend.is_filament_loaded());
        REQUIRE(backend.get_current_action() == AmsAction::IDLE);
        REQUIRE_FALSE(backend.is_running());
    }
}

// --- is_running / stop ---

TEST_CASE("AMS backends: stop when not running is safe", "[ams][characterization]") {
    AmsBackendAfc afc(nullptr, nullptr);
    REQUIRE_NOTHROW(afc.stop());
    AmsBackendHappyHare hh(nullptr, nullptr);
    REQUIRE_NOTHROW(hh.stop());
    AmsBackendToolChanger tc(nullptr, nullptr);
    REQUIRE_NOTHROW(tc.stop());
}

// --- start without client/api ---

TEST_CASE("AMS backends: start without client returns not_connected", "[ams][characterization]") {
    SECTION("AFC") {
        AmsBackendAfc backend(nullptr, nullptr);
        auto err = backend.start();
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
    SECTION("HappyHare") {
        AmsBackendHappyHare backend(nullptr, nullptr);
        auto err = backend.start();
        REQUIRE(err.has_error());
        REQUIRE(err.get_type() == AmsErrorType::NOT_CONNECTED);
    }
}
```

**Step 2: Build and run characterization tests**

Run: `make test && ./build/bin/helix-tests "[ams][characterization]"`
Expected: All PASS (these capture existing behavior)

**Step 3: Commit characterization tests**

```
git add tests/unit/test_ams_backend_base_char.cpp
git commit -m "test(ams): add characterization tests for extractable backend methods"
```

---

### Task 2: Create AmsSubscriptionBackend Base Class

**Files:**
- Create: `include/ams_subscription_backend.h`
- Create: `src/printer/ams_subscription_backend.cpp`

**Step 1: Write the base class header**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ams_backend.h"
#include "moonraker_api.h"
#include "moonraker_client.h"
#include "subscription_guard.h"

#include <atomic>
#include <mutex>
#include <spdlog/spdlog.h>

namespace helix::printer {

/// Base class for AMS backends that use Moonraker subscription-based status updates.
/// Extracts common lifecycle, event, and state query logic from AFC/HappyHare/ToolChanger.
///
/// Derived classes MUST implement:
///   - get_type() - return backend-specific AmsType
///   - handle_status_update() - parse backend-specific JSON notifications
///   - on_started() - post-start initialization (version detection, config loading, etc.)
///   - backend_log_tag() - return log prefix like "[AMS AFC]"
///
/// Derived classes MAY override:
///   - get_system_info() - if they need to build info from SlotRegistry
///   - validate_slot_index() - if they need custom validation
///   - additional_start_checks() - for extra preconditions before subscribing
class AmsSubscriptionBackend : public IAmsBackend {
public:
    AmsSubscriptionBackend(MoonrakerAPI* api, helix::MoonrakerClient* client);
    ~AmsSubscriptionBackend() override;

    // --- Lifecycle (final — derived classes use hooks instead) ---
    AmsError start() final;
    void stop() final;
    void release_subscriptions() final;
    bool is_running() const final;

    // --- Event system (final) ---
    void set_event_callback(EventCallback callback) final;
    void emit_event(const std::string& event, const std::string& data = "") final;

    // --- State queries (final) ---
    AmsAction get_current_action() const final;
    int get_current_tool() const final;
    int get_current_slot() const final;
    bool is_filament_loaded() const final;

    // --- Shared utilities (final) ---
    AmsError check_preconditions() const final;
    AmsError execute_gcode(const std::string& gcode) final;

protected:
    // --- Hooks for derived classes ---

    /// Called after subscription is established and running_ is set.
    /// Lock is NOT held. Safe to call emit_event().
    virtual void on_started() {}

    /// Called before stop() releases the subscription.
    /// Lock IS held.
    virtual void on_stopping() {}

    /// Extra checks before subscribing (e.g., ToolChanger requires tools discovered).
    /// Return error to abort start. Lock IS held.
    virtual AmsError additional_start_checks() { return AmsErrorHelper::success(); }

    /// Handle incoming Moonraker status notification. Called from background thread.
    virtual void handle_status_update(const nlohmann::json& notification) = 0;

    /// Return log tag like "[AMS AFC]" for log messages.
    virtual const char* backend_log_tag() const = 0;

    // --- Protected state for derived classes ---
    MoonrakerAPI* api_;
    helix::MoonrakerClient* client_;
    mutable std::mutex mutex_;
    AmsSystemInfo system_info_;
    std::atomic<bool> running_{false};

private:
    EventCallback event_callback_;
    SubscriptionGuard subscription_;
};

} // namespace helix::printer
```

**Step 2: Write the base class implementation**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ams_subscription_backend.h"

namespace helix::printer {

AmsSubscriptionBackend::AmsSubscriptionBackend(MoonrakerAPI* api, helix::MoonrakerClient* client)
    : api_(api), client_(client) {
    // Common defaults — derived constructors set type-specific fields
    system_info_.version = "unknown";
    system_info_.current_tool = -1;
    system_info_.current_slot = -1;
    system_info_.filament_loaded = false;
    system_info_.action = AmsAction::IDLE;
    system_info_.total_slots = 0;
}

AmsSubscriptionBackend::~AmsSubscriptionBackend() {
    // Release without unsubscribe — MoonrakerClient may already be destroyed
    subscription_.release();
}

AmsError AmsSubscriptionBackend::start() {
    bool should_emit = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_) {
            return AmsErrorHelper::success();
        }

        if (!client_) {
            spdlog::error("{} Cannot start: MoonrakerClient is null", backend_log_tag());
            return AmsErrorHelper::not_connected("MoonrakerClient not provided");
        }

        if (!api_) {
            spdlog::error("{} Cannot start: MoonrakerAPI is null", backend_log_tag());
            return AmsErrorHelper::not_connected("MoonrakerAPI not provided");
        }

        // Derived class extra checks (e.g., ToolChanger requires tools discovered)
        auto extra_check = additional_start_checks();
        if (extra_check.has_error()) {
            return extra_check;
        }

        SubscriptionId id = client_->register_notify_update(
            [this](const nlohmann::json& notification) { handle_status_update(notification); });

        if (id == INVALID_SUBSCRIPTION_ID) {
            spdlog::error("{} Failed to register for status updates", backend_log_tag());
            return AmsErrorHelper::not_connected("Failed to subscribe to Moonraker updates");
        }

        subscription_ = SubscriptionGuard(client_, id);
        running_ = true;
        spdlog::info("{} Backend started, subscription ID: {}", backend_log_tag(), id);
        should_emit = true;
    }
    // Lock released before emit (prevents recursive_mutex need)
    if (should_emit) {
        emit_event(EVENT_STATE_CHANGED);
    }

    // Derived class post-start work (version detection, config loading, etc.)
    on_started();

    return AmsErrorHelper::success();
}

void AmsSubscriptionBackend::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }
    on_stopping();
    subscription_.reset();
    running_ = false;
    spdlog::info("{} Backend stopped", backend_log_tag());
}

void AmsSubscriptionBackend::release_subscriptions() {
    subscription_.release();
}

bool AmsSubscriptionBackend::is_running() const {
    return running_;
}

void AmsSubscriptionBackend::set_event_callback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = std::move(callback);
}

void AmsSubscriptionBackend::emit_event(const std::string& event, const std::string& data) {
    EventCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = event_callback_;
    }
    if (cb) {
        cb(event, data);
    }
}

AmsAction AmsSubscriptionBackend::get_current_action() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return system_info_.action;
}

int AmsSubscriptionBackend::get_current_tool() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return system_info_.current_tool;
}

int AmsSubscriptionBackend::get_current_slot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return system_info_.current_slot;
}

bool AmsSubscriptionBackend::is_filament_loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return system_info_.filament_loaded;
}

AmsError AmsSubscriptionBackend::check_preconditions() const {
    if (!running_) {
        return AmsErrorHelper::not_connected(
            std::string(backend_log_tag()) + " backend not started");
    }
    if (system_info_.is_busy()) {
        return AmsErrorHelper::busy(ams_action_to_string(system_info_.action));
    }
    return AmsErrorHelper::success();
}

AmsError AmsSubscriptionBackend::execute_gcode(const std::string& gcode) {
    if (!api_) {
        return AmsErrorHelper::not_connected("MoonrakerAPI not available");
    }
    const char* tag = backend_log_tag();
    spdlog::info("{} Executing G-code: {}", tag, gcode);
    api_->execute_gcode(
        gcode,
        [tag]() { spdlog::debug("{} G-code executed successfully", tag); },
        [tag, gcode](const MoonrakerError& err) {
            if (err.type == MoonrakerErrorType::TIMEOUT) {
                spdlog::warn("{} G-code response timed out (may still be running): {}", tag, gcode);
            } else {
                spdlog::error("{} G-code failed: {} - {}", tag, gcode, err.message);
            }
        },
        MoonrakerAPI::AMS_OPERATION_TIMEOUT_MS);
    return AmsErrorHelper::success();
}

} // namespace helix::printer
```

**Step 3: Build to verify base class compiles**

Run: `make -j`
Expected: Clean build (base class not yet used by anything)

**Step 4: Commit base class**

```
git add include/ams_subscription_backend.h src/printer/ams_subscription_backend.cpp
git commit -m "refactor(ams): add AmsSubscriptionBackend base class for shared lifecycle"
```

---

### Task 3: Migrate AFC to AmsSubscriptionBackend

**Files:**
- Modify: `include/ams_backend_afc.h`
- Modify: `src/printer/ams_backend_afc.cpp`

**Step 1: Update AFC header**

Change class declaration from `IAmsBackend` to `AmsSubscriptionBackend`. Remove members that are now in the base class:
- Remove: `api_`, `client_`, `mutex_` (now `std::mutex`, not `recursive_mutex`), `running_`, `event_callback_`, `subscription_`, and the common `system_info_` default init fields
- Remove declarations for: `start()`, `stop()`, `release_subscriptions()`, `is_running()`, `set_event_callback()`, `emit_event()`, `get_current_action()`, `get_current_tool()`, `get_current_slot()`, `is_filament_loaded()`, `check_preconditions()`, `execute_gcode()`
- Add: `on_started()` override, `backend_log_tag()` override, `additional_start_checks()` if needed
- Keep: `handle_status_update()` override, `get_system_info()` override, `get_type()` override, all AFC-specific methods

**Key mutex migration:** Change all `std::lock_guard<std::recursive_mutex>` to `std::lock_guard<std::mutex>` throughout the AFC implementation. Review every locked section to ensure no re-entrant locking paths exist. The base class `emit_event()` already releases the lock before calling the callback, so the re-entrancy that motivated `recursive_mutex` is eliminated.

**Step 2: Update AFC implementation**

- Remove implementations of all extracted methods
- Move AFC's post-start logic (`detect_afc_version()`, `initialize_slots()`, `load_afc_configs()`) into `on_started()` override
- Constructor calls base, then sets AFC-specific capability fields
- `get_system_info()` stays as override (uses SlotRegistry)
- `get_type()` stays as override (returns `AmsType::AFC`)

**Step 3: Build and run ALL AMS tests**

Run: `make test && ./build/bin/helix-tests "[ams]"`
Expected: All existing tests pass, all characterization tests pass

**Step 4: Commit**

```
git add include/ams_backend_afc.h src/printer/ams_backend_afc.cpp
git commit -m "refactor(ams): migrate AFC backend to AmsSubscriptionBackend base class"
```

---

### Task 4: Migrate HappyHare to AmsSubscriptionBackend

Same pattern as Task 3 but for HappyHare:

**Files:**
- Modify: `include/ams_backend_happy_hare.h`
- Modify: `src/printer/ams_backend_happy_hare.cpp`

**Step 1: Update header** — inherit from `AmsSubscriptionBackend`, remove extracted members/methods.

**Step 2: Update implementation**
- Move `query_tip_method_from_config()` call into `on_started()` override
- Remove all extracted method implementations
- HappyHare already uses `std::mutex` so no mutex migration needed

**Step 3: Build and run ALL AMS tests**

Run: `make test && ./build/bin/helix-tests "[ams]"`
Expected: All pass

**Step 4: Commit**

```
git add include/ams_backend_happy_hare.h src/printer/ams_backend_happy_hare.cpp
git commit -m "refactor(ams): migrate HappyHare backend to AmsSubscriptionBackend base class"
```

---

### Task 5: Migrate ToolChanger to AmsSubscriptionBackend

Same pattern. ToolChanger has one extra wrinkle: it requires `tool_names_` to be non-empty before start.

**Files:**
- Modify: `include/ams_backend_toolchanger.h`
- Modify: `src/printer/ams_backend_toolchanger.cpp`

**Step 1: Update header** — inherit from `AmsSubscriptionBackend`, remove extracted members/methods.

**Step 2: Update implementation**
- Implement `additional_start_checks()` to verify `tool_names_` is not empty
- ToolChanger's `get_system_info()` is simpler (returns `system_info_` directly) — keep as override
- Already uses `std::mutex`

**Step 3: Build and run ALL AMS tests**

Run: `make test && ./build/bin/helix-tests "[ams]"`
Expected: All pass

**Step 4: Commit**

```
git add include/ams_backend_toolchanger.h src/printer/ams_backend_toolchanger.cpp
git commit -m "refactor(ams): migrate ToolChanger backend to AmsSubscriptionBackend base class"
```

---

### Task 6: Run Full Test Suite & Cleanup

**Step 1: Run full test suite**

Run: `make test-run`
Expected: All tests pass

**Step 2: Verify no remaining duplicated code**

Search for patterns that should now only exist in the base class:
- `emit_event` implementations (should only be in `ams_subscription_backend.cpp`)
- `execute_gcode` implementations in AMS backends (same)
- `check_preconditions` implementations in AMS backends (same)

**Step 3: Final commit if any cleanup needed**

---

## Part 2: Temperature Formatting Consolidation

### Background

Two parallel systems for temperature formatting:
- `format_utils.h` (`helix::format` namespace): `format_temp()`, `format_temp_pair()`, `format_temp_range()`, `heater_display()` — takes **centi-degrees**
- `ui_temperature_utils.h` (`helix::ui::temperature` namespace): `format_temperature()`, `format_temperature_pair()`, `format_temperature_f()`, etc. — takes **degrees**

Both hardcode `tolerance = 2°C` for heating state logic. Both produce identical output formats.

**Strategy:** Make `ui_temperature_utils` the single source of truth for temperature formatting. Move `HeaterDisplayResult` and `heater_display()` there. Add `get_heating_state_color()` result to `HeaterDisplayResult`. Remove duplicate functions from `format_utils`. Keep non-temperature functions in `format_utils`.

### Task 7: Add HeaterDisplayResult to ui_temperature_utils

**Files:**
- Modify: `include/ui_temperature_utils.h`
- Modify: `src/ui/ui_temperature_utils.cpp`
- Create: `tests/unit/test_temperature_consolidation.cpp`

**Step 1: Write failing tests for the consolidated API**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include "ui_temperature_utils.h"

using namespace helix::ui::temperature;

TEST_CASE("heater_display: off state", "[temperature][consolidation]") {
    auto result = heater_display(2500, 0);  // 25°C current, 0 target (centi-degrees)
    REQUIRE(result.status == "Off");
    REQUIRE(result.pct == 0);
    // temp string should show current only
    REQUIRE(result.temp.find("25") != std::string::npos);
}

TEST_CASE("heater_display: heating state", "[temperature][consolidation]") {
    auto result = heater_display(15000, 21500);  // 150°C / 215°C
    REQUIRE(result.status == "Heating...");
    REQUIRE(result.pct > 0);
    REQUIRE(result.pct < 100);
}

TEST_CASE("heater_display: ready state (within tolerance)", "[temperature][consolidation]") {
    auto result = heater_display(21400, 21500);  // 214°C / 215°C (within 2°C)
    REQUIRE(result.status == "Ready");
    REQUIRE(result.pct >= 99);
}

TEST_CASE("heater_display: cooling state", "[temperature][consolidation]") {
    auto result = heater_display(22000, 21500);  // 220°C / 215°C
    REQUIRE(result.status == "Cooling");
    REQUIRE(result.pct == 100);  // clamped
}

TEST_CASE("heater_display: tolerance boundary", "[temperature][consolidation]") {
    // Exactly at tolerance boundary (2°C below target)
    auto result = heater_display(21300, 21500);  // 213°C / 215°C = exactly 2°C below
    REQUIRE(result.status == "Ready");  // Within tolerance

    // Just outside tolerance
    auto result2 = heater_display(21200, 21500);  // 212°C / 215°C = 3°C below
    REQUIRE(result2.status == "Heating...");
}
```

**Step 2: Run test to verify it fails**

Run: `make test && ./build/bin/helix-tests "[temperature][consolidation]"`
Expected: FAIL — `heater_display` not in `helix::ui::temperature` namespace

**Step 3: Move HeaterDisplayResult and heater_display() to ui_temperature_utils**

Add to `ui_temperature_utils.h`:
```cpp
/// Combined heater display result: temperature text, semantic status, percentage, and color.
/// Takes centi-degree inputs (PrinterState's internal format: 100 = 1°C).
struct HeaterDisplayResult {
    std::string temp;    ///< Formatted temperature string ("210 / 215°C" or "25°C")
    std::string status;  ///< Semantic status: "Off", "Heating...", "Ready", "Cooling"
    int pct;             ///< Percentage towards target (0-100, clamped)
    lv_color_t color;    ///< State color (gray=off, red=heating, green=ready, blue=cooling)
};

/// Get combined heater display info from centi-degree values.
/// Uses DEFAULT_AT_TEMP_TOLERANCE (2°C) for state determination.
HeaterDisplayResult heater_display(int current_centi, int target_centi);
```

Implement in `ui_temperature_utils.cpp` — same logic as current `format_utils.cpp` `heater_display()`, but also call `get_heating_state_color()` to populate the `color` field. Single source of truth for tolerance.

**Step 4: Run tests**

Run: `make test && ./build/bin/helix-tests "[temperature][consolidation]"`
Expected: PASS

**Step 5: Commit**

```
git add include/ui_temperature_utils.h src/ui/ui_temperature_utils.cpp tests/unit/test_temperature_consolidation.cpp
git commit -m "feat(temperature): consolidate HeaterDisplayResult into ui_temperature_utils"
```

---

### Task 8: Deprecate Duplicate Functions in format_utils

**Files:**
- Modify: `include/format_utils.h`
- Modify: `src/format_utils.cpp`

**Step 1: Mark format_utils temperature functions as deprecated**

Add `[[deprecated("Use helix::ui::temperature:: equivalent")]]` to:
- `format_temp()` → use `format_temperature()`
- `format_temp_pair()` → use `format_temperature_pair()`
- `format_temp_range()` → use `format_temperature_range()`
- `heater_display()` → use `helix::ui::temperature::heater_display()`

**Step 2: Build to see deprecation warnings**

Run: `make -j 2>&1 | grep "deprecated"`
This shows all call sites that need migration.

**Step 3: Commit deprecation markers**

```
git add include/format_utils.h
git commit -m "refactor(format): deprecate duplicate temperature functions in format_utils"
```

---

### Task 9: Migrate Callers to Consolidated API

**Step 1: Identify all callers of deprecated functions**

From the build warnings in Task 8, update each caller to use `ui_temperature_utils.h` equivalents.

Note the input format difference:
- `format_utils::format_temp(temp_c, buf, size)` takes **degrees** (despite the `_c` suffix)
- `format_utils::heater_display(current_centi, target_centi)` takes **centi-degrees**
- All `ui_temperature_utils` functions take **degrees** for formatting, **centi-degrees** for `heater_display()`

**Step 2: Update each caller one file at a time**

For each file:
1. Replace `#include "format_utils.h"` with `#include "ui_temperature_utils.h"` (if only using temp functions)
2. Update function calls to new names
3. Build and test

**Step 3: Run full test suite**

Run: `make test-run`
Expected: All pass

**Step 4: Commit**

```
git commit -m "refactor(temperature): migrate callers to consolidated temperature utils"
```

---

### Task 10: Remove Deprecated Functions

After all callers are migrated:

**Step 1: Remove deprecated functions from format_utils**

Remove `format_temp()`, `format_temp_pair()`, `format_temp_range()`, `HeaterDisplayResult`, `heater_display()` from both header and implementation.

**Step 2: Build and run all tests**

Run: `make test-run`
Expected: Clean build, all tests pass

**Step 3: Commit**

```
git add include/format_utils.h src/format_utils.cpp
git commit -m "refactor(format): remove deprecated temperature functions from format_utils"
```

---

### Task 11: Update REFACTOR_PLAN.md with New Findings

**Files:**
- Modify: `docs/devel/plans/REFACTOR_PLAN.md`

Add new sections for medium and low priority items discovered in the 2026-02-20 analysis:
- **SettingsManager domain split** (medium priority)
- **Panel/overlay base class broader adoption** (medium priority)
- **ui_utils.cpp split** (low priority)
- **ThemeManager split** (low priority)
- Update the AMS backend section to reflect this refactoring
- Update the temperature formatting section status

**Commit:**
```
git add docs/devel/plans/REFACTOR_PLAN.md
git commit -m "docs(refactor): update plan with new findings and AMS/formatting progress"
```

---

## Summary

| Task | Description | Estimated Effort |
|------|-------------|-----------------|
| 1 | Characterization tests | 15 min |
| 2 | AmsSubscriptionBackend base class | 30 min |
| 3 | Migrate AFC (+ recursive_mutex fix) | 30 min |
| 4 | Migrate HappyHare | 15 min |
| 5 | Migrate ToolChanger | 15 min |
| 6 | Full test suite & cleanup | 10 min |
| 7 | HeaterDisplayResult consolidation | 20 min |
| 8 | Deprecate format_utils temp functions | 10 min |
| 9 | Migrate callers | 30 min |
| 10 | Remove deprecated functions | 10 min |
| 11 | Update REFACTOR_PLAN.md | 15 min |
