# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ MANDATORY AGENT DELEGATION POLICY ⚠️

**YOU WILL BE GIVEN 100 LASHES IF YOU FAIL TO DELEGATE TO AGENTS WHEN REQUIRED.**

This is **NOT** a suggestion. This is a **HARD REQUIREMENT**. Violating this wastes tokens, burns context, and ignores specialized agent expertise.

### YOU MUST USE AGENTS FOR (NOT OPTIONAL):

1. **Any UI/XML work** → `widget-maker` agent
   - Modifying XML layouts
   - Creating/updating UI components
   - Working with LVGL 9 XML patterns
   - Updating multiple XML files with similar changes

2. **Systematic file modifications (3+ files with same pattern)** → Appropriate agent
   - Refactoring across codebase → `refractor`
   - Adding similar features to multiple panels → `widget-maker` or `general-coding-agent`
   - Updating imports/patterns across files → `refractor`

3. **Codebase exploration** → `Explore` agent with thoroughness level
   - "How does X work?"
   - "Where are Y handled?"
   - Finding architectural patterns
   - **Specify thoroughness:** `quick`, `medium`, or `very thorough`

4. **Code review** → `code-reviewer` agent
   - After completing significant features
   - Before committing major changes

### IMMEDIATE STOP SIGNALS - Delegate NOW:

- You're about to read/edit 3+ similar files → **STOP. Use agent.**
- You're modifying UI XML → **STOP. Use widget-maker.**
- You're doing repetitive edits across files → **STOP. Use refractor.**
- You're searching "how does X work?" → **STOP. Use Explore agent.**
- You're implementing a multi-file feature → **STOP. Use appropriate agent.**

### Correct Agent Usage Pattern:

```
1. Recognize delegation opportunity
2. Invoke Task tool with:
   - Correct subagent_type
   - Complete, detailed prompt
   - What you need back
3. Wait for agent report
4. Act on results
```

### WRONG Pattern (DO NOT DO THIS):

```
1. Start reading files yourself
2. Start editing files yourself
3. Realize halfway through you should have used agent
4. Continue anyway (wasting context)
```

**Remember: Agents work independently, report concisely, and keep your context clean. USE THEM.**

---

## ⚠️ CRITICAL RULES - CHECK BEFORE CODING ⚠️

**These rules are MANDATORY and frequently violated. Check this list before proposing any code changes.**

### 1. NO Hardcoded Colors or Dimensions in C++

**Rule**: ALL colors and responsive dimensions must be defined in `globals.xml` and read at runtime via `lv_xml_get_const()`.

```cpp
// ❌ WRONG - Hardcoded color (violates architecture)
lv_obj_set_style_border_color(obj, lv_color_hex(0xE0E0E0), LV_PART_MAIN);

// ✅ CORRECT - Read from XML
const char* border_color_str = lv_xml_get_const(NULL, "card_border_light");
lv_color_t border_color = ui_theme_parse_color(border_color_str);
lv_obj_set_style_border_color(obj, border_color, LV_PART_MAIN);
```

**Pattern for theme-aware colors**:
1. Define `*_light` and `*_dark` variants in `globals.xml`
2. Read both variants in C++ init function
3. Apply appropriate color based on theme mode

**Example**: See `ui_card.cpp:39-59` for complete implementation pattern.

**Why**: No recompilation needed for theme changes, consistent styling, dark/light mode support.

### 2. Reference Existing Code Patterns FIRST

**Rule**: Before implementing any feature, find and study similar existing implementations.

- New UI panel? → Study `motion_panel.xml` / `ui_panel_motion.cpp`
- New overlay? → Study `nozzle_temp_panel.xml` / `ui_panel_controls_temp.cpp`
- Theme-aware widget? → Study `ui_card.cpp` pattern
- Responsive constants? → Study `ui_wizard.cpp:73-131`

**Why**: Maintains architectural consistency, avoids reinventing patterns, prevents subtle bugs.

### 3. Use spdlog for ALL Console Output

**Rule**: NEVER use printf, cout, cerr, LV_LOG_* macros. Use spdlog exclusively.

