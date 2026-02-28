# Home Panel

![Home Panel](../../images/user/home.png)

The Home Panel is your printer dashboard — showing status at a glance.

---

## Status Area

The top area displays:

- **Printer state**: Idle, Printing, Paused, Complete, Error
- **Print progress**: Percentage and time remaining (when printing)
- **Current filename**: What's being printed
- **Connection indicator**: Your link to Moonraker/Klipper

---

## Home Widgets

Below the status area, the Home Panel displays a row of **configurable widgets** — quick-access buttons for the features you use most. You choose which widgets appear, and in what order.

### Available Widgets

| Widget | What It Does |
|--------|-------------|
| **Power** | Toggle Moonraker power devices. Only appears if you have power devices configured. |
| **Network** | Shows WiFi signal strength or Ethernet status at a glance. Disabled by default. |
| **Firmware Restart** | Restart Klipper firmware with one tap. Always shown when Klipper is in SHUTDOWN state, even if disabled. Disabled by default. |
| **AMS Status** | Mini view of your multi-material spool slots. Only appears if an AMS/MMU system is detected. |
| **Temperature** | Nozzle temperature readout with animated heating icon. Tap to open the Temperature panel. |
| **Temperatures** | Stacked view of nozzle, bed, and chamber temperatures in a single widget. Supports two display modes: **Stack** (compact rows) and **Carousel** (full-size swipeable pages with large icons). Long-press to toggle between modes. Tap any temperature to open the temperature control panel. Disabled by default. |
| **LED Light** | Quick toggle for your LEDs. Long-press for the full LED Control Overlay (see [LED Controls](#led-controls) below). |
| **Humidity** | Enclosure humidity sensor reading. Only appears if a humidity sensor is detected. |
| **Width Sensor** | Filament width sensor reading. Only appears if a width sensor is detected. |
| **Probe** | Z probe status and offset. Only appears if a probe is configured. |
| **Filament Sensor** | Filament runout detection status. Only appears if a filament sensor is detected. |
| **Fan Speeds** | Part cooling, hotend, and auxiliary fan speeds at a glance. Fan icons spin when the fan is running. When many widgets are active, labels switch to compact abbreviations (P/H/C). Supports two display modes: **Stack** (compact rows) and **Carousel** (swipeable fan dials with 270-degree arc sliders for direct speed control). Long-press to toggle between modes. In stack mode, tap to open the Fan Control overlay. |
| **Thermistor** | Monitor a custom temperature sensor (e.g., chamber, enclosure heater). Only appears if extra temperature sensors are detected. Disabled by default. |
| **Notifications** | Pending alerts with severity badge. Tap to open notification history. |

Widgets automatically arrange into 1 or 2 rows depending on how many are active.

### Hardware-Gated Widgets

Some widgets only appear when the relevant hardware is detected by Klipper. If a sensor, probe, or AMS system isn't present, that widget is automatically hidden on the Home Panel — even if enabled in settings. In the widget configuration screen, these show "(not detected)" and their toggle is disabled.

### Customizing Widgets

To change which widgets appear and their order:

1. Go to **Settings** → **Home Widgets** (in the Appearance section)
2. **Toggle** widgets on or off with the switch on each row
3. **Reorder** by long-pressing the drag handle (arrows icon) on a row and dragging it to a new position
4. Changes take effect immediately — widgets appear and disappear in real time as you toggle them

To reset to defaults, disable all widgets and re-enable the ones you want, or edit the config file directly (see [Configuration Reference](../CONFIGURATION.md#panel-widget-settings)).

### Display Modes: Stack vs. Carousel

The **Temperatures** and **Fan Speeds** widgets support two display modes that you can switch between with a **long-press** (press and hold):

- **Stack mode** (default): Compact vertical rows showing all values at once. Best when you want to see everything at a glance.
- **Carousel mode**: Full-size swipeable pages with one item per page and indicator dots at the bottom. Swipe left/right to move between pages.

**Temperatures carousel** shows each sensor (nozzle, bed, chamber) as a full page with a large icon and temperature display. Tap any page to open the temperature control panel for that sensor.

**Fan Speeds carousel** shows each fan as an interactive dial with a 270-degree arc slider. Drag the arc to change fan speed directly without opening the full Fan Control overlay.

Your display mode preference is saved per widget and persists across restarts.

---

## Active Tool Badge

On printers with a toolchanger, the Home Panel displays an active tool badge:

- Shows the current active tool (e.g. "T0", "T1")
- Updates automatically when tools are switched during a print or via macros
- Only visible on multi-tool printers — single-extruder printers will not see this badge

---

## Emergency Stop

The **Emergency Stop** button halts all motion immediately (confirmation required unless disabled in Safety Settings).

---

## LED Controls

Long-pressing the LED button opens the LED Control Overlay — a full control panel for all your printer's lighting. What you see depends on your hardware.

### Strip Selector

If you have more than one LED strip configured, a row of chips lets you pick which strip to control. The overlay heading updates to show the selected strip name.

### Color & Brightness (Klipper Native LEDs)

For neopixel, dotstar, and other Klipper-native strips:

- **Color presets**: Tap one of the 8 preset swatches (White, Warm, Orange, Blue, Red, Green, Purple, Cyan)
- **Custom color**: Tap the custom color button to open an HSV color picker — pick any color and it automatically separates into a base color and brightness level
- **Brightness slider**: Adjust from 0–100%, independent of color
- **Color swatch**: Shows the actual output color (base color adjusted by brightness)
- **Turn Off**: Stops any active effects and turns off the selected strip

### Output Pin Lights

For `[output_pin]` lights (auto-detected by name — see [LED Settings](settings/led-settings.md#output-pin-lights-brightness-only)):

- **Brightness slider**: 0–100% for PWM pins
- **On/Off toggle**: For non-PWM pins
- Color controls are hidden since output pins don't support color

### LED Effects

If you have the [klipper-led_effect](https://github.com/julianschill/klipper-led_effect) plugin installed, an effects section appears with cards for each available effect. Effects are filtered to show only those that target the currently selected strip. The active effect is highlighted, and a **Stop All Effects** button lets you kill all running effects at once.

### WLED Controls

For WLED network strips:

- **On/Off toggle**: Turn the WLED strip on or off
- **Brightness slider**: 0–100%
- **Presets**: Buttons for each WLED preset — fetched directly from your WLED device, with the active preset highlighted

### Macro Device Controls

Custom macro devices you've configured in [LED Settings](settings.md#led-settings) appear here with controls matching their type:

- **On/Off devices**: Separate "Turn On" and "Turn Off" buttons
- **Toggle devices**: A single "Toggle" button
- **Preset devices**: Named buttons for each preset action

---

## Printer Manager

**Tap the printer image** on the Home Panel to open the Printer Manager overlay. This is your central place to view and customize your printer's identity, check software versions, and see detected hardware capabilities.

### Changing the Printer Name

Your printer name appears on the Home Panel and at the top of the Printer Manager. To change it:

1. Tap the **printer image** on the Home Panel to open the Printer Manager
2. Tap the **printer name** (shown with a pencil icon) — it switches to an editable text field
3. Type your new name (e.g., "Workshop Voron", "Printer #2")
4. Press **Enter** to save, or **Escape** to cancel

The name defaults to "My Printer" if left empty. It is saved to your config file and persists across restarts.

> **Tip:** You can also set the name directly in the config file under the `printer.name` key — see [Configuration Reference](../CONFIGURATION.md#name).

### Changing the Printer Image

The printer image appears on the Home Panel and in the Printer Manager. To change it:

1. Tap the **printer image** on the Home Panel to open the Printer Manager
2. Tap the **printer image** again (marked with a pencil badge) to open the Printer Image picker
3. The picker shows a scrollable list on the left and a live preview on the right
4. Choose from one of three sources:
   - **Auto-Detect** (default) — HelixScreen selects an image based on your printer type reported by Klipper
   - **Shipped Images** — Over 25 pre-rendered images covering Voron, Creality, FlashForge, Anycubic, RatRig, FLSUN, and more
   - **Custom Images** — Your own images (see below)
5. Tap an image to select it — your choice takes effect immediately and persists across restarts

### Using Custom Printer Images

You can use your own printer photo or rendering. There are two ways to import custom images:

#### Option A: Copy to the Custom Images Folder

1. Copy a PNG, JPEG, BMP, or GIF file into the `custom_images/` directory inside your HelixScreen config folder:

   | Platform | Custom images directory |
   |----------|----------------------|
   | MainsailOS (Pi) | `~/helixscreen/config/custom_images/` |
   | AD5M Forge-X | `/opt/helixscreen/config/custom_images/` |
   | AD5M Klipper Mod | `/root/printer_software/helixscreen/config/custom_images/` |
   | K1 Simple AF | `/usr/data/helixscreen/config/custom_images/` |

2. Open the Printer Image picker (tap printer image → tap image again)
3. Your image appears under the **Custom Images** section — HelixScreen automatically converts it to an optimized format on first load

#### Option B: Import from a USB Drive

1. Insert a USB drive containing image files (PNG, JPEG, BMP, or GIF) into your printer's host
2. Open the Printer Image picker (tap printer image → tap image again)
3. A **USB Import** section appears at the bottom of the list showing images found on the drive
4. Tap an image to import it — HelixScreen copies and converts it automatically
5. Once imported, the image appears under Custom Images and the USB drive can be removed

#### Custom Image Requirements

- **Formats:** PNG, JPEG, BMP, or GIF
- **Maximum file size:** 5 MB
- **Maximum dimensions:** 2048×2048 pixels — images exceeding this limit will not be imported (resize before importing)
- HelixScreen automatically generates optimized 300px and 150px display variants

#### Removing Custom Images

To remove a custom image, delete its files from the `custom_images/` directory via SSH:

```bash
# Example: remove an image called "my-printer"
cd ~/helixscreen/config/custom_images/   # adjust path for your platform
rm my-printer.png my-printer-300.bin my-printer-150.bin
```

If the deleted image was active, HelixScreen falls back to auto-detect the next time the image is loaded.

### Software Versions

Below the identity card, the overlay displays current software versions for Klipper, Moonraker, and HelixScreen.

### Hardware Capabilities

A row of chips shows detected hardware capabilities: Probe, Bed Mesh, Heated Bed, LEDs, ADXL, QGL, Z-Tilt, and others depending on your Klipper configuration.

---

**Next:** [Printing](printing.md) | **Prev:** [Getting Started](getting-started.md) | [Back to User Guide](../USER_GUIDE.md)
