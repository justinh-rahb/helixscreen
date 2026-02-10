# Sound System v2 — Multi-Backend Synthesizer Engine

**Date**: 2026-02-07
**Status**: Implemented (feature/sound-system-v2, 9 commits, 135 tests)
**Approach**: TDD — tests first, implementation follows

---

## Motivation

HelixScreen has a `SoundManager` that sends M300 G-codes through Moonraker. The infrastructure works: speaker capability detection (`output_pin beeper`), settings UI (sounds toggle, completion alert mode), and three sound methods (`play_test_beep`, `play_print_complete`, `play_error_alert`).

**Problems:**
1. `play_print_complete()` and `play_error_alert()` are implemented but **never called**
2. No UI interaction sounds (button taps, navigation, toggles)
3. Everything goes through M300/Moonraker — no local audio for platforms with real speakers
4. AD5M's PWM buzzer could be driven directly for lower latency
5. M300 is limited to simple `frequency + duration` — no expressiveness

**Goal:** A multi-backend sequence player with synth primitives that reads sound definitions from JSON. One sound definition, three levels of fidelity. Make a PWM buzzer sound expensive.

---

## Architecture Overview

```
                         +---------------------+
   SoundManager          |   SoundSequencer    |
   (public API)          |   (dedicated thread) |
                         |                      |
  play("button_tap") -->  |  sequence queue      |
  play("print_complete")->|  tick @ ~1ms         |
  stop()                 |  ADSR state machine  |
  set_theme("retro")    |  LFO oscillators     |
                         |  sweep interpolation |
                         +----------+-----------+
                                    |
                              SoundBackend*
                                    |
                    +---------------+---------------+
                    v               v               v
              SDLBackend       PWMBackend      M300Backend
              (simulator)      (AD5M sysfs)    (Moonraker)
                    |               |               |
              SDL_QueueAudio   /sys/class/pwm   gcode_script()
              real waveforms   duty+freq+enable  "M300 S_ P_"
```

### Backend Interface

```cpp
class SoundBackend {
public:
    virtual ~SoundBackend() = default;

    // Called by sequencer at ~1ms tick rate
    virtual void set_tone(float freq_hz, float amplitude, float duty_cycle) = 0;
    virtual void silence() = 0;

    // Backend capabilities (sequencer adapts behavior)
    virtual bool supports_waveforms() const { return false; }
    virtual bool supports_amplitude() const { return false; }
    virtual bool supports_filter() const { return false; }
    virtual float min_tick_ms() const { return 1.0f; }
};
```

### Backend Behaviors

**SDLBackend** — full experience (desktop simulator):
- Generates actual waveform samples (square/saw/triangle/sine)
- Real amplitude control, real DSP filtering
- Pushes PCM audio via `SDL_QueueAudio`
- Reference implementation — what the sound *should* sound like

**PWMBackend** — the scrappy one (AD5M direct):
- Maps `freq` to PWM period, `duty_cycle` to PWM duty
- Approximates amplitude via duty cycle scaling
- Sweeps/LFOs work by rapid sysfs writes (~1ms tick)
- Waveform selection maps to duty cycle presets (square=50%, saw~25%, triangle~35%)

**M300Backend** — the fallback (any Klipper printer):
- Pre-computes sequence into `[{freq, duration}]` pairs at play time
- Sends via Moonraker's `gcode_script()`
- Loses continuous modulation; melodies and rhythms still work
- LFOs become repeated notes, sweeps become start/end jumps

**Auto-detection order:**
1. SDL audio available (desktop build) -> SDLBackend
2. `/sys/class/pwm/pwmchip0` exists -> PWMBackend
3. Moonraker connected + `output_pin beeper` -> M300Backend
4. None -> sounds disabled, no thread spawned

---

## Synth Primitives

Each step in a sequence is a note event processed by the synthesizer.

| Primitive | What it does | SDL | PWM | M300 |
|-----------|-------------|-----|-----|------|
| **note/freq** | Pitch (note name or Hz) | exact | exact | exact |
| **dur** | Duration (ms or musical: "4n", "8n") | exact | exact | exact |
| **wave** | Waveform: square/saw/triangle/sine | real synthesis | duty cycle approx | ignored (square) |
| **vel** | Velocity/amplitude 0.0-1.0 | volume scaling | duty cycle scaling | ignored |
| **env (ADSR)** | Amplitude envelope (ms/level) | real envelope | on/off timing | single M300 dur |
| **lfo** | Oscillate any param (rate + depth) | smooth modulation | stepped freq/duty | repeated M300s |
| **sweep** | Glide any param over step duration | smooth interpolation | stepped updates | start->end only |
| **filter** | LP/HP filter + optional sweep | real DSP filter | simulated via freq | ignored |
| **pause** | Silence between steps | exact | exact | exact |
| **repeat** | Loop the full sequence N times | exact | exact | exact |
| **portamento** | Glide between consecutive notes | smooth | stepped | two M300s |
| **bpm** | Tempo reference for musical durations | exact | exact | approx |

