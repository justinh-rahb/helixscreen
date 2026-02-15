// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afc_defaults.h"

namespace helix::printer {

std::vector<DeviceSection> afc_default_sections() {
    return {
        {"calibration", "Calibration", 0, "Bowden length and lane calibration"},
        {"speed", "Speed Settings", 1, "Move speed multipliers"},
        {"maintenance", "Maintenance", 2, "Lane tests and motor resets"},
        {"led", "LED & Modes", 3, "LED control and quiet mode"},
        {"hub", "Hub & Cutter", 4, "Blade change and parking"},
        {"tip_forming", "Tip Forming", 5, "Tip shaping configuration"},
        {"purge", "Purge & Wipe", 6, "Purge tower and brush settings"},
        {"config", "Configuration", 7, "System configuration and mapping"},
    };
}

std::vector<DeviceAction> afc_default_actions() {
    std::vector<DeviceAction> actions;

    // Calibration section
    actions.push_back({
        .id = "calibration_wizard",
        .label = "Calibration Wizard",
        .icon = "play",
        .section = "calibration",
        .description = "Interactive calibration for all lanes",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "bowden_length",
        .label = "Bowden Length",
        .icon = "ruler",
        .section = "calibration",
        .description = "Distance from hub to toolhead",
        .type = ActionType::SLIDER,
        .current_value = std::any(450.0f),
        .options = {},
        .min_value = 100,
        .max_value = 2000,
        .unit = "mm",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    // Speed section
    actions.push_back({
        .id = "speed_fwd",
        .label = "Forward Speed",
        .icon = "fast-forward",
        .section = "speed",
        .description = "Speed multiplier for forward moves",
        .type = ActionType::SLIDER,
        .current_value = std::any(1.0f),
        .options = {},
        .min_value = 0.5f,
        .max_value = 2.0f,
        .unit = "x",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "speed_rev",
        .label = "Reverse Speed",
        .icon = "rewind",
        .section = "speed",
        .description = "Speed multiplier for reverse moves",
        .type = ActionType::SLIDER,
        .current_value = std::any(1.0f),
        .options = {},
        .min_value = 0.5f,
        .max_value = 2.0f,
        .unit = "x",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    // Maintenance section
    actions.push_back({
        .id = "test_lanes",
        .label = "Test Lanes",
        .icon = "test-tube",
        .section = "maintenance",
        .description = "Run test sequence on all lanes",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "change_blade",
        .label = "Change Blade",
        .icon = "box-cutter",
        .section = "maintenance",
        .description = "Initiate blade change procedure",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "park",
        .label = "Park",
        .icon = "parking",
        .section = "maintenance",
        .description = "Park the AFC system",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "brush",
        .label = "Brush",
        .icon = "broom",
        .section = "maintenance",
        .description = "Run brush cleaning sequence",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "reset_motor",
        .label = "Reset Motor",
        .icon = "timer-refresh",
        .section = "maintenance",
        .description = "Reset motor run-time counter",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    // LED & Modes section
    actions.push_back({
        .id = "led_toggle",
        .label = "LED On",
        .icon = "lightbulb-on",
        .section = "led",
        .description = "Toggle AFC LED strip",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    actions.push_back({
        .id = "quiet_mode",
        .label = "Quiet Mode",
        .icon = "volume-off",
        .section = "led",
        .description = "Enable/disable quiet operation mode",
        .type = ActionType::BUTTON,
        .current_value = {},
        .options = {},
        .min_value = 0,
        .max_value = 0,
        .unit = "",
        .slot_index = -1,
        .enabled = true,
        .disable_reason = "",
    });

    return actions;
}

AfcCapabilities afc_default_capabilities() {
    return {
        .supports_endless_spool = true,
        .supports_spoolman = true,
        .supports_tool_mapping = true,
        .supports_bypass = true,
        .supports_purge = true,
        .tip_method = TipMethod::CUT,
    };
}

} // namespace helix::printer
