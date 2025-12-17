# HelixScreen Installation Guide

This guide walks you through installing HelixScreen on your 3D printer's touchscreen display.

**Target Audience:** Klipper users who want to use pre-built packages. If you're a developer building from source, see [DEVELOPMENT.md](../DEVELOPMENT.md).

---

## Table of Contents

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [MainsailOS Installation](#mainsailos-installation)
- [Adventurer 5M Installation](#adventurer-5m-installation)
- [First Boot & Setup Wizard](#first-boot--setup-wizard)
- [Display Configuration](#display-configuration)
- [Starting on Boot](#starting-on-boot)
- [Updating HelixScreen](#updating-helixscreen)
- [Uninstalling](#uninstalling)
- [Getting Help](#getting-help)

---

## Quick Start

**One-liner install (recommended):**
```bash
# Raspberry Pi (MainsailOS)
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash

# Adventurer 5M (run as root)
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash
```

The installer automatically detects your platform and downloads the correct release.

**Manual installation** (if you prefer):

<details>
<summary>MainsailOS (Raspberry Pi)</summary>

```bash
# Download the latest release
cd ~
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-pi.tar.gz
tar -xzf helixscreen-pi.tar.gz

# Install (copies to /opt/helixscreen and sets up service)
cd helixscreen
sudo cp -r . /opt/helixscreen/
sudo cp /opt/helixscreen/config/helixscreen.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now helixscreen
```
</details>

<details>
<summary>Adventurer 5M</summary>

```bash
# From your computer, copy to printer
# Note: AD5M requires scp -O (legacy protocol) since BusyBox lacks sftp-server
scp -O helixscreen-ad5m-*.tar.gz root@<printer-ip>:/tmp/helixscreen.tar.gz

# SSH into printer and install
ssh root@<printer-ip>
cd /opt && gunzip -c /tmp/helixscreen.tar.gz | tar xf -
cp /opt/helixscreen/config/helixscreen.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now helixscreen

# Clean up
rm /tmp/helixscreen.tar.gz
```

> **Note:** AD5M uses BusyBox utilities with some limitations:
> - Use `scp -O` (legacy protocol) instead of plain `scp` - BusyBox lacks sftp-server
> - Use `gunzip -c | tar xf -` instead of `tar -xzf` - BusyBox tar lacks `-z` flag
</details>

After installation, the setup wizard will guide you through initial configuration.

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
  - [Forge-X](https://github.com/DrA1ex/ff5m) or similar Klipper firmware installed and working
  - SSH access to the printer (usually `root@<printer-ip>`)
  - About 100MB free disk space

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

> **Important:** Installing HelixScreen replaces the stock FlashForge UI. Make sure you have a backup method to access your printer (SSH, Mainsail/Fluidd web interface).

### Step 1: Download the AD5M Package

On your computer, download the Adventurer 5M release:

```bash
# From the releases page
wget https://github.com/prestonbrown/helixscreen/releases/latest/download/helixscreen-ad5m.tar.gz
```

### Step 2: Copy to Your Printer

Transfer the package to your Adventurer 5M:

```bash
# AD5M requires scp -O (legacy protocol) since BusyBox lacks sftp-server
scp -O helixscreen-ad5m.tar.gz root@<printer-ip>:/tmp/
```

Replace `<printer-ip>` with your printer's IP address (check your router or printer settings).

> **Note:** We copy to `/tmp/` first because `/opt/` may not have enough space for both the tarball and extracted files.
> AD5M requires `scp -O` (OpenSSH legacy protocol) since BusyBox lacks the sftp-server that modern scp uses by default.

### Step 3: Install on the Printer

SSH into your printer and install:

```bash
ssh root@<printer-ip>

cd /opt
# Note: AD5M uses BusyBox tar which doesn't support -z flag
gunzip -c /tmp/helixscreen.tar.gz | tar xf -

# Stop existing screen UI (GuppyScreen, FeatherScreen, etc.)
# The init script will do this automatically, but you can do it manually:
/opt/config/mod/.root/S80guppyscreen stop 2>/dev/null || true

# Install SysV init script (AD5M uses BusyBox init, NOT systemd)
cp /opt/helixscreen/config/helixscreen.init /etc/init.d/S90helixscreen
chmod +x /etc/init.d/S90helixscreen

# Start HelixScreen
/etc/init.d/S90helixscreen start

# Clean up the tarball to free /tmp space
rm /tmp/helixscreen.tar.gz
```

> **Note:** AD5M runs as root, so `sudo` is not needed.
> **Note:** AD5M uses BusyBox utilities. Use `gunzip -c | tar xf -` instead of `tar -xzf`.
> **Note:** AD5M uses SysV init (BusyBox), not systemd. Use `/etc/init.d/S90helixscreen` for service management.

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
```bash
# Start HelixScreen
/etc/init.d/S90helixscreen start

# Stop HelixScreen
/etc/init.d/S90helixscreen stop

# Restart (after config changes)
/etc/init.d/S90helixscreen restart

# View status
/etc/init.d/S90helixscreen status

# View logs
cat /tmp/helixscreen.log
```

### Disabling Other UIs

If you have another UI installed, disable it to avoid conflicts:

**MainsailOS (systemd):**
```bash
# Disable KlipperScreen (if installed)
sudo systemctl stop KlipperScreen
sudo systemctl disable KlipperScreen
```

**AD5M (SysV init):**
```bash
# Disable GuppyScreen (if installed via Forge-X)
/opt/config/mod/.root/S80guppyscreen stop
chmod -x /opt/config/mod/.root/S80guppyscreen

# Disable FeatherScreen (if installed)
/etc/init.d/S*featherscreen stop 2>/dev/null || true
chmod -x /etc/init.d/S*featherscreen 2>/dev/null || true
```

> **Note:** The HelixScreen installer automatically stops and disables competing UIs.

---

## Updating HelixScreen

### Check Current Version

On the touchscreen: **Settings > About** shows the current version.

Or via SSH, check the help output:
```bash
/opt/helixscreen/bin/helix-screen --help | head -1
```

### Update Using Install Script (Recommended)

The easiest way to update is using the install script with `--update`:

```bash
# Both Pi and AD5M
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash -s -- --update
```

This preserves your configuration and updates to the latest version.

### Update to Specific Version

```bash
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash -s -- --update --version v1.2.0
```

### Preserving Configuration

The update process preserves your `helixconfig.json` settings. If you want to reset to defaults:

```bash
sudo rm /opt/helixscreen/helixconfig.json
sudo systemctl restart helixscreen
```

---

## Uninstalling

### Using Uninstall Script (Recommended)

The uninstall script removes HelixScreen and **restores your previous UI** (GuppyScreen, FeatherScreen, etc.):

```bash
# If HelixScreen is installed (AD5M - run as root)
/opt/helixscreen/scripts/uninstall.sh

# If HelixScreen is installed (Pi - use sudo)
sudo /opt/helixscreen/scripts/uninstall.sh

# Or download and run
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/uninstall.sh | sudo bash
```

### Using Install Script

```bash
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | bash -s -- --uninstall
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
<summary>Adventurer 5M</summary>

```bash
# Stop service (SysV init)
/etc/init.d/S90helixscreen stop

# Remove init script
rm /etc/init.d/S90helixscreen

# Remove files
rm -rf /opt/helixscreen

# Re-enable previous UI (if desired)
chmod +x /opt/config/mod/.root/S80guppyscreen 2>/dev/null || true

# Reboot to restore previous UI
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

The AD5M has limited RAM (~36MB free with Klipper running). HelixScreen is built with static linking and memory optimization for this environment.

If you experience memory issues:
- Reduce print history retention in Moonraker
- Avoid keeping many G-code files on the printer

---

*Next: [User Guide](USER_GUIDE.md) - Learn how to use HelixScreen*
