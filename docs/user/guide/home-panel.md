# Home Panel

![Home Panel](../../images/user/home.png)

The Home Panel is your printer dashboard — a customizable grid of widgets showing status at a glance. You control what appears, where it goes, and how big each widget is.

---

## The Widget Grid

Your dashboard is built from **widgets** — individual cards that show printer information and controls. Widgets live on a flexible grid that you can customize:

- **Move** widgets anywhere on the grid
- **Resize** widgets to be bigger or smaller
- **Add** new widgets from the catalog
- **Remove** widgets you don't need

Changes are saved automatically and persist across restarts.

---

## Editing Your Dashboard

### Entering Edit Mode

**Long-press** (press and hold) anywhere on the widget grid. After about half a second, the dashboard enters **Edit Mode**:

- A toolbar appears at the top with **Done**, **Add Widget**, and **Reset** buttons
- All widgets show a subtle border indicating they can be moved
- Widget tap actions are disabled while editing

### Moving a Widget

1. Enter Edit Mode (long-press the grid)
2. **Tap** a widget to select it — a blue highlight appears around it
3. **Drag** the selected widget to a new position on the grid
4. Release to drop it — other widgets shift to make room
5. Tap **Done** when finished

### Resizing a Widget

1. Enter Edit Mode (long-press the grid)
2. **Tap** a widget to select it
3. **Drag the edges or corners** of the selected widget to resize it
4. Widgets snap to grid cells as you resize
5. Each widget has minimum and maximum sizes — you'll feel it "stop" at the limits

### Adding a Widget

1. Enter Edit Mode (long-press the grid)
2. Tap the **Add Widget** button in the toolbar (or the **+** icon)
3. The **Widget Catalog** opens, showing all available widgets with descriptions
4. Tap a widget to add it — it appears on your dashboard
5. Widgets that depend on specific hardware (like AMS or humidity sensors) show as unavailable if that hardware isn't detected

### Removing a Widget

1. Enter Edit Mode (long-press the grid)
2. **Tap** the widget you want to remove to select it
3. **Drag it to the trash icon** that appears, or tap the **delete** button
4. The widget is removed from your grid (you can always add it back later)

### Resetting the Layout

If your layout gets messy, tap the **Reset** button in the Edit Mode toolbar. This restores the default widget arrangement without changing which widgets are enabled.

### Exiting Edit Mode

Tap the **Done** button in the toolbar, or navigate to a different panel. If you navigated away, edit mode exits automatically.

---

## Available Widgets

### Information Widgets

| Widget | Description | Default Size |
|--------|-------------|-------------|
| **Printer Image** | Your printer's photo. Tap to open the Printer Manager. | 2x2 |
| **Print Status** | Print progress, filename, ETA. Tap to open full print status when printing, or file browser when idle. | 2x2 |
| **Digital Clock** | Current time and date. Shows time only at small sizes, adds date and uptime at larger sizes. | 2x1 |
| **Notifications** | Pending alerts with severity badge. Tap to open notification history. | 1x1 |
| **Tips** | Helpful printing tips that rotate periodically. Tap for full details. | 2x1 |

### Temperature & Sensors

| Widget | Description | Default Size |
|--------|-------------|-------------|
| **Temperature** | Nozzle readout with animated heating icon. Tap to open the temperature graph. | 1x1 |
| **Temperatures** | Stacked nozzle, bed, and chamber temps. Also available in carousel mode (see below). | 2x1 |
| **Thermistor** | Extra temperature sensor monitoring. Only appears if sensors are detected. | 1x1 |
| **Humidity** | Enclosure humidity reading. Only appears if a sensor is detected. | 1x1 |
| **Width Sensor** | Filament width reading. Only appears if a sensor is detected. | 1x1 |

### Controls

| Widget | Description | Default Size |
|--------|-------------|-------------|
| **Fan Speeds** | Part cooling, hotend, and auxiliary fan speeds. Also available in carousel mode with arc sliders. | 2x1 |
| **LED Light** | Quick toggle for your LEDs. Tap to open the full LED Control Overlay. | 1x1 |
| **Power** | Toggle Moonraker power devices. Only appears if power devices are configured. | 1x1 |
| **Shutdown** | Shutdown or restart your printer's host system. | 1x1 |
| **Firmware Restart** | Restart Klipper firmware. Always shown in SHUTDOWN state. | 1x1 |
| **Favorite Macro 1/2** | Quick-access buttons for your most-used macros. Configure which macro to run in Settings. | 1x1 |

### Multi-Material & Print Queue

| Widget | Description | Default Size |
|--------|-------------|-------------|
| **AMS Status** | Mini view of your spool slots. Only appears if AMS/MMU is detected. | 2x1 |
| **Filament** | Filament sensor status. Only appears if a sensor is detected. | 1x1 |
| **Job Queue** | Shows queued print jobs with count. Tap to open the full queue manager where you can delete jobs, start/pause the queue, and launch queued prints. | 2x2 |

