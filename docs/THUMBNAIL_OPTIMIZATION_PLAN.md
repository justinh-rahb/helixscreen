# Thumbnail Pre-Scaling Optimization Plan

> **Status**: Planned for future implementation
> **Priority**: Performance optimization for embedded displays
> **Branch**: TBD (separate from main platform rendering work)

## Problem
LVGL scales 300×300 thumbnails to ~140×150 **every frame** via `inner_align="contain"`. On AD5M (ARM Cortex-A7, no GPU), this causes severe UI lag when scrolling through print file cards.

## Solution
Pre-scale thumbnails once at download time, store as raw LVGL binary, display at 1:1.

## Key Design Decisions
- **Scaling library**: `stb_image_resize` (already in `lib/tinygl/include-demo/`)
- **Storage format**: Raw LVGL binary with `lv_image_header_t` (magic 0x19)
- **Threading**: `HThreadPool` from libhv for background processing
- **Target sizing**: Derived from display breakpoint + card dimensions

---

## Implementation Phases

### Phase 1: Smart Thumbnail Selection
**File**: `include/moonraker_types.h`

Add `get_best_thumbnail(target_w, target_h)` to `FileMetadata`:
- Find smallest thumbnail where `width >= target_w && height >= target_h`
- Fallback to largest if none meets minimum
- Returns `const ThumbnailInfo*` (nullptr if no thumbnails)

### Phase 2: ThumbnailProcessor Class
**New files**: `include/thumbnail_processor.h`, `src/thumbnail_processor.cpp`

```
ThumbnailProcessor
├── HThreadPool (2 workers)
├── process(png_data, source_path, target, callbacks)
│   ├── stbi_load_from_memory() - decode PNG
│   ├── stbir_resize_uint8() - scale to target
│   ├── Convert to ARGB8888 or RGB565
│   └── Write LVGL binary file
├── get_if_processed(source_path, target) - cache lookup
└── ThumbnailTarget get_thumbnail_target_for_display()
```

**Cache file naming**: `{hash}_{w}x{h}_{format}.lvbin`
- Example: `12345678_150x150_ARGB8888.lvbin`

**LVGL binary format** (12-byte header + raw pixels):
```c
struct LvglBinHeader {
    uint8_t magic;      // 0x19 (LV_IMAGE_HEADER_MAGIC)
    uint8_t cf;         // LV_COLOR_FORMAT_ARGB8888 or RGB565
    uint16_t flags;     // 0
    uint16_t w, h;
    uint16_t stride;
    uint16_t reserved;
};
// Followed by: raw pixel data (w * h * bpp)
```

### Phase 3: Display-Aware Target Sizing
**File**: `src/thumbnail_processor.cpp`

`get_thumbnail_target_for_display()` returns target based on breakpoint:

| Breakpoint | Max Screen | Card Width | Thumb Target | Format |
|------------|------------|------------|--------------|--------|
| SMALL | ≤480px | ~107px | 120×120 | ARGB8888 |
| MEDIUM | ≤800px | ~151px | 160×160 | ARGB8888 |
| LARGE | >800px | ~205px | 220×220 | ARGB8888 |

Note: Use ARGB8888 initially (AD5M framebuffer format). RGB565 can be added later as compile flag.

### Phase 4: ThumbnailCache Integration
**Files**: `include/thumbnail_cache.h`, `src/thumbnail_cache.cpp`

Add methods:
```cpp
// New: fetch with pre-scaling
void fetch_optimized(MoonrakerAPI* api,
                     const std::vector<ThumbnailInfo>& thumbnails,
                     SuccessCallback on_success, ErrorCallback on_error);

// New: check for pre-scaled version
std::string get_if_optimized(const std::string& source_path) const;
```

Flow:
1. `get_if_optimized()` → return immediately if cached
2. Select best thumbnail via `get_best_thumbnail()`
3. Download PNG to temp location
4. Queue to ThumbnailProcessor
5. On completion, call success callback with `A:` path

### Phase 5: PrintSelectPanel Integration
**File**: `src/ui_panel_print_select.cpp`

Modify `fetch_metadata_range()` (around line 660):

```cpp
// OLD:
std::string thumb_path = metadata.get_largest_thumbnail();

// NEW:
ThumbnailTarget target = get_thumbnail_target_for_display();
const ThumbnailInfo* best = metadata.get_best_thumbnail(target.width, target.height);
std::string thumb_path = best ? best->relative_path : "";
```

Replace direct `api->download_thumbnail()` calls with `get_thumbnail_cache().fetch_optimized()`.

### Phase 6: XML Card Update
**File**: `ui_xml/print_file_card.xml`

Change thumbnail image from scaled to 1:1:
```xml
<!-- OLD: LVGL scales at render time -->
<lv_image name="thumbnail" width="100%" height="100%" inner_align="contain" .../>

<!-- NEW: Pre-scaled, display at natural size (centered) -->
<lv_image name="thumbnail" align="center" inner_align="center" .../>
```

---

## Critical Files to Modify

| File | Changes |
|------|---------|
| `include/moonraker_types.h` | Add `get_best_thumbnail()` method |
| `include/thumbnail_processor.h` | **NEW** - ThumbnailProcessor class |
| `src/thumbnail_processor.cpp` | **NEW** - Implementation with stb_image |
| `include/thumbnail_cache.h` | Add `fetch_optimized()`, `get_if_optimized()` |
| `src/thumbnail_cache.cpp` | Implement optimized fetch, handle both formats |
| `src/ui_panel_print_select.cpp` | Use smart selection + optimized fetch |
| `ui_xml/print_file_card.xml` | Remove `inner_align="contain"` scaling |
| `Makefile` or `mk/rules.mk` | Add thumbnail_processor.cpp to build |

---

## Fallback Strategy

1. **Processing fails**: Use original PNG (unscaled, slower but works)
2. **No adequate thumbnail**: Use largest available, pre-scale anyway
3. **Single small thumbnail**: Upscale slightly rather than display tiny

---

## Cache Migration

- Check for `.lvbin` first (new format)
- Fall back to `.png` (legacy)
- Background-convert legacy PNGs on access
- LRU eviction handles both extensions

---

## Expected Performance Improvement

| Metric | Before | After |
|--------|--------|-------|
| Per-card render | ~5-10ms (scaling) | ~0.1ms (direct blit) |
| Scroll FPS | Laggy on AD5M | Smooth |
| Memory per thumb | ~350KB (300×300 ARGB) | ~100KB (160×160 ARGB) |

---

## Testing Checklist

- [ ] Verify stb_image_resize builds on ARM (AD5M cross-compile)
- [ ] Test with various slicer thumbnails (PrusaSlicer, OrcaSlicer, Cura)
- [ ] Measure scroll FPS improvement on AD5M
- [ ] Test cache eviction with mixed .lvbin/.png files
- [ ] Verify fallback when processing fails
- [ ] Test with files that have only 32×32 thumbnails
