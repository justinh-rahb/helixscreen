# Moonraker Layer Architecture

This document describes the architecture of the Moonraker integration layer after the 2025-11 refactor.

## Overview

The Moonraker integration is split into three distinct layers:

```
┌─────────────────────────────────────────────────────────────────┐
│                         UI Panels                                │
│  (BedMeshPanel, Wizards, etc.)                                  │
└─────────────────────────────┬───────────────────────────────────┘
                              │ uses
┌─────────────────────────────▼───────────────────────────────────┐
│                      MoonrakerAPI                                │
│  (Domain Logic Layer)                                           │
│  ├─ Hardware guessing (guess_bed_heater, guess_hotend_sensor)   │
│  ├─ Bed mesh access (get_active_bed_mesh, get_bed_mesh_profiles)│
│  ├─ G-code execution (execute_gcode)                            │
│  ├─ Temperature control (set_temperature)                       │
│  └─ Object exclusion (get_excluded_objects, get_available_objects)│
└─────────────────────────────┬───────────────────────────────────┘
                              │ delegates to
┌─────────────────────────────▼───────────────────────────────────┐
│                    MoonrakerClient                               │
│  (Transport Layer)                                               │
│  ├─ WebSocket connection/reconnection                           │
│  ├─ JSON-RPC request/response handling                          │
│  ├─ Event emission (CONNECTION_FAILED, KLIPPY_DISCONNECTED...)  │
│  ├─ Printer state subscriptions                                 │
│  └─ Hardware discovery (get_heaters, get_fans, etc.)            │
└─────────────────────────────────────────────────────────────────┘
```

## Layer Responsibilities

### MoonrakerClient (Transport Layer)

**Location:** `include/moonraker_client.h`, `src/moonraker_client.cpp`

**Responsibilities:**
- WebSocket connection management (connect, disconnect, reconnect)
- JSON-RPC 2.0 protocol handling
- Request timeout management
- Event emission for transport events
- Raw hardware discovery (`get_heaters()`, `get_fans()`, `get_sensors()`)
- Printer state subscriptions (`register_notify_update()`)

**Does NOT do:**
- UI notifications (replaced with event emission)
- Business logic decisions
- Hardware "guessing" logic

### MoonrakerAPI (Domain Logic Layer)

**Location:** `include/moonraker_api.h`, `src/moonraker_api.cpp`

**Responsibilities:**
- Hardware guessing (`guess_bed_heater()`, `guess_hotend_sensor()`, etc.)
- Bed mesh access and profile management
- G-code command execution
- Temperature control
- Object exclusion state queries
- Print control (pause, resume, cancel)

**Key Pattern:** All async methods use callback pattern:
```cpp
void method_name(
    std::function<void(ResultType)> on_success,
    ErrorCallback on_error
);
```

### Event System

**Location:** `include/moonraker_events.h`

The event system decouples the transport layer from the UI:

```cpp
enum class MoonrakerEventType {
    CONNECTION_FAILED,    // Max reconnect attempts exceeded
    CONNECTION_LOST,      // WebSocket closed unexpectedly
    RECONNECTING,         // Attempting to reconnect
    RECONNECTED,          // Successfully reconnected
    MESSAGE_OVERSIZED,    // Received message exceeds size limit
    RPC_ERROR,            // JSON-RPC request failed
    KLIPPY_DISCONNECTED,  // Klipper firmware disconnected
    KLIPPY_READY,         // Klipper firmware ready
    DISCOVERY_FAILED,     // Printer discovery failed
    REQUEST_TIMEOUT       // JSON-RPC request timed out
};

struct MoonrakerEvent {
    MoonrakerEventType type;
    std::string message;
    std::string details;
    bool is_error;
};
```

**Usage:**
```cpp
// Register handler in main.cpp
moonraker_client->register_event_handler([](const MoonrakerEvent& evt) {
    if (evt.is_error) {
        ui_notification_error(title, evt.message.c_str(), true);
    } else {
        ui_notification_warning(evt.message.c_str());
    }
});
```

## Mock Architecture

For testing, parallel mock implementations exist:

```
┌─────────────────────────────────────────────────────────────────┐
│                      MockPrinterState                            │
│  (Shared State - tests/mocks/mock_printer_state.h)              │
│  ├─ Temperatures (atomic<double>)                               │
│  ├─ Print state                                                 │
│  ├─ Excluded objects (mutex-protected set)                      │
│  └─ Available objects                                           │
└─────────────────────┬─────────────────┬─────────────────────────┘
                      │                 │
┌─────────────────────▼───────┐  ┌─────▼─────────────────────────┐
│   MoonrakerClientMock       │  │    MoonrakerAPIMock           │
│   (Transport Mock)          │  │    (Domain Mock)              │
│   ├─ Simulated temps        │  │    ├─ Local file downloads    │
│   ├─ EXCLUDE_OBJECT parsing │  │    ├─ Mock uploads            │
│   └─ Synthetic bed mesh     │  │    └─ Shared state access     │
└─────────────────────────────┘  └─────────────────────────────────┘
```

### MockPrinterState

Thread-safe shared state between mock implementations:

