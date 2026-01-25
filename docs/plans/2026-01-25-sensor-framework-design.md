# Sensor Framework Design

## Overview

Unified sensor management framework supporting multiple sensor categories with low-friction extensibility for adding new sensor types.

**Phase A implements:**
- Switch sensors (filament runout, Z probe, dock detect)
- Filament width sensors (tsl1401cl, hall)
- Color/TD sensors (TD-1 for HueForge)
- Humidity sensors (BME280, HTU21D)
- Native probes (probe, bltouch, smart_effector, probe_eddy_current)
- Accelerometers (adxl345, lis2dw, lis3dh, mpu9250)

**Future phases:** Temperature sensors (B), Endstops (C)

## Klipper Sensor Types Reference

| Category | Klipper Objects | State Data | Phase |
|----------|-----------------|------------|-------|
| Switch | `filament_switch_sensor`, `filament_motion_sensor` | triggered, enabled, detection_count | A |
| Width | `tsl1401cl_filament_width_sensor`, `hall_filament_width_sensor` | diameter (mm) | A |
| Color/TD | TD-1 via AFC/Moonraker (`/machine/td1/data`) | color (hex), transmission_distance | A |
| Humidity | BME280, HTU21D (via `bme280 <name>`, `htu21d <name>`) | humidity (%), pressure (hPa) | A |
| Probe | `probe`, `bltouch`, `smart_effector`, `probe_eddy_current` | triggered, last_z_result, z_offset | A |
| Accelerometer | `adxl345`, `lis2dw`, `lis3dh`, `mpu9250`, `icm20948` | connected state | A |
| Temperature | `temperature_sensor`, `temperature_fan` | temperature (°C) | B |
| Endstop | via `[stepper_*]` config | triggered state | C |

## Design Goals

1. **Low friction** - Adding a new sensor category requires minimal boilerplate
2. **Consistent patterns** - All categories use discovery → config → state → subjects flow
3. **Flexible roles** - Categories define their own role enums without central registration
4. **Self-contained** - Each category manager owns its types, config, and subjects
5. **Extensible actions** - Role-based default actions now, per-sensor config later

## Architecture

### Discovery Timing

- **Startup only** - Query `printer.objects.list` at connection
- Sensor hardware changes require Klipper restart → reconnection → rediscovery
- No live monitoring needed

### Base Pattern

```cpp
template<typename RoleEnum, typename ConfigT, typename StateT>
class SensorManagerBase {
protected:
    std::vector<ConfigT> sensors_;
    std::map<std::string, StateT> states_;
    std::recursive_mutex mutex_;

    virtual std::string category_name() const = 0;
    virtual bool matches_klipper_name(const std::string& name) const = 0;
    virtual void parse_klipper_name(const std::string& full_name, ConfigT& config) = 0;
    virtual void update_state_from_json(const std::string& name, const json& data) = 0;
    virtual void update_subjects() = 0;

public:
    void discover(const std::vector<std::string>& klipper_objects);
    void update_from_status(const json& status);
    void load_config(const json& config);
    json save_config() const;
};
```

### Registry Pattern

```cpp
class SensorRegistry {
    std::map<std::string, std::unique_ptr<ISensorManager>> managers_;

public:
    void register_manager(std::string category, std::unique_ptr<ISensorManager> mgr);
    void discover_all(const std::vector<std::string>& klipper_objects);
    void update_all_from_status(const json& status);
    void load_config(const json& root_config);
    json save_config() const;
};
```

### Category Managers

