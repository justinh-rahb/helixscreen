# Codebase Refactor Plan

This document outlines remaining refactoring work identified during the December 2025 codebase audit. The audit analyzed RAII patterns, design tokens, event callbacks, architecture, and code style.

## Status

- **Phase 1: COMPLETED** (commit df72783) - SubscriptionGuard RAII wrapper, AMS backend migration, initial design token fixes
- **Phase 2: COMPLETED** (commit 70ffefd) - ObserverGuard RAII migration for ui_status_bar.cpp, ui_nav.cpp, ui_notification.cpp
- **Phase 3.1: COMPLETED** - Color token migration for priority visualization files (ui_step_progress, ui_temp_graph, ui_filament_path_canvas, ui_jog_pad, ui_ams_slot)
- **Phase 3.1b: COMPLETED** - Color token migration for remaining files (ui_bed_mesh, ui_keyboard, ui_spool_canvas, ui_panel_print_select, ui_panel_screws_tilt, ui_wizard_printer_identify)
- **Phase 3.2: DEFERRED** - Padding/spacing tokens (445+ instances - low priority)
- **Phase 4: ANALYZED** - Event callback migration (see analysis below)
- **Phase 5: COMPLETED** - Architecture refactoring (class-based managers)

---

## Phase 2: RAII Migrations ✅ COMPLETED

**Commit:** 70ffefd

### 2.1 Global Observers - DONE

Migrated to static `ObserverGuard` instances:
- ✅ `src/ui_status_bar.cpp` - 3 observers (network, connection, klippy)
- ✅ `src/ui_nav.cpp` - 2 observers (active panel, connection state)
- ✅ `src/ui_notification.cpp` - 1 observer (notification subject)

**Remaining (LOW priority):**
- `src/ui_text_input.cpp:168` - Widget-attached observer (may need widget cleanup callback instead)

### 2.2 Future RAII Wrappers (Optional)

Consider for future work:
1. **TimerGuard** - For `lv_timer_t*` handles (auto-delete on destruction)
2. **AnimGuard** - For `lv_anim_t` handles (auto-stop on destruction)

---

## Phase 3: Design Token Migration (HIGH PRIORITY)

### 3.1 Color Token Violations ✅ PARTIALLY COMPLETED

**41 instances** of `lv_color_hex()` in 18 files should use `ui_theme_get_color()` or `ui_theme_parse_color()`.

#### Completed Files:

| File | Violations Fixed | Status |
|------|-----------------|--------|
| `src/ui_step_progress.cpp` | 6 | ✅ Migrated to `step_*` tokens |
| `src/ui_temp_graph.cpp` | 4 | ✅ Migrated to `graph_bg_*` tokens |
| `src/ui_filament_path_canvas.cpp` | 12+ | ✅ Migrated to `filament_*` tokens |
| `src/ui_jog_pad.cpp` | 10 | ✅ Migrated to `jog_*` tokens |
| `src/ui_ams_slot.cpp` | 8+ | ✅ Migrated to `ams_*` + semantic tokens |

#### Remaining Files (LOW priority):

| File | Violations | Colors Used |
|------|------------|-------------|
| `src/ui_bed_mesh.cpp` | 3+ | Mesh visualization colors |
| `src/ui_keyboard.cpp` | 4+ | Key highlights, alternatives popup |
| `src/ui_panel_print_select.cpp` | 3+ | Selection highlight, card borders |
| `src/ui_panel_history_dashboard.cpp` | 2+ | Chart colors |
| `src/ui_panel_screws_tilt.cpp` | 2+ | Screw indicator colors |
| `src/ui_wizard_printer_identify.cpp` | 2+ | Selection states |
| `src/ui_spool_canvas.cpp` | 2+ | Spool visualization |
| `src/ui_panel_temp_control.cpp` | 2+ | Temperature indicators |
| `src/ui_panel_ams.cpp` | 2+ | Slot state colors |
| `src/ui_card.cpp` | 1 | Card styling |
| `src/ui_theme.cpp` | N/A | Defines theme colors (acceptable) |

**Note:** `ui_fatal_error.cpp` is intentionally exempt (bootstrap code).

#### Color Tokens Added to `globals.xml`:

