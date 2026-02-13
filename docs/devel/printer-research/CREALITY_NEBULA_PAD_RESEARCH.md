# Creality Nebula Pad Research

**Category**: Add-on touchscreen device (competitor/alternative to HelixScreen)
**Manufacturer**: Creality
**Model**: N-Pad 01
**Research Date**: February 2026

## Overview

The Nebula Pad is Creality's budget touchscreen add-on for existing 3D printers. Unlike the larger Sonic Pad (which runs actual Klipper), the Nebula Pad uses Creality's proprietary OS on more modest hardware.

## Hardware Specifications

| Component | Specification |
|-----------|---------------|
| **Display** | 4.3" IPS |
| **Resolution** | 480 x 272 pixels |
| **Touch Type** | Resistive |
| **CPU** | Dual-core ARM @ 1.2GHz (likely Allwinner T113, Cortex-A7) |
| **Storage** | 8GB |
| **WiFi** | 802.11 b/g/n (2.4GHz only) |
| **USB** | USB 2.0 |
| **OS** | Creality OS (proprietary, NOT open Klipper) |

## Nebula Camera (Bundled in Smart Kit)

| Feature | Specification |
|---------|---------------|
| **Connection** | USB |
| **Resolution** | High-resolution (exact spec unknown) |
| **Focus** | Manual adjustable |
| **Night Vision** | Yes, 940nm IR fill light |
| **AI Features** | Spaghetti detection |
| **Monitoring** | 24-hour real-time + time-lapse |

## Printer Compatibility

Compatible with Ender series printers that have 32-bit mainboards:
- Ender-3 (32-bit mainboard required)
- Ender-3 Pro
- Ender-3 V2 / V2 Neo
- Ender-3 V3 SE
- Ender-3 Max Neo
- Ender-3 S1 / S1 Pro

## Comparison: Nebula Pad vs Sonic Pad

| Feature | Nebula Pad | Sonic Pad |
|---------|------------|-----------|
| **Display** | 4.3" IPS | 7" IPS |
| **Resolution** | 480x272 | 1024x600 |
| **Touch** | Resistive | Capacitive |
| **OS** | Creality OS (proprietary) | Klipper-based |
| **Openness** | Closed | Semi-open (Klipper) |
| **Price** | ~$60-80 | ~$140-160 |
| **Target** | Budget users | Power users |

## HelixScreen Relevance

### Why This Matters

The Nebula Pad represents Creality's budget answer to the demand for touchscreen upgrades. Understanding its limitations helps position HelixScreen:

1. **Low resolution (480x272)** - HelixScreen targets higher resolution displays
2. **Resistive touch** - Outdated UX compared to capacitive
3. **Proprietary OS** - No community customization, locked ecosystem
4. **Limited connectivity** - No Ethernet, 2.4GHz WiFi only

### Competitive Position

HelixScreen advantages over Nebula Pad:
- Open source, community-driven
- Runs actual Klipper/Moonraker (not proprietary fork)
- Higher resolution display support
- Capacitive touch
- Full customization potential

### Hardware Similarity

The Allwinner T113 SoC is the same family as the AD5M Pro - both are dual-core Cortex-A7. This confirms ARM-based Klipper touchscreens are viable on modest hardware.

## Sources

- [Micro Center - Nebula Smart Kit](https://www.microcenter.com/product/676651/creality-nebula-smart-kit)
- [Creality Official Store](https://store.creality.com/products/creality-nebula-smart-kit)
- [Creality Downloads - Firmware](https://www.crealitycloud.com/downloads/box/nebula-pad)
- [GitHub - Klipper configs for Nebula](https://github.com/deanh8/Klipper)
- [Creality Community Forum Discussions](https://forum.creality.com/)

## Notes

- Official documentation is frustratingly sparse on hardware details
- No public teardown found as of research date
- The "dual-core 1.2GHz" spec strongly suggests Allwinner T113 (same as AD5M)
- Community has extracted Klipper configs but full root access status unclear
