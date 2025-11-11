# HelixScreen AI Agents

This directory contains specialized AI agents for the HelixScreen project. Each agent is designed to be an expert in specific aspects of the codebase and can be used with Claude Code to accelerate development.

## Available Agents

### 1. 🎨 [Widget Maker Agent](./widget-maker.md) ⭐ PRIMARY
**Expertise:** LVGL 9 XML UI system, reactive data binding, modern declarative UI patterns

**Use this agent for:**
- Creating/modifying LVGL 9.4 XML UI components
- Implementing reactive data binding with subjects
- Working with XML layouts (app_layout.xml, panel XMLs, component XMLs)
- Designing navigation, panels, or custom widgets
- Event callbacks and theming in the XML/C++ hybrid system

**Example prompts:**
- "Create a temperature card widget with theme-aware colors"
- "Implement a navigation overlay panel with back button"
- "Add click handlers to XML-defined buttons"
- "Update this panel to use reactive subjects instead of direct updates"

---

### 2. 🔍 [UI Reviewer Agent](./ui-reviewer.md)
**Expertise:** LVGL 9 UI verification, requirements validation, visual analysis, screenshot comparison

**Use this agent for:**
- Review UI screenshots against detailed requirements
- Verify LVGL 9.4 XML implementations match design specs
- Identify layout, sizing, or styling issues
- Generate specific XML/C++ fixes with line numbers
- Validate changelog items were applied correctly

**Example prompts:**
- "Review this screenshot against the home panel requirements"
- "Verify all spacing matches the design spec in globals.xml"
- "Check if the recent theme fixes in the changelog are visible"
- "Compare these before/after screenshots for visual regressions"

---

### 3. 🔎 [Critical Reviewer Agent](./critical-reviewer.md)
**Expertise:** Security analysis, edge case testing, paranoid code review, quality assurance

**Use this agent for:**
- Security and safety analysis (input validation, resource leaks, race conditions)
- Comprehensive edge case testing
- Performance and maintainability review
- Testing validation (demands proof: build output, test results, runtime validation)
- Identifying hidden complexity and technical debt

**Example prompts:**
- "Review this WiFi backend for security vulnerabilities"
- "Analyze this WebSocket client for thread safety issues"
- "What edge cases am I missing in this file parser?"
- "Review this PR for production readiness"

---

### 4. 🌐 [Moonraker API Agent](./moonraker-api-agent.md)
**Expertise:** WebSocket communication, Klipper API, real-time updates, printer state management

**Use this agent for:**
- Implementing new Klipper/Moonraker API calls
- Handling WebSocket notifications and callbacks
- Managing printer state synchronization
- Debugging communication issues
- Adding new printer control features

**Example prompts:**
- "How do I subscribe to temperature updates?"
- "Implement a panel that controls LED lights via Moonraker"
- "Debug why my WebSocket callbacks aren't being triggered"
- "Add support for the new Klipper exclude_object API"

---

### 5. 🖼️ [G-code Preview Agent](./gcode-preview-agent.md)
**Expertise:** Thumbnail extraction, file browsers, image optimization, G-code metadata parsing

**Use this agent for:**
- Extract and display G-code thumbnails
- Implement file browser functionality
- Optimize image loading for embedded displays
- Parse G-code metadata (print time, filament, etc.)
- Handle different slicer formats (PrusaSlicer, OrcaSlicer, SuperSlicer, Cura)

**Example prompts:**
- "Extract thumbnails from PrusaSlicer G-code files"
- "Implement a thumbnail cache with LRU eviction"
- "Add support for QOI image format from OrcaSlicer"
- "Parse metadata from Cura G-code comments"

---

### 6. 🔨 [Cross-Platform Build Agent](./cross-platform-build-agent.md)
**Expertise:** Multi-platform builds, dependency management, toolchain configuration

**Use this agent for:**
- Configure builds for different targets (SDL2 simulator, Raspberry Pi, embedded)
- Troubleshoot compilation or linking errors
- Optimize build performance
- Handle platform-specific build issues (macOS, Linux)
- Verify binary dependencies and library versions

**Example prompts:**
- "Help me set up cross-compilation for Raspberry Pi"
- "Why is libhv.so not found when running the simulator?"
- "How do I optimize the build for ARM processors?"
- "Debug these linker errors related to SDL2"

