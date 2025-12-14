# Ideas & Future Improvements

* Clean up NULL vs nullptr usage (use nullptr consistently in C++ code)
* More test coverage
* Have libhv use spdlog for logging if possible
* Easy calibration workflow
* **Lazy panel initialization** - Defer `init_subjects()` and `setup()` until first navigation. Challenge: LVGL XML binding requires subjects to exist before parsing. Solution: register empty subjects at startup, populate on first use. Would reduce startup time and memory.
* Belt tension visualization: controlled belt excitation + stroboscopic LED feedback to visualize resonance
* Time-lapse toggle in pre-print options (if camera present)
* **LVGL slider knob clipping bug** - When width="100%" the knob extends beyond widget bounds at min/max. Workaround: extra padding + flag_overflow_visible. Root cause: slider doesn't account for knob radius. Check `lv_slider.c` position_knob() and ext_draw_size.
* **LVGL lv_bar value=0 bug** (upstream issue) - Bar shows FULL instead of empty when created with cur_value=0 and XML sets value=0. `lv_bar_set_value()` returns early without invalidation. Workaround: set to 1 then 0.

---

## Deferred: Agent Documentation Compression

Agents (~3,700 lines total) could be compressed ~60% using table format and external examples.
Currently not needed - agents work well. Revisit if startup times degrade or context limits hit.
See git history (2025-11-10) for full analysis.