```cpp
// ❌ WRONG - Using printf/cout/LV_LOG_*
printf("[Temp] Temperature: %d°C\n", temp);
std::cout << "Error: file not found" << std::endl;
LV_LOG_USER("Panel initialized");

// ✅ CORRECT - Using spdlog with fmt-style formatting
#include <spdlog/spdlog.h>
spdlog::info("[Temp] Temperature: {}°C", temp);
spdlog::error("Error: file not found");
spdlog::info("Panel initialized");
```

**Log levels**: `trace()` (very detailed), `debug()` (development), `info()` (normal events), `warn()` (recoverable issues), `error()` (failures)

**Verbosity flags**: `-v` (info), `-vv` (debug), `-vvv` (trace). Default: warn only.

**Why**: Consistent output, configurable verbosity, proper formatting.

### 4. NEVER Use Mock Implementations in Production Builds

**Rule**: Mock implementations must NEVER be automatically used in production. Always check TestConfig before using mocks.

```cpp
// ❌ WRONG - Automatic mock fallback (DANGEROUS)
if (!backend->start()) {
    return std::make_unique<WifiBackendMock>();  // NEVER DO THIS
}

// ✅ CORRECT - Check test mode first
const auto& config = get_test_config();
if (config.should_mock_wifi()) {
    return std::make_unique<WifiBackendMock>();  // OK in test mode
}

// Production: Try real backend, return nullptr if unavailable
auto backend = std::make_unique<WifiBackendMacOS>();
if (!backend->start()) {
    return nullptr;  // Fail gracefully, NO FALLBACK
}
```

**Production Safety Checklist**:
1. **No automatic fallbacks** - Production fails gracefully without hardware
2. **Explicit test mode** - Mocks require `--test` command-line flag
3. **Clear error messages** - Show user-friendly errors when hardware unavailable
4. **Visual indicators** - Test mode must show banner/badge

**Why**: Production users should NEVER accidentally get mock data. It hides real problems, creates security issues, and causes debugging nightmares.

### 5. Read Documentation Before Implementation

**Rule**: For areas with known gotchas, read the relevant docs BEFORE coding:

- XML syntax/attributes → **docs/LVGL9_XML_GUIDE.md** "Troubleshooting"
- Flex/Grid layouts → **docs/LVGL9_XML_GUIDE.md** "Layouts & Positioning"
- Build system/patches → **docs/BUILD_SYSTEM.md**
- Architecture patterns → **ARCHITECTURE.md**

**Why**: Prevents repeating known mistakes, saves debugging time.

---

## Project Overview

This is the **LVGL 9 UI Prototype** for HelixScreen - a declarative XML-based touch UI system using LVGL 9.4 with reactive Subject-Observer data binding. The prototype runs on SDL2 for rapid development and will eventually target framebuffer displays on embedded hardware.

**Key Innovation:** Complete separation of UI layout (XML) from business logic (C++), similar to modern web frameworks. No manual widget management - all updates happen through reactive subjects.

## Quick Start

**See DEVELOPMENT.md for complete setup instructions.**

**Essential dependencies**: SDL2, clang/gcc, make, python3, npm
**Install**: `brew install sdl2` (macOS) or `sudo apt install libsdl2-dev` (Linux), then `npm install`

**Build & Run**:

```bash
make -j                          # Parallel build
./build/bin/helix-ui-proto       # Run (default: home panel, small screen)
./build/bin/helix-ui-proto -p motion -s large  # Specific panel/size
```

**Common commands**:
- `make -j` - Parallel build (auto-detects cores)
- `make build` - Clean build
- `make help` - Show all targets
- **NEVER invoke compilers directly** - always use `make`

**Binary**: `build/bin/helix-ui-proto`
**Panels**: home, controls, motion, nozzle-temp, bed-temp, extrusion, filament, settings, advanced, print-select

**Screenshots**: Use `./scripts/screenshot.sh helix-ui-proto output [panel_name]` (see DEVELOPMENT.md)

## Architecture

