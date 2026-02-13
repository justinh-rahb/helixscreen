# Pre-Rendered Image System

HelixScreen pre-renders splash screen images to LVGL binary format (`.lvbin`) at build time for instant display on embedded devices. This eliminates runtime PNG decoding, dramatically improving startup performance.

## Performance Impact

| Mode | FPS on AD5M | Time to Display |
|------|-------------|-----------------|
| PNG decoding (runtime) | ~2 FPS | ~500ms |
| Pre-rendered `.lvbin` | ~116 FPS | ~8ms |

## How It Works

1. **Build time**: The `scripts/regen_images.sh` script converts `helixscreen-logo.png` to LVGL binary format at exact pixel sizes matching each supported screen resolution
2. **Runtime**: `splash_screen.cpp` checks for pre-rendered images and uses them directly if available, falling back to PNG with runtime scaling otherwise

### Pre-rendered Image Sizes

Images are pre-rendered at **exact pixel sizes** matching the splash screen calculations in `splash_screen.cpp`:

| Screen Category | Resolution | Logo Size | Calculation |
|-----------------|------------|-----------|-------------|
| tiny | 480x320 | 240x240 | 480 × 50% (height < 500) |
| small | 800x480 | 400x400 | 800 × 50% (height < 500) |
| medium | 1024x600 | 614x614 | 1024 × 60% |
| large | 1280x720 | 768x768 | 1280 × 60% |

### File Format

`.lvbin` files contain:
- 12-byte header (magic, version, dimensions, color format)
- Raw ARGB8888 pixel data (4 bytes per pixel)

Example: `splash-logo-small.lvbin` = 12 + (400 × 400 × 4) = 640,012 bytes (~625KB)

## Usage

### Generate Images

```bash
# All sizes (for Pi with variable displays)
make gen-images

# AD5M only (800x480 fixed display)
make gen-images-ad5m

# Specific sizes
TARGET_SIZES=small,medium ./scripts/regen_images.sh
```

### Makefile Targets

| Target | Description |
|--------|-------------|
| `gen-images` | Generate all pre-rendered images |
| `gen-images-ad5m` | Generate only 800x480 size for AD5M |
| `gen-images-pi` | Generate all sizes for Pi |
| `clean-images` | Remove generated images |
| `list-images` | Show what would be generated |

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `OUTPUT_DIR` | `build/assets/images/prerendered` | Output directory |
| `TARGET_SIZES` | (all sizes) | Comma-separated sizes: `tiny,small,medium,large` |

## Integration with Build System

Pre-rendered images are **build artifacts** (not committed to the repository). They are automatically generated during:

- `make deploy-pi` / `make deploy-pi-*` (generates all sizes)
- `make deploy-ad5m` / `make deploy-ad5m-*` (generates only `small`)
- `make release-pi` (generates all sizes)
- `make release-ad5m` (generates only `small`)

### Platform-Specific Generation

The AD5M has a fixed 800×480 display, so only the `small` size is generated. The Raspberry Pi can have various displays, so all sizes are generated.

## Runtime Behavior

`splash_screen.cpp` implements this logic:

```cpp
std::string get_prerendered_splash_path(int screen_width) {
    // Select size based on screen width breakpoints
    if (screen_width <= 480) return "tiny";
    else if (screen_width <= 800) return "small";
    else if (screen_width <= 1024) return "medium";
    else return "large";

    // Check if file exists
    if (std::filesystem::exists(path)) {
        return "A:" + path;  // LVGL filesystem prefix
    }
    return "";  // Fall back to PNG
}
```

If no pre-rendered image is found, the splash screen falls back to PNG loading with runtime scaling (slower but works for any screen size).

## Adding New Pre-rendered Images

1. Add the source image to `assets/images/`
2. Update `IMAGES_TO_RENDER` in `scripts/regen_images.sh`:
   ```bash
   IMAGES_TO_RENDER=(
       "assets/images/helixscreen-logo.png:splash-logo:Splash screen logo"
       "assets/images/new-image.png:new-image:Description"
   )
   ```
3. Update the C++ code to use the pre-rendered image

## Troubleshooting

### Images not found at runtime

Check that images exist in the correct location:
```bash
ls -la build/assets/images/prerendered/
```

### Wrong image size displayed

The screen width breakpoints in `splash_screen.cpp` must match the sizes defined in `regen_images.sh`. If you add new screen sizes, update both files.

### Build fails with missing Pillow

Install the Python dependency:
```bash
pip install Pillow
# or with the project venv:
.venv/bin/pip install Pillow
```

## Technical Details

### LVGLImage.py

The `scripts/LVGLImage.py` script (from LVGL's tools) handles the conversion with these key options:

- `--cf ARGB8888`: Color format (32-bit with alpha)
- `--ofmt BIN`: Output format (LVGL binary)
- `--resize WxH`: Target size in pixels
- `--resize-fit`: Preserve aspect ratio (letterbox if needed)

### Why Not Use LVGL's Built-in Image Conversion?

LVGL can convert images at build time via its CMake integration, but:

1. This project uses Make, not CMake
2. We need exact pixel sizes for each screen resolution
3. The Python script gives us more control over the conversion process

### Memory Considerations

Pre-rendered images are larger than PNGs (uncompressed vs. compressed), but:
- They're stored on the filesystem, not in RAM
- They're loaded directly into LVGL's image cache
- The AD5M has 512MB RAM and 8GB storage, so space isn't a concern
