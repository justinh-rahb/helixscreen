// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sound_backend.h"
#include "sound_theme.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

/// Sound priority levels (higher numeric value = more important)
enum class SoundPriority {
    UI = 0,    // button taps, nav sounds — can be interrupted by anything
    EVENT = 1, // print complete, errors — only interrupted by ALARM
    ALARM = 2  // critical alerts — never interrupted
};

/// Core playback engine for synthesized sounds.
/// Runs a dedicated thread that ticks at ~1ms to drive the backend.
class SoundSequencer {
  public:
    explicit SoundSequencer(std::shared_ptr<SoundBackend> backend);
    ~SoundSequencer();

    /// Non-blocking: queues sound for playback from any thread
    void play(const SoundDefinition& sound, SoundPriority priority = SoundPriority::UI);

    /// Stop current playback immediately
    void stop();

    /// Check if currently playing
    bool is_playing() const;

    /// Start the sequencer thread
    void start();

    /// Stop the sequencer thread (blocks until joined)
    void shutdown();

  private:
    /// Request pushed from play() to the sequencer thread
    struct PlayRequest {
        SoundDefinition sound;
        SoundPriority priority;
    };

    /// Internal state for tracking playback within a single step
    struct StepState {
        float elapsed_ms = 0;     // time elapsed in current step
        float total_ms = 0;       // total duration of the step
        int step_index = 0;       // current step index in the sequence
        int repeat_remaining = 0; // repeats left after current pass
    };

    /// Sequencer thread function
    void sequencer_loop();

    /// Process one tick (~1ms) of the current sound
    void tick(float dt_ms);

    /// Advance to the next step, or finish playback
    void advance_step();

    /// ADSR envelope computation
    /// @return amplitude multiplier 0.0-1.0
    float compute_envelope(const ADSREnvelope& env, float elapsed_ms, float duration_ms) const;

    /// LFO computation
    /// @return modulation offset to apply to the target parameter
    float compute_lfo(const LFOParams& lfo, float elapsed_ms) const;

    /// Sweep interpolation
    /// @return interpolated value between start and end
    float compute_sweep(float start, float end, float progress) const;

    /// Start playing a request (called from sequencer thread)
    void begin_playback(PlayRequest&& req);

    /// End the current playback and silence the backend
    void end_playback();

    std::shared_ptr<SoundBackend> backend_;
    std::thread sequencer_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> playing_{false};

    // Queue protected by mutex + condvar for efficient wakeup
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<PlayRequest> request_queue_;

    // Current playback state (only accessed from sequencer thread)
    SoundDefinition current_sound_;
    SoundPriority current_priority_ = SoundPriority::UI;
    StepState step_state_;

    // Signaled by stop() to halt current playback
    std::atomic<bool> stop_requested_{false};
};
