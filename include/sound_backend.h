// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

/// Forward declaration — defined in sound_theme.h
enum class Waveform;

/**
 * @brief Abstract interface for sound output backends
 *
 * The sequencer calls set_tone() at ~1ms tick rate to produce sound.
 * Backends report their capabilities so the sequencer can adapt behavior
 * (e.g., skip waveform selection for M300, skip filter for PWM).
 *
 * Implementations: SDLBackend (desktop), PWMBackend (AD5M), M300Backend (Klipper)
 */
class SoundBackend {
  public:
    virtual ~SoundBackend() = default;

    /// Called by sequencer at ~1ms tick rate to set current output
    /// @param freq_hz   Frequency in Hz (20-20000)
    /// @param amplitude Volume level 0.0-1.0
    /// @param duty_cycle Duty cycle 0.0-1.0 (for square-ish waveforms)
    virtual void set_tone(float freq_hz, float amplitude, float duty_cycle) = 0;

    /// Stop all sound output immediately
    virtual void silence() = 0;

    /// Whether backend can synthesize different waveform shapes
    virtual bool supports_waveforms() const {
        return false;
    }

    /// Whether backend has real amplitude/volume control
    virtual bool supports_amplitude() const {
        return false;
    }

    /// Whether backend can apply DSP filters (lowpass/highpass)
    virtual bool supports_filter() const {
        return false;
    }

    /// Set the active waveform type (only called if supports_waveforms() is true)
    virtual void set_waveform(Waveform /* w */) {}

    /// Set active filter parameters (only called if supports_filter() is true)
    /// @param type "lowpass" or "highpass"
    /// @param cutoff Filter cutoff frequency in Hz
    virtual void set_filter(const std::string& /* type */, float /* cutoff */) {}

    /// Minimum tick interval the backend can handle (ms)
    virtual float min_tick_ms() const {
        return 1.0f;
    }
};
