# File Documentation Headers Tracking

This document tracks which files have Doxygen-style documentation headers and which may benefit from adding them.

## Header Format

```cpp
/**
 * @file filename.cpp
 * @brief One-line purpose statement
 *
 * @pattern Design pattern (if applicable)
 * @threading Threading model (if applicable)
 * @gotchas Known pitfalls (if applicable)
 *
 * @see Related files (if applicable)
 */
```

## Selection Criteria

Files are prioritized for documentation based on:
1. **Multi-category** - Files with threading + gotchas + patterns (highest priority)
2. **Threading complexity** - Async callbacks, WebSocket, background threads
3. **Pattern-defining** - Gold standard implementations others should follow
4. **Gotcha-prone** - Files with non-obvious behaviors or known pitfalls
5. **Frequently referenced** - Core infrastructure used by many other files

---

## Completed Files (27)

### Tier 1: Critical Infrastructure
| File | Status | Notes |
|------|--------|-------|
| `src/application/application.cpp` | ✅ Done | Shutdown sequence |
| `include/ui_update_queue.h` | ✅ Had header | Already documented |
| `src/moonraker_client.cpp` | ✅ Done | WebSocket, [L012] |
| `src/printer_state.cpp` | ✅ Done | Reactive state, [L004][L005][L021] |
| `src/application/display_manager.cpp` | ✅ Done | LVGL lifecycle |
| `src/application/moonraker_manager.cpp` | ✅ Done | WebSocket lifecycle |
| `include/static_subject_registry.h` | ✅ Done | Singleton subject cleanup registry |
| `src/application/static_subject_registry.cpp` | ✅ Done | LIFO cleanup order |
| `include/static_panel_registry.h` | ✅ Had header | Panel/overlay cleanup registry |

### Tier 2: Threading & Async
| File | Status | Notes |
|------|--------|-------|
| `include/async_helpers.h` | ✅ Had header | Already documented |
| `src/ams_state.cpp` | ✅ Done | Shutdown flag pattern |
| `src/wifi_manager.cpp` | ✅ Done | Weak reference pattern |
| `include/command_sequencer.h` | ✅ Done | Thread-safe state machine |

### Tier 3: Gold Standard Patterns
| File | Status | Notes |
|------|--------|-------|
| `include/ui_panel_base.h` | ✅ Done | Base class for all panels |
| `src/ui_panel_bed_mesh.cpp` | ✅ Done | Cited gold standard |
| `include/wifi_backend.h` | ✅ Done | Factory pattern |
| `include/ams_backend.h` | ✅ Done | Factory pattern |
| `include/display_backend.h` | ✅ Done | Factory pattern |
| `include/ui_theme.h` | ✅ Done | Responsive tokens |
| `include/gcode_parser.h` | ✅ Done | Streaming architecture |

### Tier 4: UI Patterns & Registration
| File | Status | Notes |
|------|--------|-------|
| `include/xml_registration.h` | ✅ Done | Registration ordering |
| `include/ui_modal.h` | ✅ Done | RAII modal pattern |
| `include/ui_icon.h` | ✅ Done | Semantic widgets |
| `include/ui_text.h` | ✅ Done | Semantic typography |
| `include/ui_observer_guard.h` | ✅ Done | RAII observer cleanup |
| `src/ui_notification.cpp` | ✅ Done | Thread-safe notifications |

### Tier 5: Config & Test Infrastructure
| File | Status | Notes |
|------|--------|-------|
| `include/config.h` | ✅ Done | JSON config singleton |
| `include/runtime_config.h` | ✅ Done | Test harness pattern |
| `src/ui_panel_print_select.cpp` | ✅ Done | Deferred deps [L022] |

---

## Candidates for Future Documentation

### High Priority (Threading/Async)
| File | Why |
|------|-----|
| `src/filament_sensor_manager.cpp` | Recursive mutex, Moonraker callbacks |
| `src/helix_plugin_installer.cpp` | Async HTTP operations |
| `src/ui_plugin_install_modal.cpp` | Async modal with destruction flag |
| `src/thumbnail_processor.cpp` | Background image processing |
| `src/active_print_media_manager.cpp` | Async print media handling |

### Medium Priority (Patterns)
| File | Why |
|------|-----|
| `include/ethernet_backend.h` | Factory pattern (like wifi_backend) |
| `include/usb_backend.h` | Factory pattern |
| `include/backlight_backend.h` | Factory pattern |
| `src/ethernet_manager.cpp` | Manager pattern |
| `src/usb_manager.cpp` | Manager pattern |
| `src/settings_manager.cpp` | Config persistence |
| `src/print_history_manager.cpp` | History with async cleanup |

### Lower Priority (Utilities)
| File | Why |
|------|-----|
| `include/format_utils.h` | Pure utility, no patterns |
| `include/ui_utils.h` | Mixed utilities |
| `src/ui_busy_overlay.cpp` | Simple UI component |

---

## Maintenance Guidelines

### When to Add Headers
- New files with threading complexity
- Files that define patterns for others to follow
- Files with non-obvious gotchas discovered through bugs

### When to Update Headers
- If a file's purpose changes significantly
- If threading model changes
- If new gotchas are discovered (add to lessons system too)

### Avoiding Staleness
- Focus on **purpose** and **patterns**, not implementation details
- Avoid specific line numbers or code snippets
- Use conceptual descriptions that survive refactoring

---

## Related Resources

- `.coding-agent-lessons/LESSONS.md` - Gotchas captured as lessons
- `docs/LVGL9_XML_GUIDE.md` - UI pattern documentation
- `docs/ARCHITECTURE.md` - System design overview
