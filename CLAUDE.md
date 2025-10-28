# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **LVGL 9 UI Prototype** for HelixScreen - a declarative XML-based touch UI system using LVGL 9.4 with reactive Subject-Observer data binding. The prototype runs on SDL2 for rapid development and will eventually target framebuffer displays on embedded hardware.

**Key Innovation:** Complete separation of UI layout (XML) from business logic (C++), similar to modern web frameworks. No manual widget management - all updates happen through reactive subjects.

## Build Requirements

### macOS (Homebrew)
```bash
brew install sdl2 bear imagemagick python3 node
```

### Debian/Ubuntu (apt)
```bash
sudo apt install libsdl2-dev bear imagemagick python3 clang make npm
```

### Fedora/RHEL/CentOS (dnf/yum)
```bash
sudo dnf install SDL2-devel bear ImageMagick python3 clang make npm
# OR on older systems:
sudo yum install SDL2-devel bear ImageMagick python3 clang make npm
```

**Required:**
- `clang` - C/C++ compiler (C++17 support)
- `libsdl2-dev` / `SDL2-devel` - SDL2 display simulator
- `make` - GNU Make build system
- `python3` - Icon generation scripts
- `node` / `npm` - Font generation (lv_font_conv)

**Optional (for IDE support):**
- `bear` - Generates `compile_commands.json` for LSP/clangd

**Optional (for screenshots/icon generation):**
- `imagemagick` - BMP to PNG conversion in screenshot script, icon generation (`make icon`)
- `iconutil` - macOS .icns icon generation (macOS only, built-in; Linux generates PNG only)

## Build System

```bash
# Common commands
make -j8          # Parallel build (NOT auto-parallel by default)
make build        # Clean parallel build with progress/timing
make V=1          # Verbose mode (shows full compiler commands)
make help         # Show all targets and options
make check-deps   # Verify dependencies

# Development
make compile_commands  # Generate compile_commands.json for IDE/LSP
make run              # Build and run
```

**Key features:**
- Color-coded output: `[CXX]` (blue), `[CC]` (cyan), `[LD]` (magenta)
- Verbose mode with `V=1` shows full commands
- Automatic dependency checking before builds
- Fail-fast with clear error messages and full command on failure

**Important:** Use `-j` flag explicitly for parallel builds. See `make help` for all options.

**Binary:** `build/bin/helix-ui-proto`
**Panels:** home, controls, motion, nozzle-temp, bed-temp, extrusion, filament, settings, advanced, print-select

### Multi-Display Support (macOS)

The prototype supports multi-monitor workflows with automatic display positioning:

```bash
# Run on specific display (centered)
./build/bin/helix-ui-proto --display 0    # Display 0 (main)
./build/bin/helix-ui-proto --display 1    # Display 1 (secondary)

# Exact positioning
./build/bin/helix-ui-proto --x-pos 100 --y-pos 200

# Combine with other options
./build/bin/helix-ui-proto -d 1 -s small --panel home
```

**Implementation:** Uses LVGL submodule patch (auto-applied by Makefile) that reads environment variables to control SDL window position. See `patches/lvgl_sdl_window_position.patch`.

### Screenshot Workflow ⚠️

**ALWAYS use the screenshot script instead of manual BMP/magick commands:**

```bash
# Basic usage (auto-opens on display 1):
./scripts/screenshot.sh helix-ui-proto output [panel_name]

# With flags (panel optional - flags can be 3rd arg if no panel specified):
./scripts/screenshot.sh helix-ui-proto output panel -s small
./scripts/screenshot.sh helix-ui-proto wizard-test --wizard -s tiny

# Override display:
HELIX_SCREENSHOT_DISPLAY=0 ./scripts/screenshot.sh helix-ui-proto output panel

# Examples:
./scripts/screenshot.sh helix-ui-proto extrusion-test extrusion
./scripts/screenshot.sh helix-ui-proto controls-launcher controls -s large
./scripts/screenshot.sh helix-ui-proto home-panel home
./scripts/screenshot.sh helix-ui-proto wizard-tiny --wizard -s tiny
```

