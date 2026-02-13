# Hidden Tests Tracker

**Last Updated:** 2026-02-11
**Total Hidden Tests:** 5

Hidden tests are excluded from normal runs using Catch2's `[.]` tag prefix. They exist for legitimate reasons (network dependencies, benchmarks, stress tests) and should be run manually when relevant.

```bash
# Run all hidden tests
./build/bin/helix-tests "[.]"

# Run by category
./build/bin/helix-tests "[.benchmark]"
./build/bin/helix-tests "[.slow]"
./build/bin/helix-tests "[.network]"
```

---

## Summary

| Tag | Count | Description |
|-----|-------|-------------|
| `[.slow]` | 3 | Stress tests, too slow for CI |
| `[.benchmark]` | 1 | Performance measurement |
| `[.network]` | 1 | Requires live Moonraker instance |

---

## Inventory

### Stress Tests (3 tests, `[.slow]`)

| File | Line | Test | Tags |
|------|------|------|------|
| `test_moonraker_client_robustness.cpp` | 721 | Sustained stress load | `[connection][edge][stress][.slow]` |
| `test_async_callback_safety.cpp` | 724 | Rapid create/destroy with pending callbacks | `[stress][async][thread][.slow]` |
| `test_async_callback_safety.cpp` | 745 | Concurrent object creation and destruction | `[stress][async][thread][.slow]` |

All by design — these are heavy stress tests that take too long for normal CI.

### Benchmark (1 test, `[.benchmark]`)

| File | Line | Test | Tags |
|------|------|------|------|
| `test_wizard_connection.cpp` | 309 | Wizard connection performance | `[wizard][connection][performance][.benchmark]` |

Performance benchmark, excluded from fast CI runs.

### Network Integration (1 test, `[.network]`)

| File | Line | Test | Tags |
|------|------|------|------|
| `test_gcode_data_source.cpp` | 227 | MoonrakerDataSource construction | `[gcode][datasource][.network]` |

Requires a live Moonraker instance. Run when testing network features.

---

## Tag Conventions

| Tag | Meaning | When to Use |
|-----|---------|-------------|
| `[.]` | Generic hidden | Crashes, instability, catch-all |
| `[.network]` | Requires network | Live server, hardware |
| `[.benchmark]` | Performance measurement | Timing-sensitive |
| `[.slow]` | Too slow for normal runs | >5 seconds execution |
| `[.disabled]` | Temporarily disabled | Awaiting fix or decision |
| `[.flaky]` | Intermittent failures | Race conditions, timing |

---

## Verification

```bash
grep -rn '\[\.' tests/unit/*.cpp | grep 'TEST_CASE' | grep -v '^ *//' | wc -l
```

Expected output: `5`