```
SensorRegistry
  ├── SwitchSensorManager
  │     Klipper: filament_switch_sensor, filament_motion_sensor
  │     Roles: FILAMENT_RUNOUT, FILAMENT_TOOLHEAD, FILAMENT_ENTRY, Z_PROBE, DOCK_DETECT
  │
  ├── WidthSensorManager
  │     Klipper: tsl1401cl_filament_width_sensor, hall_filament_width_sensor
  │     Roles: FLOW_COMPENSATION
  │
  ├── ColorSensorManager
  │     Source: TD-1 via Moonraker /machine/td1/data
  │     Roles: FILAMENT_COLOR
  │
  ├── HumiditySensorManager
  │     Klipper: bme280, htu21d
  │     Roles: CHAMBER, DRYER
  │
  ├── ProbeSensorManager
  │     Klipper: probe, bltouch, smart_effector, probe_eddy_current
  │     Roles: Z_PROBE
  │
  └── AccelerometerManager
        Klipper: adxl345, lis2dw, lis3dh, mpu9250, icm20948
        Roles: INPUT_SHAPER
```

### Adding a New Category (Checklist)

1. Create `include/<category>_sensor_types.h` - role enum, config struct, state struct
2. Create `include/<category>_sensor_manager.h` - inherit from base, implement virtual methods
3. Create `src/sensors/<category>_sensor_manager.cpp` - implementation
4. Register in `SensorRegistry::init()` - one line: `register_manager("category", make_unique<CategoryManager>())`
5. (Optional) Add wizard step if role assignment needed
6. (Optional) Add section to sensors_overlay.xml

## Types

### Switch Sensors

```cpp
enum class SwitchSensorRole {
    NONE = 0,
    FILAMENT_RUNOUT = 1,
    FILAMENT_TOOLHEAD = 2,
    FILAMENT_ENTRY = 3,
    Z_PROBE = 10,
    DOCK_DETECT = 20,
};

enum class SwitchSensorType { SWITCH = 1, MOTION = 2 };

struct SwitchSensorConfig {
    std::string klipper_name;   // "filament_switch_sensor e1_sensor"
    std::string sensor_name;    // "e1_sensor"
    SwitchSensorType type;
    SwitchSensorRole role;
    bool enabled;
};

struct SwitchSensorState {
    bool triggered;
    bool enabled;
    int detection_count;
    bool available;
};
```

### Width Sensors

```cpp
enum class WidthSensorRole { NONE = 0, FLOW_COMPENSATION = 1 };
enum class WidthSensorType { TSL1401CL = 1, HALL = 2 };

struct WidthSensorConfig {
    std::string klipper_name;
    std::string sensor_name;
    WidthSensorType type;
    WidthSensorRole role;
    bool enabled;
};

struct WidthSensorState {
    float diameter;
    float raw_value;
    bool available;
};
```

### Color Sensors (TD-1)

```cpp
enum class ColorSensorRole { NONE = 0, FILAMENT_COLOR = 1 };

struct ColorSensorConfig {
    std::string device_id;
    std::string sensor_name;
    ColorSensorRole role;
    bool enabled;
};

struct ColorSensorState {
    std::string color_hex;
    float transmission_distance;
    bool available;
};
```

### Humidity Sensors

```cpp
enum class HumiditySensorRole { NONE = 0, CHAMBER = 1, DRYER = 2 };
enum class HumiditySensorType { BME280 = 1, HTU21D = 2 };

struct HumiditySensorConfig {
    std::string klipper_name;
    std::string sensor_name;
    HumiditySensorType type;
    HumiditySensorRole role;
    bool enabled;
};

struct HumiditySensorState {
    float humidity;
    float pressure;
    float temperature;
    bool available;
};
```

### Probe Sensors

```cpp
enum class ProbeSensorRole { NONE = 0, Z_PROBE = 1 };
enum class ProbeSensorType { STANDARD = 1, BLTOUCH = 2, SMART_EFFECTOR = 3, EDDY_CURRENT = 4 };

struct ProbeSensorConfig {
    std::string klipper_name;
    std::string sensor_name;
    ProbeSensorType type;
    ProbeSensorRole role;
    bool enabled;
};

struct ProbeSensorState {
    bool triggered;
    float last_z_result;
    float z_offset;
    bool available;
};
```

### Accelerometer Sensors