```xml
<!-- Graph/Visualization colors -->
<color name="graph_line_primary" value="#AD2724"/>
<color name="graph_line_secondary" value="#D4A84B"/>
<color name="graph_line_target" value="#2196F3"/>
<color name="graph_bg_dark" value="#2D2D2D"/>
<color name="graph_bg_light" value="#F5F5F5"/>

<!-- Mesh visualization gradient -->
<color name="mesh_low" value="#2196F3"/>
<color name="mesh_mid" value="#4CAF50"/>
<color name="mesh_high" value="#FF4444"/>

<!-- Selection/highlight states -->
<color name="selection_highlight_light" value="#E3F2FD"/>
<color name="selection_highlight_dark" value="#1E3A5F"/>

<!-- Step progress colors (wizard steps) -->
<color name="step_pending" value="#808080"/>
<color name="step_active" value="#FF4444"/>
<color name="step_completed" value="#4CAF50"/>
<color name="step_label_inactive_light" value="#666666"/>
<color name="step_label_inactive_dark" value="#CCCCCC"/>

<!-- Jog pad colors (motion panel) -->
<color name="jog_outer_ring_dark" value="#3A3A3A"/>
<color name="jog_outer_ring_light" value="#D0D0D0"/>
<color name="jog_inner_circle_dark" value="#2A2A2A"/>
<color name="jog_inner_circle_light" value="#E0E0E0"/>
<color name="jog_grid_lines_dark" value="#000000"/>
<color name="jog_grid_lines_light" value="#888888"/>
<color name="jog_home_bg_dark" value="#404040"/>
<color name="jog_home_bg_light" value="#CCCCCC"/>
<color name="jog_home_border_dark" value="#606060"/>
<color name="jog_home_border_light" value="#999999"/>
<color name="jog_boundary_lines_dark" value="#484848"/>
<color name="jog_boundary_lines_light" value="#AAAAAA"/>
<color name="jog_distance_labels_dark" value="#CCCCCC"/>
<color name="jog_distance_labels_light" value="#555555"/>

<!-- Filament path visualization colors -->
<color name="filament_idle_dark" value="#606060"/>
<color name="filament_idle_light" value="#A0A0A0"/>
<color name="filament_error" value="#E53935"/>
<color name="filament_hub_bg_dark" value="#404040"/>
<color name="filament_hub_bg_light" value="#E0E0E0"/>
<color name="filament_hub_border_dark" value="#505050"/>
<color name="filament_hub_border_light" value="#CCCCCC"/>
<color name="filament_nozzle_dark" value="#888888"/>
<color name="filament_nozzle_light" value="#666666"/>
<color name="filament_metal" value="#808890"/>

<!-- AMS slot colors -->
<color name="ams_hub_dark" value="#1A1A1A"/>
<color name="ams_hub_border" value="#333333"/>
<color name="ams_badge_bg" value="#505050"/>
```

### 3.2 Padding/Spacing Token Violations

**445+ instances** of hardcoded numeric padding in XML files. Priority files:

- `ui_xml/wizard_*.xml` - Wizard steps with hardcoded padding
- `ui_xml/settings_panel.xml` - Settings rows
- `ui_xml/*_modal.xml` - Modal layouts

**Fix Pattern:**
```xml
<!-- Before -->
<lv_obj style_pad_top="6" style_pad_bottom="20"/>

<!-- After -->
<lv_obj style_pad_top="#space_xs" style_pad_bottom="#space_xl"/>
```

---

## Phase 4: Event Callback Migration (MEDIUM PRIORITY) - ANALYZED

### 4.1 Analysis Summary

After reviewing the highest-priority files, **most event callbacks are legitimately in C++** due to:

1. **Dynamic dialogs**: Settings panel creates dialogs at runtime (theme restart, factory reset)
2. **Component nesting**: `setting_toggle_row` and `setting_action_row` components wrap interactive widgets
3. **Instance state**: Callbacks need `this` pointer for class member access
4. **Custom draw events**: `DRAW_POST`, `DELETE` are properly exempt

### 4.2 Current Architecture (Acceptable)

The codebase follows a pragmatic pattern:
- **XML components** define structure and named widgets
- **C++ panels** attach event handlers using `lv_obj_find_by_name()` + `lv_obj_add_event_cb()`
- **Instance data** is passed via `user_data` parameter

This approach works well because:
1. Components can be reused with different handlers
2. Class methods can access member state directly
3. Dynamic widgets (dialogs, modals) are handled naturally

### 4.3 True Migration Candidates (LOW priority)

Only simple, stateless buttons without component wrappers benefit from XML events:
- Toast dismiss buttons (already migrated)
- Modal backdrop clicks (acceptable in C++)
- Simple navigation buttons

### 4.4 Acceptable Exceptions (do NOT migrate)

- `LV_EVENT_DRAW_POST` - Custom draw handlers for programmatic widgets
- `LV_EVENT_DELETE` - Cleanup handlers for dynamically created widgets
- Events on programmatically-created widgets (pools, dynamic lists)
- Runtime-conditional event wiring
- Events needing instance state (`this` pointer)
- Events on component-wrapped widgets

### 4.5 Original Violation Count

