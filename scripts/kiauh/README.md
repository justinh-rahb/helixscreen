# HelixScreen KIAUH Extension

This directory contains a KIAUH (Klipper Installation And Update Helper) extension for HelixScreen.

## Installation Methods

### Method 1: Use the Direct Installer (Recommended)

The simplest way to install HelixScreen is using our bundled installer directly:

```bash
curl -sSL https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh | sh
```

This works on any platform (MainsailOS, KIAUH, manual installs, AD5M) and doesn't require KIAUH.

### Method 2: Via KIAUH Extension System

If you prefer to manage HelixScreen through KIAUH's menu system, you can add this extension:

#### Adding the Extension

1. Copy the `helixscreen` directory to KIAUH's extensions folder:

   ```bash
   cp -r ~/helixscreen/scripts/kiauh/helixscreen ~/kiauh/kiauh/extensions/
   ```

   Or if HelixScreen isn't cloned yet:

   ```bash
   git clone --depth 1 https://github.com/prestonbrown/helixscreen.git /tmp/helixscreen
   cp -r /tmp/helixscreen/scripts/kiauh/helixscreen ~/kiauh/kiauh/extensions/
   rm -rf /tmp/helixscreen
   ```

2. Restart KIAUH:

   ```bash
   ~/kiauh/kiauh.sh
   ```

3. Navigate to the Extensions menu. HelixScreen should now appear as an option.

#### Using the Extension

From the KIAUH Extensions menu, you can:

- **Install**: Downloads and installs the latest HelixScreen release
- **Update**: Updates to the latest version (preserves configuration)
- **Remove**: Uninstalls HelixScreen and restores your previous screen UI

## Note on KIAUH v5 vs v6

This extension is designed for KIAUH v6 (Python-based). If you're using an older version of KIAUH, use the direct installer method instead.

## What the Extension Does

The KIAUH extension is a thin wrapper around our bundled installer. It provides:

- Menu integration with KIAUH
- Same installation process as the direct installer
- Consistent update and removal experience

Under the hood, it downloads and runs the same `install.sh` script that the direct installation uses.

## Troubleshooting

If installation fails through KIAUH:

1. Try the direct installer method (curl|sh above)
2. Check KIAUH logs: `~/kiauh/logs/`
3. Check HelixScreen logs: `journalctl -u helixscreen` (Pi) or `/tmp/helixscreen.log` (AD5M)

## Support

- GitHub Issues: https://github.com/prestonbrown/helixscreen/issues
- Documentation: https://github.com/prestonbrown/helixscreen/blob/main/docs/user/INSTALL.md
