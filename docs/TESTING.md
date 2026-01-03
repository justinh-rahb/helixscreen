# Testing Infrastructure

**Status:** Active
**Last Updated:** 2025-12-27

---

## Overview

HelixScreen uses a multi-layered testing strategy with Catch2 v3 as the primary test framework. Tests are organized into unit tests, integration tests, and experimental test binaries for rapid prototyping.

## Quick Start

```bash
# Build tests (does not run)
make test

# Run unit tests in parallel (default, ~4-8x faster)
make test-run

# Run unit tests sequentially (for debugging)
make test-serial

# Run fast tests in parallel (skip slow tests)
make test-fast

# Run integration tests (with mocks)
make test-integration

# Build specific test binary
cd experimental && make test_multicolor_gcode
```

## Parallel Test Execution

Tests run in **parallel by default** using Catch2's sharding feature. Each shard runs in a separate process with its own LVGL instance, avoiding thread-safety issues entirely.

### Test Targets

| Target | Behavior |
|--------|----------|
| `make test-run` | **Parallel (default)** - uses 2x CPU cores as shards for ~4-8x speedup |
| `make test-serial` | Sequential - for debugging or when you need clean output |
| `make test-all` | Parallel - runs ALL tests including slow ones |
| `make test-verbose` | Sequential with timing - shows per-test duration |

### When to Use Serial Execution

Use `make test-serial` when:
- **Debugging test failures** - output is interleaved in parallel mode
- **Reproducing race conditions** - sequential execution is deterministic
- **CI environments** - some CI runners may have limited parallelism
- **Reading test output** - parallel output can be hard to follow

### How It Works

The parallel infrastructure uses Catch2's `--shard-count N --shard-index M` flags to split tests across multiple processes:

```bash
# What make test-run does internally (simplified):
for i in $(seq 0 $((NPROCS-1))); do
    ./build/bin/helix-tests "~[.] ~[slow]" --shard-count $NPROCS --shard-index $i &
done
wait
```

Each shard gets a deterministic subset of tests. Per Catch2 best practices, we use 2x the number of CPU cores to avoid long-tailed execution from uneven test distribution.

### Expected Speedup

| Machine | Serial Time | Parallel Time | Speedup |
|---------|-------------|---------------|---------|
| 4 cores | ~100s | ~30s | ~3.5x |
| 8 cores | ~100s | ~18s | ~6x |
| 14 cores | ~100s | ~12s | ~9x |

### Running Tests by Tag (Direct Binary)

**⚠️ ALWAYS use `"~[.]"` to exclude hidden/pending tests:**

```bash
# ✅ CORRECT - run [application] tests, exclude hidden
./build/bin/helix-tests "[application]" "~[.]"

# ✅ CORRECT - run [gcode] tests, exclude hidden  
./build/bin/helix-tests "[gcode]" "~[.]"

# ❌ WRONG - may hang on hidden tests!
./build/bin/helix-tests "[application]"
```

Hidden tests (tagged `.pending`, `.integration`, `.slow`) require special
environments and will hang or fail if run directly. The `make test-run`
target handles this automatically, but direct binary invocation requires
the `"~[.]"` filter.

## Sanitizer Support

Memory and thread safety testing with AddressSanitizer and ThreadSanitizer.

### Available Targets

| Target | Purpose |
|--------|---------|
| `make test-asan` | Run all tests with AddressSanitizer |
| `make test-tsan` | Run all tests with ThreadSanitizer |
| `make test-asan-one TEST="[tag]"` | Run specific test with ASAN |
| `make test-tsan-one TEST="[tag]"` | Run specific test with TSAN |

### What They Detect

**AddressSanitizer (ASAN):**
- Memory leaks
- Use-after-free
- Buffer overflows (stack and heap)
- Double-free

**ThreadSanitizer (TSAN):**
- Data races
- Deadlocks
- Thread safety violations

### Example Usage

```bash
# Run all tests with memory checking
make test-asan

# Run just streaming tests with thread checking
make test-tsan-one TEST="[streaming]"
```

**Note:** Sanitizers add overhead (~2-5x slower). Use for debugging, not regular runs.

## Feature-Based Test Targets

Run tests by feature area for focused debugging:

| Target | Description |
|--------|-------------|
| `make test-core` | Critical functionality |
| `make test-gcode` | G-code parsing/geometry |
| `make test-moonraker` | Moonraker client & mock |
| `make test-ui` | Navigation, theme, wizard |
| `make test-network` | WiFi & Ethernet |
| `make test-print` | Print workflow |
| `make test-ams` | AMS/MMU backends |
| `make test-security` | Security & injection |
| `make test-calibration` | Calibration features |
| `make test-filament` | Filament management |

## Test Organization

