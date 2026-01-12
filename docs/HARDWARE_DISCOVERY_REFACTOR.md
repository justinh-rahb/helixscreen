# Hardware Discovery Architecture Refactor

## Status: COMPLETED (2026-01-11)

This document describes the completed refactor that consolidated hardware discovery data into a single, consistent architecture.

> **Summary:** Created `PrinterHardwareDiscovery` as single source of truth for hardware discovery. Deleted `PrinterCapabilities` class. Moved bed mesh storage from `MoonrakerClient` to `MoonrakerAPI`. Clean architecture: MoonrakerClient (transport) -> callbacks -> MoonrakerAPI (domain).

## Previous State (Problem)

Hardware discovery data was scattered across multiple classes with inconsistent patterns:

| Data | Previous Location | Notes |
|------|------------------|-------|
| Heaters | `MoonrakerClient` | `get_heaters()` |
| Fans | `MoonrakerClient` | `get_fans()` |
| Temperature sensors | `MoonrakerClient` | `get_sensors()` |
| LEDs | `MoonrakerClient` | `get_leds()` |
| Hostname | `MoonrakerClient` | `get_hostname()` |
| Printer objects | `MoonrakerClient` | `get_printer_objects()` |
| Capability flags | `PrinterCapabilities` | `has_qgl()`, `has_probe()`, etc. |
| Filament sensors | `PrinterCapabilities` | `get_filament_sensor_names()` |
| AMS type/lanes | `PrinterCapabilities` | `get_mmu_type()`, `get_afc_lane_names()` |
| Macros | `PrinterCapabilities` | `macros()` |

### Problems (Now Resolved)

1. **MoonrakerClient does too much** - It was both a WebSocket/RPC client AND a hardware data store
2. **Inconsistent querying** - Some code used `client->get_X()`, other code used `caps.has_X()`
3. **Duplicate parsing** - `PrinterCapabilities::parse_objects()` parsed printer_objects that MoonrakerClient already had
4. **Initialization timing** - Wizard had to work around async race conditions because different data was in different places
5. **No single source of truth** - Wizard steps queried MoonrakerClient directly, other code used PrinterCapabilities

## Implemented Architecture

### Layer 1: MoonrakerClient (Transport Layer)
```
MoonrakerClient
├── WebSocket connection management
├── JSON-RPC 2.0 protocol handling
├── Request timeout management
├── Event emission (CONNECTION_FAILED, KLIPPY_DISCONNECTED, etc.)
├── Status subscriptions
└── Dispatches callbacks to MoonrakerAPI
```

**Responsibility:** Network communication only. No hardware state storage. Dispatches discovery data to MoonrakerAPI via callbacks.

### Layer 2: MoonrakerAPI (Domain Layer)
```
MoonrakerAPI
├── PrinterHardwareDiscovery (single source of truth)
│   ├── heaters: vector<HeaterInfo>
│   ├── fans: vector<FanInfo>
│   ├── sensors: vector<SensorInfo>
│   ├── leds: vector<LedInfo>
│   ├── filament_sensors: vector<FilamentSensorInfo>
│   ├── macros: set<string>
│   ├── hostname, printer_info
│   └── Capability queries (has_qgl, has_probe, etc.)
│
├── Bed Mesh Data (moved from MoonrakerClient)
│   ├── active_bed_mesh_
│   └── bed_mesh_profiles_
│
├── Hardware guessing (guess_bed_heater, guess_hotend_sensor)
├── G-code execution (execute_gcode)
├── Temperature control (set_temperature)
└── Object exclusion (get_excluded_objects, get_available_objects)
```

**Responsibility:** Store ALL discovered hardware data. Single place to query printer capabilities. Domain logic and accessors.

### Layer 3: State Managers (Runtime State)
```
FilamentSensorManager  - Runtime state, role assignments, runout detection
AmsState              - Backend selection, lane states, filament info
PrinterState          - Temperatures, positions, print status
```

**Responsibility:** Runtime state and business logic. Initialized FROM PrinterHardwareDiscovery data.

### Deleted Classes

- **PrinterCapabilities** - DELETED. All functionality moved to `PrinterHardwareDiscovery`.

## Migration Plan (COMPLETED)

### Phase 1: Create PrinterHardwareDiscovery class ✅
- [x] Create `include/printer_hardware_discovery.h` with new unified structure
- [x] Create `src/printer/printer_hardware_discovery.cpp` with discovery parsing
- [x] Add `PrinterHardwareDiscovery` member to MoonrakerAPI (not MoonrakerClient)
- [x] Populate via callbacks from MoonrakerClient discovery

