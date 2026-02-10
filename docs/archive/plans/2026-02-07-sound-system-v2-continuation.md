# Sound System v2 — Session Continuation Prompt

Copy everything below the line into a new session.

---

## Continue: Sound System v2 — Phase 5 (Final Polish)

I'm continuing development of the Sound System v2 for HelixScreen. Phases 1-4 are complete and committed. Here's the full context.

### Design Doc
Read `docs/plans/2026-02-07-sound-system-v2-design.md` for the full spec.

### Worktree
Work is in `.worktrees/sound-system-v2` on branch `feature/sound-system-v2`. Build with `make -j`, test with `make test -j && ./build/bin/helix-tests "[sound]"`. Full suite: `make test-run` (parallel shards, 14s). Do NOT run `./build/bin/helix-tests` without sharding — some non-sound tests hang in single-process mode.

### Branch State
6 commits on `feature/sound-system-v2` (clean, no uncommitted changes):
```
8e78531b feat(sound): add toggle sounds, theme preview, and complete minimal.json
d1e8627c feat(sound): add PWM sysfs backend for AD5M hardware buzzer
b75cd868 style(xml): format settings_panel.xml
b11f0d8b feat(sound): hook UI event sounds and add sound settings panel
addd2712 feat(sound): add SDL audio backend with waveform synthesis and biquad filter
fc84ee5a feat(sound): add multi-backend synthesizer engine with JSON sound themes
```

28 files changed vs main, ~4830 lines added. 110 sound tests, 5228 assertions, all passing.

---

### What's Done — ALL PHASES 1-4 COMPLETE

#### Phase 1 — Core Engine
| File | Description |
|------|-------------|
| `include/sound_backend.h` | Abstract interface: `set_tone(freq, amplitude, duty)`, `silence()`, `set_waveform()`, `set_filter()`, capability queries |
| `include/sound_theme.h` + `src/system/sound_theme.cpp` | JSON theme parser: note names (A4=440 12-TET), musical durations (4n/8n/dotted/triplet), ADSR/LFO/sweep/filter, defensive defaults |
| `include/sound_sequencer.h` + `src/system/sound_sequencer.cpp` | Dedicated thread, tick loop, ADSR state machine, sinusoidal LFO, linear sweep, filter sweep, priority preemption (UI < EVENT < ALARM) |
| `include/sound_manager.h` + `src/system/sound_manager.cpp` | Singleton: `play(name, priority)`, theme management, backend auto-detection (SDL > PWM > M300), backward compat (`play_test_beep`, `play_print_complete`, `play_error_alert`), `get_available_themes()`, `set_theme()` |
| `config/sounds/default.json` | 12 sounds: button_tap, toggle_on/off, nav_forward/back, dropdown_open, print_complete, print_cancelled, error_alert, error_tone, alarm_urgent, test_beep |
| `config/sounds/minimal.json` | 6 sounds: print_complete, print_cancelled, error_alert, error_tone, alarm_urgent, test_beep |
| `include/settings_manager.h` + `src/system/settings_manager.cpp` | Added `ui_sounds_enabled` (bool) + `sound_theme` (string) settings with persistence |
| `config/helixconfig.json.template` | Updated with new settings |
| `src/print/print_completion.cpp` | Wired: plays sound on COMPLETE/ERROR/CANCELLED state transitions |

#### Phase 2 — SDL Backend (Desktop Audio)
| File | Description |
|------|-------------|
| `include/sdl_sound_backend.h` + `src/system/sdl_sound_backend.cpp` | Full SDL2 audio: waveform synthesis (square/saw/triangle/sine), SDL audio callback (low-latency), atomic params, Butterworth biquad filter (LP/HP), phase continuity. All capabilities = true. |

#### Phase 3 — UI Event Hooks
| File | Description |
|------|-------------|
| `src/ui/ui_nav_manager.cpp` | `nav_forward` on panel switch + overlay push, `nav_back` on overlay close (4 lines) |
| `src/ui/ui_toast_manager.cpp` | `error_tone` (EVENT priority) on ERROR severity toasts (6 lines) |
| `src/ui/ui_button.cpp` | Component-level hook: `button_tap` on ALL `<ui_button>` CLICKED events (15 lines) |
| `src/ui/ui_switch.cpp` | Component-level hook: `toggle_on`/`toggle_off` on ALL `<ui_switch>` VALUE_CHANGED events. Checks LV_STATE_CHECKED for on/off. |
| `src/ui/ui_panel_settings.cpp` + `include/ui_panel_settings.h` | 3 new callbacks: `on_ui_sounds_changed`, `on_sound_theme_changed`, `on_test_beep`. Theme dropdown populated from `get_available_themes()`. Theme change plays `test_beep` as preview. |
| `ui_xml/settings_panel.xml` | UI Sounds toggle, Sound Theme dropdown, Test Sound button — all inside `container_sounds`, hidden when master sounds off |