**Script features:**
- ✅ Dependency validation (ImageMagick)
- ✅ Panel name validation with helpful error messages
- ✅ Flexible argument handling (panel optional, flags pass-through)
- ✅ Colored output with progress indicators
- ✅ Opens window on display 1 by default (keeps terminal visible)
- ✅ BMP → PNG conversion with cleanup
- ✅ Comprehensive error handling
- ✅ Optional auto-open: `HELIX_SCREENSHOT_OPEN=1 ./scripts/screenshot.sh ...`
- ✅ Auto-screenshot and auto-quit (via `--screenshot 2 --timeout 3` flags)
- ✅ No external timeout utility required (uses native binary flags)

**Environment variables:**
- `HELIX_SCREENSHOT_DISPLAY` - Override display (default: 1)
- `HELIX_SCREENSHOT_OPEN` - Auto-open in Preview after capture

**Manual screenshot usage:**
- The binary accepts `--screenshot [seconds]` flag to take a screenshot after the specified delay
- Default delay is 2 seconds if no value is provided (e.g., `--screenshot` or `--screenshot 2`)
- Without the flag, no screenshot is taken (useful for interactive development)
- The screenshot script automatically passes `--screenshot 2 --timeout 3` for automated capture
- Example: `./build/bin/helix-ui-proto --panel home --screenshot 3 --timeout 5` (screenshot at 3s, quit at 5s)

**Auto-quit timeout:**
- Use `--timeout <seconds>` or `-t <seconds>` to automatically quit after specified time
- Useful for automated screenshot workflows and CI/CD pipelines
- Range: 1-3600 seconds (1 hour max)
- Example: `./build/bin/helix-ui-proto --timeout 10` (quit after 10 seconds)

**❌ Avoid:** Reading raw BMPs from `/tmp` and manually running `magick` commands. The screenshot script is the canonical way to capture UI states.

## Logging Policy

**CRITICAL:** All debug, info, warning, and error messages must use **spdlog** for console output.

### Usage

```cpp
#include <spdlog/spdlog.h>

// Log levels (use appropriately):
spdlog::trace("Very detailed tracing");           // Function entry/exit, frequent events
spdlog::debug("Debug information");               // Development/debugging details
spdlog::info("General information");              // Normal operation events
spdlog::warn("Warning condition");                // Recoverable issues, fallback behavior
spdlog::error("Error condition");                 // Failed operations, missing resources
```

### Default Log Level & Verbosity Control

**Default Level:** `warn` (shows only warnings and errors)

To increase verbosity, use command-line flags:
```bash
# Default (warn): Only warnings and errors
./build/bin/helix-ui-proto

# Info level: General information + warnings + errors
./build/bin/helix-ui-proto -v
./build/bin/helix-ui-proto --verbose

# Debug level: Detailed debugging + info + warnings + errors
./build/bin/helix-ui-proto -vv

# Trace level: Everything (very verbose)
./build/bin/helix-ui-proto -vvv
```

**Tip:** For development and debugging, use `-v` or `-vv` to see initialization messages, state changes, and connection events. Use `-vvv` only when diagnosing complex issues (produces very verbose output).

### Formatting

- Use **fmt-style formatting** (modern C++ format): `spdlog::info("Value: {}", val);`
- **NOT** printf-style: ~~`spdlog::info("Value: %d", val);`~~
- Enums must be cast: `spdlog::debug("Panel ID: {}", (int)panel_id);`
- Pointers: `spdlog::debug("Widget: {}", (void*)widget);`

### Best Practices

1. **Choose appropriate log levels:**
   - `trace()` - Observer callbacks, frequent update loops
   - `debug()` - Button clicks, panel transitions, internal state changes
   - `info()` - Initialization complete, major milestones, user actions
   - `warn()` - Invalid input (with fallback), deprecated behavior
   - `error()` - Failed resource loading, NULL pointers, invalid state

2. **Add context to messages:**
   - Include component prefix: `[Temp]`, `[Motion]`, `[Nav]`
   - Include relevant values: `"Temperature: {}°C", temp`
   - Use descriptive messages: "Nozzle panel created and initialized"

3. **Do NOT use:**
   - `printf()` / `fprintf()` - Use spdlog instead
   - `std::cout` / `std::cerr` - Use spdlog instead
   - `LV_LOG_*` macros - Use spdlog instead