---

### 7. 🧪 [Test Harness Agent](./test-harness-agent.md)
**Expertise:** Unit testing, mocking, test fixtures, CI/CD pipelines

**Use this agent for:**
- Create unit tests for panels or components
- Mock WebSocket responses and hardware backends
- Generate test fixtures for different printer states
- Set up automated testing pipelines (GitHub Actions)
- Debug test failures and flaky tests

**Example prompts:**
- "Create unit tests for the PrintStatusPanel"
- "Mock a complete print workflow for integration testing"
- "Set up GitHub Actions for automated testing on macOS and Linux"
- "Generate test fixtures for emergency stop scenarios"

---

## Agent Usage Patterns

### With Claude Code

Agents are automatically available to Claude Code when working in this project. Reference them explicitly:

```
@widget-maker create a new overlay panel for filament loading
```

### Loading Context Manually

If you need to explicitly load an agent:

1. **Reference the agent in your prompt:** `I need help with [specific task]. Use the widget-maker agent.`
2. **Be specific:** Provide code snippets, file paths, or error messages
3. **Provide context:** Share relevant requirements or design specs

Example:
```
@ui-reviewer Review this screenshot of the home panel against the requirements
in docs/REQUIREMENTS.md. The screenshot is at screenshots/home-panel.png.
```

### Combining Agents

For complex tasks, multiple agents can work together:

```
@widget-maker create the XML layout for a new temperature control panel
@moonraker-api-agent integrate it with Klipper heater control
@ui-reviewer verify the implementation matches the design spec
```

---

## Project-Specific Agent Priorities

**For UI work (80% of development):**
1. 🎨 **Widget Maker** - Primary agent for all UI/XML work
2. 🔍 **UI Reviewer** - Verify implementations match requirements
3. 🔎 **Critical Reviewer** - Pre-commit code review

**For backend/API work:**
1. 🌐 **Moonraker API** - WebSocket and Klipper integration
2. 🖼️ **G-code Preview** - File handling and thumbnails
3. 🧪 **Test Harness** - Comprehensive testing

**For build/infra work:**
1. 🔨 **Cross-Platform Build** - Build system issues
2. 🧪 **Test Harness** - CI/CD pipelines

---

## Agent Maintenance Guidelines

### When to Update Agents

Update agents when:
- Major architectural changes occur (e.g., LVGL 9.3 → 9.4)
- New patterns or conventions are adopted
- Common issues are discovered and solved
- Dependencies are updated or added
- Project structure changes significantly

### How to Update Agents

1. **Test first:** Verify the agent's advice is still accurate
2. **Document changes:** Note what changed and why in git commit
3. **Update examples:** Ensure code examples compile and run
4. **Verify context:** Check that project-specific references are still valid

### Agent Versioning

Agents are versioned with the project. When updating:
- Reference the current LVGL version (9.4)
- Reference current dependency versions
- Keep examples synchronized with CLAUDE.md patterns
- Remove deprecated patterns and APIs

---

## Project Context

- **Framework:** LVGL 9.4 with declarative XML-based UI system
- **Architecture:** Reactive Subject-Observer data binding
- **Backend:** Moonraker/Klipper WebSocket API via libhv
- **Build:** GNU Make with modular build system
- **Platform:** SDL2 simulator (development), framebuffer (production)

**Key Documentation:**
- [CLAUDE.md](../../CLAUDE.md) - Project patterns and critical rules
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - System design
- [LVGL9_XML_GUIDE.md](../../docs/LVGL9_XML_GUIDE.md) - XML syntax reference
- [BUILD_SYSTEM.md](../../docs/BUILD_SYSTEM.md) - Build internals

---

## Quick Reference

| Task | Agent |
|------|-------|
| Create/modify XML UI | 🎨 Widget Maker |
| Review UI screenshots | 🔍 UI Reviewer |
| Security/quality review | 🔎 Critical Reviewer |
| Moonraker API integration | 🌐 Moonraker API |
| G-code file handling | 🖼️ G-code Preview |
| Build system issues | 🔨 Cross-Platform Build |
| Testing & CI/CD | 🧪 Test Harness |

---

Last updated: November 2025
HelixScreen version: Phase 2 (Setup Wizard & Connectivity)
