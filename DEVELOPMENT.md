# Development Guide

This document covers development environment setup, build processes, and daily development workflows for the HelixScreen prototype.

## Development Environment Setup

### Prerequisites by Platform

**macOS (Homebrew):**
```bash
brew install sdl2 bear imagemagick python3 node
npm install  # Install lv_font_conv and lv_img_conv
```

**Minimum macOS Version:** macOS 10.15 (Catalina) or newer required for CoreWLAN/CoreLocation WiFi APIs. The build system enforces this via `-mmacosx-version-min=10.15` deployment target.

**Debian/Ubuntu (apt):**
```bash
sudo apt install libsdl2-dev bear imagemagick python3 clang make npm
npm install  # Install lv_font_conv and lv_img_conv
```

**Fedora/RHEL/CentOS (dnf):**
```bash
sudo dnf install SDL2-devel bear ImageMagick python3 clang make npm
npm install  # Install lv_font_conv and lv_img_conv
```

### Dependency Overview

**Core Dependencies (Required):**
- **`clang`** - C/C++ compiler with C++17 support
- **`libsdl2-dev`** / **`SDL2-devel`** - SDL2 display simulator
- **`make`** - GNU Make build system
- **`python3`** - Icon generation scripts
- **`node`** / **`npm`** - Package manager for JavaScript dependencies
- **`lv_font_conv`** - Font converter (installed via `npm install`)

**Development Tools (Optional):**
- **`bear`** - Generates `compile_commands.json` for IDE/LSP support
- **`imagemagick`** - Screenshot conversion and icon generation

### Automated Dependency Management

**Check what's missing:**
```bash
make check-deps
```

**Automatically install missing dependencies:**
```bash
make install-deps  # Interactive confirmation before installing
```

The install script:
- Auto-detects your platform (macOS/Debian/Fedora)
- Lists packages to be installed
- Shows the exact command it will run
- Asks for confirmation before proceeding
- Handles npm packages and git submodules

## Build System

### Quick Build Commands

```bash
# Parallel incremental build (recommended for daily development)
make -j

# Clean parallel build with progress and timing
make build

# Run after building
./build/bin/helix-ui-proto

# Generate IDE support (one-time setup)
make compile_commands

# Clean rebuild (only when needed)
make clean && make -j
```

### Build System Features

- **Auto-parallel builds** - Detects CPU cores automatically
- **Color-coded output** - `[CXX]` (blue), `[CC]` (cyan), `[LD]` (magenta)
- **Incremental compilation** - Only rebuilds changed files
- **Automatic patch application** - LVGL patches applied transparently
- **Dependency validation** - Checks for missing tools before building

**Verbose mode** (for debugging build issues):
```bash
make V=1  # Shows full compiler commands
```

For complete build system documentation, see **[BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)**.

## Multi-Display Development (macOS)

Control which display the UI window appears on for dual-monitor workflows:

```bash
# Center window on specific display
./build/bin/helix-ui-proto --display 0     # Display 0 (main)
./build/bin/helix-ui-proto --display 1     # Display 1 (secondary)

# Position at exact coordinates
./build/bin/helix-ui-proto --x-pos 100 --y-pos 200

# Combine with other options
./build/bin/helix-ui-proto -d 1 -s small --panel home
```

**How it works:**
- Uses `SDL_GetDisplayBounds()` to query actual display geometry
- Calculates true center position for the specified display
- Supports multi-monitor setups with different resolutions

**Available displays:**
Run without arguments to see auto-detected display information in logs.

## macOS Location Permission (WiFi Development)

**Required for:** Testing real WiFi scanning/connection on macOS (CoreWLAN backend)

macOS 10.15+ requires Location Services permission for WiFi scanning because network SSIDs can reveal physical location. The app will automatically fall back to mock WiFi backend if permission is not granted.

### Why Manual Permission Grant is Needed

macOS's TCC (Transparency, Consent, and Control) system only shows automatic permission dialogs for:
- Signed app bundles (`.app` packages)
- Apps distributed via Mac App Store

Command-line binaries (like our development build) require manual permission grant.

### Grant Location Permission for Development

**Option 1: System Settings GUI (Recommended)**

