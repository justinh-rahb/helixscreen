# AMS Mock Backend Realism Improvement Plan

## Overview

This document outlines enhancements to `AmsBackendMock` to more accurately simulate real AMS/MMU system behavior. The goal is to enable thorough UI testing, error handling validation, and realistic user experience testing without requiring physical hardware.

## Current State

### What Works Well
- **Path segment progression**: Load/unload animates through SPOOL→PREP→LANE→HUB→OUTPUT→TOOLHEAD→NOZZLE
- **Bypass mode**: Enable/disable virtual bypass
- **Dryer simulation**: Temperature ramping, countdown timer, cool-down phase
- **Spoolman integration**: Enriches slot info from mock Spoolman data
- **Tool mapping**: T0-Tn to slot mapping
- **Error injection**: Manual `simulate_error()` for testing

### What's Missing
| Gap | Impact |
|-----|--------|
| Single action state (LOADING only) | Real systems show HEATING→LOADING→CHECKING |
| Fixed timing | Operations always take exactly `operation_delay_ms_` |
| No random failures | Can't test error recovery UI automatically |
| Single unit | Can't test multi-AMS configurations |
| No mid-operation events | Real systems report progress within operations |
| Unused action states | FORMING_TIP, HEATING, CHECKING, PAUSED never used |

## Proposed Enhancements

### 1. Multi-Phase Operations

Real filament operations involve multiple phases. The mock should simulate these:

#### Load Sequence
```
HEATING (if nozzle cold)  →  2-5 seconds
LOADING                   →  3-8 seconds (segment animation)
CHECKING (sensor verify)  →  1-2 seconds
IDLE (complete)
```

#### Unload Sequence
```
HEATING (if needed)       →  2-5 seconds
FORMING_TIP               →  3-5 seconds
UNLOADING                 →  3-8 seconds (reverse segment animation)
IDLE (complete)
```

#### Implementation
```cpp
struct OperationPhase {
    AmsAction action;
    int base_duration_ms;
    float variance;  // ±percentage (0.2 = ±20%)
};

const std::vector<OperationPhase> LOAD_PHASES = {
    {AmsAction::HEATING, 3000, 0.3},
    {AmsAction::LOADING, 5000, 0.2},
    {AmsAction::CHECKING, 1500, 0.2}
};
```

### 2. Timing System with Speedup Support

Integrate with existing `RuntimeConfig::sim_speedup` for consistent timing control.

#### Design
```cpp
class AmsBackendMock {
    // Get effective delay considering speedup
    int get_effective_delay_ms(int base_ms) const {
        double speedup = get_runtime_config().sim_speedup;
        if (speedup <= 0) speedup = 1.0;
        return static_cast<int>(base_ms / speedup);
    }

    // Add variance to timing for realism
    int get_varied_delay_ms(int base_ms, float variance = 0.2f) const {
        int effective = get_effective_delay_ms(base_ms);
        float factor = 1.0f + (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 2 * variance;
        return static_cast<int>(effective * factor);
    }
};
```

#### CLI Usage
```bash
# Real-time simulation (1x speed)
./helix-screen --test -p ams

# Fast testing (10x speed)
./helix-screen --test -p ams --sim-speed 10

# Ultra-fast for automated tests (100x)
./helix-screen --test --sim-speed 100
```

### 3. Random Failure Injection

Add configurable probability of failures during operations.

#### Failure Types
| Failure | Trigger Point | Recovery |
|---------|---------------|----------|
| Filament jam | During LOADING at HUB/TOOLHEAD | User clicks "Retry" |
| Sensor timeout | During CHECKING | Auto-retry or manual |
| Tip forming fail | During FORMING_TIP | Retry with higher temp |
| Communication error | Any phase | Reconnect |

#### Implementation
```cpp
struct FailureConfig {
    float jam_probability = 0.0f;      // 0.0-1.0
    float sensor_fail_probability = 0.0f;
    float tip_fail_probability = 0.0f;
};

void AmsBackendMock::set_failure_config(const FailureConfig& config);

// Environment variable override for testing
// HELIX_MOCK_AMS_FAIL_RATE=0.1  (10% failure rate)
```

### 4. Multi-Unit Support

Support simulating multiple AMS units (e.g., 2x Box Turtle = 16 slots).

