# Plan: Helix XML Engine — Extract, Upgrade & Extend

**Created:** 2026-02-18
**Branch:** `feature/helix-xml` (worktree: `.worktrees/helix-xml`)
**Issue:** #25 (XML engine improvements)

---

## Context

LVGL 9.5.0 released Feb 18, 2026 — the first version **without** XML support. The XML engine was removed on Jan 27 (PR #9565) and moved behind LVGL Pro (paid). HelixScreen's entire UI is XML-based: 187 files, 16K lines, 140+ source files calling `lv_xml_*` APIs.

Kevin (coevin), one of the original XML authors, extracted it into [lui-xml/lui-xml](https://github.com/lui-xml/lui-xml) as a standalone LVGL extension. He's expressed strong interest in collaborating (issue #25).

**Goal**: Extract the XML engine, upgrade LVGL to 9.5, and extend the XML system with dynamic lists, richer bindings, slots, and hot reload.

---

## Decision: lui-xml vs Self-Extract

**Decision: Collaborate with lui-xml, but keep our own copy (`lib/helix-xml/`).**

Rationale:
- lui-xml renamed everything to `lui_xml_*` — touching 140 source files is massive churn for no functional benefit
- We preserve the `lv_xml_*` namespace (zero app-code changes)
- We can contribute improvements upstream to lui-xml when they stabilize
- Long-term: adopt lui-xml's namespace with a compatibility header, or Kevin supports `lv_xml_*` as build option

---

## Phase 1: Extract XML & Upgrade LVGL to 9.5 — COMPLETE

**Status:** Done (2026-02-18) | **Branch:** `feature/helix-xml` | **Tests:** 4921 assertions, 132 test cases

### Commits

| Hash | Description |
|------|-------------|
| `37598563` | refactor(xml): extract XML engine into lib/helix-xml with patches baked in |
| `6f3f1524` | build(lvgl): upgrade submodule from 9.4-pre to v9.5.0, regenerate SDL patch |
| `83547c13` | refactor(xml): decouple helix-xml from LVGL internals for v9.5 compat |

### What Was Done

**Step 1B — Extract XML into `lib/helix-xml/`:**
- Copied `lib/lvgl/src/xml/` and `lib/lvgl/src/libs/expat/` into `lib/helix-xml/`
- Baked in all 4 XML patches permanently (no more patch files for XML)
- Symlinked shared LVGL internals (`src/misc`, `src/core`, etc.) to avoid duplication
- Updated Makefile source discovery and include paths
- Added `helix_xml.h` and `helix_xml_private.h` umbrella headers

**Step 1C — Upgrade LVGL to v9.5.0:**
- Advanced submodule from `c849861b2` (Nov 2025) to `85aa60d18` (v9.5.0) — 274 commits of improvements
- Regenerated `lvgl_sdl_window.patch` against v9.5.0's refactored SDL backend
- 5/6 non-XML patches applied cleanly; SDL patch manually ported to new `lv_sdl_backend_ops` pattern
- Removed 4 XML patches (now baked into helix-xml)

**Decoupling from LVGL internals:**
- Created `lv_xml_types.h` — forward declarations removed from v9.5's `lv_types.h`
- Created `lv_xml_globals.h/.c` — standalone globals removed from v9.5's `_lv_global_t`
- Replaced all relative `../../../lvgl.h` includes with `<lvgl.h>` system includes
- Updated 44 app source files from `lvgl/src/xml/` to `helix-xml/src/xml/` paths
- Added `helix_xml.h` to PCH for seamless app-level access
- All new files use MIT license (matching LVGL's original)

### Key Files Created

| File | Purpose |
|------|---------|
| `lib/helix-xml/helix_xml.h` | Umbrella public header (included from PCH) |
| `lib/helix-xml/helix_xml_private.h` | Umbrella private header |
| `lib/helix-xml/src/xml/lv_xml_types.h` | Forward declarations for XML types |
| `lib/helix-xml/src/xml/lv_xml_globals.h` | Global state declarations |
| `lib/helix-xml/src/xml/lv_xml_globals.c` | Global state storage |

### Patches After Upgrade

Remaining in `patches/` (LVGL-only, non-XML):
- `lvgl_sdl_window.patch` — Window title, positioning, quit, Android scaling (regenerated for v9.5)
- `lvgl_theme_breakpoints.patch` — Theme breakpoints
- `lvgl_fbdev_stride_bpp.patch` — Framebuffer stride/BPP fixes
- `lvgl_observer_debug.patch` — Observer debugging
- `lvgl_slider_scroll_chain.patch` — Slider scroll chain fix
- `lvgl_strdup_null_guard.patch` — Null guard for strdup

Removed (baked into helix-xml):
- ~~`lvgl_xml_const_silent.patch`~~
- ~~`lvgl_image_parser_contain.patch`~~
- ~~`lvgl_translate_percent.patch`~~
- ~~Other XML patches~~

---

## Phase 2: Hot Reload for Development (Quick Win, Huge DX Impact)

**Status:** Not started | **Effort:** Small | **Blocks:** Phase 1 (done)

The XML system already supports re-registration. Core mechanism:

1. Use `inotify` (Linux) / `kqueue` (macOS) to watch `ui_xml/` for changes
2. On change: `lv_xml_register_component_from_file()` to re-register the component
3. Destroy and recreate the current panel/overlay to pick up the new definition

**Implementation:**
- New class: `XmlHotReloader` in `src/application/`
- Only active when `HELIX_HOT_RELOAD=1` env var is set (dev builds only)
- File watcher thread → queue "reload component X" via `ui_queue_update()`
- Reload handler: re-register XML, then `NavigationManager` refreshes current view

**Challenges:**
- Component re-registration: verify `lv_xml_register_component_from_file()` handles replacing existing registrations
- State preservation: scroll position, active inputs lost on reload (acceptable for dev)
- Subject bindings: re-created widgets get subjects re-bound via observer factory (global subjects)

**Verification:** Change XML file while app running → UI updates without restart.

---

## Phase 3: Slots (Available — Need Migration)

**Status:** Not started | **Effort:** Small | **Blocks:** Phase 1 (done)

Slot support (`f38718108`) is included in our extracted XML. The syntax is available now.

**Work needed:**
- Verify slot syntax with our component registration flow
- Implement designs from `docs/devel/SLOT_COMPONENT_DESIGNS.md`:
  - `kinematics_icon.xml` — dual-icon switcher
  - `tune_slider_card.xml` — card with slider
  - `conditional_container.xml` — visibility wrapper
  - `z_offset_button.xml` — z-offset button with slot
  - `state_icon.xml` — state-mapped icon switcher
- Migrate existing XML to use slots
- **Estimated savings:** ~140 lines of XML reduced

---

## Phase 4: Richer Reactive Bindings (Medium Effort, High Impact)

**Status:** Not started | **Blocks:** Phase 1 (done)

### 4A: `bind_text_switch` — Conditional Text by Subject Value

```xml
<text_body bind_text_switch="connection_state"
           case_0="Disconnected" case_1="Connecting" case_2="Connected"/>
```

~100 lines of C in `lv_xml_update.c`. Eliminates dual-label visibility pattern.

### 4B: `bind_flag_if` with Compound Conditions

```xml
<bind_flag_if flag="hidden" condition="state == 1 AND enabled == 1"/>
```

~200-300 lines of C. Expression parser for simple boolean logic, multiple subject observers.

### 4C: Value Transforms / Formatters

```xml
<text_body bind_text="temp_c" bind_text_fmt="%d°C"/>
```

~50-100 lines. Extend existing `fmt` attribute to support `%d`, `%f`, `%%` formatting.

**Estimated impact:** Eliminates ~717 anti-pattern calls across 77 files.

---

## Phase 5: Dynamic List Rendering (`<repeat>`) (High Effort, Highest Impact)

**Status:** Not started | **Effort:** Large | **Blocks:** Phase 1 (done)

```xml
<repeat source="macro_list" template="macro_card" key="name"/>
```

**Approach:**
- Start simple: `<repeat>` maps to a C++ list provider via `lv_xml_register_list_source()`
- Provider callback returns count + per-item attributes
- On change: destroy children, recreate from template
- Phase 5B: key-based diffing
- Phase 5C: virtual scrolling for large lists

**Estimated impact:** Eliminates 30-40% of UI C++ code.

---

## Priority Summary

| Phase | Feature | Effort | Impact | Status |
|-------|---------|--------|--------|--------|
| 1 | Extract XML + Upgrade to 9.5 | Medium | Foundational | **COMPLETE** |
| 2 | Hot Reload | Small | DX game-changer | Not started |
| 3 | Slots | Small | ~140 XML lines | Not started |
| 4A | bind_text_switch | Small | High | Not started |
| 4B | Compound conditions | Medium | High | Not started |
| 4C | Value transforms/fmt | Small | Medium | Not started |
| 5 | Dynamic lists (`<repeat>`) | Large | Highest | Not started |

Phases 2, 3, and 4A-C can proceed in parallel.

---

## Collaboration Note

Kevin (coevin, lui-xml author) was encouraging about the ideas in issue #25 and mentioned unreleased work. Recommend reaching out about sharing improvements and coordinating on bigger features (repeat, bindings).
