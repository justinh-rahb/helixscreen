// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef HELIX_DISPLAY_SDL

#include "sdl_sound_backend.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SDLSoundBackend::SDLSoundBackend() = default;

SDLSoundBackend::~SDLSoundBackend() {
    shutdown();
}

bool SDLSoundBackend::initialize() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        spdlog::error("[SDLSound] SDL_InitSubSystem(AUDIO) failed: {}", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = sample_rate_;
    desired.format = AUDIO_F32SYS;
    desired.channels = 1;
    desired.samples = 256; // Low latency buffer
    desired.callback = audio_callback;
    desired.userdata = this;

    SDL_AudioSpec obtained{};
    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (device_id_ == 0) {
        spdlog::error("[SDLSound] SDL_OpenAudioDevice failed: {}", SDL_GetError());
        return false;
    }

    sample_rate_ = obtained.freq;
    SDL_PauseAudioDevice(device_id_, 0); // Start playback
    initialized_ = true;

    spdlog::info("[SDLSound] Audio initialized: {} Hz, {} samples buffer", sample_rate_,
                 obtained.samples);
    return true;
}

void SDLSoundBackend::shutdown() {
    if (!initialized_)
        return;
    if (device_id_) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    initialized_ = false;
    spdlog::info("[SDLSound] Audio shutdown");
}

void SDLSoundBackend::set_tone(float freq_hz, float amplitude, float duty_cycle) {
    current_freq_.store(freq_hz, std::memory_order_relaxed);
    current_amplitude_.store(amplitude, std::memory_order_relaxed);
    current_duty_.store(duty_cycle, std::memory_order_relaxed);
}

void SDLSoundBackend::silence() {
    current_amplitude_.store(0, std::memory_order_relaxed);
}

void SDLSoundBackend::set_waveform(Waveform w) {
    current_wave_.store(w, std::memory_order_relaxed);
}

void SDLSoundBackend::set_filter(const std::string& type, float cutoff) {
    if (type.empty()) {
        filter_active_.store(false, std::memory_order_relaxed);
        return;
    }

    filter_type_ = type;
    filter_cutoff_.store(cutoff, std::memory_order_relaxed);

    // Compute coefficients directly into filter state.
    // The audio callback reads these -- rare torn reads cause a brief glitch,
    // which is acceptable for a buzzer synth on a desktop simulator.
    compute_biquad_coeffs(filter_, type, cutoff, static_cast<float>(sample_rate_));
    filter_.z1 = 0;
    filter_.z2 = 0;
    filter_active_.store(true, std::memory_order_relaxed);
}

// --- Static helpers ---

void SDLSoundBackend::generate_samples(float* buffer, int num_samples, int sample_rate,
                                       Waveform wave, float freq, float amplitude, float duty_cycle,
                                       float& phase) {
    const float phase_inc = freq / static_cast<float>(sample_rate);

    for (int i = 0; i < num_samples; ++i) {
        float sample = 0.0f;

        switch (wave) {
        case Waveform::SQUARE:
            sample = (phase < duty_cycle) ? amplitude : -amplitude;
            break;

        case Waveform::SAW:
            sample = amplitude * (2.0f * phase - 1.0f);
            break;

        case Waveform::TRIANGLE:
            sample = amplitude * (4.0f * std::abs(phase - 0.5f) - 1.0f);
            break;

        case Waveform::SINE:
            sample = amplitude * std::sin(2.0f * static_cast<float>(M_PI) * phase);
            break;
        }

        buffer[i] = sample;

        phase += phase_inc;
        phase -= std::floor(phase);
    }
}

void SDLSoundBackend::compute_biquad_coeffs(BiquadFilter& f, const std::string& type, float cutoff,
                                            float sample_rate) {
    constexpr float Q = 0.707107f; // 1/sqrt(2), Butterworth

    // Clamp cutoff to valid range (above 0, below Nyquist)
    cutoff = std::clamp(cutoff, 20.0f, sample_rate * 0.499f);

    const float omega = 2.0f * static_cast<float>(M_PI) * cutoff / sample_rate;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    const float alpha = sin_omega / (2.0f * Q);

    float a0 = 1.0f + alpha;

    if (type == "lowpass") {
        f.b0 = (1.0f - cos_omega) / 2.0f;
        f.b1 = 1.0f - cos_omega;
        f.b2 = (1.0f - cos_omega) / 2.0f;
    } else if (type == "highpass") {
        f.b0 = (1.0f + cos_omega) / 2.0f;
        f.b1 = -(1.0f + cos_omega);
        f.b2 = (1.0f + cos_omega) / 2.0f;
    } else {
        spdlog::warn("[SDLSound] Unknown filter type '{}', defaulting to lowpass", type);
        f.b0 = (1.0f - cos_omega) / 2.0f;
        f.b1 = 1.0f - cos_omega;
        f.b2 = (1.0f - cos_omega) / 2.0f;
    }

    f.a1 = -2.0f * cos_omega;
    f.a2 = 1.0f - alpha;

    // Normalize by a0
    f.b0 /= a0;
    f.b1 /= a0;
    f.b2 /= a0;
    f.a1 /= a0;
    f.a2 /= a0;

    f.active = true;
}

void SDLSoundBackend::apply_filter(BiquadFilter& f, float* buffer, int num_samples) {
    if (!f.active)
        return;

    for (int i = 0; i < num_samples; ++i) {
        float x = buffer[i];
        float y = f.b0 * x + f.z1;
        f.z1 = f.b1 * x - f.a1 * y + f.z2;
        f.z2 = f.b2 * x - f.a2 * y;
        buffer[i] = y;
    }
}

void SDLSoundBackend::audio_callback(void* userdata, uint8_t* stream, int len) {
    auto* self = static_cast<SDLSoundBackend*>(userdata);
    auto* out = reinterpret_cast<float*>(stream);
    int num_samples = len / static_cast<int>(sizeof(float));

    float freq = self->current_freq_.load(std::memory_order_relaxed);
    float amp = self->current_amplitude_.load(std::memory_order_relaxed);
    float duty = self->current_duty_.load(std::memory_order_relaxed);
    Waveform wave = self->current_wave_.load(std::memory_order_relaxed);

    if (amp <= 0.001f || freq <= 0.0f) {
        std::memset(stream, 0, static_cast<size_t>(len));
        return;
    }

    generate_samples(out, num_samples, self->sample_rate_, wave, freq, amp, duty, self->phase_);

    if (self->filter_active_.load(std::memory_order_relaxed)) {
        apply_filter(self->filter_, out, num_samples);
    }
}

#endif // HELIX_DISPLAY_SDL