```cpp
auto state = std::make_shared<MockPrinterState>();

MoonrakerClientMock client(PrinterType::VORON_24);
client.set_mock_state(state);

MoonrakerAPIMock api(client, printer_state);
api.set_mock_state(state);

// Now excluded objects sync between mocks
client.gcode_script("EXCLUDE_OBJECT NAME=Part_1", ...);
// api.get_excluded_objects_from_mock() returns {"Part_1"}
```

## Deprecated Methods

The following `MoonrakerClient` methods are deprecated in favor of `MoonrakerAPI`:

| Deprecated Method | Use Instead |
|-------------------|-------------|
| `MoonrakerClient::guess_bed_heater()` | `MoonrakerAPI::guess_bed_heater()` |
| `MoonrakerClient::guess_hotend_heater()` | `MoonrakerAPI::guess_hotend_heater()` |
| `MoonrakerClient::guess_bed_sensor()` | `MoonrakerAPI::guess_bed_sensor()` |
| `MoonrakerClient::guess_hotend_sensor()` | `MoonrakerAPI::guess_hotend_sensor()` |
| `MoonrakerClient::get_active_bed_mesh()` | `MoonrakerAPI::get_active_bed_mesh()` |
| `MoonrakerClient::get_bed_mesh_profiles()` | `MoonrakerAPI::get_bed_mesh_profiles()` |
| `MoonrakerClient::has_bed_mesh()` | `MoonrakerAPI::has_bed_mesh()` |

**Note:** Deprecated methods will emit compiler warnings. Migrate callers to `MoonrakerAPI` methods.

## Key Differences: API vs Client

| Aspect | MoonrakerAPI | MoonrakerClient |
|--------|--------------|-----------------|
| Return types | Pointers (nullable) | References |
| Bed mesh | `const BedMeshProfile*` | `const BedMeshProfile&` |
| G-code | `execute_gcode()` (async) | `gcode_script()` (sync-ish) |
| Thread safety | Delegates to client | Internal mutexes |
| UI coupling | None | None (events only) |

## Testing

### Running Moonraker Tests

```bash
# All moonraker-related tests
./build/bin/run_tests "[moonraker]"

# Just event tests
./build/bin/run_tests "[events]"

# Integration tests
./build/bin/run_tests "[integration]"

# Domain method parity tests
./build/bin/run_tests "[domain]"

# Shared state tests
./build/bin/run_tests "[shared_state]"
```

### Test Coverage

| Test File | Focus |
|-----------|-------|
| `test_moonraker_events.cpp` | Event emission, handler registration |
| `test_moonraker_api_domain.cpp` | Domain method parity (API vs Client) |
| `test_moonraker_mock_behavior.cpp` | Mock client behavior |
| `test_mock_shared_state.cpp` | MockPrinterState thread safety |
| `test_moonraker_full_stack.cpp` | Integration across all layers |

## File Reference

### Headers

| File | Purpose |
|------|---------|
| `include/moonraker_client.h` | Transport layer (WebSocket, JSON-RPC) |
| `include/moonraker_api.h` | Domain logic layer |
| `include/moonraker_events.h` | Event types and callbacks |
| `include/moonraker_domain_service.h` | Domain interface + BedMeshProfile struct |
| `include/moonraker_client_mock.h` | Transport layer mock |
| `include/moonraker_api_mock.h` | Domain layer mock |

### Sources

| File | Purpose |
|------|---------|
| `src/moonraker_client.cpp` | Transport implementation |
| `src/moonraker_api.cpp` | Domain logic implementation |
| `src/moonraker_client_mock.cpp` | Mock transport (simulated temps, etc.) |
| `src/moonraker_api_mock.cpp` | Mock domain (local file access) |

### Test Files

| File | Purpose |
|------|---------|
| `tests/mocks/mock_printer_state.h` | Shared mock state |
| `tests/unit/test_moonraker_*.cpp` | Unit tests |
| `tests/unit/test_mock_shared_state.cpp` | Shared state tests |
| `tests/unit/test_moonraker_full_stack.cpp` | Integration tests |

## Migration Guide

### Migrating from MoonrakerClient to MoonrakerAPI

**Before:**
```cpp
MoonrakerClient* client = get_moonraker_client();
std::string bed_heater = client->guess_bed_heater();  // Deprecated!
const BedMeshProfile& mesh = client->get_active_bed_mesh();  // Deprecated!
```

**After:**
```cpp
MoonrakerAPI* api = get_moonraker_api();
std::string bed_heater = api->guess_bed_heater();
const BedMeshProfile* mesh = api->get_active_bed_mesh();  // Note: pointer!
if (mesh) {
    // Use mesh data
}
```

### Key Changes

1. **Null checks:** `MoonrakerAPI` returns pointers, not references
2. **Async pattern:** Use callbacks for operations that communicate with printer
3. **Global accessor:** Use `get_moonraker_api()` instead of `get_moonraker_client()` for domain ops

## See Also

- `docs/TESTING_MOONRAKER_API.md` - Manual testing procedures
- `docs/TESTING.md` - General testing guide
- `include/moonraker_api.h` - Full API documentation (Doxygen)
