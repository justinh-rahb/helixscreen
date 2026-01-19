<p align="center">
  <img src="assets/images/helix-icon-256.png" alt="HelixScreen" width="128"/>
  <br>
  <h1 align="center">HelixScreen</h1>
  <p align="center"><em>A modern touch interface for Klipper/Moonraker 3D printers</em></p>
</p>

<p align="center">
  <a href="https://github.com/prestonbrown/helixscreen/actions/workflows/build.yml"><img src="https://github.com/prestonbrown/helixscreen/actions/workflows/build.yml/badge.svg?branch=main" alt="Build"></a>
  <a href="https://github.com/prestonbrown/helixscreen/actions/workflows/quality.yml"><img src="https://github.com/prestonbrown/helixscreen/actions/workflows/quality.yml/badge.svg?branch=main" alt="Code Quality"></a>
  <a href="https://www.gnu.org/licenses/gpl-3.0"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3"></a>
  <a href="https://lvgl.io/"><img src="https://img.shields.io/badge/LVGL-9.4.0-green.svg" alt="LVGL"></a>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey.svg" alt="Platform">
</p>

HelixScreen is a printer control interface built with LVGL 9's declarative XML system. Designed for embedded hardware, it brings advanced Klipper features to printers that ship with limited vendor UIs.

---

> **Status: Beta — Seeking Testers**
>
> Core features are complete. We're looking for early adopters to help find edge cases.
>
> **Tested on:** Raspberry Pi 5, FlashForge Adventurer 5M Pro ([Forge-X](https://github.com/DrA1ex/ff5m) firmware)
>
> **Ready to help?** See [Quick Start](#quick-start). Issues and feedback welcome!

---

**Quick Links:** [Features](#features) · [Screenshots](#screenshots) · [Quick Start](#quick-start) · [FAQ](#faq) · [Contributing](docs/DEVELOPMENT.md#contributing) · [Roadmap](docs/ROADMAP.md)

---

## Why HelixScreen?

- **Declarative XML UI** — Change layouts without recompiling
- **Reactive Data Binding** — Subject-Observer pattern for automatic UI updates
- **Resource Efficient** — ~50-80MB footprint, runs on constrained hardware
- **Modern C++17** — Type-safe architecture with RAII memory management

| Feature | HelixScreen | GuppyScreen | KlipperScreen |
|---------|-------------|-------------|---------------|
| UI Framework | LVGL 9 XML | LVGL 8 C | GTK 3 (Python) |
| Declarative UI | Full XML | C only | Python only |
| Disk Size | ~50-80MB | ~60-80MB | ~150-200MB |
| RAM Usage | ~12MB | ~15-20MB | ~40MB |
| Reactive Binding | Built-in | Manual | Manual |
| Status | Beta | Stable | Mature |
| Language | C++17 | C | Python 3 |

## Screenshots

### Home Panel
<img src="docs/images/screenshot-home-panel.png" alt="Home Panel" width="800"/>

### Print File Browser
<img src="docs/images/screenshot-print-select-card.png" alt="Print Select" width="800"/>

### Bed Mesh Visualization
<img src="docs/images/screenshot-bed-mesh-panel.png" alt="Bed Mesh" width="800"/>

See [docs/GALLERY.md](docs/GALLERY.md) for all screenshots.

## Features

**Printer Control** — Print management, motion controls, temperature presets, fan control, Z-offset

**Multi-Material** — AFC, Happy Hare, tool changers, ValgACE, Spoolman integration

**Visualization** — G-code layer preview, 3D bed mesh, print thumbnails

**Calibration** — Input shaper, bed mesh, screws tilt, PID tuning, firmware retraction

**Integrations** — HelixPrint plugin, power devices, print history, timelapse, exclude objects

**System** — First-run wizard, 30 panels, light/dark themes, responsive 480×320 to 1024×600+

## Quick Start

```bash
# Check/install dependencies
make check-deps && make install-deps

# Build
make -j

# Run with mock printer (no hardware needed)
./build/bin/helix-screen --test

# Run with real printer
./build/bin/helix-screen
```

**Controls:** Click navigation icons, press 'S' for screenshot, use `-v` or `-vv` for logging.

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup, cross-compilation, and test modes.

## FAQ

**Is HelixScreen production-ready?**
Beta status. Core features work, but we're seeking testers. Suitable for enthusiasts willing to provide feedback.

**How is this different from GuppyScreen/KlipperScreen?**
HelixScreen uses LVGL 9's declarative XML—change layouts without recompiling. Lower memory than KlipperScreen. See the [comparison table](#why-helixscreen).

**Which printers are supported?**
Any Klipper + Moonraker printer. Currently tested on Voron 2.4, Voron 0.2, FlashForge Adventurer 5M Pro, and Doron Velta. The wizard auto-discovers your printer's capabilities.

**What multi-material systems work?**
AFC (Box Turtle), Happy Hare (ERCF, 3MS, Tradrack), tool changers, and ValgACE.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| SDL2 or build tools missing | `make install-deps` |
| Submodule empty | `git submodule update --init --recursive` |
| Can't connect to Moonraker | Check IP/port in helixconfig.json |
| Wizard not showing | Delete helixconfig.json to trigger it |

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) or open a [GitHub issue](https://github.com/prestonbrown/helixscreen/issues).

## Documentation

| Guide | Description |
|-------|-------------|
| [DEVELOPMENT.md](docs/DEVELOPMENT.md) | Build system, workflow, contributing |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design, patterns |
| [GALLERY.md](docs/GALLERY.md) | All screenshots |
| [ROADMAP.md](docs/ROADMAP.md) | Feature timeline |
| [LVGL9_XML_GUIDE.md](docs/LVGL9_XML_GUIDE.md) | XML syntax reference |

## License

GPL v3 — See individual source files for copyright headers.

## Acknowledgments

**Inspired by:** [GuppyScreen](https://github.com/ballaswag/guppyscreen) (general architecture, LVGL-based approach), [KlipperScreen](https://github.com/KlipperScreen/KlipperScreen) (feature inspiration)

**Stack:** [LVGL 9.4](https://lvgl.io/), [Klipper](https://www.klipper3d.org/), [Moonraker](https://github.com/Arksine/moonraker), [libhv](https://github.com/ithewei/libhv), [spdlog](https://github.com/gabime/spdlog), [SDL2](https://www.libsdl.org/)