#### Phase 4 — PWM Backend (AD5M Hardware)
| File | Description |
|------|-------------|
| `include/pwm_sound_backend.h` + `src/system/pwm_sound_backend.cpp` | Sysfs PWM: writes `period`/`duty_cycle`/`enable` to `/sys/class/pwm/pwmchipN/pwmM/`. Waveform approximation via duty ratio (square=50%, saw=25%, triangle=35%, sine=40%). Amplitude via duty scaling. `min_tick_ms()=2.0f`. Testable via `base_path` constructor param. |
| Auto-detection in `sound_manager.cpp` | SDL > PWM (checks sysfs existence) > M300 > disabled |

#### Tests (110 cases, 5240 assertions)
| File | Cases | Description |
|------|-------|-------------|
| `tests/unit/test_sound_theme.cpp` | 28 | JSON parsing, note names, musical durations, ADSR/LFO/sweep/filter, defaults, errors, clamping |
| `tests/unit/test_sound_sequencer.cpp` | 20 | Single tone, multi-step, pause, ADSR, LFO, sweep, priority, repeat, stop, threading |
| `tests/unit/test_sdl_sound_backend.cpp` | 30 | Waveform correctness, amplitude, phase continuity, biquad filter, duty cycle, edge cases |
| `tests/unit/test_pwm_sound_backend.cpp` | 32 | Sysfs paths, freq→period, duty mapping, amplitude, enable/disable, lifecycle, error handling |

---

### What's Next — Phase 5 (Final Polish)

Phase 5 has two remaining parts:

#### 5A. `retro.json` Theme (Chiptune Vibes)
Create `config/sounds/retro.json` — an 8-bit chiptune theme with:
- Fast square wave arpeggios for button taps
- Classic game-style up/down chirps for nav
- Triumphant chiptune melody for print_complete (think Mario victory)
- Harsh buzzy alarm sounds
- Higher BPM, snappier envelopes, more aggressive waveforms
- ALL 12 sound names must be present (same as default.json)
- No code changes needed — just a JSON file

#### 5B. M300 Backend Refactor
Currently `M300SoundBackend` is defined INLINE in `sound_manager.cpp` (lines 28-83). Refactor to:
- `include/m300_sound_backend.h` + `src/system/m300_sound_backend.cpp` — proper separate files
- `tests/unit/test_m300_sound_backend.cpp` — unit tests

Current M300Backend behavior to preserve:
- `set_tone()`: sends `M300 S{freq} P{min_tick_ms}` via `client_->gcode_script()`
- Skips redundant same-frequency commands (`last_freq_` dedup)
- `silence()`: sends `M300 S0 P1`
- `min_tick_ms()` = 50.0f (M300 has high latency)
- No waveform/amplitude/filter support (all default false)

Tests should cover:
- Frequency clamping (100-10000 Hz range)
- Redundant frequency dedup
- Silence command
- Amplitude threshold (< 0.01 → silence)
- GCode format correctness
- Requires a MockMoonrakerClient — check if one exists in tests already, or create a simple one

Stretch: batch mode optimization — pre-compute a sequence into a list of M300 commands and send them all at once instead of tick-by-tick. This would be a bigger change to the backend interface.

#### Already Done (previously deferred, now complete):
- **Toggle on/off sounds**: Component-level hook in `ui_switch.cpp` — ALL switches play `toggle_on`/`toggle_off` automatically
- **Sound preview on theme change**: `handle_sound_theme_changed()` plays `test_beep` after switching themes
- **minimal.json complete**: Added `print_cancelled` and `alarm_urgent`
- **`dropdown_open` sound**: NOT hooked — `lv_dropdown` has no custom C++ wrapper, so no clean component-level hook point. Low priority. Could add per-instance in XML callbacks if desired.

---

### Architecture Reference