```
tests/
├── catch_amalgamated.hpp       # Catch2 v3 amalgamated header
├── catch_amalgamated.cpp       # Catch2 v3 implementation
├── test_main.cpp               # Main test runner entry point
├── ui_test_utils.cpp           # UI testing utilities
├── ui_test_utils.h
├── unit/                       # Unit tests (use real LVGL)
│   ├── test_config.cpp
│   ├── test_gcode_parser.cpp
│   ├── test_multicolor_gcode.cpp
│   ├── test_moonraker_*.cpp
│   └── ...
├── integration/                # Integration tests (use mocks)
│   └── test_mock_example.cpp
└── mocks/                      # Mock implementations
    ├── mock_lvgl.cpp
    ├── mock_moonraker_client.cpp
    └── ...

experimental/
└── src/
    ├── test_gcode_geometry.cpp     # G-code geometry tests
    ├── test_gcode_analysis.cpp     # G-code analysis
    ├── test_multicolor_gcode.cpp   # Multi-color rendering
    └── ...
```

## Framework: Catch2 v3

HelixScreen uses **Catch2 v3** (amalgamated build) for all tests.

### Including Catch2

```cpp
// tests/unit/test_example.cpp
#include "your_module.h"
#include "../catch_amalgamated.hpp"  // Catch2 v3

using Catch::Approx;  // For floating-point comparisons

TEST_CASE("Module - Feature description", "[module][feature]") {
    SECTION("Test scenario") {
        REQUIRE(result == expected);
    }
}
```

### Common Patterns

**Basic assertions:**
```cpp
REQUIRE(condition);                // Must be true
CHECK(condition);                  // Continue on failure
REQUIRE_FALSE(condition);          // Must be false
```

**Floating-point comparisons:**
```cpp
using Catch::Approx;

REQUIRE(value == Approx(3.14159).epsilon(0.01));
REQUIRE(layer.z_height == Approx(0.2f));
```

**Test organization:**
```cpp
TEST_CASE("Component - Feature", "[component][feature][optional-tag]") {
    // Setup common to all sections

    SECTION("First scenario") {
        // Test first scenario
    }

    SECTION("Second scenario") {
        // Test second scenario
    }
}
```

**Skipping tests:**
```cpp
if (!file_exists("test_data.gcode")) {
    SKIP("Test data not found");
}
```

**Info logging:**
```cpp
INFO("Parsed " << line_count << " lines");
REQUIRE(result.layers.size() > 0);
```

## Unit Tests

Unit tests use **real LVGL** and test individual components in isolation.

### Writing Unit Tests

**Location:** `tests/unit/test_<module>.cpp`

**Example:** Multi-color G-code parser test

```cpp
// tests/unit/test_multicolor_gcode.cpp
#include "gcode_parser.h"
#include "../catch_amalgamated.hpp"

using namespace gcode;

TEST_CASE("MultiColor - Parse extruder_colour", "[gcode][multicolor][parser]") {
    GCodeParser parser;

    SECTION("Parse 4-color OrcaSlicer format") {
        parser.parse_line("; extruder_colour = #ED1C24;#00C1AE;#F4E2C1;#000000");

        const auto& palette = parser.get_tool_color_palette();

        REQUIRE(palette.size() == 4);
        REQUIRE(palette[0] == "#ED1C24");  // Red
        REQUIRE(palette[1] == "#00C1AE");  // Teal
        REQUIRE(palette[2] == "#F4E2C1");  // Beige
        REQUIRE(palette[3] == "#000000");  // Black
    }
}
```

### Test Tags

Use tags to organize and filter tests:

- `[gcode]` - G-code parsing/rendering
- `[parser]` - Parser-specific tests
- `[geometry]` - Geometry building
- `[multicolor]` - Multi-color features
- `[integration]` - End-to-end tests
- `[file]` - Tests requiring external files
- `[moonraker]` - Moonraker API tests
- `[wifi]` - WiFi/networking tests
- `[application]` - Application module tests

**Special tags (hidden tests):**
- `[.pending]` - Test not yet implemented (placeholder)
- `[.integration]` - Requires full environment (skip in unit tests)
- `[.slow]` - Long-running test (skip in fast runs)
- `[.disabled]` - Temporarily disabled