### Graceful Degradation

One JSON definition, three fidelity levels. SDL is the reference. PWM approximates through duty cycle and timing tricks. M300 gets the broad strokes. Backends never error on unsupported features — they just do their best.

---

## JSON Sound Theme Format

```json
{
  "name": "default",
  "description": "Balanced, tasteful sound theme",
  "version": 1,
  "defaults": {
    "wave": "square",
    "vel": 0.8,
    "env": { "a": 5, "d": 40, "s": 0.6, "r": 80 }
  },
  "sounds": {
    "button_tap": {
      "description": "Crisp tactile click",
      "steps": [
        { "freq": 4000, "dur": 6, "wave": "square", "vel": 0.9,
          "env": { "a": 1, "d": 5, "s": 0, "r": 1 } }
      ]
    },
    "toggle_on": {
      "description": "Satisfying two-tone confirmation",
      "steps": [
        { "freq": 1200, "dur": 30 },
        { "freq": 1800, "dur": 40 }
      ]
    },
    "toggle_off": {
      "steps": [
        { "freq": 1800, "dur": 30 },
        { "freq": 1200, "dur": 40 }
      ]
    },
    "nav_forward": {
      "description": "Ascending chirp with filter sweep",
      "steps": [
        { "freq": 600, "dur": 50, "wave": "saw",
          "sweep": { "target": "freq", "end": 2400 },
          "filter": { "type": "lowpass", "cutoff": 800, "sweep_to": 4000 },
          "env": { "a": 2, "d": 10, "s": 0.7, "r": 20 } }
      ]
    },
    "nav_back": {
      "description": "Descending chirp",
      "steps": [
        { "freq": 2400, "dur": 50, "wave": "saw",
          "sweep": { "target": "freq", "end": 600 },
          "filter": { "type": "lowpass", "cutoff": 4000, "sweep_to": 800 },
          "env": { "a": 2, "d": 10, "s": 0.7, "r": 20 } }
      ]
    },
    "dropdown_open": {
      "description": "Soft pop",
      "steps": [
        { "freq": 1800, "dur": 15, "vel": 0.5,
          "env": { "a": 1, "d": 14, "s": 0, "r": 1 } }
      ]
    },
    "print_complete": {
      "description": "Triumphant arpeggio",
      "bpm": 140,
      "steps": [
        { "note": "C5", "dur": "8n", "wave": "square", "vel": 0.8,
          "env": { "a": 5, "d": 40, "s": 0.6, "r": 80 } },
        { "note": "E5", "dur": "8n" },
        { "note": "G5", "dur": "8n" },
        { "note": "C6", "dur": "4n", "vel": 1.0,
          "env": { "a": 5, "d": 60, "s": 0.8, "r": 200 } }
      ]
    },
    "print_cancelled": {
      "description": "Descending minor tone",
      "steps": [
        { "note": "E5", "dur": 120, "vel": 0.6 },
        { "note": "C5", "dur": 200, "vel": 0.4,
          "env": { "a": 5, "d": 50, "s": 0.3, "r": 150 } }
      ]
    },
    "error_alert": {
      "description": "Urgent pulsing alert",
      "steps": [
        { "freq": 2400, "dur": 150, "wave": "saw",
          "lfo": { "target": "amplitude", "rate": 8, "depth": 0.5 },
          "env": { "a": 2, "d": 20, "s": 0.9, "r": 30 } },
        { "pause": 80 },
        { "freq": 2400, "dur": 150,
          "lfo": { "target": "amplitude", "rate": 8, "depth": 0.5 } }
      ],
      "repeat": 3
    },
    "error_tone": {
      "description": "Short error feedback for toasts",
      "steps": [
        { "freq": 200, "dur": 80, "wave": "square", "vel": 0.7 },
        { "pause": 40 },
        { "freq": 150, "dur": 120, "vel": 0.5 }
      ]
    },
    "alarm_urgent": {
      "description": "Continuous alarm for critical failures",
      "steps": [
        { "freq": 2500, "dur": 200, "wave": "saw",
          "lfo": { "target": "freq", "rate": 4, "depth": 300 } },
        { "pause": 100 },
        { "freq": 1800, "dur": 200,
          "lfo": { "target": "freq", "rate": 4, "depth": 300 } },
        { "pause": 100 }
      ],
      "repeat": 5
    },
    "test_beep": {
      "description": "Settings test sound",
      "steps": [
        { "freq": 1000, "dur": 100, "vel": 0.8,
          "env": { "a": 5, "d": 20, "s": 0.6, "r": 30 } }
      ]
    }
  }
}
```