```
XML Components (ui_xml/*.xml)
    ↓ Theme constants + bind_text/bind_value/bind_flag
LVGL Theme System (ui_theme.cpp)
    ↓ Reads XML constants, initializes theme
Subjects (reactive data)
    ↓ lv_subject_set_*/copy_*
C++ Wrappers (src/ui_*.cpp)
```

**Theme System:**
- `globals.xml` defines theme colors with light/dark variants (e.g., `*_light`/`*_dark` suffix pattern) and semantic fonts
- `ui_theme.cpp` reads color variants from XML and overrides runtime constants based on theme preference
- **NO hardcoded colors in C++** - all color values defined in XML
- Supports dark/light mode via `--dark`/`--light` command-line flags
- Theme preference persisted in `helixconfig.json` and restored on next launch
- **No recompilation needed** - edit `globals.xml` to change theme colors

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

### Pluggable Backend Pattern

**Platform-specific functionality (WiFi, Ethernet) uses abstract interfaces:**

```
Manager (high-level API)
  ↓ Factory Pattern
Backend (abstract interface)
  ↓ Implemented by platform-specific backends
{macOS Backend, Linux Backend, Mock Backend}
```

**Why this pattern:**
- No platform `#ifdef` clutter in manager code
- All backends compile on all platforms (factory selects at runtime)
- Mock backends enable testing without hardware
- Consistent pattern across WiFi, Ethernet, future features

**Example:** WiFi uses WiFiManager → WifiBackend → {WifiBackendMacOS, WifiBackendWpaSupplicant, WifiBackendMock}
**Example:** Ethernet uses EthernetManager → EthernetBackend → {EthernetBackendMacOS, EthernetBackendLinux, EthernetBackendMock}

### WiFi Backend Architecture

**Security-hardened modular WiFi system using pluggable backends:**

```
WiFiManager (UI Integration)
    ↓ Factory Pattern
WifiBackend (Abstract Interface)
    ├── WifiBackendWpaSupplicant (Linux - Real wpa_supplicant)
    └── WifiBackendMock (macOS/Simulator - LVGL timers)
```

**Key Design Principles:**
- **No platform ifdefs** in manager code - clean abstraction
- **Security-first** - Input validation, resource cleanup, thread safety
- **Event-driven** - libhv async I/O + callback dispatch to UI
- **Extensible** - Easy to add NetworkManager, systemd-networkd backends

**Thread Safety Model:**
```
libhv EventLoop Thread          LVGL Main Thread
    ↓ wpa_supplicant events          ↓ UI updates
WifiBackend::handle_events  →  WiFiManager callbacks
    (mutex protected)           (LVGL timer dispatch)
```

**Security Features:**
- Command injection prevention (input validation)
- Resource leak elimination (RAII cleanup)
- Password redaction in logs
- Buffer overflow protection
- Exception-safe callback dispatch

**Usage Pattern:**
```cpp
// Initialize in wizard/app
auto wifi_manager = std::make_unique<WiFiManager>();

// Register callbacks
wifi_manager->register_event_callback("SCAN_COMPLETE", [](const std::string& data) {
    // Handle scan results in UI thread
});

// Trigger operations
wifi_manager->start_scan(on_networks_updated);
wifi_manager->connect_network(ssid, password, on_connect_complete);
```

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

### 8. API Documentation Standards ⚠️

**Rule**: All public APIs must be documented with Doxygen-style comments before committing.

```cpp
// ❌ WRONG - Undocumented public method
class WiFiManager {
public:
    void connect(const std::string& ssid, const std::string& password);
};

// ✅ CORRECT - Properly documented with @brief, @param descriptions
class WiFiManager {
public:
    /**
     * @brief Connect to WiFi network
     *
     * Attempts to connect to the specified network. Operation is asynchronous;
     * callback invoked when connection succeeds or fails.
     *
     * @param ssid Network name
     * @param password Network password (empty for open networks)
     */
    void connect(const std::string& ssid, const std::string& password);
};
```

**Pattern for complete documentation:**
1. `@brief` - Mandatory one-line summary for all public classes/methods
2. `@param` - Document ALL parameters with clear descriptions
3. `@return` - Document return values for non-void functions
4. Detailed description - Explain async behavior, thread safety, side effects
5. Usage examples - Add for complex or non-obvious APIs

