// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file ams_types.h
 * @brief Data structures for multi-filament/AMS system support
 *
 * Supports both Happy Hare (MMU) and AFC-Klipper-Add-On systems.
 * These structures are platform-agnostic - backends translate from
 * their specific APIs to these common types.
 *
 * @note Thread Safety: These structures are NOT thread-safe. The AmsState
 * class provides thread-safe access through LVGL subjects. Direct mutation
 * of these structures should only occur in the backend layer.
 */

/// Default color for slots without filament info (medium gray)
constexpr uint32_t AMS_DEFAULT_SLOT_COLOR = 0x808080;

/**
 * @brief Type of AMS system detected
 */
enum class AmsType {
    NONE = 0,       ///< No AMS detected
    HAPPY_HARE = 1, ///< Happy Hare MMU (mmu object in Moonraker)
    AFC = 2         ///< AFC-Klipper-Add-On (afc object, lane_data database)
};

/**
 * @brief Get string name for AMS type
 * @param type The AMS type enum value
 * @return Human-readable string for the type
 */
inline const char* ams_type_to_string(AmsType type) {
    switch (type) {
    case AmsType::HAPPY_HARE:
        return "Happy Hare";
    case AmsType::AFC:
        return "AFC";
    default:
        return "None";
    }
}

/**
 * @brief Parse AMS type from string (for Moonraker responses)
 * @param str String to parse (case-insensitive)
 * @return Matching AmsType or NONE if not recognized
 */
inline AmsType ams_type_from_string(std::string_view str) {
    // Simple comparison - backends will use their own detection
    if (str == "mmu" || str == "happy_hare" || str == "Happy Hare") {
        return AmsType::HAPPY_HARE;
    }
    if (str == "afc" || str == "AFC") {
        return AmsType::AFC;
    }
    return AmsType::NONE;
}

/**
 * @brief Slot/Lane status
 *
 * Our internal status representation. Use conversion functions to
 * translate from Happy Hare's gate_status values (-1, 0, 1, 2).
 */
enum class SlotStatus {
    UNKNOWN = 0,     ///< Status not known
    EMPTY = 1,       ///< No filament in slot
    AVAILABLE = 2,   ///< Filament available, not loaded
    LOADED = 3,      ///< Filament loaded to extruder
    FROM_BUFFER = 4, ///< Filament available from buffer
    BLOCKED = 5      ///< Slot blocked/jammed
};

/**
 * @brief Get string name for slot status
 * @param status The slot status enum value
 * @return Human-readable string for the status
 */