1. Open **System Settings** → **Privacy & Security** → **Location Services**
2. Ensure "Location Services" is enabled at the top
3. Click the **(+)** button at the bottom of the app list
4. Navigate to: `/Users/YOUR_USERNAME/code/guppyscreen/prototype-ui9/build/bin/helix-ui-proto`
5. Click "Open" to add the binary
6. Toggle the switch next to "helix-ui-proto" to **ON**

**Option 2: TCC Database (Advanced)**

Directly add permission to the TCC database:

```bash
# Add location permission for the binary
sqlite3 ~/Library/Application\ Support/com.apple.TCC/TCC.db \
  "INSERT OR REPLACE INTO access VALUES('kTCCServiceLocation','$(pwd)/build/bin/helix-ui-proto',0,2,4,1,NULL,NULL,0,'UNUSED',NULL,0,1687910400);"

# Restart the binary to pick up new permission
```

**Note:** You may need to grant Terminal.app "Full Disk Access" in System Settings for the `sqlite3` command to work.

### Verify Permission Status

Run the app with verbose logging to see permission status:

```bash
./build/bin/helix-ui-proto --wizard -vv 2>&1 | grep -i "location\|permission\|wifi"
```

**Expected output with permission granted:**
```
[info] [macOS] Location permission already granted
[info] [WiFiMacOS] CoreWLAN backend initialized successfully
```

**Expected output without permission (falls back to mock):**
```
[warning] [WiFiMacOS] Location permission not determined
[warning] [WifiBackend] CoreWLAN backend failed - falling back to mock
```

### Revoking Permission

To test permission denial or reset state:

```bash
# Remove permission via System Settings
# Settings → Privacy & Security → Location Services → Remove helix-ui-proto

# Or reset via tccutil (requires SIP disabled or Full Disk Access)
tccutil reset Location
```

## Screenshot Workflow

### Interactive Screenshots

```bash
# Run the UI and press 'S' to take screenshot
./build/bin/helix-ui-proto
# Press 'S' key -> saves timestamped PNG to /tmp/
```

### Automated Screenshot Script

```bash
# Basic usage (auto-opens on display 1)
./scripts/screenshot.sh helix-ui-proto output-name [panel] [options]

# Examples
./scripts/screenshot.sh helix-ui-proto home-screen home
./scripts/screenshot.sh helix-ui-proto motion-panel motion -s small
./scripts/screenshot.sh helix-ui-proto controls controls -s large

# Override display for screenshots
HELIX_SCREENSHOT_DISPLAY=0 ./scripts/screenshot.sh helix-ui-proto test home

# Auto-open in Preview after capture
HELIX_SCREENSHOT_OPEN=1 ./scripts/screenshot.sh helix-ui-proto review home
```

**Script features:**
- ✅ **Automatic display positioning** - Opens on display 1 by default
- ✅ **Panel validation** - Catches invalid panel names before running
- ✅ **Dependency checking** - Verifies ImageMagick is installed
- ✅ **Smart cleanup** - Auto-removes BMP, keeps only compressed PNG
- ✅ **Error handling** - Clear error messages with troubleshooting hints

Screenshots saved to `/tmp/ui-screenshot-[name].png`

## Icon & Asset Workflow

### FontAwesome Icon Generation

FontAwesome icons are auto-generated to avoid UTF-8 encoding issues:

```bash
# After editing icon definitions in include/ui_fonts.h
python3 scripts/generate-icon-consts.py
```

This updates `ui_xml/globals.xml` with UTF-8 byte sequences for all icons.

### Application Icon Generation

Generate platform-specific application icons:

```bash
make icon
```

**Output:**
- **macOS**: `helix-icon.icns` (multi-resolution bundle) + `helix-icon.png`
- **Linux**: `helix-icon.png` (650x650 for application use)

**Requirements**: `imagemagick`, `iconutil` (macOS only, built-in)

## IDE & Editor Support

### Generate LSP Support

```bash
make compile_commands  # Requires 'bear' to be installed
```

This creates `compile_commands.json` for IDE language servers (clangd, etc.).

### Recommended Setup

**VS Code:**
- Install C/C++ extension pack
- Install clangd extension
- Ensure `compile_commands.json` is in project root