| File | Violations | Analysis |
|------|------------|----------|
| `src/ui_panel_settings.cpp` | 21 | ~12 on component widgets, ~9 on dynamic dialogs - all acceptable |
| `src/ui_panel_print_select.cpp` | 16 | Dynamic card creation, scroll events - acceptable |
| `src/ui_keyboard.cpp` | 11 | Programmatic keyboard - acceptable |
| `src/ui_panel_print_status.cpp` | 8 | Control buttons need state - acceptable |
| `src/ui_bed_mesh.cpp` | 7 | Custom draw + touch - acceptable |
| `src/ui_modal_base.cpp` | 4 | Dynamic modal creation - acceptable |
| `src/ui_panel_ams.cpp` | 5 | Dynamic slot handling - acceptable |

**Recommendation**: Phase 4 is **DEFERRED** - the current architecture is appropriate for the use cases.

---

## Phase 5: Architecture Refactoring (MEDIUM PRIORITY)

### 5.1 Function-to-Class Migration

These global function-based UI systems should become class-based managers:

| System | File | Global Variables | Recommended Class |
|--------|------|-----------------|-------------------|
| Keyboard | `src/ui_keyboard.cpp` | 15+ globals (g_keyboard, g_mode, etc.) | `KeyboardManager` |
| Navigation | `src/ui_nav.cpp` | 8+ globals (panel_stack, active_panel, etc.) | `NavigationManager` |
| Modal | `src/ui_modal.cpp` | 6+ globals (modal_stack, subjects) | `ModalManager` |
| Toast | `src/ui_toast.cpp` | 5+ globals (active_toast, timer) | `ToastManager` |
| Status Bar | `src/ui_status_bar.cpp` | Multiple static subjects | `StatusBarManager` |

**Benefits:**
- Encapsulated state (no global pollution)
- Proper RAII lifecycle
- Testable (can mock/inject)
- Clear initialization order

**Reference Implementation:** See `HomePanel`, `PrintStatusPanel`, `TempControlPanel` for the correct PanelBase + ObserverGuard pattern.

### 5.2 Refactoring Order

1. **KeyboardManager** - Most complex, most global state
2. **NavigationManager** + **ModalManager** - Interconnected
3. **ToastManager** + **StatusBarManager** - Simpler

---

## Phase 6: Code Style Cleanup (LOW PRIORITY)

### 6.1 Copyright Header Cleanup

**133 files** have verbose 20-line GPL boilerplate in addition to SPDX headers. Per CLAUDE.md Rule 8, only the SPDX line is needed:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
```

**Affected directories:**
- `src/` - 59 files
- `include/` - 74 files

**Automation Script:**
```bash
#!/bin/bash
# Strip verbose GPL boilerplate, keep only SPDX
for f in src/*.cpp include/*.h; do
  if head -1 "$f" | grep -q "^// SPDX"; then
    # Keep first line (SPDX) and everything after the closing */
    (head -1 "$f"; sed -n '/^\*\//,$p' "$f" | tail -n +2) > "$f.tmp"
    mv "$f.tmp" "$f"
  fi
done
```

### 6.2 Minor Logging Fix

| File | Line | Issue |
|------|------|-------|
| `src/config.cpp` | 124, 153 | `std::endl` → `'\n'` |

---

## Prioritization Summary

| Phase | Priority | Effort | Impact | Status |
|-------|----------|--------|--------|--------|
| 1. SubscriptionGuard | HIGH | DONE | RAII for subscriptions | ✅ COMPLETED |
| 2. RAII Migrations | HIGH | DONE | Prevents memory leaks, use-after-free | ✅ COMPLETED |
| 3. Design Tokens | HIGH | DONE | Enables proper theming, consistency | ✅ COMPLETED |
| 4. Event Callbacks | MEDIUM | N/A | Cleaner architecture | ⏸️ DEFERRED (architecture appropriate) |
| 5. Architecture | MEDIUM | 1-2 weeks | Testability, maintainability | TODO (incremental) |
| 6. Code Style | LOW | DONE | Consistency, reduced noise | ✅ COMPLETED |

---

## Testing Checklist

After each phase, verify:

- [ ] `make -j` builds successfully
- [ ] `./build/bin/helix-tests` passes (note: some pre-existing flaky tests)
- [ ] `./build/bin/helix-screen --test -p home` runs and displays correctly
- [ ] Light/dark theme toggle works (`settings` panel)
- [ ] AMS panel shows slot states correctly (if AMS backend changes)

---

## References

- `docs/ARCHITECTURE.md` - System design patterns
- `docs/LVGL9_XML_GUIDE.md` - XML component reference
- `CLAUDE.md` - Project coding standards
- `include/ui_observer_guard.h` - ObserverGuard implementation
- `include/ui_subscription_guard.h` - SubscriptionGuard implementation (Phase 1)
