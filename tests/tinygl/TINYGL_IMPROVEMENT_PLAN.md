# TinyGL Quality & Performance Improvement Plan

**Status**: Active Planning Phase
**Last Updated**: 2025-11-19
**Target**: Improve visual quality and performance for embedded G-code preview

---

## Executive Summary

TinyGL currently achieves ~90% visual match with OrcaSlicer but has opportunities for quality and performance improvements suitable for embedded hardware. This document outlines planned enhancements, their current status, and lessons learned from initial implementation attempts.

---

## 1. Color Banding Reduction (Ordered Dithering)

### Status: 🟡 **Infrastructure Added, Integration Paused**

### Description
Reduce visible 8-bit color quantization artifacts in gradients using 4x4 Bayer matrix ordered dithering.

### Current State
- ✅ Created `zdither.h/c` with dithering implementation
- ✅ Added Bayer matrix lookup tables (4x4)
- ✅ Implemented `glSetDithering()`/`glGetDithering()` API
- ✅ Added `RGB_TO_PIXEL_COND` conditional macro
- ✅ Included in build system
- ❌ **Not yet integrated into rasterization pipeline**

### Lessons Learned
1. **Test incrementally** - Initial implementation modified too many macros at once without testing
2. **Rendering tests must verify pixels** - Test framework created files but didn't verify content
3. **Macro changes are subtle** - Small errors in PUT_PIXEL macros silently broke all rendering
4. **Committed code needs verification** - Broke rendering twice without catching it in tests

### Next Steps (When Resumed)
1. Test `RGB_TO_PIXEL_COND` in isolated context first
2. Modify ONE PUT_PIXEL variant and test before others
3. Add pixel verification to test framework (not just file creation)
4. Start with flat shading before smooth shading
5. Test after EACH change, not batch changes

### Complexity: **Medium**
### Visual Impact: **Low-Medium** (reduces gradient banding)
### Performance Impact: **Minimal** (<3% when enabled, 0% when disabled)

---

## 2. Edge Anti-Aliasing

### Status: 🔴 **Not Started**

### Description
Implement coverage-based anti-aliasing for triangle edges to reduce jagged appearance on diagonal lines.

### Approach Options
1. **Supersampling** - Render at higher resolution, downsample (simple but expensive)
2. **Coverage masks** - Calculate sub-pixel coverage for edge pixels (better performance)
3. **MSAA-style** - Multi-sample approach with sample pattern

### Technical Requirements
- Modify edge rasterization to calculate coverage
- Add sub-pixel precision to edge calculations
- Blend edge pixels based on coverage percentage

### Estimated Complexity: **High**
### Visual Impact: **High** (significantly improves diagonal lines)
### Performance Impact: **Medium** (5-15% depending on approach)

### Recommended Approach
Start with 2x2 coverage masks on edge pixels only (not full supersampling).

---

## 3. Per-Pixel Lighting (Phong Shading)

### Status: 🔴 **Not Started**

### Description
Replace current Gouraud shading (per-vertex) with Phong shading (per-pixel) for smoother lighting on curved surfaces.

### Current Limitation
- TinyGL uses Gouraud shading (interpolate vertex colors)
- Creates visible lighting bands on low-poly curved surfaces
- Most noticeable on spheres and cylinders with <20 segments

### Technical Approach
1. Interpolate normals across triangle (not colors)
2. Calculate lighting per-pixel in rasterizer
3. Hybrid mode: Phong for curved surfaces, Gouraud for flat

### Challenges
- Significant rasterizer modifications required
- Need normal interpolation in scanline renderer
- Per-pixel lighting calculation cost

### Estimated Complexity: **Very High**
### Visual Impact: **High** (eliminates lighting bands on curves)
### Performance Impact: **High** (30-50% slower, needs profiling)

### Recommendation
**Lower priority** - Current Gouraud shading is acceptable for G-code preview. Only pursue if visual quality becomes critical requirement.

---

## 4. Tile-Based Parallel Rasterization

### Status: 🔴 **Not Started**

### Description
Replace scanline rasterizer with tile-based approach to enable parallel processing.

### Current Architecture
- Scanline-based: Process triangles top-to-bottom sequentially
- Single-threaded
- Good cache locality for depth buffer

### Proposed Architecture
- Divide framebuffer into tiles (e.g., 32x32 pixels)
- Sort triangles into bins by tile coverage
- Process tiles in parallel (thread per tile)
- Requires careful depth buffer handling

### Benefits
- Enables multi-core utilization
- Better cache locality for small tiles
- Can render tiles out of order

### Challenges
- Complex implementation (major rewrite)
- Thread synchronization overhead
- Memory overhead for tile bins
- Depth buffer synchronization