### System

| Widget | Description | Default Size |
|--------|-------------|-------------|
| **Network** | WiFi signal or Ethernet status. | 1x1 |

---

## Hardware-Gated Widgets

Some widgets only appear when the relevant hardware is detected by Klipper:

- **AMS Status** — requires AMS, AFC, Happy Hare, or compatible MMU
- **Humidity** — requires a humidity sensor in Klipper config
- **Width Sensor** — requires a filament width sensor
- **Thermistor** — requires extra temperature sensors beyond nozzle/bed
- **Filament** — requires a filament sensor
- **Power** — requires Moonraker power devices

If the hardware isn't present, these widgets won't appear in the catalog. If hardware is later disconnected, the widget hides automatically.

---

## Display Modes: Stack vs. Carousel

The **Temperatures** and **Fan Speeds** widgets each support two display modes:

- **Stack mode** (default): Compact vertical rows showing all values at once. Best for seeing everything at a glance.
- **Carousel mode**: Full-size swipeable pages with one item per page and indicator dots. Swipe left/right to browse.

**Temperatures carousel** shows each sensor (nozzle, bed, chamber) as a large icon with temperature display. Tap any page to open the temperature graph.

**Fan Speeds carousel** shows each fan as an interactive dial with a 270-degree arc slider. Drag the arc to change fan speed directly.

To switch modes, go to **Settings > Home Widgets** and change the display mode for the widget. Your preference is saved per widget.

---

## Job Queue Manager

Tap the **Job Queue** widget to open the queue manager modal:

- **View** all queued print jobs with filenames and time queued
- **Delete** individual jobs by tapping the trash icon
- **Start/Pause** the queue using the toggle button
- **Start a print** by tapping a job — if the printer is idle, the job is removed from the queue and printing begins immediately

The queue syncs with Moonraker's job queue, so jobs added from other interfaces (Mainsail, Fluidd, API) appear here too.

---

## Active Tool Badge

On printers with a toolchanger, the Home Panel displays an active tool badge:

- Shows the current active tool (e.g. "T0", "T1")
- Updates automatically when tools are switched
- Only visible on multi-tool printers

---

## Emergency Stop

The **Emergency Stop** button halts all motion immediately (confirmation required unless disabled in Safety Settings).

---

## LED Controls

Tap the LED widget to open the LED Control Overlay — a full control panel for all your printer's lighting.

### Strip Selector

If you have more than one LED strip, a row of chips lets you pick which strip to control.

### Color & Brightness (Klipper Native LEDs)

For neopixel, dotstar, and other Klipper-native strips:

- **Color presets**: 8 preset swatches (White, Warm, Orange, Blue, Red, Green, Purple, Cyan)
- **Custom color**: HSV color picker with automatic color/brightness separation
- **Brightness slider**: 0-100%, independent of color
- **Turn Off**: Stops effects and turns off the strip

### Output Pin Lights

For `[output_pin]` lights: brightness slider (PWM) or on/off toggle (non-PWM).

### LED Effects

If you have [klipper-led_effect](https://github.com/julianschill/klipper-led_effect) installed, effect cards appear filtered to the selected strip. Active effects are highlighted with a **Stop All** button.

### WLED Controls

For WLED network strips: on/off toggle, brightness slider, and device presets.

### Macro Device Controls

Custom macro devices configured in LED Settings appear with matching controls (on/off, toggle, or preset buttons).

---

## Printer Manager

**Tap the printer image** widget to open the Printer Manager overlay.

### Changing the Printer Name

1. Open the Printer Manager (tap the printer image)
2. Tap the **printer name** (pencil icon) to edit
3. Type your new name and press **Enter**

### Changing the Printer Image

1. Open the Printer Manager (tap the printer image)
2. Tap the **image** again to open the picker
3. Choose from:
   - **Auto-Detect** — matches your printer type
   - **Shipped Images** — 25+ pre-rendered images
   - **Custom Images** — your own photos (see below)

### Using Custom Printer Images

**Copy to config folder:**

| Platform | Directory |
|----------|-----------|
| MainsailOS (Pi) | `~/helixscreen/config/custom_images/` |
| AD5M Forge-X | `/opt/helixscreen/config/custom_images/` |

Supported formats: PNG, JPEG, BMP, GIF. Max 5 MB, max 2048x2048.

**Import from USB:** Insert a USB drive with images, open the picker, and tap to import.

**Remove:** Delete files from the `custom_images/` directory via SSH.

### Software Versions & Hardware

The overlay shows Klipper, Moonraker, and HelixScreen versions, plus detected hardware capabilities.

---

**Next:** [Printing](printing.md) | **Prev:** [Getting Started](getting-started.md) | [Back to User Guide](../USER_GUIDE.md)