#### Implementation
```cpp
// Constructor with unit configuration
AmsBackendMock(const std::vector<int>& slots_per_unit);

// Example: Two 4-slot units
auto mock = std::make_unique<AmsBackendMock>(std::vector<int>{4, 4});

// Environment variable
// HELIX_AMS_UNITS=4,4,4  (three 4-slot units = 12 total)
```

#### Unit-Specific Features
- Each unit can have independent connection status
- Cross-unit tool changes take longer (simulate physical distance)
- Unit names: "Box Turtle 1", "Box Turtle 2", etc.

### 5. Enhanced Dryer Simulation

#### Additional Features
- **Humidity sensor**: Report chamber humidity (decreases during drying)
- **Power consumption**: Simulate wattage for energy-conscious users
- **Maintenance reminders**: "Filter check due" after X hours

```cpp
struct DryerInfo {
    // ... existing fields ...
    float humidity_pct = 50.0f;      // Chamber humidity
    float power_watts = 0.0f;        // Current power draw
    int filter_hours_remaining = -1; // -1 = not tracked
};
```

### 6. Filament Runout Simulation

Simulate mid-print filament runout for testing runout handling UI.

```cpp
// Trigger runout on specific slot after delay
void AmsBackendMock::schedule_runout(int slot_index, int delay_ms);

// Or trigger immediately
void AmsBackendMock::trigger_runout(int slot_index);
```

### 7. Pause/Resume Support

Implement PAUSED state for operations.

```cpp
AmsError AmsBackendMock::pause_operation();
AmsError AmsBackendMock::resume_operation();

// UI can show "Paused - Waiting for user" with Resume button
```

## Implementation Priority

### Phase 1: Core Realism (High Impact) ✅ COMPLETE
1. **Multi-phase operations** - Shows realistic action sequences ✅
2. **Timing with speedup** - Integrate with `--sim-speed` ✅
3. **Timing variance** - ±20% makes it feel real ✅

*Implemented as part of AMS Phase 4 (Rich Feedback) work.*

### Phase 2: Error Testing (Medium Impact) - NOT STARTED
4. **Random failure injection** - Test error recovery UI
5. **Pause/resume** - Test intervention flows

### Phase 3: Advanced Features (Lower Priority) - NOT STARTED
6. **Multi-unit support** - For power users with multiple AMS
7. **Enhanced dryer** - Humidity, power stats
8. **Filament runout** - Mid-print scenarios

## Environment Variables Summary

| Variable | Default | Description |
|----------|---------|-------------|
| `HELIX_AMS_GATES` | 4 | Number of slots in mock |
| `HELIX_AMS_UNITS` | "4" | Comma-separated slots per unit |
| `HELIX_MOCK_DRYER` | 0 | Enable dryer simulation |
| `HELIX_MOCK_DRYER_SPEED` | 60 | Dryer time multiplier |
| `HELIX_MOCK_AMS_FAIL_RATE` | 0 | Random failure probability (0.0-1.0) |
| `HELIX_MOCK_AMS_REALISTIC` | 0 | Enable multi-phase operations |

## Testing Checklist

After implementation, verify:

- [ ] Load shows HEATING→LOADING→CHECKING sequence
- [ ] Unload shows FORMING_TIP→UNLOADING sequence
- [ ] `--sim-speed 10` makes operations 10x faster
- [ ] Timing varies ±20% between operations
- [ ] `HELIX_MOCK_AMS_FAIL_RATE=0.5` causes ~50% failures
- [ ] Failed operations show error UI with recovery options
- [ ] Multi-unit config shows multiple units in system info
- [ ] Dryer humidity decreases during drying cycle

## Code Locations

| File | Changes |
|------|---------|
| `include/ams_backend_mock.h` | New config structs, phase definitions |
| `src/ams_backend_mock.cpp` | Multi-phase scheduling, failure injection |
| `include/ams_types.h` | DryerInfo humidity/power fields |
| `src/ams_backend.cpp` | Parse new env vars in factory |
| `include/runtime_config.h` | Already has `sim_speedup` ✓ |

## Success Criteria

1. **Developers** can test all UI states without hardware
2. **Automated tests** can run at 100x speed
3. **Demo mode** looks realistic with natural timing variation
4. **Error handling** can be thoroughly tested with failure injection
5. **Multi-AMS users** can test their specific configurations