### Note Name Support

Standard note names map to frequencies. Supports octaves 0-8:

```
C4 = 261.63 Hz (middle C)
A4 = 440.00 Hz (concert pitch)
C5 = 523.25 Hz
```

Sharps: `"C#5"`, `"Eb5"` (flats also accepted). Parsed at load time, stored as Hz internally.

### Musical Duration Support

When `bpm` is set on a sound, durations can use musical notation:

| Notation | Meaning |
|----------|---------|
| `"1n"` | Whole note |
| `"2n"` | Half note |
| `"4n"` | Quarter note |
| `"8n"` | Eighth note |
| `"16n"` | Sixteenth note |
| `"4n."` | Dotted quarter |
| `"8t"` | Eighth triplet |

Numeric durations (in ms) are always accepted regardless of bpm.

---

## Integration Points

### Event Hookup

| Event | Sound | Trigger Location |
|-------|-------|-----------------|
| Button tap | `button_tap` | Global XML event callback |
| Toggle on/off | `toggle_on` / `toggle_off` | Global event callback (detect state) |
| Navigation forward | `nav_forward` | `NavigationManager::push()` / `push_overlay()` |
| Navigation back | `nav_back` | `NavigationManager::go_back()` |
| Dropdown open | `dropdown_open` | Global event callback |
| Print complete | `print_complete` | `print_completion.cpp` |
| Print error | `error_alert` | `print_completion.cpp` |
| Print cancelled | `print_cancelled` | `print_completion.cpp` |
| Error toast | `error_tone` | `ui_toast_show()` with ERROR severity |
| Critical alarm | `alarm_urgent` | Temperature runaway, connectivity loss |
| Settings test | `test_beep` | Settings panel sound toggle |

Most UI sounds hook at 2-3 chokepoints (nav manager, global event cb, toast system).

### Thread Model

```
LVGL thread (main)                  Sequencer thread
     |                                    |
     |  play("button_tap")               |
     |------ push to lock-free queue --->|
     |                                    | tick tick tick
     |                                    | set_tone(4000, 0.9, 50)
     |                                    | ...6ms later...
     |                                    | silence()
```

- `play()` is non-blocking — pushes sound ID onto a lock-free queue
- Sequencer thread pops and plays
- Priority rules: `alarm_urgent` > `print_complete`/`error_alert` > UI sounds
- UI sounds (taps, nav) interrupt each other freely
- Event sounds (completion, error) are never interrupted by UI sounds

### Settings

| Setting | Type | Values | Notes |
|---------|------|--------|-------|
| `sounds_enabled` | bool | true/false | Existing — master switch |
| `completion_alert` | enum | Off/Notification/Alert | Existing — ALERT now triggers sound |
| `sound_theme` | string | "default", "retro", etc. | **New** — active JSON theme |
| `ui_sounds_enabled` | bool | true/false | **New** — separate toggle for tap/nav sounds |

### File Layout

```
config/sounds/
  default.json          # Ships with HelixScreen — balanced, tasteful
  minimal.json          # Completion + errors only, no UI sounds
  retro.json            # Chiptune vibes, 8-bit arpeggios
  silent.json           # All sounds defined as empty sequences
```

Users can drop custom JSON files here. Theme picker shows all available files.

---

## Implementation Phases

All phases use TDD: write failing tests first, then implement to make them pass.

### Phase 1 — Core Engine (Foundation)

**New files:**
- `include/sound_backend.h` — backend interface
- `include/sound_sequencer.h` + `src/system/sound_sequencer.cpp` — tick loop, ADSR, LFO, sweeps
- `include/sound_theme.h` + `src/system/sound_theme.cpp` — JSON parser, note name resolution
- `tests/test_sound_sequencer.cpp` — sequencer unit tests
- `tests/test_sound_theme.cpp` — JSON parsing tests