### Estimated Complexity: **Very High**
### Visual Impact: **None** (performance only)
### Performance Impact: **High positive** (2-4x speedup on multi-core)

### Recommendation
**Defer indefinitely** - Current performance (597 FPS for 6K triangles) is more than sufficient for embedded G-code preview. Only pursue if target hardware has multiple cores AND performance becomes bottleneck.

---

## 5. SIMD Acceleration

### Status: 🔴 **Not Started**

### Description
Use SIMD instructions (SSE/AVX/NEON) for vector math operations (transforms, lighting calculations).

### Target Operations
- Matrix-vector multiplications (vertex transforms)
- Lighting calculations (dot products, vector normalization)
- Color blending
- Possibly 4 pixels at a time in rasterizer

### Platforms
- **x86/x64**: SSE2, SSE4, AVX
- **ARM**: NEON (important for embedded targets)

### Estimated Complexity: **High**
### Visual Impact: **None** (performance only)
### Performance Impact: **Medium positive** (20-40% faster transforms/lighting)

### Recommendation
**Low priority** - Profile first to identify actual bottlenecks. May be worthwhile for ARM embedded targets if transform/lighting is bottleneck.

---

## 6. Other Potential Improvements

### 6.1 Depth Buffer Optimization
**Status**: 🟢 **Already Good**
Current 16-bit depth buffer is appropriate for viewing distances. No changes needed.

### 6.2 Frustum Culling Optimization
**Status**: 🟢 **Likely Sufficient**
Standard frustum culling should be adequate. Only profile if performance issues arise.

### 6.3 Texture Filtering
**Status**: 🔴 **Not Applicable**
G-code preview doesn't use textures. Not relevant for this use case.

### 6.4 Backface Culling
**Status**: 🟢 **Likely Enabled**
Standard feature. Verify it's enabled but no changes needed.

---

## Priority Recommendations

### Immediate Priority (Do Next)
1. **None** - Current TinyGL quality is acceptable for G-code preview

### Short-term (If Quality Issues Arise)
1. **Edge Anti-Aliasing** - If diagonal lines look too jagged
2. **Ordered Dithering** - If gradient banding is reported as issue

### Long-term (Only If Necessary)
1. **SIMD Acceleration** - Only if profiling shows transform/lighting bottleneck on ARM
2. **Per-Pixel Lighting** - Only if Gouraud artifacts become unacceptable
3. **Parallel Rasterization** - Only if multi-core embedded target AND performance critical

### Never (Not Worth The Effort)
- Supersampling anti-aliasing (too expensive)
- Full Phong lighting on all surfaces (too expensive)

---

## Test Framework Status

### Completed ✅
- Basic test framework infrastructure
- 5 test scenes (sphere tessellation, cube grid, Gouraud artifacts, color banding, lighting)
- Performance benchmarking (FPS, triangle throughput)
- Image saving (PPM format)
- Integrated into build system (`make test-tinygl-*` targets)
- **NEW (2025-11-19): Pixel verification system**
  - Automated comparison against reference images
  - Multi-metric validation (PSNR, SSIM, max pixel diff)
  - Visual diff generation on test failures
  - Regression test mode with exit codes (--verify flag)
  - Reference image management with safeguards

### Test Verification Thresholds
- **PSNR** (Peak Signal-to-Noise Ratio): ≥30 dB (excellent match)
- **SSIM** (Structural Similarity Index): ≥0.95 (perceptually similar)
- **Max Pixel Difference**: ≤10/255 (no major errors)

### Usage
```bash
# Generate reference images (do this once, or after intentional changes)
make test-tinygl-reference

# Run tests with verification (returns exit code 0 or 1)
./build/bin/tinygl_test_runner --verify

# Verify specific test
./build/bin/tinygl_test_runner gouraud --verify

# Just render tests (no verification)
make test-tinygl-quality
```

### ✅ All Issues Resolved
- ✅ Tests now verify actual pixel content
- ✅ Tests fail when rendering breaks
- ✅ Automated visual regression testing enabled
- ✅ Visual diffs generated automatically on failures

---

## Performance Baseline

From testing (2025-11-19):
- **597 FPS** for 6,144 triangles (sphere subdivision 3)
- **~10,000 triangles/ms** throughput
- Excellent performance for software rendering on modern CPU

This is more than adequate for real-time G-code preview on embedded hardware.

---

## Conclusion

TinyGL's current performance and quality are **excellent for the G-code preview use case**. The ordered dithering work has been paused after discovering integration challenges. Future improvements should be driven by actual user-reported quality issues rather than speculative enhancements.

**Key Takeaway**: Don't fix what isn't broken. Profile before optimizing.