inline const char* slot_status_to_string(SlotStatus status) {
    switch (status) {
    case SlotStatus::EMPTY:
        return "Empty";
    case SlotStatus::AVAILABLE:
        return "Available";
    case SlotStatus::LOADED:
        return "Loaded";
    case SlotStatus::FROM_BUFFER:
        return "From Buffer";
    case SlotStatus::BLOCKED:
        return "Blocked";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert Happy Hare gate_status integer to SlotStatus enum
 *
 * Happy Hare uses: -1 = unknown, 0 = empty, 1 = available, 2 = from buffer
 * The "loaded" state is determined by comparing with current_slot, not from
 * gate_status directly.
 *
 * @param hh_status Happy Hare gate_status value (-1, 0, 1, or 2)
 * @return Corresponding SlotStatus enum value
 */
inline SlotStatus slot_status_from_happy_hare(int hh_status) {
    switch (hh_status) {
    case -1:
        return SlotStatus::UNKNOWN;
    case 0:
        return SlotStatus::EMPTY;
    case 1:
        return SlotStatus::AVAILABLE;
    case 2:
        return SlotStatus::FROM_BUFFER;
    default:
        return SlotStatus::UNKNOWN;
    }
}

/**
 * @brief Convert SlotStatus enum to Happy Hare gate_status integer
 * @param status Our SlotStatus enum value
 * @return Happy Hare gate_status value (-1, 0, 1, or 2)
 */
inline int slot_status_to_happy_hare(SlotStatus status) {
    switch (status) {
    case SlotStatus::UNKNOWN:
        return -1;
    case SlotStatus::EMPTY:
        return 0;
    case SlotStatus::AVAILABLE:
        return 1;
    case SlotStatus::FROM_BUFFER:
        return 2;
    // LOADED and BLOCKED don't have direct HH equivalents
    case SlotStatus::LOADED:
        return 1; // Treat as available
    case SlotStatus::BLOCKED:
        return -1; // Treat as unknown
    default:
        return -1;
    }
}

/**
 * @brief Current AMS action/operation
 *
 * Maps to Happy Hare's action strings:
 * "Idle", "Loading", "Unloading", "Forming Tip", "Heating", etc.
 */
enum class AmsAction {
    IDLE = 0,        ///< No operation in progress
    LOADING = 1,     ///< Loading filament to extruder
    UNLOADING = 2,   ///< Unloading filament from extruder
    SELECTING = 3,   ///< Selecting tool/slot
    RESETTING = 4,   ///< Resetting system (MMU_HOME for HH, AFC_RESET for AFC)
    FORMING_TIP = 5, ///< Forming filament tip for retraction
    HEATING = 6,     ///< Heating for operation
    CHECKING = 7,    ///< Checking slots
    PAUSED = 8,      ///< Operation paused (requires attention)
    ERROR = 9        ///< Error state
};

/**
 * @brief Get string name for AMS action
 * @param action The action enum value
 * @return Human-readable string for the action
 */
inline const char* ams_action_to_string(AmsAction action) {
    switch (action) {
    case AmsAction::IDLE:
        return "Idle";
    case AmsAction::LOADING:
        return "Loading";
    case AmsAction::UNLOADING:
        return "Unloading";
    case AmsAction::SELECTING:
        return "Selecting";
    case AmsAction::RESETTING:
        return "Resetting";
    case AmsAction::FORMING_TIP:
        return "Forming Tip";
    case AmsAction::HEATING:
        return "Heating";
    case AmsAction::CHECKING:
        return "Checking";
    case AmsAction::PAUSED:
        return "Paused";
    case AmsAction::ERROR:
        return "Error";
    default:
        return "Unknown";
    }
}

/**
 * @brief Parse AMS action from Happy Hare action string
 * @param action_str Action string from printer.mmu.action
 * @return Corresponding AmsAction enum value
 */
inline AmsAction ams_action_from_string(std::string_view action_str) {
    if (action_str == "Idle")
        return AmsAction::IDLE;
    if (action_str == "Loading")
        return AmsAction::LOADING;
    if (action_str == "Unloading")
        return AmsAction::UNLOADING;
    if (action_str == "Selecting")
        return AmsAction::SELECTING;
    if (action_str == "Homing" || action_str == "Resetting")
        return AmsAction::RESETTING;
    if (action_str == "Forming Tip")
        return AmsAction::FORMING_TIP;
    if (action_str == "Heating")
        return AmsAction::HEATING;
    if (action_str == "Checking")
        return AmsAction::CHECKING;
    // Happy Hare uses "Paused" for attention-required states
    if (action_str.find("Pause") != std::string_view::npos)
        return AmsAction::PAUSED;
    if (action_str.find("Error") != std::string_view::npos)
        return AmsAction::ERROR;
    return AmsAction::IDLE;
}

// ============================================================================
// Filament Path Visualization Types
// ============================================================================

/**
 * @brief Path topology - affects visual rendering of the filament path
 *
 * Both Happy Hare and AFC map to these same logical segments but are rendered
 * differently based on their physical topology:
 * - LINEAR: Selector picks one input from multiple gates (Happy Hare ERCF)
 * - HUB: Multiple lanes merge into a common hub/merger (AFC Box Turtle)
 */
enum class PathTopology {
    LINEAR = 0, ///< Happy Hare: selector picks one input
    HUB = 1     ///< AFC: merger combines inputs through hub
};

/**
 * @brief Get string name for path topology
 * @param topology The topology enum value
 * @return Human-readable string for the topology
 */
inline const char* path_topology_to_string(PathTopology topology) {
    switch (topology) {
    case PathTopology::LINEAR:
        return "Linear (Selector)";
    case PathTopology::HUB:
        return "Hub (Merger)";
    default:
        return "Unknown";
    }
}

/**
 * @brief Unified path segments (AFC-inspired naming)
 *
 * Both Happy Hare and AFC map to these same logical segments. The path
 * canvas widget draws them differently based on PathTopology.
 *
 * Physical filament path (top to bottom in UI):
 *   SPOOL → PREP → LANE → HUB → OUTPUT → TOOLHEAD → NOZZLE
 *
 * Happy Hare mapping:
 *   SPOOL=Gate storage, PREP=Gate sensor, LANE=Gate-to-selector,
 *   HUB=Selector, OUTPUT=Bowden tube, TOOLHEAD=Extruder sensor, NOZZLE=Loaded
 *
 * AFC mapping:
 *   SPOOL=Lane spool, PREP=Prep sensor, LANE=Lane tube,
 *   HUB=Hub/Merger, OUTPUT=Output tube, TOOLHEAD=Toolhead sensor, NOZZLE=Loaded
 */
enum class PathSegment {
    NONE = 0,     ///< No segment / idle / filament not present
    SPOOL = 1,    ///< At spool (filament storage area)
    PREP = 2,     ///< At entry sensor (prep/gate sensor)
    LANE = 3,     ///< In lane/gate-to-router segment
    HUB = 4,      ///< At router (selector or hub/merger)
    OUTPUT = 5,   ///< In output tube (bowden or hub output)
    TOOLHEAD = 6, ///< At toolhead sensor
    NOZZLE = 7    ///< Fully loaded in nozzle
};

/// Number of path segments for iteration (NONE through NOZZLE)
constexpr int PATH_SEGMENT_COUNT = 8;

/**
 * @brief Get string name for path segment
 * @param segment The segment enum value
 * @return Human-readable string for the segment
 */
inline const char* path_segment_to_string(PathSegment segment) {
    switch (segment) {
    case PathSegment::NONE:
        return "None";
    case PathSegment::SPOOL:
        return "Spool";
    case PathSegment::PREP:
        return "Prep Sensor";
    case PathSegment::LANE:
        return "Lane";
    case PathSegment::HUB:
        return "Hub/Selector";
    case PathSegment::OUTPUT:
        return "Output Tube";
    case PathSegment::TOOLHEAD:
        return "Toolhead";
    case PathSegment::NOZZLE:
        return "Nozzle";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert Happy Hare filament_pos to unified PathSegment
 *
 * Happy Hare filament_pos values:
 *   0 = unloaded (at spool)
 *   1 = homed at gate
 *   2 = in gate
 *   3 = in bowden
 *   4 = end of bowden
 *   5 = homed at extruder
 *   6 = extruder entry
 *   7 = in extruder
 *   8 = fully loaded
 *
 * @param filament_pos Happy Hare filament_pos value
 * @return Corresponding PathSegment
 */
inline PathSegment path_segment_from_happy_hare_pos(int filament_pos) {
    switch (filament_pos) {
    case 0:
        return PathSegment::SPOOL;
    case 1:
    case 2:
        return PathSegment::PREP; // Gate area
    case 3:
        return PathSegment::LANE; // Moving through
    case 4:
        return PathSegment::HUB; // At selector
    case 5:
        return PathSegment::OUTPUT; // In bowden
    case 6:
        return PathSegment::TOOLHEAD; // At extruder
    case 7:
    case 8:
        return PathSegment::NOZZLE; // Loaded
    default:
        return PathSegment::NONE;
    }
}

/**
 * @brief Infer PathSegment from AFC sensor states
 *
 * AFC uses binary sensor states to determine filament position.
 * Logic: filament is at or past the last sensor that detects it.
 *
 * @param prep_sensor Prep sensor triggered (filament at lane entry)
 * @param hub_sensor Hub sensor triggered (filament in hub)
 * @param toolhead_sensor Toolhead sensor triggered (filament at extruder)
 * @return Inferred PathSegment based on sensor states
 */
inline PathSegment path_segment_from_afc_sensors(bool prep_sensor, bool hub_sensor,
                                                 bool toolhead_sensor) {
    if (toolhead_sensor)
        return PathSegment::NOZZLE;
    if (hub_sensor)
        return PathSegment::TOOLHEAD; // Past hub, approaching toolhead
    if (prep_sensor)
        return PathSegment::HUB; // Past prep, approaching hub
    return PathSegment::SPOOL;   // Not yet at prep
}

/**
 * @brief Information about a single slot/lane
 *
 * This represents one filament slot in an AMS unit.
 * Happy Hare calls these "gates" internally, AFC calls them "lanes".
 */
struct SlotInfo {
    int slot_index = -1;   ///< Slot/lane number (0-based within unit)
    int global_index = -1; ///< Global index across all units
    SlotStatus status = SlotStatus::UNKNOWN;

    // Filament information
    std::string color_name;                      ///< Named color (e.g., "Red", "Blue")
    uint32_t color_rgb = AMS_DEFAULT_SLOT_COLOR; ///< RGB color for UI (0xRRGGBB)
    std::string material;                        ///< Material type (e.g., "PLA", "PETG", "ABS")
    std::string brand;                           ///< Brand name (e.g., "Polymaker", "eSUN")

    // Temperature recommendations (from Spoolman or manual entry)
    int nozzle_temp_min = 0; ///< Minimum nozzle temp (°C)
    int nozzle_temp_max = 0; ///< Maximum nozzle temp (°C)
    int bed_temp = 0;        ///< Recommended bed temp (°C)

    // Tool mapping
    int mapped_tool = -1; ///< Which tool this slot maps to (-1=none)

    // Spoolman integration
    int spoolman_id = 0;           ///< Spoolman spool ID (0=not tracked)
    std::string spool_name;        ///< Spool name from Spoolman
    float remaining_weight_g = -1; ///< Remaining filament weight in grams (-1=unknown)
    float total_weight_g = -1;     ///< Total spool weight in grams (-1=unknown)

    // Endless spool support (Happy Hare)
    int endless_spool_group = -1; ///< Endless spool group (-1=not grouped)

    /**
     * @brief Get remaining percentage
     * @return 0-100 or -1 if unknown
     */
    [[nodiscard]] float get_remaining_percent() const {
        if (remaining_weight_g < 0 || total_weight_g <= 0)
            return -1;
        return (remaining_weight_g / total_weight_g) * 100.0f;
    }

    /**
     * @brief Check if this slot has filament data configured
     * @return true if material or custom color is set
     */
    [[nodiscard]] bool has_filament_info() const {
        return !material.empty() || color_rgb != AMS_DEFAULT_SLOT_COLOR;
    }
};

/**
 * @brief Information about an AMS unit
 *
 * Supports multi-unit configurations (e.g., 2x Box Turtles = 16 slots).
 * Most setups have a single unit with 4-8 slots.
 */
struct AmsUnit {
    int unit_index = 0;              ///< Unit number (0-based)
    std::string name;                ///< Unit name/identifier (e.g., "MMU", "Box Turtle 1")
    int slot_count = 0;              ///< Number of slots on this unit
    int first_slot_global_index = 0; ///< Global index of first slot

    std::vector<SlotInfo> slots; ///< Slot information

    // Unit-level status
    bool connected = false;       ///< Unit communication status
    std::string firmware_version; ///< Firmware version if available

    // Sensors (Happy Hare)
    bool has_encoder = false;         ///< Has filament encoder
    bool has_toolhead_sensor = false; ///< Has toolhead filament sensor
    bool has_slot_sensors = false;    ///< Has per-slot sensors

    /**
     * @brief Get slot by local index (within this unit)
     * @param local_index Index within this unit (0 to slot_count-1)
     * @return Pointer to slot info or nullptr if out of range
     */
    [[nodiscard]] const SlotInfo* get_slot(int local_index) const {
        if (local_index < 0 || local_index >= static_cast<int>(slots.size())) {
            return nullptr;
        }
        return &slots[local_index];
    }

    /**
     * @brief Get mutable slot by local index (within this unit)
     * @param local_index Index within this unit (0 to slot_count-1)
     * @return Pointer to slot info or nullptr if out of range
     */
    [[nodiscard]] SlotInfo* get_slot(int local_index) {
        if (local_index < 0 || local_index >= static_cast<int>(slots.size())) {
            return nullptr;
        }
        return &slots[local_index];
    }
};

/**
 * @brief Complete AMS system state
 *
 * This is the top-level structure containing all AMS information.
 */
struct AmsSystemInfo {
    AmsType type = AmsType::NONE;
    std::string type_name; ///< "Happy Hare", "AFC", etc.
    std::string version;   ///< System version string

    // Current state
    int current_tool = -1;              ///< Active tool (-1=none, -2=bypass for HH)
    int current_slot = -1;              ///< Active slot (-1=none, -2=bypass for HH)
    bool filament_loaded = false;       ///< Filament at extruder
    AmsAction action = AmsAction::IDLE; ///< Current operation
    std::string operation_detail;       ///< Detailed operation string

    // Units
    std::vector<AmsUnit> units; ///< All AMS units
    int total_slots = 0;        ///< Sum of all slots across units

    // Capability flags
    bool supports_endless_spool = false;
    bool supports_spoolman = false;
    bool supports_tool_mapping = false;
    bool supports_bypass = false;            ///< Has bypass selector position
    bool has_hardware_bypass_sensor = false; ///< true=auto-detect sensor, false=virtual/manual

    // Tool-to-slot mapping (Happy Hare uses "gate" internally)
    std::vector<int> tool_to_slot_map; ///< tool_to_slot_map[tool] = slot

    /**
     * @brief Get slot by global index (across all units)
     * @param global_index Global slot index (0 to total_slots-1)
     * @return Pointer to slot info or nullptr if out of range
     */
    [[nodiscard]] const SlotInfo* get_slot_global(int global_index) const {
        for (const auto& unit : units) {
            if (global_index >= unit.first_slot_global_index &&
                global_index < unit.first_slot_global_index + unit.slot_count) {
                int local_idx = global_index - unit.first_slot_global_index;
                return unit.get_slot(local_idx);
            }
        }
        return nullptr;
    }

    /**
     * @brief Get mutable slot by global index (across all units)
     * @param global_index Global slot index (0 to total_slots-1)
     * @return Pointer to slot info or nullptr if out of range
     */
    [[nodiscard]] SlotInfo* get_slot_global(int global_index) {
        for (auto& unit : units) {
            if (global_index >= unit.first_slot_global_index &&
                global_index < unit.first_slot_global_index + unit.slot_count) {
                int local_idx = global_index - unit.first_slot_global_index;
                return unit.get_slot(local_idx);
            }
        }
        return nullptr;
    }

    /**
     * @brief Get the currently active slot info
     * @return Pointer to active slot or nullptr if none selected
     */
    [[nodiscard]] const SlotInfo* get_active_slot() const {
        if (current_slot < 0)
            return nullptr;
        return get_slot_global(current_slot);
    }

    /**
     * @brief Check if system is available and connected
     * @return true if AMS type is detected and has at least one unit
     */
    [[nodiscard]] bool is_available() const {
        return type != AmsType::NONE && !units.empty();
    }

    /**
     * @brief Check if an operation is in progress
     * @return true if actively loading, unloading, etc.
     */
    [[nodiscard]] bool is_busy() const {
        return action != AmsAction::IDLE && action != AmsAction::ERROR;
    }
};

/**
 * @brief Filament requirement from G-code analysis
 *
 * Used for print preview to show which colors are needed.
 */
struct FilamentRequirement {
    int tool_index = -1;                         ///< Tool number from G-code (T0, T1, etc.)
    uint32_t color_rgb = AMS_DEFAULT_SLOT_COLOR; ///< Color hint from slicer
    std::string material;                        ///< Material hint from slicer (if available)
    int mapped_slot = -1;                        ///< Which slot is mapped to this tool

    /**
     * @brief Check if this requirement is satisfied by a slot
     * @return true if a slot is mapped to this tool
     */
    [[nodiscard]] bool is_satisfied() const {
        return mapped_slot >= 0;
    }
};

/**
 * @brief Print color requirements summary
 */
struct PrintColorInfo {
    std::vector<FilamentRequirement> requirements;
    int initial_tool = 0;       ///< First tool used in print
    bool all_satisfied = false; ///< All requirements have mapped slots
};