4. **Note:** `snprintf()` is fine for **formatting strings into buffers** (not logging).

### Example Conversions

```cpp
// BEFORE:
printf("[Temp] Temperature set to %d°C\n", temp);
std::cerr << "Error: file not found" << std::endl;
LV_LOG_USER("Panel initialized");

// AFTER:
spdlog::info("[Temp] Temperature set to {}°C", temp);
spdlog::error("Error: file not found");
spdlog::info("Panel initialized");
```

## Architecture

```
XML Components (ui_xml/*.xml)
    ↓ bind_text/bind_value/bind_flag
Subjects (reactive data)
    ↓ lv_subject_set_*/copy_*
C++ Wrappers (src/ui_*.cpp)
```

**Component Hierarchy:**
```
app_layout.xml
├── navigation_bar.xml (5 buttons)
└── content_area
    ├── home_panel.xml
    ├── controls_panel.xml (launcher → motion/temps/extrusion sub-screens)
    ├── print_select_panel.xml
    └── [filament/settings/advanced]_panel.xml
```

All components reference `globals.xml` for shared constants (`#primary_color`, `#nav_width`, etc).

## LVGL 9.4 API Changes

**Upgraded from v9.3.0 to v9.4.0** (2025-10-28)

### C++ API Renames

```cpp
// OLD (v9.3):
lv_xml_component_register_from_file("A:/ui_xml/globals.xml");
lv_xml_widget_register("widget_name", create_cb, apply_cb);

// NEW (v9.4):
lv_xml_register_component_from_file("A:/ui_xml/globals.xml");
lv_xml_register_widget("widget_name", create_cb, apply_cb);
```

**Pattern:** All XML registration functions now use `lv_xml_register_*` prefix for consistency.

### XML Event Syntax Change

```xml
<!-- OLD (v9.3): -->
<lv_button>
    <lv_event-call_function trigger="clicked" callback="my_callback"/>
</lv_button>

<!-- NEW (v9.4): -->
<lv_button>
    <event_cb trigger="clicked" callback="my_callback"/>
</lv_button>
```

**Why:** The event callback is now a proper child element (`access="add"` in schema), not a standalone widget tag. This aligns with LVGL's pattern where child elements use simple names.

### Object Alignment Values

```xml
<!-- CORRECT: -->
<lv_obj align="left_mid"/>    <!-- Object positioning -->
<lv_label style_text_align="left"/>  <!-- Text alignment within object -->

<!-- WRONG: -->
<lv_obj align="left"/>  <!-- "left" is not a valid lv_align_t value -->
```

**Valid align values:** `left_mid`, `right_mid`, `top_left`, `top_mid`, `top_right`, `bottom_left`, `bottom_mid`, `bottom_right`, `center`

## Critical Patterns (Project-Specific)

**⚠️ IMPORTANT:** When implementing new features, **always reference existing, working code/XML implementations** for:
- Design patterns and architectural approaches
- Naming conventions and file organization
- Event handler patterns and reactive data flow
- XML component structure and styling patterns
- Error handling and logging practices

**Example:** When creating a new sub-screen panel, review `motion_panel.xml` / `ui_panel_motion.cpp` or `nozzle_temp_panel.xml` / `ui_panel_controls_temp.cpp` for established patterns rather than inventing new approaches.

### 1. Subject Initialization Order ⚠️

**MUST initialize subjects BEFORE creating XML:**

```cpp
// CORRECT ORDER:
lv_xml_register_component_from_file("A:/ui_xml/globals.xml");
lv_xml_register_component_from_file("A:/ui_xml/home_panel.xml");

ui_nav_init();                      // Initialize subjects
ui_panel_home_init_subjects();

lv_xml_create(screen, "app_layout", NULL);  // NOW create UI
```

If subjects are created in XML before C++ initialization, they'll have empty/default values.

### 2. Component Instantiation Names ⚠️

**CRITICAL:** Always add explicit `name` attributes to component tags:

```xml
<!-- app_layout.xml -->
<lv_obj name="content_area">
  <controls_panel name="controls_panel"/>  <!-- Explicit name required -->
  <home_panel name="home_panel"/>
</lv_obj>
```

