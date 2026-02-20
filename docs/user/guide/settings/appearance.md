# Settings: Appearance

The first section in the Settings panel covers visual preferences and display configuration.

---

## Language

Choose the display language for all UI text. Currently English is the only supported language; more translations are planned.

---

## Animations

Toggle UI motion effects (transitions, panel slides, confetti). Disable for better performance on slower hardware like Raspberry Pi 3.

---

## 3D Preview

Enable interactive 3D G-code visualization during prints. When off, only the 2D layer view is available. Disable if your hardware struggles with 3D rendering.

---

## Display Settings

Tap **Display Settings** to open an overlay with detailed display options:

| Setting | Options |
|---------|---------|
| **Dark Mode** | Switch between light and dark themes. Disabled when the active theme doesn't support both modes. |
| **Theme Colors** | Open the theme explorer to browse, preview, and apply color themes. |
| **Brightness** | Slider from 10–100%. Only shown on hardware with backlight control (hidden on Android). |
| **Bed Mesh Render** | Auto, 3D View, or 2D Heatmap visualization for bed mesh data. |
| **Screen Dim** | When the screen dims to lower brightness: Never, 30s, 1m, 2m, or 5m of inactivity. |
| **Display Sleep** | When the screen turns off completely: Never, 1m, 5m, 10m, or 30m of inactivity. |
| **Sleep While Printing** | Allow the display to sleep during active prints. Off by default so you can monitor progress. |
| **Time Format** | 12-hour or 24-hour clock display. |

> **Tip:** Touch the screen to wake from sleep.

### Display Rotation

Display rotation (0°, 90°, 180°, 270°) is configured via the `display.rotate` option in `helixconfig.json` or the `--rotate` CLI flag. It's not exposed in the UI because it requires restarting all three binaries (main app, splash screen, watchdog).

### Layout Auto-Detection

HelixScreen automatically selects the best layout for your display size:

| Layout | Resolution | Use Case |
|--------|-----------|----------|
| **Standard** | 800x480 | Most 7" touchscreens |
| **Ultrawide** | 1920x480 | Bar-style displays |
| **Compact** | 480x320 | Small 3.5" screens |

Override with the `--layout` command-line flag if auto-detection picks the wrong layout.

---

## Home Widgets

Tap **Home Widgets** to configure which widgets appear on your Home Panel dashboard and in what order.

- **Toggle** individual widgets on or off (maximum 10 enabled)
- **Reorder** by long-pressing the drag handle on any row and dragging it up or down
- Widgets tied to specific hardware (AMS, humidity sensor, probe, etc.) show "(not detected)" and are disabled if that hardware isn't present
- Changes apply immediately when you close the overlay

See [Home Panel — Home Widgets](../home-panel.md#home-widgets) for details on each widget.

---

[Back to Settings](../settings.md) | [Next: Printer Settings](printer.md)