Any tag starting with `.` is a **hidden test** in Catch2. See [Running Tests by Tag](#running-tests-by-tag-direct-binary) for filtering examples.

## Integration Tests

Integration tests use **mocked dependencies** instead of real LVGL.

### Writing Integration Tests

**Location:** `tests/integration/test_<feature>.cpp`

Integration tests use mocks from `tests/mocks/` to simulate UI components,
Moonraker clients, and other external dependencies without requiring real
hardware or network connections.

**Example:**
```cpp
#include "mock_moonraker_client.h"
#include "../catch_amalgamated.hpp"

TEST_CASE("Moonraker - Connection handling", "[moonraker][integration]") {
    MockMoonrakerClient client;
    client.set_mock_response("printer.info", R"({"state": "ready"})");

    auto result = client.send_request("printer.info");
    REQUIRE(result.success);
}
```

## Experimental Tests

Experimental tests are standalone binaries for rapid prototyping and analysis.

### Building Experimental Tests

**Location:** `experimental/src/test_<feature>.cpp`

```bash
cd experimental
make                              # Build all experimental tests
make test_multicolor_gcode        # Build specific test
./bin/test_multicolor_gcode       # Run test
```

### Example: G-code Geometry Analysis

```cpp
// experimental/src/test_gcode_geometry.cpp
#include "gcode_geometry_builder.h"
#include <iostream>

int main() {
    // Load G-code
    auto gcode = load_gcode_file("model.gcode");

    // Build geometry
    GeometryBuilder builder;
    auto geometry = builder.build(gcode);

    // Analyze
    std::cout << "Vertices: " << geometry.vertices.size() << "\n";
    std::cout << "Triangles: " << geometry.indices.size() << "\n";

    return 0;
}
```

## Test Data

### G-code Test Files

Test G-code files are located in `assets/gcode/`:

- `assets/gcode/OrcaCube_ABS_Multicolor.gcode` - 4-color multi-extruder test (5.8MB, 51 tool changes)
- `assets/gcode/OrcaCube AD5M.gcode` - Single-color test
- Additional test files as needed

### Using Test Files in Tests

```cpp
TEST_CASE("MultiColor - Real file parsing", "[gcode][multicolor][file]") {
    const char* filename = "assets/gcode/OrcaCube_ABS_Multicolor.gcode";

    std::ifstream file(filename);
    if (!file.is_open()) {
        SKIP("Test file not found: " << filename);
    }

    GCodeParser parser;
    std::string line;
    while (std::getline(file, line)) {
        parser.parse_line(line);
    }

    auto result = parser.finalize();

    REQUIRE(result.tool_color_palette.size() == 4);
    REQUIRE(result.total_segments > 0);
}
```

## Build System Integration

### Makefile Targets

```bash
make test              # Build and run unit tests
make test-integration  # Build and run integration tests
make test-cards        # Test dynamic card instantiation
```

### Test Binary Locations

- Unit tests: `build/bin/helix-tests`
- Integration tests: `build/bin/run_integration_tests`
- Experimental: `experimental/bin/test_<name>`

### Adding New Tests

1. **Create test file:**
   ```bash
   # Unit test
   touch tests/unit/test_new_feature.cpp

   # Integration test
   touch tests/integration/test_new_feature.cpp
   ```

2. **Write test using Catch2:**
   ```cpp
   #include "your_module.h"
   #include "../catch_amalgamated.hpp"

   TEST_CASE("NewFeature - Description", "[module][feature]") {
       REQUIRE(true);
   }
   ```

3. **Build and run:**
   ```bash
   make test
   ```

The Makefile automatically discovers test files in `tests/unit/` and `tests/integration/`.

## UI Testing Utilities

HelixScreen provides UI testing utilities in `tests/ui_test_utils.h`:

```cpp
#include "../ui_test_utils.h"

// Initialize LVGL for UI tests
void setup_lvgl_for_testing();

// Create mock display
lv_display_t* create_test_display(int width, int height);

// Simulate user interactions
void simulate_click(lv_obj_t* obj);
void simulate_swipe(lv_obj_t* obj, lv_dir_t direction);
```

**Example:**
```cpp
TEST_CASE("UI - Button click", "[ui]") {
    setup_lvgl_for_testing();

    lv_obj_t* btn = lv_button_create(lv_screen_active());
    bool clicked = false;

    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        bool* flag = (bool*)lv_event_get_user_data(e);
        *flag = true;
    }, LV_EVENT_CLICKED, &clicked);

    simulate_click(btn);

    REQUIRE(clicked == true);
}
```

## Mocking Infrastructure

### Available Mocks

- **MoonrakerClientMock:** Simulates Moonraker WebSocket connection
- **MockLVGL:** Minimal LVGL stubs for integration tests
- **MockPrintFiles:** Simulates filesystem operations

### MoonrakerClientMock API

Simulates WebSocket connection behavior without real network I/O:

```cpp
#include "tests/mocks/moonraker_client_mock.h"

MoonrakerClientMock client;

// Simulate connection lifecycle
client.connect(url, on_connected, on_disconnected);
client.trigger_connected();   // Fire on_connected callback
client.trigger_disconnected(); // Fire on_disconnected callback

// Verify behavior
client.get_last_connect_url();  // Returns URL passed to connect()
client.get_rpc_methods();       // Returns all JSON-RPC methods called
client.is_connected();          // Connection state
client.is_identified();         // Whether identify was sent

// Reset for next test
client.reset();
```

**Example Test:**
```cpp
TEST_CASE("Wizard - Connection test", "[wizard][connection]") {
    MoonrakerClientMock client;
    bool connected = false;

    client.connect("ws://printer:7125/websocket",
                   [&]() { connected = true; },
                   []() {});

    client.trigger_connected();
    REQUIRE(connected);
    REQUIRE(client.get_last_connect_url() == "ws://printer:7125/websocket");
}
```

### Creating New Mocks

**Location:** `tests/mocks/mock_<component>.cpp`

```cpp
// tests/mocks/mock_wifi.h
#pragma once

class MockWiFiManager {
public:
    void connect(const std::string& ssid, const std::string& password);
    bool is_connected() const { return connected_; }

    // Test control
    void set_should_fail(bool fail) { should_fail_ = fail; }

private:
    bool connected_ = false;
    bool should_fail_ = false;
};
```

## Best Practices

### 1. Test Isolation
- Each test should be independent
- Use `SECTION` for related scenarios
- Clean up resources in destructors

### 2. Descriptive Names
```cpp
// Good
TEST_CASE("GCodeParser - Parse multi-color metadata", "[gcode][parser]")

// Bad
TEST_CASE("Test 1", "[test]")
```

### 3. Test One Thing
```cpp
// Good - focused test
TEST_CASE("Parser - Tool changes", "[parser]") {
    // Test only tool change detection
}

// Bad - testing multiple unrelated features
TEST_CASE("Parser - Everything", "[parser]") {
    // Tests colors, layers, moves, objects...
}
```

### 4. Use Meaningful Assertions
```cpp
// Good
REQUIRE(palette.size() == 4);  // Expect 4-color palette

// Bad
REQUIRE(palette.size() > 0);  // Any size is okay?
```

### 5. Document Test Intent
```cpp
TEST_CASE("MultiColor - Backward compatibility", "[multicolor][compatibility]") {
    // Verify single-color files still work without palette metadata

    SECTION("No color metadata") {
        // Test should pass even without any color info
    }
}
```

## Debugging Tests

### Run Specific Tests
```bash
# Run all multicolor tests (excluding hidden)
./build/bin/helix-tests "[multicolor]" "~[.]"

# Run specific test case by name
./build/bin/helix-tests "MultiColor - Parse extruder_colour"

# Run tests matching multiple tags
./build/bin/helix-tests "[gcode][parser]" "~[.]"

# List all tests (including hidden)
./build/bin/helix-tests --list-tests

# List tests matching a tag
./build/bin/helix-tests --list-tests "[application]"
```

### ⚠️ Avoiding Test Hangs

If tests hang, you're likely hitting hidden tests. See [Running Tests by Tag](#running-tests-by-tag-direct-binary) for the required `"~[.]"` filter.

### Verbose Output
```bash
# Show successful assertions
./build/bin/helix-tests -s

# Show all output
./build/bin/helix-tests -s -v high
```

### Debugging in IDE

Set breakpoints in test code and run:
```bash
lldb build/bin/helix-tests
(lldb) run "[multicolor]"
```

## Continuous Integration

Tests run automatically on push via GitHub Actions (see `.github/workflows/test.yml`).

CI runs:
1. Unit tests (`make test`)
2. Integration tests (`make test-integration`)
3. Code quality checks

## Coverage

Generate code coverage reports:
```bash
# TODO: Add coverage target
make coverage
```

## LVGL Observer Testing Gotcha

**CRITICAL:** `lv_subject_add_observer()` immediately fires the callback with the current value (LVGL 9.4 behavior).

Tests must account for this auto-notification:

```cpp
lv_subject_add_observer(subject, callback, &count);
REQUIRE(count == 1);  // Fired immediately!

state.set_value(new_value);
REQUIRE(count == 2);  // Fired again on change
```

**Source:** `lib/lvgl/src/core/lv_observer.c:459` - `observer->cb(observer, subject);`

**Reference:** `tests/unit/test_printer_state.cpp` observer tests

---

## Common Test Issues

### Issue: "Catch2 header not found"
**Solution:** Use `#include "../catch_amalgamated.hpp"` not `<catch2/...>`

### Issue: "Approx not found"
**Solution:** Add `using Catch::Approx;` after includes

### Issue: "Test binary won't link"
**Solution:** Check that required .o files are included in `Makefile` test link command

### Issue: "LVGL functions undefined in integration tests"
**Solution:** Integration tests should use mocks, not real LVGL. Move to unit tests if LVGL needed.

## Related Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md):** System design, thread safety patterns
- **[BUILD_SYSTEM.md](BUILD_SYSTEM.md):** Build configuration and troubleshooting
- **[CONTRIBUTING.md](CONTRIBUTING.md):** Code standards and contribution workflow
- **[tests/README.md](../tests/README.md):** Additional test examples