**Why:** Component names in `<view name="...">` definitions do NOT propagate to `<component_tag/>` instantiations. Without explicit names, `lv_obj_find_by_name()` returns NULL.

See **docs/LVGL9_XML_GUIDE.md** section "Component Instantiation: Always Add Explicit Names" for details.

### 3. Widget Lookup by Name

Always use `lv_obj_find_by_name(parent, "widget_name")` instead of index-based `lv_obj_get_child(parent, 3)`.

```cpp
// In XML: <lv_label name="temp_display" bind_text="temp_text"/>
// In C++:
lv_obj_t* label = lv_obj_find_by_name(panel, "temp_display");
```

See **docs/QUICK_REFERENCE.md** for common patterns.

### 4. Copyright Headers ⚠️

**CRITICAL:** All new source files MUST include GPL v3 copyright header.

**Reference:** `docs/COPYRIGHT_HEADERS.md` for templates (C/C++, XML variants)

### 5. Image Scaling in Flex Layouts ⚠️

**When scaling images immediately after layout changes:** Call `lv_obj_update_layout()` first, otherwise containers report 0x0 size (LVGL uses deferred layout calculation).

**Utility functions:** `ui_image_scale_to_cover()` / `ui_image_scale_to_contain()` in ui_utils.h

**Reference:** ui_panel_print_status.cpp:249-314, ui_utils.cpp:213-276

### 6. Navigation History Stack ⚠️

**Always use `ui_nav_push_overlay()` and `ui_nav_go_back()` for overlay panels (motion, temps, extrusion):**

```cpp
// When showing overlay
ui_nav_push_overlay(motion_panel);  // Pushes current to history, shows overlay

// In back button callback
if (!ui_nav_go_back()) {
    // Fallback: manual navigation if history is empty
}
```

**Behavior:**
- Clicking nav bar icons clears history automatically
- State preserved when navigating back
- All back buttons should use this pattern

**Reference:** ui_nav.h:54-62, ui_nav.cpp:250-327, HANDOFF.md Pattern #0

### 7. LVGL Public API Only ⚠️

**NEVER use private LVGL interfaces or internal structures:**

```cpp
// ❌ WRONG - Private interfaces (will break on updates):
lv_obj_mark_dirty()              // Internal layout/rendering
obj->coords.x1                   // Direct structure access
_lv_* functions                  // Underscore-prefixed internals

// ✅ CORRECT - Public API:
lv_obj_get_x()                   // Public getters/setters
lv_obj_update_layout()           // Public layout control
lv_obj_invalidate()              // Public redraw trigger
```

**Why:** Private APIs can change without notice between LVGL versions, breaking compatibility. They also bypass safety checks and validation.

**When you need internal behavior:** Search for public API alternatives or file an issue with LVGL if no public interface exists.

## Common Gotchas

**⚠️ READ DOCUMENTATION FIRST:** Before implementing features in these areas, **ALWAYS read the relevant documentation** to avoid common pitfalls:
- **Build system/patches:** Read **docs/BUILD_SYSTEM.md** for patch management and multi-display support
- **XML syntax/attributes:** Read **docs/LVGL9_XML_GUIDE.md** "Troubleshooting" section FIRST
- **Flex/Grid layouts:** Read **docs/LVGL9_XML_GUIDE.md** "Layouts & Positioning" section - comprehensive flex and grid reference with verified attributes
- **Data binding patterns:** Read **docs/LVGL9_XML_GUIDE.md** "Data Binding" section for attribute vs child element bindings
- **Component API patterns:** Read **docs/LVGL9_XML_GUIDE.md** "Custom Component API" for advanced component properties
- **Component patterns/registration:** Read **docs/QUICK_REFERENCE.md** "Registration Order" and examples FIRST
- **Icon workflow:** Read **docs/QUICK_REFERENCE.md** "Icon & Image Assets" section FIRST
- **Architecture patterns:** Reference existing working implementations (motion_panel, nozzle_temp_panel) FIRST

### Quick Gotcha Reference