**Thread model:**
```
LVGL thread (main)                  Sequencer thread
     |                                    |
     |  play("button_tap")               |
     |------ push to mutex queue ------->|
     |                                    | tick @ ~1ms (or backend min_tick_ms)
     |                                    | ADSR + LFO + sweep interpolation
     |                                    | backend->set_tone(freq, amp, duty)
     |                                    | backend->silence()
```

**Backend auto-detection order** (in `SoundManager::create_backend()`):
1. `#ifdef HELIX_DISPLAY_SDL` → `SDLSoundBackend` (desktop)
2. `/sys/class/pwm/pwmchip0/pwm6` exists → `PWMSoundBackend` (AD5M)
3. Moonraker connected + `output_pin beeper` → `M300SoundBackend` (any Klipper)
4. None → sounds disabled, no sequencer thread

**Key APIs:**
```cpp
SoundManager::instance().play("sound_name");                    // UI priority
SoundManager::instance().play("sound_name", SoundPriority::EVENT);  // Event priority
SoundManager::instance().set_theme("retro");                    // Load theme
SoundManager::instance().get_available_themes();                // List JSON files
SoundManager::instance().play_test_beep();                      // Backward compat
```

**Priority levels:** `UI(0) < EVENT(1) < ALARM(2)` — higher preempts lower.

**UI sounds** (affected by `ui_sounds_enabled` toggle): `button_tap`, `toggle_on`, `toggle_off`, `nav_forward`, `nav_back`, `dropdown_open`. Hardcoded in `SoundManager::is_ui_sound()`.

**Settings subjects** (for XML binding):
- `settings_sounds_enabled` — master sounds toggle
- `settings_ui_sounds_enabled` — UI sounds toggle
- `printer_has_speaker` — hides all sound settings when no speaker detected

---

### Known Issues & Nice-to-Haves
- **Pre-existing flaky tests**: `test_ams_backend_mock_realistic`, `test_display_manager` sometimes fail in sharded runs — NOT related to sound system
- **Single-process test hang**: `./build/bin/helix-tests` (without sharding) hangs on some stress/thread tests. Always use `make test-run` for full suite.
- **`dropdown_open` not hooked**: No custom C++ wrapper for `lv_dropdown`, so no clean component-level hook. Could add per-instance via XML callbacks.
- **SoundDefinition copies**: `play()` copies SoundDefinition by value under mutex. Could use move/reference for performance (low priority — sounds are small).
- **M300 batch mode**: Pre-compute sequence into M300 command list to reduce latency. Addressed in 5B stretch goal.
- **ADSR edge case**: Undocumented behavior when `release_ms > duration_ms`.
- **is_ui_sound() hardcoded**: Could mark sounds as UI/event in JSON schema instead.
- **Duplicate callback registrations**: `init_subjects()` and `register_settings_panel_callbacks()` both register the same XML callbacks. Pre-existing pattern, harmless (second call overwrites first in name→callback map), but wasteful.

---

### Development Methodology — FOLLOW THIS EXACTLY

1. **TDD**: Write failing tests FIRST, then implement to make them pass
2. **Agent swarm**: Use `TeamCreate` to create a team, spawn `general-purpose` sub-agents for implementation work. Delegate — don't do implementation in the main session
3. **Task tracking**: Use TaskCreate/TaskUpdate to track progress with dependencies
4. **Code reviews** after each phase: Spawn a `superpowers:code-reviewer` agent that reviews ALL new/modified files
5. **Worktree isolation**: All work in `.worktrees/sound-system-v2`
6. **Build verification**: `make -j && make test-run` after every change. Sound-specific: `./build/bin/helix-tests "[sound]"`
7. **Commit style**: `feat(sound):`, `test(sound):`, `refactor(sound):` — conventional commits

---

### After Phase 5 — Wrapping Up

Once Phase 5 is done, the sound system v2 is feature-complete. Final steps:
1. Run the full code review one more time across ALL sound system files
2. Ensure `make test-run` passes clean (ignore pre-existing flaky tests)
3. Update the design doc status from "ready for implementation" to "implemented"
4. Consider squashing or keeping the 5+ commits as-is (ask user preference)
5. The branch is ready to merge to main or create a PR

Start with whichever Phase 5 sub-task feels most impactful. 5A (retro.json) is the easiest win. 5B (M300 refactor) is the most architecturally important. 5C (preview) is a one-liner.
