# Changelog

All notable changes to HelixScreen will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.6] - 2026-02-08

### Added
- Per-object G-code toolpath thumbnails in Print Objects overlay
- AFC (Armored Turtle) support: live device state, tool mapping, endless spool, per-lane reset, maintenance and LED controls, quiet mode, and mock simulation
- Active object count shown on layer progress line during printing
- Change Host modal for switching Moonraker connection in settings
- Z movement style override setting and E-Stop relocated to Motion section
- K1 dynamic linking toolchain and build target
- Creality K2 series cross-compilation target (ARM, static musl — untested, needs hardware validation)
- CDN-first installer downloads with GitHub fallback
- Multi-channel R2 update distribution with GitHub API fallback

### Fixed
- Toasts now render on top layer instead of active screen (fixes toasts hidden behind overlays)
- Print cancel timeout increased to 15s with active state observation for more reliable cancellation
- Pre-print time estimates seeded from slicer data with blended early progress
- Thread-safe slicer estimate seeding during print start
- G-code viewer cache thrash from current_object changes during exclude-object prints
- ForgeX startup framebuffer stomping by S99root init script
- Wrong-platform binary install prevented with ELF architecture validation and safe rollback
- Use-after-free crash on Print Objects overlay close
- Isometric thumbnail rendering with shared projection, depth shading, and thicker lines
- Install warning text centered in update download modal
- Missing alert_circle icon codepoint
- Settings About section consolidated with cleaner version row layout
- Z baby step icons and color swatch labels
- Exclude object mock mode: objects populated from G-code on print start with proper status dispatch

## [0.9.5] - 2026-02-07

