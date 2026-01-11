// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/**
 * @file printer_hardware_discovery.h
 * @brief Single source of truth for all discovered printer hardware
 *
 * This class consolidates:
 * - Hardware lists (heaters, fans, sensors, leds, steppers) from MoonrakerClient
 * - Capability flags (has_qgl, has_probe, etc.) from PrinterCapabilities
 * - Macros from PrinterCapabilities
 * - AMS/MMU detection from PrinterCapabilities
 */

#include "ams_types.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

#include "hv/json.hpp"

namespace helix {

class PrinterHardwareDiscovery {
  public:
    PrinterHardwareDiscovery() = default;

    /**
     * @brief Parse Klipper objects from printer.objects.list response
     *
     * Extracts all hardware components and capabilities from the object list.
     * This is the single entry point for hardware discovery.
     *
     * @param objects JSON array of object names from printer.objects.list
     */
    void parse_objects(const nlohmann::json& objects) {
        clear();

        // Validate input is an array
        if (!objects.is_array()) {
            return;
        }

        for (const auto& obj : objects) {
            // Skip non-string elements
            if (!obj.is_string()) {
                continue;
            }
            std::string name = obj.template get<std::string>();

            // Skip empty strings
            if (name.empty()) {
                continue;
            }

            std::string upper_name = to_upper(name);

            // ================================================================
            // Steppers (stepper_x, stepper_y, stepper_z, stepper_z1, etc.)
            // ================================================================
            if (name.rfind("stepper_", 0) == 0) {
                steppers_.push_back(name);
            }
            // ================================================================
            // Heaters: extruders, heater_bed, heater_generic
            // ================================================================
            // Match "extruder", "extruder1", etc., but NOT "extruder_stepper"
            else if (name.rfind("extruder", 0) == 0 && name.rfind("extruder_stepper", 0) != 0) {
                heaters_.push_back(name);
            }
            // Heated bed
            else if (name == "heater_bed") {
                heaters_.push_back(name);
                has_heater_bed_ = true;
            }
            // Generic heaters (e.g., "heater_generic chamber")
            else if (name.rfind("heater_generic ", 0) == 0) {
                heaters_.push_back(name);
                // Check for chamber heater
                std::string heater_name = name.substr(15); // Remove "heater_generic " prefix
                if (to_upper(heater_name).find("CHAMBER") != std::string::npos) {
                    has_chamber_heater_ = true;
                }
            }
            // ================================================================
            // Sensors: temperature_sensor, temperature_fan (dual-purpose)
            // ================================================================
            else if (name.rfind("temperature_sensor ", 0) == 0) {
                sensors_.push_back(name);
                // Check for chamber sensor
                std::string sensor_name = name.substr(19); // Remove "temperature_sensor " prefix
                if (to_upper(sensor_name).find("CHAMBER") != std::string::npos) {
                    has_chamber_sensor_ = true;
                }
            }
            // Temperature-controlled fans (also act as sensors)
            else if (name.rfind("temperature_fan ", 0) == 0) {
                sensors_.push_back(name);
                fans_.push_back(name); // Also add to fans for control
            }
            // ================================================================
            // Fans: fan, heater_fan, fan_generic, controller_fan
            // ================================================================
            else if (name == "fan") {
                fans_.push_back(name);
            } else if (name.rfind("heater_fan ", 0) == 0) {
                fans_.push_back(name);
            } else if (name.rfind("fan_generic ", 0) == 0) {
                fans_.push_back(name);
            } else if (name.rfind("controller_fan ", 0) == 0) {
                fans_.push_back(name);
            }
            // ================================================================
            // LEDs: neopixel, dotstar, led
            // ================================================================
            else if (name.rfind("neopixel ", 0) == 0 || name == "neopixel") {
                leds_.push_back(name);
                has_led_ = true;
            } else if (name.rfind("dotstar ", 0) == 0 || name == "dotstar") {
                leds_.push_back(name);
                has_led_ = true;
            } else if (name.rfind("led ", 0) == 0) {
                leds_.push_back(name);
                has_led_ = true;
            }
            // Output pins with LED/LIGHT in name
            else if (name.rfind("output_pin ", 0) == 0) {
                std::string pin_name = name.substr(11); // Remove "output_pin " prefix
                std::string upper_pin = to_upper(pin_name);
                if (upper_pin.find("LIGHT") != std::string::npos ||
                    upper_pin.find("LED") != std::string::npos) {
                    has_led_ = true;
                }
            }
            // ================================================================
            // Capability flags
            // ================================================================
            else if (name == "quad_gantry_level") {
                has_qgl_ = true;
            } else if (name == "z_tilt") {
                has_z_tilt_ = true;
            } else if (name == "bed_mesh") {
                has_bed_mesh_ = true;
            } else if (name == "probe" || name == "bltouch") {
                has_probe_ = true;
            } else if (name.rfind("probe_eddy_current ", 0) == 0) {
                has_probe_ = true;
            } else if (name == "firmware_retraction") {
                has_firmware_retraction_ = true;
            } else if (name == "timelapse") {
                has_timelapse_ = true;
            }
            // Accelerometer detection
            else if (name == "adxl345" || name.rfind("adxl345 ", 0) == 0 || name == "lis2dw" ||
                     name.rfind("lis2dw ", 0) == 0 || name == "mpu9250" ||
                     name.rfind("mpu9250 ", 0) == 0 || name == "resonance_tester") {
                has_accelerometer_ = true;
            }
            // ================================================================
            // MMU/AMS detection
            // ================================================================
            else if (name == "mmu") {
                has_mmu_ = true;
                mmu_type_ = AmsType::HAPPY_HARE;
            } else if (name == "AFC") {
                has_mmu_ = true;
                mmu_type_ = AmsType::AFC;
            }
            // AFC lane discovery
            else if (name.rfind("AFC_stepper ", 0) == 0) {
                std::string lane_name = name.substr(12); // Remove "AFC_stepper " prefix
                if (!lane_name.empty()) {
                    afc_lane_names_.push_back(lane_name);
                }
            }
            // AFC hub discovery
            else if (name.rfind("AFC_hub ", 0) == 0) {
                std::string hub_name = name.substr(8); // Remove "AFC_hub " prefix
                if (!hub_name.empty()) {
                    afc_hub_names_.push_back(hub_name);
                }
            }
            // Tool changer detection
            else if (name == "toolchanger") {
                has_tool_changer_ = true;
            }
            // Tool object discovery
            else if (name.rfind("tool ", 0) == 0) {
                std::string tool_name = name.substr(5); // Remove "tool " prefix
                if (!tool_name.empty()) {
                    tool_names_.push_back(tool_name);
                }
            }
            // ================================================================
            // Filament sensors
            // ================================================================
            else if (name.rfind("filament_switch_sensor ", 0) == 0 ||
                     name.rfind("filament_motion_sensor ", 0) == 0) {
                filament_sensor_names_.push_back(name);
            }
            // ================================================================
            // Macro detection
            // ================================================================
            else if (name.rfind("gcode_macro ", 0) == 0) {
                std::string macro_name = name.substr(12); // Remove "gcode_macro " prefix
                std::string upper_macro = to_upper(macro_name);

                macros_.insert(upper_macro);

                // Check for common macro patterns and cache them
                if (nozzle_clean_macro_.empty()) {
                    static const std::vector<std::string> nozzle_patterns = {
                        "CLEAN_NOZZLE", "NOZZLE_WIPE", "WIPE_NOZZLE", "PURGE_NOZZLE",
                        "NOZZLE_CLEAN"};
                    if (matches_any(upper_macro, nozzle_patterns)) {
                        nozzle_clean_macro_ = macro_name;
                    }
                }

                if (purge_line_macro_.empty()) {
                    static const std::vector<std::string> purge_patterns = {
                        "PURGE_LINE", "PRIME_LINE", "INTRO_LINE", "LINE_PURGE"};
                    if (matches_any(upper_macro, purge_patterns)) {
                        purge_line_macro_ = macro_name;
                    }
                }

                if (heat_soak_macro_.empty()) {
                    static const std::vector<std::string> soak_patterns = {
                        "HEAT_SOAK", "CHAMBER_SOAK", "SOAK", "BED_SOAK"};
                    if (matches_any(upper_macro, soak_patterns)) {
                        heat_soak_macro_ = macro_name;
                    }
                }
            }
        }

        // Sort AFC lane names for consistent ordering
        if (!afc_lane_names_.empty()) {
            std::sort(afc_lane_names_.begin(), afc_lane_names_.end());
        }

        // Sort tool names for consistent ordering
        if (!tool_names_.empty()) {
            std::sort(tool_names_.begin(), tool_names_.end());
        }

        // Set mmu_type_ for tool changers (after all objects processed)
        if (has_tool_changer_ && !tool_names_.empty()) {
            mmu_type_ = AmsType::TOOL_CHANGER;
        }
    }

