// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sound_backend.h"

#include <functional>
#include <string>

/// M300 G-code backend -- sends M300 beeper commands via Moonraker/Klipper
///
/// Uses a GcodeSender callback to decouple from MoonrakerClient, allowing
/// easy testing with a captured lambda. Deduplicates redundant frequency
/// commands and clamps to the M300-safe range of 100-10000 Hz.
class M300SoundBackend : public SoundBackend {
  public:
    /// Callback type for sending G-code strings (e.g., wrapping MoonrakerClient::gcode_script)
    using GcodeSender = std::function<int(const std::string&)>;

    /// @param sender  Function that sends a G-code string. May be empty/null (safe no-op).
    explicit M300SoundBackend(GcodeSender sender);

    // SoundBackend interface
    void set_tone(float freq_hz, float amplitude, float duty_cycle) override;
    void silence() override;
    float min_tick_ms() const override;

  private:
    GcodeSender sender_;
    int last_freq_ = 0;
};
