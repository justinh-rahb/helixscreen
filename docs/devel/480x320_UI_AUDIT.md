# 480x320 UI Audit

Panel-by-panel audit of HelixScreen at 480x320 (TINY preset).
Goal: Catalog MAJOR issues only, then prioritize fixes.

## Home Panel

1. **Navbar icons cut off** — Left and right edges clipping. Icons need to shrink more at this size.
2. **Navbar outlines overlapping** — Borders fighting each other. Likely need to remove outlines at tiny size. May also have overlapping click targets.
3. **Tip text borderline** — Barely acceptable, feels "big" for the available space. Not blocking.
4. **Status icon temp text clipped** — Padding on left/right of temperature text under nozzle icon cuts it off. Easy fix: eliminate horizontal padding on that text (safe at all resolutions).

## Controls Panel

1. **Position card: labels overlap "Position" header** — X/Y/Z labels collide with the card header text.
2. **Z-Offset value wraps** — Value text wrapping to next line, breaking layout.
3. **Temps card: "Nozzle:" label collides with value** — Not enough horizontal space between label and temperature value.
4. **Cooling card: fan list overflows** — Additional fans pushed off the bottom of the card, not visible.
5. **Quick Actions: buttons clipped** — Buttons below Home Actions (All/XY/Z) don't fit, getting cut off at bottom.

### Motion Overlay — LOOKS GREAT, no issues.

## Print Select Panel

### Card/Thumbnail View
1. **Metadata area way too tall** — Filename + stats area covers >50% of vertical card space, squeezing thumbnails.

### List View — MOST BROKEN SCREEN
1. **Filter row too wide** — Causing horizontal scrollbar.
2. **Enormous top padding** — Pushes all content to bottom of screen, huge dead space.
3. **Rows display as oversized cards** — Missing most content, scrolled off to the right. Bad width calculations.
4. **Overall** — Layout fundamentally broken at this size, needs significant rework.

## Settings Panel

1. **Dropdown menus too wide** — Currently hardcoded width, needs to be responsive/smarter sizing.

Otherwise looks pretty good. Scrollable list layout handles small screens well.

### Theme View & Edit — LOOKS GREAT, no issues.

## Filament Panel

1. **Temp card: Nozzle/Bed labels collide with values** — Same issue as controls panel temps.
2. **Multi-Filament card pushed off screen** — Mostly invisible, pushed below bottom edge.
3. **Material buttons dominate top** — PLA/PETG/ABS/TPU buttons smushed into the "Operations" label below them.
4. **Purge length chips smushed** — 5mm/10mm/25mm chip buttons colliding with material buttons above.

### Numeric Keypad Overlay — GLOBAL ISSUE

Doesn't fit vertically at 480x320. Bottom rows cut off. Needs responsive layout work — this affects every panel that uses the numeric keypad.

## Advanced Settings Panel — LOOKS GOOD, no major issues.

## Spoolman Overlay

1. **Too much padding** — Wasted vertical space, can only see 3 spools at a time. Needs tighter spacing at tiny size.

## G-Code Console

1. **Major button issues** — _(details TBD)_

## Print History Dashboard — LOOKS GOOD, no major issues.

## Print History Full List

1. **Search/filter fields too wide** — Pushing off display to the right. List itself is fine.

## Z-Offset Calibration

1. **Slightly too tall** — Bed heating toggle and text clipped at bottom. Probably needs to scroll.

## PID Calibration

1. **Temp chips slightly clipped** — Material preset buttons getting cut off.
2. **Fan speed text doesn't wrap** — "Match your typical print fan speed" truncated.
3. **Part fan slider needs left padding** — Slightly too close to edge.
4. **Temperature value wrapping** — Value between - and + buttons getting clipped/wrapping.

## Print File Detail Overlay

1. **Pre-print steps + options cramped** — Toggle labels collide with toggle controls ("Record Time" getting cut). With more than 1 option it'll be too tight. Not terrible but needs breathing room.

## Print Status (printing overlay)

1. **Action buttons pushed off screen** — Too many buttons crammed into one row, look terrible and clip.
2. **Temp cards dominate right side** — Taking too much horizontal space relative to available width.
3. **Metadata/pre-print progress overlapping** — Layer info, filename, elapsed time colliding with print progress area.
4. **Overall too cramped** — Needs significant layout rethink for this size.

## Modals/Dialogs — GLOBAL ISSUE

Many modals don't use responsive height. Content pushes off top and bottom of screen.
Example: Printer tips modal — title clipped at top, text clipped at bottom.
Needs audit across all modals to ensure they respect available viewport height (scroll if needed).

---

## Issue Summary by Category

### Global / Cross-Cutting
- **Navbar**: Icons clipped, outlines overlapping, click targets may overlap (affects ALL panels)
- **Numeric keypad overlay**: Doesn't fit vertically (affects every panel using keypad)
- **Modals/dialogs**: Many don't respect viewport height, clip top/bottom
- **Temp label collisions**: "Nozzle:"/"Bed:" labels collide with values (controls, filament)
- **Dropdown widths**: Hardcoded, too wide at tiny size (settings)

### Broken / Needs Major Rework
- **Print Select list view**: Most broken screen. Padding, row sizing, horizontal overflow all wrong.
- **Print Status**: Action buttons, temp cards, metadata all fighting for space. Needs layout rethink.
- **Filament panel**: Multi-filament card invisible, material buttons crushing operations section.

### Needs Moderate Fixes
- **Controls panel**: Position card overlap, Z-offset wrap, cooling overflow, quick actions clipped.
- **Print Select card view**: Metadata area too tall, squeezes thumbnails.
- **PID Calibration**: Multiple small issues (chips, text wrap, slider padding, value clipping).

### Minor / Cosmetic
- **Home panel**: Tip text borderline big, status icon temp padding.
- **Print File Detail**: Pre-print options cramped with toggles, workable but tight.
- **Z-Offset Calibration**: Slightly too tall, needs scroll.
- **Spoolman**: Too much padding, wasted space.
- **Print History list**: Filter fields too wide.

### Looking Good Already
- Motion overlay
- Advanced settings
- Settings (except dropdowns)
- Theme view & edit
- Print History dashboard

## Priority / Attack Plan

_(TBD — categorize into fix waves)_
