# GuppyScreen Bed Mesh Renderer - Complete Documentation Index

## Document Overview

This directory contains a comprehensive analysis of the GuppyScreen bed mesh 3D visualization system. Two primary documents provide different perspectives on the implementation.

### Primary Documents

#### 1. **GUPPYSCREEN_BEDMESH_ANALYSIS.md** (829 lines)
**Complete technical deep-dive suitable for architects and algorithm enthusiasts**

- Executive summary and key achievements
- File locations with exact line numbers for all algorithms
- Complete data structure documentation with memory layouts
- 6-stage rendering pipeline flow diagram
- Detailed 3D math implementation:
  - Perspective projection with rotation matrices (lines 1016-1063, 48 LOC)
  - Scanline triangle rasterization (lines 1066-1127, 62 LOC)
  - Scanline gradient rasterization (lines 1130-1267, 138 LOC)
  - Quad generation with color assignment (lines 1270-1330, 61 LOC)
  - Depth sorting/painter's algorithm (lines 1333-1339, 7 LOC)
  - Quad rendering with culling (lines 1342-1392, 51 LOC)
- Color mapping strategy: scientific heat-map (lines 1593-1678, 85 LOC)
- LVGL canvas integration approach
- Complete public API surface documentation
- External dependencies analysis (minimal - LVGL 9.4 + stdlib only)
- 15 critical rendering constants explained
- 10 major architectural gotchas with detailed explanations
- Strengths/weaknesses analysis
- 10 recommended porting patterns for HelixScreen
- Performance characteristics and estimates

**Best for:**
- Understanding the complete 3D graphics pipeline
- Learning how perspective projection and depth sorting work
- Identifying algorithms suitable for adaptation/reuse
- Performance optimization decisions
- Making architectural choices for new implementations

#### 2. **BEDMESH_IMPLEMENTATION_PATTERNS.md** (515 lines)
**Practical code reference with ready-to-use templates**

- 10 complete code snippets covering major algorithms:
  1. Complete rendering pipeline orchestration
  2. Perspective projection template (LVGL-independent)
  3. Scanline triangle rasterization (optimized)
  4. Depth sorting implementation
  5. Heat-map color mapping (with LUT variant)
  6. Adaptive rendering strategy (quality vs performance)
  7. Interactive rotation control state machine
  8. Data structure definitions
  9. Bounding box culling for performance
  10. LVGL canvas integration patterns
- Quick formulas reference (perspective, Z-scale, color normalization, FOV)
- Common gotchas and their solutions (7 entries)

**Best for:**
- Copy-paste starting points for new implementations
- Quick reference when stuck on specific algorithms
- Understanding code patterns used in GuppyScreen
- Rapid prototyping of 3D mesh visualization

---

## Quick Navigation

### By Topic

#### 3D Graphics Fundamentals
- Perspective projection: See ANALYSIS.md "Perspective Projection (lines 1016-1063)"
- Triangle rasterization: See PATTERNS.md section 3
- Depth sorting: See PATTERNS.md section 4

#### Implementation Details
- Data structures: See ANALYSIS.md "Data Structures"
- Color mapping: See PATTERNS.md section 5
- LVGL integration: See PATTERNS.md section 10

#### Performance & Optimization
- See ANALYSIS.md "Performance Characteristics"
- Adaptive rendering: See PATTERNS.md section 6
- Culling: See PATTERNS.md section 9

#### Known Issues
- See ANALYSIS.md "Gotchas & Important Patterns"
- See PATTERNS.md "Common Implementation Gotchas"

### By Filename in GuppyScreen

| File | Lines | Key Content |
|------|-------|---|
| bedmesh_panel.h | 1-284 | All data structures and public API |
| bedmesh_panel.cpp | 1-200 | Constructor, UI setup, memory management |
| bedmesh_panel.cpp | 685-704 | Table cell colorization callback |
| bedmesh_panel.cpp | 706-739 | Canvas resize handler |
| bedmesh_panel.cpp | 743-832 | Surface/wireframe rendering (legacy) |
| bedmesh_panel.cpp | 834-1013 | Axes, labels, reference planes |
| bedmesh_panel.cpp | 1016-1063 | **Perspective projection** (core algorithm) |
| bedmesh_panel.cpp | 1066-1127 | **Triangle rasterization (solid)** (core algorithm) |
| bedmesh_panel.cpp | 1130-1267 | **Triangle rasterization (gradient)** (core algorithm) |
| bedmesh_panel.cpp | 1270-1330 | **Quad generation** (core algorithm) |
| bedmesh_panel.cpp | 1333-1339 | **Depth sorting** (core algorithm) |
| bedmesh_panel.cpp | 1342-1392 | **Quad rendering** (core algorithm) |
| bedmesh_panel.cpp | 1394-1491 | **Main 3D render loop** (orchestration) |
| bedmesh_panel.cpp | 1493-1575 | Zoom controls (interactive) |
| bedmesh_panel.cpp | 1578-1678 | **Color gradient mapping** (core algorithm) |
| bedmesh_panel.cpp | 1733-1810 | **Color legend band** (visualization) |
| bedmesh_panel.cpp | 1869-1891 | FOV scale calculation |
| bedmesh_panel.cpp | 1893+ | Drag/gesture handling (interactive) |

