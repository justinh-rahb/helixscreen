# Input Shaper Results Page Makeover — Session Start Prompt

Copy everything below the line into a fresh Claude Code session.

---

## Task

Implement the **Input Shaper Results Page Makeover** (Chunk 3 from the plan). The calibration flow (state machine, progress bar, mock) is done. Now we need to make the results page actually useful by showing all the data we collect.

Read the full design vision: `.claude/scratchpad/input_shaper_results_design.md`
Read the full plan: `.claude/plans/cryptic-stargazing-ocean.md` (Chunk 3 section)

## What Exists Now

The results page currently shows a basic centered layout per axis:
- Check icon + "Calibration Complete!"
- Per-axis cards showing: recommended shaper ("MZV @ 53.8 Hz"), vibration ("4.2%"), max accel ("8400 mm/s²")
- Close + Save buttons

**But we collect WAY more data than we show.** Each axis has ALL 5 fitted shapers (zv, mzv, ei, 2hump_ei, 3hump_ei) with frequency, vibration%, smoothing, and max_accel. We also capture the CSV path to frequency response data. None of this is shown.

## What We Want

### 3a: Full Shaper Comparison Table
Each axis card should show ALL 5 shapers, not just the recommended one:
```
X Axis — Recommended: MZV @ 53.8 Hz
┌──────────────────────────────────────────┐
│ Type       Freq    Vibration  Max Accel  │
│ zv         59.0    5.2%       13400      │
│ mzv ★      53.8    1.6%       4000       │ ← recommended
│ ei         56.2    0.8%       2600       │
│ 2hump_ei   71.8    0.3%       1800       │
│ 3hump_ei   89.6    0.1%       1200       │
└──────────────────────────────────────────┘
```
- Recommended row highlighted (color or star)
- Vibration column color-coded by quality (< 3% Excellent/green, < 7% Good, < 15% Acceptable/warning, >= 15% High/danger)
- Quality functions already exist in C++: `get_vibration_quality()`, `get_quality_description()`
- Populate imperatively (40+ values = too many for subjects, acceptable per CLAUDE.md exceptions for chart/table data)

### 3b: Frequency Response Chart
**We already have a purpose-built widget!** `ui_frequency_response_chart` (602 lines):
- `include/ui_frequency_response_chart.h` — full API docs
- `src/ui/ui_frequency_response_chart.cpp` — implementation with hardware tiers
- `tests/unit/test_frequency_response_chart.cpp` — tests

Hardware tiers (via `PlatformCapabilities::detect()` and `include/platform_capabilities.h`):
- EMBEDDED (AD5M, <512MB RAM): table only, NO chart → just show the comparison table
- BASIC (Pi 3, 512MB-2GB): simplified chart, max 50 points, no animations
- STANDARD (Pi 4+, ≥2GB): full chart, max 200 points, with animations

CSV access strategy:
- Klipper writes `/tmp/calibration_data_{axis}_{timestamp}.csv` (132 rows)
- Moonraker can't serve `/tmp/` files — **read locally with `std::ifstream`**
- Check locality: `MoonrakerApi::get_websocket_url()` → extract host → compare to localhost/127.0.0.1/::1
  - See `helix_plugin_installer.cpp` for `extract_host_from_websocket_url()` and `is_local_host()`
- If remote: degrade gracefully (no chart, table still shows everything)
- Memory budget: ~12 KB total per chart (trivial even on AD5M's 37MB free)

CSV format: `freq, psd_x, psd_y, psd_z, psd_xyz, zv(59.0), mzv(53.8), ei(56.2), 2hump_ei(71.8), 3hump_ei(89.6)`
- Show: raw psd_xyz (primary color) + recommended shaper curve (success color)
- Mark peak frequency via `ui_frequency_response_chart_mark_peak()`

Data flow:
1. Collector captures `csv_path` → stored in `InputShaperResult.csv_path`
2. After calibration completes, try local file read
3. Parse CSV → populate `InputShaperResult.freq_response` (vector<pair<float,float>>) — field already exists but NOT populated
4. Feed to chart widget
5. Cache via existing serialization in `input_shaper_cache.cpp:133-172` (already implemented)

Mock mode: generate synthetic CSV file in `/tmp/` with resonance peak at ~53 Hz.

### 3c: Before/After Comparison
- `InputShaperConfig` struct has: `shaper_type_x/y`, `shaper_freq_x/y`, `is_configured`
- Snapshot current config on panel activate (before calibration starts)
- After calibration: show "Previous: MZV @ 36.7 Hz → New: EI @ 53.8 Hz" if config changed
- Only show row if `is_configured` was true before calibration

## Key Files

| File | What |
|------|------|
| `ui_xml/input_shaper_panel.xml` | Results state XML (lines ~146-210) |
| `src/ui/ui_panel_input_shaper.cpp` | `populate_axis_result()`, result subjects |
| `include/ui_panel_input_shaper.h` | Subjects, MAX_SHAPERS=5 |
| `include/calibration_types.h:140-218` | InputShaperResult, ShaperOption, InputShaperConfig structs |
| `src/api/moonraker_api_advanced.cpp` | InputShaperCollector state machine |
| `src/api/moonraker_client_mock.cpp` | Timer-based mock (around line 1389) |
| `include/ui_frequency_response_chart.h` | Chart widget API (252 lines) |
| `src/ui/ui_frequency_response_chart.cpp` | Chart implementation (602 lines) |
| `src/calibration/input_shaper_cache.cpp:125-184` | freq_response serialization (done) |
| `include/platform_capabilities.h` | PlatformTier enum, hardware detection |
| `include/helix_plugin_installer.h` | `is_local_host()`, `extract_host_from_websocket_url()` |
| `include/moonraker_api.h:996` | `get_websocket_url()` |
| `tests/unit/test_moonraker_api_input_shaper.cpp` | Existing 108 tests |

## Key Patterns

- **Declarative UI**: data in C++, appearance in XML, subjects connect them. Exceptions allowed for chart/table data population.
- **LVGL flex centering**: need ALL THREE: `style_flex_main_place="center"`, `style_flex_cross_place="center"`, `style_flex_track_place="center"`
- **Design tokens**: use `#space_md`, `#card_bg`, `<text_heading>`, `<text_small>` etc. — never hardcoded values
- **Threading**: WebSocket callbacks are background thread. Use `ui_async_call()` for LVGL updates.
- **Test-first**: write failing tests before implementation. Run with `./build/bin/helix-tests "[input_shaper]"`
- **Build**: `make -j` (Makefile auto-discovers sources, no edits needed for new .cpp)
- **XML is runtime**: XML changes don't need rebuild — just relaunch the app

## Suggested Implementation Order

1. **Shaper comparison table** (3a) — most impactful, no external dependencies
2. **CSV parsing + chart** (3b) — needs locality detection, chart widget integration
3. **Before/after** (3c) — simple, builds on existing subjects
4. **Mock CSV generation** — enables visual testing of chart
5. **Tests for all of the above**

## Build & Test

```bash
make -j                                          # Build binary
make test && ./build/bin/helix-tests "[input_shaper]"  # Build & run tests
./build/bin/helix-screen --test -vv              # Visual test with mock printer
```

Current test baseline: 108 test cases, 468 assertions, all passing.
