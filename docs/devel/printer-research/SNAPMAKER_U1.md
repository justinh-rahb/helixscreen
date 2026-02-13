# Snapmaker U1 Support

## Hardware Profile

| Spec | Value |
|------|-------|
| SoC | Rockchip ARM64 |
| Display | 480x320 framebuffer (`/dev/fb0`) |
| Touch | `/dev/input/event0` |
| Firmware | Klipper + Moonraker |
| Drivers | TMC2240 |
| Filament | RFID (FM175xx, OpenSpool NTAG215/216) |
| Camera | MIPI CSI + USB, Rockchip MPP/VPU |
| OS | Debian Trixie ARM64 |
| SSH | `lava@<ip>` (password: snapmaker) via extended firmware |

## Current Status

### Implemented (committed on `main`)
- ARM64 cross-compilation target: `make PLATFORM_TARGET=snapmaker-u1` / `make snapmaker-u1-docker`
- Docker toolchain: `docker/Dockerfile.snapmaker-u1` (Debian Trixie, static linking)
- Printer database entry with RFID reader detection heuristics
- Print start profile: `config/print_start_profiles/snapmaker_u1.json` (weighted/heuristic)
- Platform hooks: `config/platform/hooks-snapmaker-u1.sh`
- Deployment targets: `deploy-snapmaker-u1`, `deploy-snapmaker-u1-fg`, `deploy-snapmaker-u1-bin`

### NOT in CI/release pipeline
The snapmaker-u1 target is deliberately excluded from `release-all` and `package-all`.
It will not build in GitHub Actions until a workflow job is explicitly added to `.github/workflows/release.yml`.

### 480x320 Display
See [480x320 UI Audit](480x320_UI_AUDIT.md) for a panel-by-panel breakdown of what works
and what needs fixing at this resolution. The audit is resolution-specific, not Snapmaker-specific
— any 480x320 device benefits from the same fixes.

## Build & Deploy

```bash
# Build via Docker (recommended)
make snapmaker-u1-docker

# Deploy to U1
make deploy-snapmaker-u1 SNAPMAKER_U1_HOST=192.168.1.xxx

# Deploy binary only (fast iteration)
make deploy-snapmaker-u1-bin SNAPMAKER_U1_HOST=192.168.1.xxx

# SSH
make snapmaker-u1-ssh SNAPMAKER_U1_HOST=192.168.1.xxx
```

Default SSH user is `lava` (override with `SNAPMAKER_U1_USER`).
Default deploy dir is `/opt/helixscreen` (override with `SNAPMAKER_U1_DEPLOY_DIR`).

## Future Work

### RFID Filament Integration (Nice-to-Have)
The U1 has a 4-channel RFID reader (FM175xx) with OpenSpool support. Could implement
`AmsBackendRfid` following the existing AMS backend pattern.

Klipper commands:
- `FILAMENT_DT_UPDATE CHANNEL=<n>` — Read tag
- `FILAMENT_DT_QUERY CHANNEL=<n>` — Query cached data
- OpenSpool JSON format with material type, color, temp ranges

### Extended Firmware Overlay
Could package HelixScreen as an extended firmware overlay for cleaner user installation
(vs manual SSH deployment).

## References
- [Extended Firmware repo](https://github.com/paxx12/SnapmakerU1-Extended-Firmware)