    /**
     * @brief Reset all discovered hardware to initial state
     */
    void clear() {
        // Hardware lists
        heaters_.clear();
        fans_.clear();
        sensors_.clear();
        leds_.clear();
        steppers_.clear();

        // AMS/MMU discovery
        afc_lane_names_.clear();
        afc_hub_names_.clear();
        tool_names_.clear();
        filament_sensor_names_.clear();

        // Macros
        macros_.clear();
        nozzle_clean_macro_.clear();
        purge_line_macro_.clear();
        heat_soak_macro_.clear();

        // Capability flags
        has_qgl_ = false;
        has_z_tilt_ = false;
        has_bed_mesh_ = false;
        has_probe_ = false;
        has_heater_bed_ = false;
        has_mmu_ = false;
        has_tool_changer_ = false;
        has_chamber_heater_ = false;
        has_chamber_sensor_ = false;
        has_led_ = false;
        has_accelerometer_ = false;
        has_firmware_retraction_ = false;
        has_timelapse_ = false;
        mmu_type_ = AmsType::NONE;
    }

    // ========================================================================
    // Hardware Lists
    // ========================================================================

    [[nodiscard]] const std::vector<std::string>& heaters() const {
        return heaters_;
    }

    [[nodiscard]] const std::vector<std::string>& fans() const {
        return fans_;
    }