**Tests first:**
- Parse JSON theme, validate all primitive types
- Note name to frequency conversion (all octaves, sharps, flats)
- Musical duration to ms conversion (with bpm)
- ADSR envelope state machine (attack ramp, decay, sustain hold, release)
- LFO modulation (sinusoidal, applied to freq/amplitude/duty)
- Sweep interpolation (linear, over step duration)
- Sequence playback (correct order, correct timing)
- Priority preemption (UI sound interrupted by event sound)
- Invalid JSON handling (missing fields, bad values, graceful defaults)

**Then implement:**
- `SoundBackend` interface
- `SoundTheme` JSON loader with validation
- `SoundSequencer` thread with tick loop
- Note/duration parsing utilities
- Lock-free queue for play requests
- Refactor `SoundManager` to use new sequencer

### Phase 2 — SDL Backend (Hear It)

**New files:**
- `include/sdl_sound_backend.h` + `src/system/sdl_sound_backend.cpp`
- `tests/test_sdl_sound_backend.cpp`

**Tests first:**
- Waveform generation (square/saw/triangle/sine sample correctness)
- Amplitude scaling
- Filter (lowpass/highpass with cutoff)
- SDL audio initialization and cleanup
- Tone output produces expected sample buffer

**Then implement:**
- Waveform sample generators
- Simple biquad filter (lowpass/highpass)
- SDL audio device init + `SDL_QueueAudio` output
- Wire into backend auto-detection

### Phase 3 — Integration (Sounds Come Alive)

**Modified files:**
- `src/print/print_completion.cpp` — wire up sound calls
- `src/ui/ui_nav_manager.cpp` — nav sounds
- `src/ui/ui_toast.cpp` — error tone
- `src/ui/ui_panel_settings.cpp` — theme picker, ui_sounds toggle
- `ui_xml/settings_panel.xml` — new settings rows
- `config/helixconfig.json.template` — new settings

**New files:**
- `config/sounds/default.json`
- `config/sounds/minimal.json`

**Tests first:**
- Sound triggered on print completion state transition
- Sound triggered on error state transition
- Sound respects `sounds_enabled` master toggle
- Sound respects `ui_sounds_enabled` toggle
- Theme loading and switching
- Fallback behavior when theme file missing

**Then implement:**
- Hook `SoundManager::play()` calls into trigger points
- Add settings UI for theme picker and ui_sounds toggle
- Ship default.json and minimal.json themes
- Wire completion alert mode to include sound

### Phase 4 — PWM Backend (Real Hardware)

**New files:**
- `include/pwm_sound_backend.h` + `src/system/pwm_sound_backend.cpp`
- `tests/test_pwm_sound_backend.cpp`

**Tests first:**
- Sysfs path construction for pwmchip/channel
- Frequency to period conversion
- Duty cycle mapping for waveform approximation
- Enable/disable sequencing
- Error handling (missing sysfs, permission denied)

**Then implement:**
- Sysfs PWM control wrapper
- Duty cycle approximation for waveforms
- Platform detection (check for `/sys/class/pwm/pwmchip0`)
- Auto-select PWM over M300 on AD5M

### Phase 5 — M300 Backend Refactor + Polish

**Modified files:**
- Refactor existing M300 code into `M300SoundBackend`

**New files:**
- `include/m300_sound_backend.h` + `src/system/m300_sound_backend.cpp`
- `config/sounds/retro.json`

**Tests first:**
- Sequence flattening to M300 command list
- Timing accuracy of batched commands
- Moonraker connectivity handling

**Then implement:**
- M300Backend using new interface
- `retro.json` theme (chiptune arpeggios, 8-bit vibes)
- Sound preview in settings panel
- Remove old `SoundManager::send_m300()` code

---

## Explicitly NOT in v1

- No audio file playback (WAV/MP3) — pure synthesis only
- No polyphony — one sound at a time with priority preemption
- No per-sound volume slider — master on/off + velocity primitive
- No MIDI file import
- No recording/export

---

## Code Review Checkpoints

After each phase, a thorough code review covering:
- **Architecture**: Does it follow HelixScreen patterns? Clean separation of concerns?
- **Correctness**: Edge cases, thread safety, error handling
- **Performance**: No unnecessary allocations in the tick loop, lock-free where needed
- **DRY**: No duplicated logic between backends or between old/new code
- **Test quality**: Tests that fail if feature removed, not just happy path
- **Integration**: No regressions in existing functionality
