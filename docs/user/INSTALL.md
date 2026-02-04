# HelixScreen Installation Guide

This guide walks you through installing HelixScreen on your 3D printer's touchscreen display.

**Target Audience:** Klipper users who want to use pre-built packages. If you're a developer building from source, see [DEVELOPMENT.md](../DEVELOPMENT.md).

---

## Table of Contents

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [MainsailOS Installation](#mainsailos-installation)
- [Adventurer 5M Installation](#adventurer-5m-installation)
- [Creality K1 Installation](#creality-k1-series-simple-af)
- [First Boot & Setup Wizard](#first-boot--setup-wizard)
- [Display Configuration](#display-configuration)
- [Starting on Boot](#starting-on-boot)
- [Updating HelixScreen](#updating-helixscreen)
- [Uninstalling](#uninstalling)
- [Getting Help](#getting-help)

---

## Quick Start

**Raspberry Pi (MainsailOS) or Creality K1:**
```bash
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | sh
```

The installer automatically detects your platform and downloads the correct release.

**Adventurer 5M:** The AD5M's BusyBox doesn't support HTTPS downloads. See [Adventurer 5M Installation](#adventurer-5m-installation) for the two-step process (download on your computer, then copy to printer).

> **Note:** Both `bash` and `sh` work. The installer is POSIX-compatible for BusyBox environments.

**KIAUH users:** The one-liner works on KIAUH installations too! Alternatively, see [scripts/kiauh/](https://github.com/prestonbrown/helixscreen/tree/main/scripts/kiauh) to add HelixScreen to your KIAUH menu.

After installation, the setup wizard will guide you through initial configuration.

> **Upgrading from an older version?** If HelixScreen keeps showing the setup wizard after an update, see [UPGRADING.md](UPGRADING.md) for how to fix configuration issues.

---

## Prerequisites

### MainsailOS (Raspberry Pi)

- **Hardware:**
  - Raspberry Pi 3, 4, or 5 (Pi 3 is minimum, Pi 4/5 recommended)
  - Raspberry Pi Zero 2 W also supported
  - Touchscreen display (HDMI, DSI, or SPI)
  - Network connection (Ethernet or WiFi)

- **Software:**
  - MainsailOS installed and working
  - Klipper running and printing works via Mainsail web interface
  - SSH access to your Pi
  - About 100MB free disk space

### Adventurer 5M / 5M Pro

- **Hardware:**
  - FlashForge Adventurer 5M or 5M Pro
  - Stock 4.3" touchscreen (800x480)
  - Network connection

- **Software:**
  - Custom Klipper firmware: [Forge-X](https://github.com/DrA1ex/ff5m) **or** [Klipper Mod](https://github.com/xblax/flashforge_ad5m_klipper_mod)
  - SSH access to the printer (usually `root@<printer-ip>`)
  - About 100MB free disk space

#### AD5M Firmware Variants

The installer automatically detects which firmware you're running and configures paths accordingly:

| Firmware | Replaces | Install Location | Init Script |
|----------|----------|------------------|-------------|
| **Forge-X** | GuppyScreen | `/opt/helixscreen/` | `S90helixscreen` |
| **Klipper Mod** | KlipperScreen | `/root/printer_software/helixscreen/` | `S80helixscreen` |

**Memory Savings:** On Klipper Mod, HelixScreen (~12MB) replaces KlipperScreen (~36MB + 4.6MB X Server), freeing ~29MB RAM on the memory-constrained AD5M.

#### Forge-X Prerequisites

**Important:** ForgeX must be installed and configured for GuppyScreen mode **before** installing HelixScreen. HelixScreen uses ForgeX's infrastructure (Klipper, Moonraker, backlight control) but replaces the GuppyScreen UI.

1. Install ForgeX following [their instructions](https://github.com/DrA1ex/ff5m)
2. Configure ForgeX with `display = 'GUPPY'` in variables.cfg
3. Verify GuppyScreen works on the touchscreen
4. Then run the HelixScreen installer

The HelixScreen installer will:
- Keep ForgeX in GUPPY display mode (required for backlight control)
- Disable GuppyScreen's init scripts (so HelixScreen takes over)
- Disable the stock FlashForge UI in auto_run.sh
- Patch ForgeX's `screen.sh` to prevent backlight dimming conflicts
- Install HelixScreen as the replacement touchscreen UI

On uninstall, all ForgeX changes are reversed and GuppyScreen is restored.

### Creality K1 Series (Simple AF)

- **Hardware:**
  - Creality K1, K1C, K1 Max, or similar
  - Stock touchscreen display
  - Network connection

- **Software:**
  - [Simple AF](https://github.com/pellcorp/creality) installed and working
  - SSH access to the printer (`root@<printer-ip>`)
  - About 100MB free disk space in `/usr/data`

The installer automatically detects Simple AF and configures paths:

| Environment | Replaces | Install Location | Init Script |
|-------------|----------|------------------|-------------|
| **Simple AF** | GuppyScreen | `/usr/data/helixscreen/` | `S99helixscreen` |

**Prerequisites:**
1. Install Simple AF following [their instructions](https://github.com/pellcorp/creality)
2. Verify GuppyScreen works on the touchscreen
3. Then run the HelixScreen installer

The HelixScreen installer will:
- Stop and disable GuppyScreen
- Install HelixScreen to `/usr/data/helixscreen/`
- Configure Moonraker update_manager for one-click updates from Fluidd/Mainsail

On uninstall, GuppyScreen is restored.

---

## MainsailOS Installation

### Step 1: Connect to Your Pi

Open a terminal and SSH into your Raspberry Pi:

```bash
ssh pi@mainsailos.local
# Or use your Pi's IP address:
ssh pi@192.168.1.xxx
```

Default password is usually `raspberry` unless you changed it.

### Step 2: Download HelixScreen

```bash
# Create installation directory
cd ~

# Download the latest release
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-pi.tar.gz

# Extract
tar -xzf helixscreen-pi.tar.gz
```

### Step 3: Install HelixScreen

```bash
cd helixscreen

# Copy files to installation directory
sudo mkdir -p /opt/helixscreen
sudo cp -r bin config ui_xml assets /opt/helixscreen/

# Install systemd service
sudo cp config/helixscreen.service /etc/systemd/system/
sudo systemctl daemon-reload
```

The package structure is:
```
helixscreen/
├── bin/                    # Executables
│   ├── helix-screen        # Main UI binary
│   ├── helix-splash        # Splash screen binary
│   ├── helix-watchdog      # Process supervisor
│   └── helix-launcher.sh   # Launch script
├── config/                 # Service config files
│   ├── helixscreen.init    # SysV init script
│   └── helixscreen.service # systemd unit file
├── ui_xml/                 # UI layout definitions
└── assets/                 # Fonts, images, themes
```

This will:
1. Copy files to `/opt/helixscreen/`
2. Install the systemd service
3. Configure for automatic startup

### Step 4: Start HelixScreen

```bash
# Start the service
sudo systemctl start helixscreen

# Verify it's running
sudo systemctl status helixscreen
```

You should see a splash screen briefly, then the setup wizard on your display.

### Step 5: Complete the Setup Wizard

The on-screen wizard will guide you through:
1. WiFi configuration (if not connected via Ethernet)
2. Finding your Moonraker instance
3. Identifying your printer
4. Selecting heaters, fans, and LEDs

See [First Boot & Setup Wizard](#first-boot--setup-wizard) for details.

---

## Adventurer 5M Installation

> **Important:** Installing HelixScreen replaces your current screen UI (GuppyScreen on Forge-X, KlipperScreen on Klipper Mod). Make sure you have a backup method to access your printer (SSH, Mainsail/Fluidd web interface).

### Automated Installation (Recommended)

The AD5M uses BusyBox which doesn't support HTTPS downloads directly. You'll need to download the release on your computer first, then copy it to the printer:

**Step 1: Download on your computer**

```bash
# Download the latest release and installer
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-ad5m.tar.gz
wget https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh
```

Or download manually from: https://github.com/prestonbrown/helixscreen/releases/latest

**Step 2: Copy to your printer**

```bash
# AD5M requires -O flag for scp (BusyBox lacks sftp-server)
# Note: Use /data/ not /tmp/ - AD5M's /tmp is a tiny tmpfs (~54MB)
scp -O helixscreen-ad5m.tar.gz install.sh root@<printer-ip>:/data/
```

**Step 3: Run the installer**

```bash
ssh root@<printer-ip>
sh /data/install.sh --local /data/helixscreen-ad5m.tar.gz
```

The install script automatically detects your firmware (Forge-X or Klipper Mod) and installs to the correct location.

**What the installer does on Forge-X:**
- Verifies ForgeX is installed and sets display mode to `GUPPY`
- Stops and disables GuppyScreen (`chmod -x` on init scripts)
- Disables stock FlashForge UI in `/opt/auto_run.sh`
- Patches `/opt/config/mod/.shell/screen.sh` to skip backlight commands when HelixScreen is running (prevents ForgeX's delayed_gcode from dimming the screen)
- Installs HelixScreen to `/opt/helixscreen/`
- Creates init script at `/etc/init.d/S90helixscreen`

**What the installer does on Klipper Mod:**
- Stops Xorg and KlipperScreen
- Disables their init scripts (`chmod -x`)
- Installs HelixScreen to `/root/printer_software/helixscreen/`
- Creates init script at `/etc/init.d/S80helixscreen`

### Manual Installation

<details>
<summary>Forge-X Manual Installation</summary>

```bash
# Download on your computer
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-ad5m.tar.gz

# Copy to printer (AD5M requires scp -O for legacy protocol)
# Note: Use /data/ not /tmp/ - AD5M's /tmp is a tiny tmpfs (~54MB)
scp -O helixscreen-ad5m.tar.gz root@<printer-ip>:/data/

# SSH into printer
ssh root@<printer-ip>

# Extract to /opt (Forge-X location)
cd /opt
gunzip -c /data/helixscreen-ad5m.tar.gz | tar xf -

# Stop GuppyScreen
/opt/config/mod/.root/S80guppyscreen stop 2>/dev/null || true
chmod -x /opt/config/mod/.root/S80guppyscreen

# Install init script
cp /opt/helixscreen/config/helixscreen.init /etc/init.d/S90helixscreen
chmod +x /etc/init.d/S90helixscreen

# Start HelixScreen
/etc/init.d/S90helixscreen start

# Clean up
rm /data/helixscreen-ad5m.tar.gz
```

</details>

<details>
<summary>Klipper Mod Manual Installation</summary>

> **Note:** Klipper Mod's `/tmp` is a small tmpfs (~54MB). The package is ~70MB, so we must use `/mnt/data` instead.

```bash
# Download on your computer
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-ad5m.tar.gz

# Copy to printer's data partition (NOT /tmp - it's too small!)
scp -O helixscreen-ad5m.tar.gz root@<printer-ip>:/mnt/data/

# SSH into printer
ssh root@<printer-ip>

# Extract to /root/printer_software (Klipper Mod location)
cd /root/printer_software
gunzip -c /mnt/data/helixscreen-ad5m.tar.gz | tar xf -

# Stop KlipperScreen
/etc/init.d/S80klipperscreen stop 2>/dev/null || true
chmod -x /etc/init.d/S80klipperscreen

# Install init script (S80 to match KlipperScreen's boot order)
cp /root/printer_software/helixscreen/config/helixscreen.init /etc/init.d/S80helixscreen
chmod +x /etc/init.d/S80helixscreen

# Update the install path in the init script
sed -i 's|DAEMON_DIR=.*|DAEMON_DIR="/root/printer_software/helixscreen"|' /etc/init.d/S80helixscreen

# Start HelixScreen
/etc/init.d/S80helixscreen start

# Clean up
rm /mnt/data/helixscreen-ad5m.tar.gz
```

</details>

> **Note:** AD5M runs as root, so `sudo` is not needed.
> **Note:** AD5M uses BusyBox utilities. Use `gunzip -c | tar xf -` instead of `tar -xzf`.
> **Note:** AD5M uses SysV init (BusyBox), not systemd.

### Step 4: Reboot

```bash
reboot
```

After reboot, HelixScreen will start automatically on the touchscreen.

### Step 5: Complete Setup

Use the touchscreen to complete the setup wizard. The printer should auto-detect since it's running locally.

---

## First Boot & Setup Wizard

When HelixScreen starts for the first time, a 7-step setup wizard guides you through configuration:

### Step 1: Welcome
Brief introduction to HelixScreen.

### Step 2: WiFi Setup
Connect to your wireless network. You can:
- Select from detected networks
- Enter a hidden network name manually
- Skip if using Ethernet

### Step 3: Moonraker Connection
Enter your Moonraker host. For most setups:
- **MainsailOS:** `localhost` or `127.0.0.1`
- **AD5M:** `localhost`
- **Remote printer:** Enter the IP address

The wizard will test the connection before proceeding.

### Step 4: Printer Identification
HelixScreen will try to identify your printer from its configuration. You can:
- Confirm the detected printer type
- Select from a database of 50+ printers
- Enter custom settings

### Step 5: Heater Selection
Choose which heaters to display and control:
- Hotend/nozzle heater
- Bed heater
- Chamber heater (if available)

### Step 6: Fan Selection
Select your cooling fans:
- Part cooling fan
- Hotend fan
- Other auxiliary fans

### Step 7: LED Selection (Optional)
If your printer has controllable LEDs:
- Chamber lights
- Status LEDs
- NeoPixel strips

### Completion
After the wizard, you'll be taken to the home screen. Your settings are saved automatically.

---

## Display Configuration

### HDMI Displays (Plug and Play)

Most HDMI touchscreens work automatically. If touch input isn't working:

1. Check that the USB cable from the display is connected to your Pi
2. Verify the display appears in `/dev/input/`:
   ```bash
   ls /dev/input/event*
   ```

### Official Raspberry Pi Touchscreen (DSI)

The official 7" Pi touchscreen is detected automatically via DSI connector.

If using non-standard orientation, edit `/boot/config.txt`:
```ini
# Rotate display 180 degrees
lcd_rotate=2
```

### SPI Displays (Requires Configuration)

For SPI displays (like many small LCDs):

1. Enable SPI in `/boot/config.txt`
2. Install the appropriate overlay
3. Configure framebuffer settings

See the [MainsailOS display documentation](https://docs.mainsail.xyz/) for specific display setup.

### BTT Pad 7 and Similar

The BTT Pad 7 and similar "Klipper Pad" devices typically include:
- Pre-configured display output
- Touch input via USB

HelixScreen should detect and use these automatically.

### Screen Rotation

To rotate the display, add to `/opt/helixscreen/helixconfig.json`:

```json
{
  "display_rotate": 180
}
```

Valid values: `0`, `90`, `180`, `270`

---

## Starting on Boot

### Enable Automatic Start

The installer configures systemd to start HelixScreen on boot. Verify with:

```bash
sudo systemctl is-enabled helixscreen
# Should show: enabled
```

If not enabled:
```bash
sudo systemctl enable helixscreen
```

### Service Management

**MainsailOS (systemd):**
```bash
# Start HelixScreen
sudo systemctl start helixscreen

# Stop HelixScreen
sudo systemctl stop helixscreen

# Restart (after config changes)
sudo systemctl restart helixscreen

# View status
sudo systemctl status helixscreen

# View logs
sudo journalctl -u helixscreen -f
```

**AD5M (SysV init):**

*Forge-X:*
```bash
/etc/init.d/S90helixscreen start|stop|restart|status
cat /tmp/helixscreen.log  # View logs
```

*Klipper Mod:*
```bash
/etc/init.d/S80helixscreen start|stop|restart|status
cat /tmp/helixscreen.log  # View logs
```

### Disabling Other UIs

If you have another UI installed, disable it to avoid conflicts:

**MainsailOS (systemd):**
```bash
# Disable KlipperScreen (if installed)
sudo systemctl stop KlipperScreen
sudo systemctl disable KlipperScreen
```

**AD5M Forge-X (SysV init):**
```bash
# Disable GuppyScreen
/opt/config/mod/.root/S80guppyscreen stop
chmod -x /opt/config/mod/.root/S80guppyscreen
```

**AD5M Klipper Mod (SysV init):**
```bash
# Disable KlipperScreen
/etc/init.d/S80klipperscreen stop
chmod -x /etc/init.d/S80klipperscreen
```

> **Note:** The HelixScreen installer automatically stops and disables competing UIs.

---

## Updating HelixScreen

### Check Current Version

On the touchscreen: **Settings > About** shows the current version.

Or via SSH, check the help output:
```bash
# Path varies by platform:
#   Pi: /opt/helixscreen/bin/helix-screen
#   K1: /usr/data/helixscreen/bin/helix-screen
#   AD5M Klipper Mod: /root/printer_software/helixscreen/bin/helix-screen
/opt/helixscreen/bin/helix-screen --help | head -1
```

### Update from Mainsail/Fluidd Web UI (Pi Only)

If you installed via the installer script, it automatically configures Moonraker's update_manager. You can update HelixScreen with one click from the Mainsail or Fluidd web interface:

1. Open Mainsail/Fluidd in your browser
2. Navigate to **Machine** (Mainsail) or **Settings** (Fluidd)
3. Find **HelixScreen** in the update manager
4. Click **Update** when a new version is available

> **Note:** The installer adds an `[update_manager helixscreen]` section to your `moonraker.conf`. If you installed manually, see [Manual Update Manager Setup](#manual-update-manager-setup) below.

### Update Using Install Script (Recommended)

The easiest way to update is using the install script with `--update`:

```bash
# All platforms (Pi, AD5M, K1)
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | sh -s -- --update
```

This preserves your configuration and updates to the latest version.

### Update to Specific Version

```bash
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | sh -s -- --update --version v1.2.0
```

### Preserving Configuration

The update process preserves your `helixconfig.json` settings. If you want to reset to defaults:

```bash
sudo rm /opt/helixscreen/helixconfig.json
sudo systemctl restart helixscreen
```

### Manual Update Manager Setup

If you installed manually or the installer couldn't find your `moonraker.conf`, add this to enable web UI updates:

```ini
# Add to moonraker.conf
# NOTE: The 'path' varies by platform:
#   Pi: /opt/helixscreen
#   K1/Simple AF: /usr/data/helixscreen
#   AD5M Klipper Mod: /root/printer_software/helixscreen
[update_manager helixscreen]
type: git_repo
channel: stable
path: /opt/helixscreen
origin: https://github.com/prestonbrown/helixscreen.git
primary_branch: main
managed_services: helixscreen
install_script: scripts/install.sh
```

Then restart Moonraker:
```bash
sudo systemctl restart moonraker
```

---

## Uninstalling

### Using Install Script (Recommended)

The install script with `--uninstall` removes HelixScreen and **restores your previous UI** (GuppyScreen, KlipperScreen, etc.):

```bash
# All platforms (Pi, AD5M, K1)
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | sh -s -- --uninstall
```

### Manual Uninstall

<details>
<summary>MainsailOS</summary>

```bash
# Stop and disable service
sudo systemctl stop helixscreen
sudo systemctl disable helixscreen

# Remove service file
sudo rm /etc/systemd/system/helixscreen.service
sudo systemctl daemon-reload

# Remove installation
sudo rm -rf /opt/helixscreen
```
</details>

<details>
<summary>AD5M Forge-X</summary>

```bash
# Stop and remove service
/etc/init.d/S90helixscreen stop
rm /etc/init.d/S90helixscreen

# Remove files
rm -rf /opt/helixscreen

# Re-enable GuppyScreen
chmod +x /opt/config/mod/.root/S80guppyscreen 2>/dev/null || true
chmod +x /opt/config/mod/.root/S35tslib 2>/dev/null || true

# Restore stock FlashForge UI in auto_run.sh (if it was disabled)
sed -i 's|^# Disabled by HelixScreen: /opt/PROGRAM/ffstartup-arm|/opt/PROGRAM/ffstartup-arm|' /opt/auto_run.sh 2>/dev/null || true

# Remove HelixScreen patch from screen.sh (restores backlight control)
# The automated uninstaller handles this; for manual removal, edit:
# /opt/config/mod/.shell/screen.sh and remove the helixscreen_active check

# Reboot to restore GuppyScreen
reboot
```

> **Note:** The automated uninstaller (`install.sh --uninstall`) handles all ForgeX restoration automatically, including unpatching `screen.sh`.
</details>

<details>
<summary>AD5M Klipper Mod</summary>

```bash
# Stop and remove service
/etc/init.d/S80helixscreen stop
rm /etc/init.d/S80helixscreen

# Remove files
rm -rf /root/printer_software/helixscreen

# Re-enable KlipperScreen
chmod +x /etc/init.d/S80klipperscreen 2>/dev/null || true

# Reboot to restore KlipperScreen
reboot
```
</details>

---

## Getting Help

### Check Logs First

Most issues are diagnosed from the logs:

**MainsailOS (systemd):**
```bash
# View recent logs
sudo journalctl -u helixscreen -n 100

# Follow live logs
sudo journalctl -u helixscreen -f

# Filter by error/warning level
sudo journalctl -u helixscreen -p err
```

**AD5M (SysV init):**
```bash
# View log file
cat /tmp/helixscreen.log

# Follow live logs
tail -f /tmp/helixscreen.log
```

### Common Issues

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for solutions to:
- Connection problems
- Display issues
- Touch not responding
- Configuration errors

### Still Stuck?

1. Check [GitHub Issues](https://github.com/prestonbrown/helixscreen/issues) for known problems
2. Open a new issue with:
   - Your hardware (Pi model, display type)
   - HelixScreen version
   - Relevant log output
   - Steps to reproduce

---

## Platform-Specific Notes

### Raspberry Pi 5

Pi 5 has multiple DRM devices. HelixScreen auto-detects the correct one, but if you have issues:

```json
// helixconfig.json
{
  "display": {
    "drm_device": "/dev/dri/card1"
  }
}
```

Common Pi 5 DRM devices:
- `/dev/dri/card0` - v3d (3D acceleration only, no display)
- `/dev/dri/card1` - DSI touchscreen (if connected)
- `/dev/dri/card2` - HDMI output

### Low Memory Systems (Pi 3, Pi Zero 2 W)

HelixScreen is optimized for low memory, but if you experience issues:

1. Disable other services that aren't needed
2. Reduce Moonraker's cache size
3. Consider a lighter Mainsail configuration

### Adventurer 5M Memory Constraints

The AD5M has limited RAM (~108MB total, with only ~24MB free after Klipper, Moonraker, and screen UI). HelixScreen is built with static linking and memory optimization for this environment.

**Measured memory comparison (VmRSS):**
| Component | KlipperScreen | HelixScreen |
|-----------|---------------|-------------|
| Screen UI | ~36 MB (Python) | **~12 MB** (C++) |
| X Server | ~4.6 MB | **0 MB** (framebuffer) |
| **Total** | ~40.5 MB | **~12 MB** |

On Klipper Mod systems, switching from KlipperScreen to HelixScreen frees approximately **29 MB** of RAM - a significant improvement on a memory-constrained device!

> **Note:** The 12 MB footprint includes the full LVGL widget tree, draw buffers for UI elements (gradients, color pickers, AMS spool icons), and runtime state for all panels. Images are loaded on-demand, not pre-cached.

If you experience memory issues:
- Reduce print history retention in Moonraker
- Avoid keeping many G-code files on the printer
- Consider disabling the camera stream if not needed

---

*Next: [User Guide](USER_GUIDE.md) - Learn how to use HelixScreen*