### Phase 2: Migrate callers ✅
- [x] Update wizard steps to use `api->hardware_discovery().heaters()` etc.
- [x] Update PrinterState to use PrinterHardwareDiscovery
- [x] Update HardwareValidator to use PrinterHardwareDiscovery

### Phase 3: Merge PrinterCapabilities into PrinterHardwareDiscovery ✅
- [x] Move capability flag logic from PrinterCapabilities to PrinterHardwareDiscovery
- [x] Move macro detection to PrinterHardwareDiscovery
- [x] Update all PrinterCapabilities users to use PrinterHardwareDiscovery
- [x] Delete PrinterCapabilities class

### Phase 4: Move bed mesh to MoonrakerAPI ✅
- [x] Move `active_bed_mesh_` from MoonrakerClient to MoonrakerAPI
- [x] Move `bed_mesh_profiles_` from MoonrakerClient to MoonrakerAPI
- [x] MoonrakerClient dispatches bed mesh data via callbacks

### Phase 5: Cleanup ✅
- [x] Remove deprecated methods from MoonrakerClient
- [x] Remove old includes and forward declarations
- [x] Update documentation

## Data Structures (Implemented)

```cpp
// Located in include/printer_hardware_discovery.h

class PrinterHardwareDiscovery {
public:
    // Populate from Moonraker discovery callbacks
    void set_heaters(std::vector<std::string> heaters);
    void set_fans(std::vector<std::string> fans);
    void set_sensors(std::vector<std::string> sensors);
    void set_leds(std::vector<std::string> leds);
    void set_macros(std::unordered_set<std::string> macros);
    void set_hostname(std::string hostname);
    void set_printer_info(const nlohmann::json& info);

    // Hardware queries
    const std::vector<std::string>& heaters() const;
    const std::vector<std::string>& fans() const;
    const std::vector<std::string>& sensors() const;
    const std::vector<std::string>& leds() const;

    // Capability queries
    bool has_qgl() const;
    bool has_z_tilt() const;
    bool has_bed_mesh() const;
    bool has_probe() const;
    bool has_led() const;
    // ... etc.

    // Macro queries
    bool has_macro(const std::string& name) const;
    const std::unordered_set<std::string>& macros() const;

    // Metadata
    const std::string& hostname() const;
    const std::string& klipper_version() const;
    const std::string& mcu_version() const;
};
```

## Benefits (Realized)

1. **Single source of truth** - All hardware data in `PrinterHardwareDiscovery`
2. **Clear separation** - MoonrakerClient (transport) -> callbacks -> MoonrakerAPI (domain)
3. **Simpler initialization** - State managers init from `PrinterHardwareDiscovery`
4. **Consistent API** - `api->hardware_discovery().heaters()` everywhere
5. **Easier testing** - Mock `PrinterHardwareDiscovery` instead of multiple classes
6. **Clean ownership** - Bed mesh data owned by MoonrakerAPI (domain), not MoonrakerClient (transport)

## Files Changed

### New Files
- `include/printer_hardware_discovery.h`
- `src/printer/printer_hardware_discovery.cpp`

### Deleted Files
- `include/printer_capabilities.h` - DELETED
- `src/printer/printer_capabilities.cpp` - DELETED

### Major Changes
- `include/moonraker_api.h` - Added `PrinterHardwareDiscovery` member, owns bed mesh data
- `src/api/moonraker_api.cpp` - Receives discovery data via callbacks, stores bed mesh
- `include/moonraker_client.h` - Removed hardware storage, now pure transport
- `src/api/moonraker_client.cpp` - Dispatches discovery data to MoonrakerAPI via callbacks

### Updated Files
- `src/ui/ui_wizard_*.cpp` - All wizard steps use `hardware_discovery()`
- `src/printer/printer_state.cpp` - Uses `PrinterHardwareDiscovery`
- `src/printer/hardware_validator.cpp` - Uses `PrinterHardwareDiscovery`
- `src/application/application.cpp` - Updated initialization
- Panel files that query heaters/fans/etc.

## Related Issues (Resolved)

- Wizard filament sensor step race condition - FIXED
- Wizard AMS step race condition - FIXED
- `init_subsystems_from_capabilities()` architectural smell - FIXED