**Reference examples:**
- `include/moonraker_client.h` - Comprehensive method documentation
- `include/wifi_manager.h` - Clear API with behavioral notes
- `include/wifi_backend.h` - Abstract interface patterns
- `include/ethernet_manager.h` - Concise with usage examples

**Why**: Generated API documentation is automatically published to GitHub Pages on releases. Good documentation enables external developers to integrate with HelixScreen without reading source code.

**Complete guide:** See **docs/DOXYGEN_GUIDE.md** for detailed patterns and best practices.

**CI/CD**: Documentation is auto-generated and published:
1. Push version tag: `git tag v0.1.0 && git push origin v0.1.0`
2. GitHub Actions runs Doxygen
3. Generated HTML committed to `docs/api/`
4. GitHub Pages deploys automatically

**Test locally before pushing:**
```bash
doxygen Doxyfile
open build/docs/html/index.html  # Verify formatting and completeness
```

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

### Documentation Philosophy

**Keep it lean:**
- HANDOFF.md ≤ 150 lines (prune completed work aggressively)
- Git commits are the source of truth for "what happened"
- Patterns go in CLAUDE.md once established
- Delete documentation when it's no longer relevant

**Trust the tools:**
- Git history > development journals
- Code > comments
- Working examples > abstract explanations

### Developer Documentation

**[README.md](README.md)** - **PROJECT OVERVIEW & QUICK START**
- Project description and goals
- Target hardware overview
- Essential build and run instructions
- Key features demonstration
- **Reference this:** For project introduction and getting started

**[DEVELOPMENT.md](DEVELOPMENT.md)** - **BUILD SYSTEM & DAILY WORKFLOW**
- Complete dependency installation guide
- Build system usage and troubleshooting
- Multi-display support and screenshot workflow
- IDE setup and development tools
- **Reference this:** For daily development tasks and environment setup

**[ARCHITECTURE.md](ARCHITECTURE.md)** - **SYSTEM DESIGN & TECHNICAL PATTERNS**
- XML → Subject → C++ data flow architecture
- Component hierarchy and design decisions
- Memory management and thread safety
- Critical implementation patterns
- **Reference this:** For understanding system design and architectural decisions

**[CONTRIBUTING.md](CONTRIBUTING.md)** - **CODE STANDARDS & SUBMISSION GUIDELINES**
- Git workflow and commit standards
- Code style and logging requirements
- Testing procedures and submission guidelines
- Asset management workflows
- **Reference this:** For code standards and contribution process

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

**[BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)** - **BUILD CONFIGURATION & PATCHES**
- Modular Makefile architecture
- Automatic patch application system
- Dependency management and platform detection
- **Reference this:** For advanced build system details

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

**See MANDATORY AGENT DELEGATION POLICY at top of this file.**

**Project agents**: `widget-maker` (UI/XML work), `ui-reviewer` (UI auditing), `moonraker-api-agent` (Klipper integration)
**General agents**: `Explore` (codebase exploration), `general-coding-agent` (multi-file C++), `refractor` (systematic changes 3+ files)

## Development Workflow

### Session Startup (CRITICAL)

**ALWAYS start sessions by checking recent git history:**

```bash
git log --oneline -10   # Recent commits and what was just completed
git log --stat -5       # Recent changes with file details
git status              # Current working state
```

**Why this matters:**
- Understand recent work and current project focus
- Avoid suggesting approaches that were just tried/rejected
- Build on recent architectural decisions instead of starting from scratch
- Maintain context about what was just implemented or refactored

### Daily Development Cycle

1. **Check git history** (see above) to understand recent context
2. Edit XML for layout changes (no recompilation needed)
3. Edit C++ for logic/subjects changes → `make`
4. Test with `./build/bin/helix-ui-proto [panel_name]` (default size: `small`)
5. Screenshot with `./scripts/screenshot.sh` or press 'S' in UI
6. For complex multi-step tasks → use appropriate agent (see above)

**For current work status:** See HANDOFF.md
**For planned features:** See docs/ROADMAP.md
**For development history:** Use `git log`