    [[nodiscard]] const std::vector<std::string>& sensors() const {
        return sensors_;
    }

    [[nodiscard]] const std::vector<std::string>& leds() const {
        return leds_;
    }

    [[nodiscard]] const std::vector<std::string>& steppers() const {
        return steppers_;
    }

    // ========================================================================
    // Capability Flags
    // ========================================================================

    [[nodiscard]] bool has_qgl() const {
        return has_qgl_;
    }

    [[nodiscard]] bool has_z_tilt() const {
        return has_z_tilt_;
    }

    [[nodiscard]] bool has_bed_mesh() const {
        return has_bed_mesh_;
    }

    [[nodiscard]] bool has_probe() const {
        return has_probe_;
    }

    [[nodiscard]] bool has_heater_bed() const {
        return has_heater_bed_;
    }

    [[nodiscard]] bool has_mmu() const {
        return has_mmu_;
    }

    [[nodiscard]] bool has_tool_changer() const {
        return has_tool_changer_;
    }

    [[nodiscard]] bool has_chamber_heater() const {
        return has_chamber_heater_;
    }

    [[nodiscard]] bool has_chamber_sensor() const {
        return has_chamber_sensor_;
    }

    [[nodiscard]] bool has_led() const {
        return has_led_;
    }

    [[nodiscard]] bool has_accelerometer() const {
        return has_accelerometer_;
    }

    [[nodiscard]] bool has_filament_sensors() const {
        return !filament_sensor_names_.empty();
    }

    [[nodiscard]] bool has_firmware_retraction() const {
        return has_firmware_retraction_;
    }

    [[nodiscard]] bool has_timelapse() const {
        return has_timelapse_;
    }

    [[nodiscard]] bool supports_leveling() const {
        return has_qgl() || has_z_tilt() || has_bed_mesh();
    }

    [[nodiscard]] bool supports_chamber() const {
        return has_chamber_heater() || has_chamber_sensor();
    }

    // ========================================================================
    // AMS/MMU Detection
    // ========================================================================

    [[nodiscard]] AmsType mmu_type() const {
        return mmu_type_;
    }

    [[nodiscard]] const std::vector<std::string>& afc_lane_names() const {
        return afc_lane_names_;
    }

    [[nodiscard]] const std::vector<std::string>& afc_hub_names() const {
        return afc_hub_names_;
    }

    [[nodiscard]] const std::vector<std::string>& tool_names() const {
        return tool_names_;
    }

    [[nodiscard]] const std::vector<std::string>& filament_sensor_names() const {
        return filament_sensor_names_;
    }

    // ========================================================================
    // Macro Detection
    // ========================================================================

    [[nodiscard]] const std::unordered_set<std::string>& macros() const {
        return macros_;
    }

    /**
     * @brief Check if a macro exists (case-insensitive)
     * @param name Macro name to check
     * @return true if the macro exists
     */
    [[nodiscard]] bool has_macro(const std::string& name) const {
        return macros_.count(to_upper(name)) > 0;
    }

    [[nodiscard]] std::string nozzle_clean_macro() const {
        return nozzle_clean_macro_;
    }

    [[nodiscard]] std::string purge_line_macro() const {
        return purge_line_macro_;
    }

    [[nodiscard]] std::string heat_soak_macro() const {
        return heat_soak_macro_;
    }

  private:
    // Helper: convert string to uppercase
    static std::string to_upper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    // Helper: check if name matches any pattern
    static bool matches_any(const std::string& name, const std::vector<std::string>& patterns) {
        for (const auto& pattern : patterns) {
            if (name == pattern) {
                return true;
            }
        }
        return false;
    }

    // Hardware lists
    std::vector<std::string> heaters_;
    std::vector<std::string> fans_;
    std::vector<std::string> sensors_;
    std::vector<std::string> leds_;
    std::vector<std::string> steppers_;

    // AMS/MMU discovery
    std::vector<std::string> afc_lane_names_;
    std::vector<std::string> afc_hub_names_;
    std::vector<std::string> tool_names_;
    std::vector<std::string> filament_sensor_names_;

    // Macros
    std::unordered_set<std::string> macros_;
    std::string nozzle_clean_macro_;
    std::string purge_line_macro_;
    std::string heat_soak_macro_;

    // Capability flags
    bool has_qgl_ = false;
    bool has_z_tilt_ = false;
    bool has_bed_mesh_ = false;
    bool has_probe_ = false;
    bool has_heater_bed_ = false;
    bool has_mmu_ = false;
    bool has_tool_changer_ = false;
    bool has_chamber_heater_ = false;
    bool has_chamber_sensor_ = false;
    bool has_led_ = false;
    bool has_accelerometer_ = false;
    bool has_firmware_retraction_ = false;
    bool has_timelapse_ = false;
    AmsType mmu_type_ = AmsType::NONE;
};

} // namespace helix