### Added
- Exclude object support for streaming/2D mode with selection brackets and long-press interaction
- Print Objects list overlay showing defined objects during a print
- LED selection dropdown in settings for multi-LED printers
- Version number displayed on splash screen
- Beta and dev update channels with UI toggle and R2 upload script
- Beta feature wrapper component with badge indicator
- 32-bit ARM (armv7l) Raspberry Pi build target (#10)
- Auto-publish tagged releases to R2 with platform detection
- Exclude object G-code parsing and status dispatch in mock mode

### Fixed
- Use-after-free race in wpa_supplicant backend shutdown (#8)
- Deadlock in Happy Hare and ToolChanger AMS backend start (#9)
- DNS resolver fallback for static glibc builds
- Crash when navigating folders during metadata fetch in print selection
- LED detection excluding toolhead LEDs from main LED control
- WebSocket max message size increased from 1MB to 5MB (#7)
- Elapsed/remaining time display during mock printing
- Crash on window close from SDL event handling during shutdown
- Accidental scroll taps by increasing scroll limit default
- G-code parser now reads layer_height, first_layer_height, object_height from metadata
- Invalid text_secondary color token replaced with text_muted
- KIAUH metadata wrapper key and moonraker updater path (#3)
- Installer sparse checkout for updater repo (#11)
- Output_pin lights detected as LEDs with fallback to first LED (#14)
- Percentage rounding instead of truncating to fix float precision (#14)
- Z offset display sync when print tune overlay opens (#14)
- CoreXZ treated as gantry-moves-Z instead of bed-moves (#14)

### Changed
- Log levels cleaned up: INFO is concise, DEBUG is useful without per-layer/shutdown spam
- Duplicate log bugs fixed (PrintStartCollector double-completion, PluginManager double-unload)
- Settings panel version rows deduplicated
- Exclude object modal XML registration and single-select behavior

## [0.9.4] - 2026-02-07

### Added
- Pre-print time predictions based on historical heating/homing data
- Heater status text on temperature cards (Heating, Cooling, At Target)
- Slicer estimated time fallback for remaining time
- Seconds in duration display under 5 minutes

### Fixed
- Crash on 16bpp HDMI screens from forced 32-bit color format
- Elapsed time using wall-clock duration instead of print-only time
- Pre-print overlay showing when it shouldn't
- Backlight not turning off on AD5M
- Heater status colors (heating=red, added cooling state)
- AMS row hidden when no AMS connected
- Modal button alignment
- Install script version detection on Pi (#6)

## [0.9.3] - 2026-02-06

First public beta release. Core features are complete — we're looking for early
adopters to help find edge cases.

**Supported platforms:** Raspberry Pi (aarch64), FlashForge AD5M (armv7l),
Creality K1 (MIPS32)

> **Note:** K1 binaries are included but have not been tested on hardware. If you
> have a K1, we'd love your help verifying it works!

### Added
- Print start profiles with modular, JSON-driven signal matching for per-printer phase detection
- NetworkManager WiFi backend for broader Linux compatibility
- `.3mf` file support in print file browser
- Non-printable file filtering in print selection
- Beta features gating system for experimental UI (HelixPrint plugin)
- Platform detection and preset system for zero-config installs
- Settings action rows with bind_description for richer UI
- Restart logic consolidated into single `app_request_restart_service()` entry point

### Fixed
- Print start collector not restarting after a completed print
- Sequential progress regression on repeated signals during print start
- Bed mesh triple-rendering and profile row click targets
- Wizard WiFi step layout, password visibility toggle, and dropdown corruption
- Touch calibration skipped for USB HID touchscreens (HDMI displays)
- CJK glyph inclusion from C++ sources in font generation
- File ownership for non-root deploy targets
- Console cursor hidden on fbdev displays

### Changed
- Pi deploys now use `systemctl restart` instead of stop/start
- fbdev display backend for Pi (avoids DRM master contention)
- Comprehensive architectural documentation from 5-agent audit
- Troubleshooting guide updated with debug logging instructions

## [0.9.2] - 2026-02-05

Major internal release with live theming, temperature sensor support, and
extensive UI polish across all panels.

### Added
- Live theme switching without restart — change themes in settings instantly
- Dark/light gradient backgrounds and themed overlay constants
- Full-screen 3D splash images with dark/light mode support
- Temperature sensor manager for auxiliary temp sensors (chamber, enclosure, etc.)
- Responsive fan dial with knob glow effect
- Software update checker with download progress and install-during-idle safety
- Platform hook architecture for modularized installer functions
- Auto-detect Pi install path from Klipper ecosystem
- AD5M preset with auto-detection for zero-config setup
- Beta features config flag for gating experimental UI
- CJK glyph support (Chinese, Japanese, Russian) in generated fonts
- Pencil edit icons next to temperature controls
- OS version, MCU versions, and printer name in About section
- Shell tests (shellcheck, bats) gating release builds

### Fixed
- Shutdown crash: stop animations before destroying panels to prevent use-after-free
- Observer crash: reorder display/subject teardown sequence
- Stale widget pointer guards for temperature and fan updates
- Theme palette preservation across dark/light mode switches
- Button text contrast for layout=column buttons with XML children
- Navbar background not updating on theme toggle
- Dropdown corruption with `&#10;` newline entities in XML
- Wizard initialization: fan subscriptions, sensor select, toast suppression
- Kinematics detection and Z button icons for bed-moves printers
- Bed mesh data normalization and zero plane visibility
- Filament panel deferred `set_limits` to main thread
- Touch calibration target spread and full-screen capture

### Changed
- Pi builds target Debian Bullseye for wider compatibility
- Static-link OpenSSL for cross-platform SSL support
- Binaries relocated to `bin/` subdirectory in deploy packages
- Fan naming uses configured roles instead of heuristics
- HelixScreen brand theme set as default
- Installer modularized with platform dispatchers
- Release build timeout increased to 60 minutes

## [0.9.1] - 2026-02-04

Initial tagged release. Foundation for all subsequent development.

### Added
- 30 panels and 16 overlays covering full printer control workflow
- First-run setup wizard with 8-step guided configuration
- Multi-material support: AFC, Happy Hare, tool changers, ValgACE, Spoolman
- G-code preview and 3D bed mesh visualization
- Calibration tools: input shaper, mesh leveling, screws tilt, PID, firmware retraction
- Internationalization system with hot-reload language switching
- Light and dark themes with responsive 800x480+ layout
- Cross-compilation for Pi (aarch64), AD5M (armv7l), K1 (MIPS32)
- Automated GitHub Actions release pipeline
- One-liner installation script with platform auto-detection

[0.9.6]: https://github.com/prestonbrown/helixscreen/compare/v0.9.5...v0.9.6
[0.9.5]: https://github.com/prestonbrown/helixscreen/compare/v0.9.4...v0.9.5
[0.9.4]: https://github.com/prestonbrown/helixscreen/compare/v0.9.3...v0.9.4
[0.9.3]: https://github.com/prestonbrown/helixscreen/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/prestonbrown/helixscreen/compare/v0.9.1...v0.9.2
[0.9.1]: https://github.com/prestonbrown/helixscreen/releases/tag/v0.9.1
