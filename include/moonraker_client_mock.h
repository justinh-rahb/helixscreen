/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MOONRAKER_CLIENT_MOCK_H
#define MOONRAKER_CLIENT_MOCK_H

#include "moonraker_client.h"
#include <string>
#include <vector>

/**
 * @brief Mock Moonraker client for testing without real printer connection
 *
 * Simulates printer hardware discovery with configurable test data.
 * Useful for UI development and testing without physical hardware.
 *
 * Inherits from MoonrakerClient to provide drop-in replacement compatibility.
 * Overrides discover_printer() to populate test data without WebSocket connection.
 */
class MoonrakerClientMock : public MoonrakerClient {
public:
    enum class PrinterType {
        VORON_24,           // Voron 2.4 (CoreXY, chamber heating)
        VORON_TRIDENT,      // Voron Trident (3Z, CoreXY)
        CREALITY_K1,        // Creality K1/K1 Max (bed slinger style)
        FLASHFORGE_AD5M,    // FlashForge Adventurer 5M (enclosed)
        GENERIC_COREXY,     // Generic CoreXY printer
        GENERIC_BEDSLINGER, // Generic i3-style printer
        MULTI_EXTRUDER      // Multi-extruder test case (2 extruders)
    };

    MoonrakerClientMock(PrinterType type = PrinterType::VORON_24);
    ~MoonrakerClientMock() = default;

    /**
     * @brief Simulate printer hardware discovery
     *
     * Overrides base class method to immediately populate hardware lists
     * based on configured printer type and invoke completion callback.
     *
     * @param on_complete Callback invoked after discovery completes
     */
    void discover_printer(std::function<void()> on_complete) override;

    /**
     * @brief Set printer type for mock data generation
     *
     * @param type Printer type to simulate
     */
    void set_printer_type(PrinterType type) { printer_type_ = type; }

private:
    /**
     * @brief Populate hardware lists based on configured printer type
     *
     * Directly modifies the protected member variables inherited from
     * MoonrakerClient (heaters_, sensors_, fans_, leds_).
     */
    void populate_hardware();

private:
    PrinterType printer_type_;
};

#endif // MOONRAKER_CLIENT_MOCK_H
