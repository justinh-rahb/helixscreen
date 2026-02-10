// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef HELIX_DISPLAY_SDL

#include "sound_backend.h"
#include "sound_theme.h"

#include <SDL.h>
#include <atomic>
#include <string>

/// SDL2 audio backend -- generates real waveform audio for desktop simulator
class SDLSoundBackend : public SoundBackend {
  public:
    SDLSoundBackend();
    ~SDLSoundBackend() override;

    // SoundBackend interface
    void set_tone(float freq_hz, float amplitude, float duty_cycle) override;
    void silence() override;
    void set_waveform(Waveform w) override;
    void set_filter(const std::string& type, float cutoff) override;
    bool supports_waveforms() const override {
        return true;
    }
    bool supports_amplitude() const override {
        return true;
    }
    bool supports_filter() const override {
        return true;
    }
    float min_tick_ms() const override {
        return 1.0f;
    }

    /// Initialize SDL audio device. Returns false on failure.
    bool initialize();

    /// Shutdown SDL audio device
    void shutdown();

    // ---- Static helpers for testability (called by tests directly) ----

    /// Generate waveform samples into buffer
    /// @param phase Modified in-place for continuity across calls
    static void generate_samples(float* buffer, int num_samples, int sample_rate, Waveform wave,
                                 float freq, float amplitude, float duty_cycle, float& phase);

    /// Biquad filter state (Direct Form II Transposed)
    struct BiquadFilter {
        // Coefficients
        float b0 = 1, b1 = 0, b2 = 0;
        float a1 = 0, a2 = 0;
        // State
        float z1 = 0, z2 = 0;
        bool active = false;
    };

    /// Compute biquad coefficients for lowpass or highpass
    static void compute_biquad_coeffs(BiquadFilter& f, const std::string& type, float cutoff,
                                      float sample_rate);

    /// Apply filter to buffer in-place
    static void apply_filter(BiquadFilter& f, float* buffer, int num_samples);

  private:
    static void audio_callback(void* userdata, uint8_t* stream, int len);

    // Current tone parameters -- written by main thread, read by audio callback
    std::atomic<float> current_freq_{0};
    std::atomic<float> current_amplitude_{0};
    std::atomic<float> current_duty_{0.5f};
    std::atomic<Waveform> current_wave_{Waveform::SQUARE};

    // Filter parameters
    std::atomic<float> filter_cutoff_{20000.0f};
    std::atomic<bool> filter_active_{false};
    std::string filter_type_ = "lowpass"; // Only modified from main thread before play

    // Phase accumulator (only accessed from audio callback thread)
    float phase_ = 0;

    // Filter state (only accessed from audio callback thread)
    BiquadFilter filter_;

    SDL_AudioDeviceID device_id_ = 0;
    int sample_rate_ = 44100;
    bool initialized_ = false;
};

#endif // HELIX_DISPLAY_SDL