**Vim/Neovim:**
- Configure clangd LSP client
- Ensure `compile_commands.json` is available

**CLion/other IDEs:**
- Import as Makefile project
- Point to generated `compile_commands.json`

## Daily Development Workflow

### Typical Development Cycle

1. **Edit code** in `src/` or `include/`
2. **Edit XML** in `ui_xml/` (layout/styling changes - no rebuild needed)
3. **Build** with `make -j` (parallel incremental build)
4. **Test** with `./build/bin/helix-ui-proto [panel_name]`
5. **Screenshot** with `./scripts/screenshot.sh` or press **S** in UI
6. **Commit** with working incremental changes

### XML vs C++ Changes

**XML file changes:**
- Layout, styling, colors, text content
- **No recompilation needed** - changes visible immediately on restart
- Edit `ui_xml/*.xml` files directly

**C++ file changes:**
- Business logic, subject bindings, event handlers
- **Requires rebuild** - run `make -j` after changes
- Edit `src/*.cpp` and `include/*.h` files

### Performance Tips

**Use incremental builds:**
```bash
make -j  # Only rebuilds changed files
```

**Avoid clean rebuilds unless necessary:**
```bash
# Only when troubleshooting build issues
make clean && make -j
```

**Parallel compilation:**
```bash
make -j     # Auto-detects all CPU cores (recommended)
make -j16   # Explicit core count (current system has 16 cores)
```

## Troubleshooting

### Common Build Issues

**SDL2 not found:**
```bash
# macOS
brew install sdl2

# Debian/Ubuntu
sudo apt install libsdl2-dev

# Verify installation
which sdl2-config
sdl2-config --version
```

**Compilation errors:**
```bash
# Use verbose mode to see full commands
make clean && make V=1
```

**Missing dependencies:**
```bash
# Check what's missing
make check-deps

# Auto-install missing packages
make install-deps
```

### Runtime Issues

**Missing libraries at runtime:**
```bash
# Check dynamic library dependencies
ldd build/bin/helix-ui-proto

# Verify submodules are initialized
git submodule status
git submodule update --init --recursive
```

**UI not responding:**
- Ensure you're not manually calling `SDL_PollEvent()` - LVGL handles this internally
- Check for infinite loops in timer callbacks
- Use spdlog with `-v` flag to see event flow

### Performance Issues

**Slow compilation:**
- Use `make -j` for parallel builds
- Check CPU usage during build (should be near 100% with parallel)
- Use incremental builds instead of clean rebuilds

**Slow UI performance:**
- Enable compiler optimizations: builds use `-O2` by default
- Check for expensive operations in timer callbacks
- Use `make V=1` to verify optimization flags

## Advanced Development

### Working with LVGL Patches

The build system automatically applies patches to LVGL. To modify LVGL behavior:

1. **Make changes** in the `lvgl/` directory
2. **Generate patch**:
   ```bash
   cd lvgl
   git diff > ../patches/my-feature.patch
   ```
3. **Update Makefile** to apply the patch in the `apply-patches` target
4. **Test** on clean checkout

See **[BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)** for complete patch management details.

### Cross-Platform Considerations

**This project is developed on both macOS and Linux:**
- **NEVER invoke compilers directly** (`clang++`, `g++`) - always use `make`
- Makefile auto-detects available compiler (clang > gcc priority)
- Platform-specific features are handled via Makefile platform detection
- Test on both platforms when possible
- **WiFi Backend:** macOS uses CoreWLAN, Linux uses wpa_supplicant

### Memory & Performance Analysis

For detailed analysis tools and techniques, see **[docs/MEMORY_ANALYSIS.md](docs/MEMORY_ANALYSIS.md)**.

## Related Documentation

- **[README.md](README.md)** - Project overview and quick start
- **[BUILD_SYSTEM.md](docs/BUILD_SYSTEM.md)** - Complete build system reference
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System design and patterns
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Code standards and workflow
- **[LVGL 9 XML Guide](docs/LVGL9_XML_GUIDE.md)** - Complete XML system reference
- **[Quick Reference](docs/QUICK_REFERENCE.md)** - Common patterns and gotchas