---

## Core Algorithm Summary Table

| Algorithm | Lines | Size | Complexity | Purpose |
|-----------|-------|------|-----------|---------|
| Perspective Projection | 1016-1063 | 48 LOC | O(n) vertices | Convert 3D→2D with perspective |
| Solid Triangle Raster | 1066-1127 | 62 LOC | O(pixels) | Fast fill for single color |
| Gradient Triangle Raster | 1130-1267 | 138 LOC | O(pixels × segments) | Quality fill with color interpolation |
| Quad Generation | 1270-1330 | 61 LOC | O(rows × cols) | Create 3D geometry from mesh |
| Depth Sorting | 1333-1339 | 7 LOC | O(n log n) | Sort for painter's algorithm |
| Quad Rendering | 1342-1392 | 51 LOC | O(1) per quad | Project and rasterize single quad |
| Color Gradient | 1593-1678 | 85 LOC | O(1) | Map height → RGB heat-map |
| Legend Band | 1733-1810 | 77 LOC | O(height) | Draw color scale visualization |

**Core algorithms (use these as starting points): Projection, Rasterization, Depth Sort, Color Mapping**

---

## Key Constants Reference

All in `/Users/pbrown/code/guppyscreen/src/bedmesh_panel.h`:

| Constant | Value | Impact | Tuneable |
|----------|-------|--------|----------|
| MESH_SCALE | 50.0 units | World spacing between mesh points | Yes |
| DEFAULT_Z_SCALE | 60.0 | Height amplification | Dynamic |
| DEFAULT_Z_TARGET_HEIGHT | 80.0 units | Target projected height | Yes |
| DEFAULT_Z_MIN_SCALE | 35.0 | Prevent flatness | Yes |
| DEFAULT_Z_MAX_SCALE | 120.0 | Prevent extreme projections | Yes |
| CAMERA_DISTANCE | 450.0 units | Virtual camera distance from origin | Yes |
| COLOR_COMPRESSION_FACTOR | 0.8 | **Dramatically affects contrast** | Yes |
| GRADIENT_MAX_SEGMENTS | 6 | Performance: max color segments per scanline | Yes |
| CULLING_MARGIN | 50 px | Off-screen triangle inclusion | Yes |
| Z_AXIS_EXTENSION | 0.75 | Extend axis beyond data range | Yes |

---

## Data Structure Reference

### Input (from Moonraker)
```cpp
std::vector<std::vector<double>> mesh;  // mesh[row][col] = Z height
int rows, cols;
double min_z, max_z;
```

### Core 3D Objects
```cpp
struct Point3D {
  double x, y, z;              // World 3D position
  int screen_x, screen_y;      // Projected 2D screen
  double depth;                // Z-distance for sorting
};

struct Vertex3D {
  double x, y, z;              // World 3D position
  lv_color_t color;            // Pre-computed color
};

struct Quad3D {
  Vertex3D vertices[4];        // 4 corners
  double avg_depth;            // For sorting
  lv_color_t center_color;     // Fallback color
};
```

### Color Representation
```cpp
lv_color_t    // LVGL's 24-bit RGB color
```

---

## External Dependencies

**MINIMAL FOOTPRINT:**
- ✓ LVGL 9.4 (canvas drawing only)
- ✓ C++ Standard Library (vector, algorithm, cmath, string)
- ✓ spdlog (logging, optional)
- ✗ NO OpenGL, Vulkan, Metal (software rasterization)
- ✗ NO math library (Eigen, GLM - custom 3D math)
- ✗ NO graphics library (no Assimp, no scene management)

**Significant difference from typical 3D rendering:** All mathematics hand-coded, all rasterization custom.

---

## Performance Characteristics

### Frame Rates (Estimated, SDL2 Desktop)
| Mesh Size | Quad Count | FPS | Rendering Type |
|-----------|-----------|-----|---|
| 10×10 | 81 | ~60 | Gradient |
| 20×20 | 361 | ~30-45 | Gradient |
| 50×50 | 2401 | ~10-15 | Gradient |

### Algorithmic Complexity
| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Projection | O(n) | n = vertex count |
| Sorting | O(n log n) | n ≈ (rows-1)×(cols-1) |
| Rasterization | O(pixels) | CPU-bound |
| Color interpolation | O(pixels × 6) | Max 6 segments per scanline |

