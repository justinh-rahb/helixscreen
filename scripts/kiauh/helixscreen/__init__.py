# SPDX-License-Identifier: GPL-3.0-or-later
# HelixScreen KIAUH Extension
#
# This extension integrates HelixScreen with KIAUH for easy installation,
# updates, and removal through the KIAUH menu system.

from pathlib import Path

# Module path for asset resolution
MODULE_PATH = Path(__file__).parent

# HelixScreen configuration
HELIXSCREEN_REPO = "https://github.com/prestonbrown/helixscreen"
HELIXSCREEN_DIR = Path.home() / "helixscreen"
HELIXSCREEN_SERVICE_NAME = "helixscreen"

# Possible install locations (platform-dependent)
HELIXSCREEN_INSTALL_PATHS = [
    Path("/opt/helixscreen"),              # Pi, AD5M Forge-X
    Path("/usr/data/helixscreen"),         # K1/Simple AF
    Path("/root/printer_software/helixscreen"),  # AD5M Klipper Mod
]
HELIXSCREEN_INSTALLER_URL = (
    "https://raw.githubusercontent.com/prestonbrown/helixscreen/main/scripts/install.sh"
)


def find_install_dir() -> Path | None:
    """Find the actual HelixScreen install directory."""
    for path in HELIXSCREEN_INSTALL_PATHS:
        if path.exists() and (path / "helix-screen").exists():
            return path
    return None