1. **✅ LVGL 9 XML Flag Attribute Syntax** - NEVER use `flag_` prefix in XML attributes. LVGL 9 XML uses simplified syntax:
   - ❌ Wrong: `flag_hidden="true"`, `flag_clickable="true"`, `flag_scrollable="false"`
   - ✅ Correct: `hidden="true"`, `clickable="true"`, `scrollable="false"`

   **Why:** LVGL 9 XML property system auto-generates simplified attribute names from enum values. The C enum is `LV_PROPERTY_OBJ_FLAG_HIDDEN` but the XML attribute is just `hidden`. Parser silently ignores attributes with `flag_` prefix.

   **Status:** ✅ **FIXED** (2025-10-24) - All 12 XML files updated, 80+ incorrect usages corrected.

1a. **✅ Conditional Flag Bindings Use Child Elements** - For conditional show/hide based on subjects, use child elements NOT attributes:
   - ❌ Wrong: `<lv_obj bind_flag_if_eq="subject=value flag=hidden ref_value=0"/>`
   - ✅ Correct: `<lv_obj><lv_obj-bind_flag_if_eq subject="value" flag="hidden" ref_value="0"/></lv_obj>`

   **Available:** `bind_flag_if_eq`, `bind_flag_if_ne`, `bind_flag_if_gt`, `bind_flag_if_ge`, `bind_flag_if_lt`, `bind_flag_if_le`

   **When to use:** Dynamic visibility, conditional enable/disable, responsive UI based on state