```cpp
enum class AccelSensorRole { NONE = 0, INPUT_SHAPER = 1 };
enum class AccelSensorType { ADXL345 = 1, LIS2DW = 2, LIS3DH = 3, MPU9250 = 4, ICM20948 = 5 };

struct AccelSensorConfig {
    std::string klipper_name;
    std::string sensor_name;
    AccelSensorType type;
    AccelSensorRole role;
    bool enabled;
};

struct AccelSensorState {
    bool connected;
    std::string last_measurement;
    bool available;
};
```

## LVGL Subjects

**Switch sensors (existing):**
- `filament_runout_detected`, `filament_toolhead_detected`, `filament_entry_detected`
- `filament_any_runout`, `filament_motion_active`, `filament_master_enabled`, `filament_sensor_count`

**Switch sensors (new):**
- `probe_switch_triggered` - int: -1=no sensor, 0=not triggered, 1=triggered
- `probe_switch_count` - int

**Width sensors:**
- `filament_width_diameter` - int: diameter × 1000 (1750 = 1.75mm), -1 if none
- `width_sensor_count` - int

**Color sensors:**
- `filament_color_hex` - string: "#RRGGBB" or empty
- `filament_td_value` - int: TD × 100, -1 if none
- `color_sensor_count` - int

**Humidity sensors:**
- `chamber_humidity` - int: humidity × 10, -1 if none
- `chamber_pressure` - int: pressure in Pa, -1 if none
- `dryer_humidity` - int: humidity × 10 for dryer sensor, -1 if none
- `humidity_sensor_count` - int

**Native probes:**
- `probe_triggered` - int: -1=no probe, 0=not triggered, 1=triggered
- `probe_last_z` - int: last Z result × 1000 (microns), -1 if none
- `probe_z_offset` - int: Z offset × 1000 (microns)
- `probe_count` - int

**Accelerometers:**
- `accel_connected` - int: -1=no accel, 0=disconnected, 1=connected
- `accel_count` - int

## Config Structure

```json
{
  "sensors": {
    "switch": {
      "master_enabled": true,
      "sensors": [
        { "klipper_name": "filament_switch_sensor e0_sensor", "role": "filament_runout", "enabled": true },
        { "klipper_name": "filament_switch_sensor e1_sensor", "role": "z_probe", "enabled": true }
      ]
    },
    "width": {
      "master_enabled": true,
      "sensors": [
        { "klipper_name": "hall_filament_width_sensor", "role": "flow_compensation", "enabled": true }
      ]
    },
    "color": {
      "master_enabled": true,
      "sensors": [
        { "device_id": "td1_lane0", "role": "filament_color", "enabled": true }
      ]
    },
    "humidity": {
      "master_enabled": true,
      "sensors": [
        { "klipper_name": "bme280 chamber", "role": "chamber", "enabled": true }
      ]
    }
  }
}
```

**Migration:** Auto-convert old `filament_sensors` → `sensors.switch` on load.

## UI

### Home Panel Indicators

Add to home panel bottom row (conditional like filament sensor):
- `humidity_indicator.xml` - chamber humidity % with icon
- `probe_indicator.xml` - probe triggered status
- `width_indicator.xml` - filament diameter display

### Settings Overlay

Single `sensors_overlay.xml` with sections:
- Filament Sensors (switch sensors with filament roles)
- Probe Sensors (switch + native probes)
- Width Sensors
- Color Sensors (TD-1)
- Humidity Sensors
- Accelerometers

### Wizard

New step `wizard_probe_sensor_select` after filament sensor step. Skipped if no unassigned switch sensors exist.

## Action System

Extensible architecture with role-based defaults:

```cpp
enum class SensorAction { NONE = 0, TOAST = 1, PAUSE_PRINT = 2 };

struct SensorActionConfig {
    SensorAction on_trigger;
    SensorAction on_clear;
    bool enabled;
};
```

**Default actions by role:**
- FILAMENT_RUNOUT → PAUSE_PRINT on trigger
- Z_PROBE → NONE (informational only)
- CHAMBER humidity → TOAST if threshold exceeded
- Others → NONE

Per-sensor override architecture exists but UI deferred.