### Optimization Opportunities
1. Reduce gradient segments during drag (currently 6)
2. Implement quad visibility culling
3. Cache projected vertices
4. Pre-compute color lookup tables
5. Use Structure of Arrays (SoA) instead of Array of Structures (AoS)

---

## When to Use This Analysis

### Use GuppyScreen's Approach If:
✓ Targeting embedded devices without GPU
✓ Using LVGL as your UI framework
✓ Need simple bed mesh visualization (< 30×30 points)
✓ Want minimal external dependencies
✓ Prioritize code simplicity

### Consider Alternatives If:
✗ Need large meshes (> 50×50) at 60 FPS
✗ Require per-pixel lighting or shadows
✗ Rendering multiple 3D objects
✗ Want professional 3D graphics quality
✗ Target platforms have GPU available

---

## How to Adapt This Code for HelixScreen

See ANALYSIS.md section "Recommended Porting Patterns for HelixScreen":

1. **Separate data from rendering** - Make MeshData independent of UI
2. **Extract motion state** - Create testable ViewState object
3. **Adapter pattern for colors** - Swap color schemes easily
4. **Strategy pattern for rasterization** - Choose quality vs speed
5. **Testable projection math** - Remove LVGL dependencies from core
6. **Replace magic constants** - Use RenderConfig structure
7. **Pre-compute tables** - Color lookup tables for performance
8. **Implement culling** - Only render visible quads
9. **Use SoA layout** - Improve cache efficiency
10. **Profile on hardware** - Don't trust desktop benchmarks

---

## Document Statistics

| Document | Lines | Sections | Code Blocks | Tables |
|----------|-------|----------|------------|--------|
| GUPPYSCREEN_BEDMESH_ANALYSIS.md | 829 | 27 | 85+ | 15+ |
| BEDMESH_IMPLEMENTATION_PATTERNS.md | 515 | 18 | 20 | 4 |
| **Total** | **1344** | **45** | **105+** | **19+** |

---

## Related GuppyScreen Documentation

In the original GuppyScreen repository:
- `docs/CONFIGURATION.md` - System configuration
- `docs/INSTALLATION.md` - Setup instructions
- UI research materials in `docs/ui-research/`

---

## Quick Start for Porting

**If you're adapting this for HelixScreen:**

1. **Start with** BEDMESH_IMPLEMENTATION_PATTERNS.md section 1 (complete pipeline)
2. **Study** ANALYSIS.md "3D Math Primitives" (perspective projection)
3. **Reference** ANALYSIS.md "Data Structures" for mesh representation
4. **Understand** ANALYSIS.md "Gotchas" before implementing
5. **Use** PATTERNS.md sections 3-5 for rasterization and color mapping
6. **Apply** ANALYSIS.md "Recommended Porting Patterns"

---

## File References

Original source code:
- `/Users/pbrown/code/guppyscreen/src/bedmesh_panel.h` (284 lines)
- `/Users/pbrown/code/guppyscreen/src/bedmesh_panel.cpp` (2250 lines)

This analysis:
- `/Users/pbrown/code/helixscreen/docs/GUPPYSCREEN_BEDMESH_ANALYSIS.md` (this directory)
- `/Users/pbrown/code/helixscreen/docs/BEDMESH_IMPLEMENTATION_PATTERNS.md` (this directory)
- `/Users/pbrown/code/helixscreen/docs/BEDMESH_RENDERER_INDEX.md` (this file)

---

## Questions & Clarifications

**Q: Why implement custom 3D rendering instead of using OpenGL?**
A: Embedded framebuffer displays don't have GPU access. Custom software rasterization runs on CPU.

**Q: What's the biggest performance bottleneck?**
A: Triangle rasterization - drawing millions of pixels per frame. Optimization: reduce gradient quality during interaction.

**Q: Can this handle large meshes?**
A: No - 50×50 is practical limit for interactive (60 FPS) rates. Beyond that requires optimization (culling, LOD, etc.).

**Q: Why use painter's algorithm instead of Z-buffer?**
A: Z-buffer requires per-pixel memory (expensive on embedded). Painter's algorithm only requires per-quad memory (small).

**Q: How does the color compression factor work?**
A: Smaller factor (0.8) uses narrower color range → more dramatic colors for small mesh variations. Tunable for contrast.

**Q: Can this render curved surfaces?**
A: No - mesh is flat-shaded quads. Add vertex normals and Gouraud shading for smoothness (not implemented).

---

Last updated: 2025-11-13
Analysis scope: GuppyScreen bed mesh renderer (bedmesh_panel.h/cpp)
Coverage: All core algorithms, data structures, LVGL integration, performance characteristics