1b. **✅ Flex Alignment Uses Three Properties** - Never use `flex_align` (doesn't exist). Use three separate style properties:
   - ❌ Wrong: `<lv_obj flex_align="center center center">`
   - ✅ Correct: `<lv_obj style_flex_main_place="center" style_flex_cross_place="center" style_flex_track_place="start">`

   **Three properties explained:**
   - `style_flex_main_place` - Item distribution along main axis (CSS: justify-content)
   - `style_flex_cross_place` - Item alignment along cross axis (CSS: align-items)
   - `style_flex_track_place` - Track distribution for wrapping (CSS: align-content)

   **Values:** `start`, `center`, `end`, `space_evenly`, `space_around`, `space_between`

   **Verified flex_flow values:** `row`, `column`, `row_reverse`, `column_reverse`, `row_wrap`, `column_wrap`, `row_wrap_reverse`, `column_wrap_reverse`

2. **Subject registration conflict** - If `globals.xml` declares subjects, they're registered with empty values before C++ initialization. Solution: Remove `<subjects>` from globals.xml.

3. **Icon constants not rendering** - Run `python3 scripts/generate-icon-consts.py` to regenerate UTF-8 byte sequences. **Note:** Icon values appear empty in terminal/grep (FontAwesome uses Private Use Area U+F000-U+F8FF) but contain UTF-8 bytes. Verify with: `python3 -c "import re; f=open('ui_xml/globals.xml'); print([match.group(1).encode('utf-8').hex() for line in f for match in [re.search(r'icon_backspace.*value=\"([^\"]*)\"', line)] if match])"` should output `['ef959a']`

4. **BMP screenshots too large** - Always convert to PNG before reading: `magick screenshot.bmp screenshot.png`

5. **Labels not clickable** - Use `lv_button` instead of `lv_label`. While XML has a `clickable` attribute, it doesn't work reliably with labels.

6. **Component names** - LVGL uses **filename** as component name, not view's `name` attribute. File `nozzle_temp_panel.xml` → component `nozzle_temp_panel`.

7. **Right-aligned overlays** - Use `align="right_mid"` attribute for panels docked to right edge (motion, temp, keypad).

## Documentation Structure

**IMPORTANT:** Each documentation file has a specific purpose. Do NOT duplicate content across files.

### Active Work & Planning

**[HANDOFF.md](HANDOFF.md)** - **ACTIVE WORK & ESSENTIAL PATTERNS ONLY**
- **MAXIMUM SIZE:** ~150 lines. If larger, it needs aggressive pruning.
- **Section 1:** Active work status (5-10 lines) + Next priorities (3-5 items)
- **Section 2:** Critical architecture patterns (how-to reference, ~7-8 patterns max)
- **Section 3:** Known issues/gotchas that affect current work (2-4 items max)
- **Update this:** When starting new work, completing tasks, or changing priorities
- **CRITICAL RULE:** When work is COMPLETE, DELETE it from HANDOFF immediately
- **Do NOT put:** Historical details, completed work descriptions, implementation details
- **Keep lean:** If a session added >50 lines to HANDOFF, you did it wrong - prune aggressively

**[ROADMAP.md](docs/ROADMAP.md)** - **PLANNED FEATURES & MILESTONES**
- Future work, planned phases
- Feature prioritization
- Long-term architecture goals
- **Update this:** When planning new features or completing major milestones

**[STATUS.md](STATUS.md)** - **DOCUMENTATION GUIDE & KEY DECISIONS**
- Links to all documentation
- Major architectural decisions with rationale
- **NOT a development journal** - use git history for that

### Technical Reference

**[LVGL9_XML_GUIDE.md](docs/LVGL9_XML_GUIDE.md)** - **COMPLETE LVGL 9 XML REFERENCE**
- XML syntax, attributes, event system
- Component creation patterns
- Troubleshooting (LV_SIZE_CONTENT, event callbacks, etc.)
- Comprehensive technical documentation
- **Update this:** When discovering new LVGL 9 XML patterns or bugs

**[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - **COMMON PATTERNS QUICK LOOKUP**
- Code snippets for frequent tasks
- Widget lookup, subject binding, event handlers
- Material icon conversion workflow
- **Update this:** When establishing new repeatable patterns

**[COPYRIGHT_HEADERS.md](docs/COPYRIGHT_HEADERS.md)** - **GPL v3 HEADER TEMPLATES**
- Required copyright headers for new files
- C/C++ and XML variants
- **Reference this:** When creating any new source files

**[CI_CD_GUIDE.md](docs/CI_CD_GUIDE.md)** - **GITHUB ACTIONS CI/CD PATTERNS**
- Subdirectory workflow configuration (monorepo patterns)
- Platform-specific dependency handling (macOS vs Linux)
- Dependency build order (libhv before main build)
- Quality check best practices (excluding test/generated files)
- Local CI testing procedures
- **Reference this:** When adding workflows or debugging CI failures

## File Organization

```
prototype-ui9/
├── src/              # C++ business logic
├── include/          # Headers
├── ui_xml/           # XML component definitions
├── assets/           # Fonts, images
├── scripts/          # Build/screenshot automation
├── docs/             # Documentation
└── Makefile          # Build system
```

## Using Claude Code Agents

This project has specialized agents - use them proactively to keep context smaller and leverage domain expertise.

### Project-Specific Agents

**widget-maker** - LVGL 9 UI expert
- Creating/modifying UI panels and components
- Implementing XML layouts with reactive data binding
- Working with LVGL 9 XML patterns and subjects
- **Use when:** Any UI panel or component work

**ui-reviewer** - LVGL 9 UI auditor
- Analyzing screenshots against requirements
- Identifying layout/styling issues
- Providing detailed XML fixes
- **Use when:** After taking screenshots, auditing existing UI

**moonraker-api-agent** - Klipper/Moonraker integration expert
- WebSocket communication patterns
- JSON-RPC protocol implementation
- Real-time printer state synchronization
- **Use when:** Implementing Moonraker integration (future work)

### General Agents (see global CLAUDE.md)

**Explore** - Fast codebase exploration
- "How does X work?"
- "Where are Y handled?"
- Architectural pattern discovery
- **Specify thoroughness:** `quick`, `medium`, or `very thorough`

**general-coding-agent** - C++17/embedded systems expert
- Multi-file C++ implementations
- Complex business logic
- Cross-component features

**refractor** - Code optimization
- Refactoring existing code
- Pattern improvements
- Performance optimization

**When in doubt:** Delegate to an agent. They work independently and report back concisely.

## Development Workflow

1. Edit XML for layout changes (no recompilation needed)
2. Edit C++ for logic/subjects changes → `make`
3. Test with `./build/bin/helix-ui-proto [panel_name]` (default size: `small`)
4. Screenshot with `./scripts/screenshot.sh` or press 'S' in UI
5. For complex multi-step tasks → use appropriate agent (see above)

**For current work status:** See HANDOFF.md
**For planned features:** See docs/ROADMAP.md
**For development history:** Use `git log`