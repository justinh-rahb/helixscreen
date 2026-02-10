# Sound Settings

HelixScreen can play sounds for button presses, navigation, print events, and alerts. Sounds work on all supported hardware -- from desktop speakers to the tiny buzzer on your Creality printer.

## Enabling Sounds

Open the **Settings** panel and scroll to the **Sound** section. You'll find two toggles:

- **Sounds Enabled** -- Master toggle. Turns all sounds on or off.
- **UI Sounds Enabled** -- Controls button taps, navigation sounds, and toggle sounds. When off, only important sounds still play (print complete, errors, alarms). This is useful if you want to be notified when a print finishes but don't want click feedback during normal use.

Both toggles take effect immediately.

> **Note:** Sound settings only appear when HelixScreen detects a speaker or buzzer on your printer. If you don't see the sound section in Settings, your printer may not have audio output hardware. See [Troubleshooting](#troubleshooting) below.

## Sound Themes

HelixScreen comes with three built-in themes:

| Theme | Description |
|-------|-------------|
| **Default** | Balanced, tasteful sounds. Subtle clicks, smooth navigation chirps, and a melodic fanfare when your print completes. A good choice for most users. |
| **Minimal** | Only plays sounds for important events: print complete, errors, and alarms. No button or navigation sounds at all. Choose this if you want to know when something important happens but prefer silence otherwise. |
| **Retro** | 8-bit chiptune style inspired by classic game consoles. Punchy square-wave arpeggios, a Mario-style victory fanfare for print completion, and buzzy retro alarms. Fun and distinctive. |

To change themes, go to **Settings > Sound Theme** and select a theme from the dropdown. A test sound plays immediately so you can preview how it sounds before leaving settings.

## What Sounds When

| Event | Sound | When It Plays |
|-------|-------|---------------|
| Button press | Short click | Any button in the interface is tapped |
| Toggle on | Rising chirp | A switch or toggle is turned on |
| Toggle off | Falling chirp | A switch or toggle is turned off |
| Navigate forward | Ascending tone | Moving to a new screen or opening an overlay |
| Navigate back | Descending tone | Closing an overlay or going back |
| Print complete | Victory melody | Your print job finished successfully |
| Print cancelled | Descending tone | A print job was cancelled |
| Error alert | Pulsing alarm | A significant error occurred |
| Error notification | Short buzz | An error toast or notification appeared |
| Critical alarm | Urgent siren | A critical failure requiring immediate attention |
| Test sound | Short beep | Pressing the "Test Sound" button in settings |

The first five sounds (button press, toggle on/off, navigate forward/back) are **UI sounds**. They respect the "UI Sounds Enabled" toggle. The remaining sounds always play as long as the master "Sounds Enabled" toggle is on.

## Test Sound Button

There is a **Test Sound** button in the sound settings section. Tap it to hear a sample from your current theme. This is useful for:

- Confirming your speaker works
- Previewing how a theme sounds after switching
- Checking that the volume level is acceptable

## Custom Themes

Advanced users can create custom sound themes by adding a theme file to the `config/sounds/` directory on the printer. See the developer documentation for the file format reference.

Custom themes appear automatically in the Sound Theme dropdown -- no restart required. Just select your new theme in Settings.

## Supported Hardware

HelixScreen auto-detects your audio hardware at startup:

| Hardware | How It Works |
|----------|-------------|
| **Desktop (SDL)** | Full audio synthesis through your computer speakers. Best sound quality. Used when running HelixScreen on a desktop computer. |
| **Creality AD5M / AD5M Pro** | Hardware PWM buzzer. Supports different tones and volume levels. |
| **Other Klipper Printers** | Uses beeper commands sent through Moonraker. Requires an `[output_pin beeper]` section in your Klipper printer configuration. Basic beep tones only. |

If no audio hardware is detected, the sound settings are hidden and HelixScreen operates silently.

## Troubleshooting

**I don't see sound settings in the Settings panel.**
Your printer doesn't have a detected speaker or buzzer. For Klipper-based printers, make sure you have `[output_pin beeper]` configured in your `printer.cfg` file, then restart HelixScreen.

**Sounds are too quiet or too loud.**
Volume varies by theme. Try switching to a different theme. If you create a custom theme, you can adjust the volume for each individual sound.

**Print complete sound doesn't play.**
Make sure the master "Sounds Enabled" toggle is on. The "UI Sounds" toggle does not affect print completion sounds -- they always play when the master toggle is enabled.

**Button click sounds are annoying.**
Turn off "UI Sounds Enabled" in Settings. This disables button, toggle, and navigation sounds while keeping important notifications like print complete, errors, and alarms.

**Sounds work on desktop but not on my printer.**
Confirm that your printer has audio hardware. For Klipper printers, verify the `[output_pin beeper]` section is present and correctly configured. You can test the beeper independently by sending an `M300` command from the Klipper console.
