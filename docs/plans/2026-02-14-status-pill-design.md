# Status Pill Component Design

**Date:** 2026-02-14
**Branch:** feature/multi-unit-ams

## Goal

A reusable `status_pill` XML component for displaying colored status indicators (e.g., "Active", "Inactive", "Error"). First use: bypass mode indicator on AMS Management panel.

## XML Usage

```xml
<status_pill text="Active" variant="success"/>
<status_pill text="Inactive" variant="muted"/>
<status_pill text="Bypass Active" variant="success" bind_text="bypass_label"/>
```

## Visual Style

Follows `beta_badge` pattern:

- Semi-transparent background (opa 40) in the variant color
- Full-opacity text in the variant color
- Font: `text_small` equivalent
- Radius: `#border_radius_small` (4px)
- Padding: `#space_xs` horizontal, 2px vertical
- Width: `content` (auto-sizes to text)

## Variants

Same set as `icon`: `success`, `warning`, `danger`, `info`, `muted`, `primary`, `secondary`, `tertiary`, `disabled`

## Architecture

### Shared variant module (DRY with icon)

**New files:** `include/ui_variant.h`, `src/ui/ui_variant.cpp`

Extract variant logic currently duplicated/local in `ui_icon.cpp`:

- `enum class Variant` — success, warning, danger, info, muted, primary, secondary, tertiary, disabled
- `Variant parse_variant(const char*)` — string to enum
- `lv_color_t variant_text_color(Variant)` — full-opacity text/icon color from ThemeManager
- `lv_color_t variant_bg_color(Variant)` — same color, intended for reduced-opacity backgrounds

Both functions use `ThemeManager::instance().get_style(StyleRole::Icon*)` — same path as icon uses today.

### Refactored icon widget

`ui_icon.cpp` drops its local `IconVariant` enum, `parse_variant()`, and `apply_variant()`. Calls `ui_variant::parse_variant()` and `ui_variant::variant_text_color()` instead. Zero behavior change.

### New status_pill widget

**New files:** `include/ui_status_pill.h`, `src/ui/ui_status_pill.cpp`, `ui_xml/status_pill.xml`

**XML component** (`status_pill.xml`):
```xml
<component>
  <api>
    <prop name="text" type="string" default=""/>
    <prop name="variant" type="string" default="muted"/>
  </api>
  <view extends="lv_obj"/>
</component>
```

**C++ widget** (`ui_status_pill.cpp`):
- `ui_status_pill_xml_create()` — creates `lv_obj` container + child `lv_label`
- `ui_status_pill_xml_apply()` — parses `text`/`variant` props, applies colors via `ui_variant`
- `ui_status_pill_register_widget()` — registers with `lv_xml_register_widget("status_pill", ...)`

**Public API:**
- `ui_status_pill_set_variant(lv_obj_t*, const char*)` — change variant at runtime
- `ui_status_pill_set_text(lv_obj_t*, const char*)` — change text at runtime

**Registration:** Called from `application.cpp` alongside `ui_icon_register_widget()`.

## Implementation Order

1. Create `ui_variant.h/.cpp` — extract variant enum + color lookup from `ui_icon.cpp`
2. Refactor `ui_icon.cpp` — use shared `ui_variant` module, verify no behavior change
3. Create `status_pill.xml` — component definition with props
4. Create `ui_status_pill.h/.cpp` — widget using `ui_variant` for colors
5. Register in `application.cpp`
6. Tests — verify variant color mapping, pill creation, runtime updates
