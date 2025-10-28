# CI/CD Guide for HelixScreen

This guide documents the GitHub Actions CI/CD setup for the prototype-ui9 project, including patterns for subdirectory workflows, platform-specific builds, and quality checks.

## Table of Contents

- [Overview](#overview)
- [Subdirectory CI Patterns](#subdirectory-ci-patterns)
- [Platform-Specific Dependencies](#platform-specific-dependencies)
- [Dependency Build Order](#dependency-build-order)
- [Quality Checks](#quality-checks)
- [Workflow Structure](#workflow-structure)
- [Testing Locally](#testing-locally)
- [Troubleshooting](#troubleshooting)

## Overview

The prototype-ui9 project uses GitHub Actions for continuous integration across multiple platforms:

- **Build Workflow** (`prototype-ui9-build.yml`) - Multi-platform builds (Ubuntu, macOS)
- **Quality Workflow** (`prototype-ui9-quality.yml`) - Code quality, structure, and dependency validation

**Key Challenge:** This project lives in a subdirectory (`prototype-ui9/`) within the main guppyscreen repository, requiring special workflow configuration.

## Subdirectory CI Patterns

### Problem: Running CI for a Subdirectory Project

Most GitHub Actions examples assume your project is at the repository root. When your project is in a subdirectory, you need to adjust workflow configuration.

### Solution: `working-directory` and Path Filtering

```yaml
# .github/workflows/prototype-ui9-build.yml
on:
  push:
    branches: [ main, ui-redesign ]
    paths:
      - 'prototype-ui9/**'                    # Only trigger on subdirectory changes
      - '.github/workflows/prototype-ui9-*.yml'  # Also trigger on workflow changes

jobs:
  build-ubuntu:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: prototype-ui9      # All commands run in subdirectory
```

### Pattern: Artifact Paths Must Include Subdirectory

```yaml
- name: Upload binary
  uses: actions/upload-artifact@v4
  with:
    name: helix-ui-proto-ubuntu
    path: prototype-ui9/build/bin/helix-ui-proto  # Must include subdirectory prefix
```

**Why:** The `working-directory` only affects `run` commands, not `uses` actions. Actions always run from repository root.

### Benefits

✅ **Reduced CI usage** - Workflows only trigger when relevant files change
✅ **Faster feedback** - Parent repo changes don't trigger prototype-ui9 builds
✅ **Clear separation** - Each subdirectory can have its own CI strategy

## Platform-Specific Dependencies

### Problem: macOS System Fonts Don't Exist on Linux

Our initial CI failure:
```
lv_font_conv: error: Cannot read file "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
```

**Root cause:** Font generation scripts used macOS-specific paths:
```json
{
  "convert-arrows-64": "lv_font_conv --font '/System/Library/Fonts/Supplemental/Arial Unicode.ttf' ..."
}
```

### Solution 1: Commit Pre-Generated Platform-Specific Files

Arrow fonts are **already generated and committed** to the repository:
```bash
assets/fonts/arrows_32.c  # ✅ In git
assets/fonts/arrows_48.c  # ✅ In git
assets/fonts/arrows_64.c  # ✅ In git
```

Linux builds skip regeneration and use committed files.

### Solution 2: Platform-Conditional Generation

**Makefile pattern:**
```makefile
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM := macOS
else
    PLATFORM := Linux
endif

.fonts.stamp: package.json
	@if [ "$(PLATFORM)" = "macOS" ]; then \
		npm run convert-all-fonts; \
	else \
		npm run convert-fonts-ci; \  # Skips platform-specific fonts
	fi
```

**package.json pattern:**
```json
{
  "scripts": {
    "convert-all-fonts": "npm run convert-font-64 && ... && npm run convert-arrows-64 && ...",
    "convert-fonts-ci": "npm run convert-font-64 && ... (no arrows)"
  }
}
```

### Lessons Learned

⚠️ **Never assume platform-specific paths exist** on CI runners
✅ **Commit generated files** for platform-specific dependencies
✅ **Create CI-safe variants** of scripts that skip optional platform-specific steps
✅ **Use platform detection** in Makefiles to choose appropriate commands

### Other Platform Differences to Watch

| Aspect | macOS | Linux |
|--------|-------|-------|
| System fonts | `/System/Library/Fonts/` | `/usr/share/fonts/` (varies) |
| CPU cores | `sysctl -n hw.ncpu` | `nproc` |
| Package manager | `brew` | `apt` / `dnf` / `pacman` |
| Library paths | `/usr/local/lib` | `/usr/lib` |
| Compiler | Apple Clang | GNU GCC / LLVM Clang |

## Dependency Build Order

### Problem: Missing Header Files

Initial CI error:
```
include/config.h:7:10: fatal error: 'hv/json.hpp' file not found
```

**Root cause:** Main project tried to compile before libhv was built.

### Solution: Build Dependencies First

```yaml
- name: Build libhv
  run: |
    cd ../libhv
    ./configure --with-openssl=no
    make -j$(nproc)

- name: Build project
  run: make -j$(nproc)
```

### Pattern: Symlinked Submodule Dependencies

The prototype-ui9 project uses **symlinks** to parent repo submodules:
```
prototype-ui9/
├── libhv -> ../libhv           # Symlink to parent repo submodule
├── spdlog -> ../spdlog         # Symlink to parent repo submodule
└── lvgl/                       # Real submodule in prototype-ui9
```

**CI workflow must:**
1. Checkout with `submodules: recursive` to get all submodules
2. Build parent repo dependencies (libhv) before building prototype-ui9
3. Use `working-directory` to navigate relative paths (`cd ../libhv`)

### Lessons Learned

⚠️ **Symlinked dependencies aren't automatically built** by GitHub Actions
✅ **Build dependencies explicitly** before main project
✅ **Check dependency status** in CI logs (e.g., "⚠ libhv not built")
✅ **Use relative paths** in CI to navigate to symlinked dependencies

## Quality Checks

### Problem: False Positives in Copyright Header Checks

Initial CI failure:
```
⚠️  Missing copyright header: src/test_dynamic_cards.cpp
⚠️  Missing copyright header: include/helix_icon_data.h
```

**Root cause:** Test files and auto-generated files don't need copyright headers.

### Solution: Exclude Auto-Generated and Test Files

```yaml
- name: Check C++ copyright headers
  run: |
    for file in src/*.cpp include/*.h; do
      basename=$(basename "$file")

      # Skip test files and generated files
      if [[ "$basename" == test_*.cpp ]] || [[ "$basename" == *_data.h ]]; then
        echo "⏭️  Skipping: $file"
        continue
      fi

      if ! head -n 5 "$file" | grep -q "Copyright"; then
        echo "⚠️  Missing copyright header: $file"
        MISSING=$((MISSING + 1))
      fi
    done
```

### Problem: ASCII Files Reported as Not UTF-8

Initial CI failure:
```
❌ ui_xml/advanced_panel.xml is not UTF-8 encoded
```

**Root cause:** The `file` command reports ASCII files as "ASCII text", not "UTF-8 text", even though ASCII is a valid subset of UTF-8.

### Solution: Accept Both UTF-8 and ASCII

```yaml
- name: Check XML files
  run: |
    for xml in ui_xml/*.xml; do
      if ! file "$xml" | grep -qE "UTF-8|ASCII"; then
        echo "❌ $xml is not UTF-8/ASCII encoded"
        exit 1
      fi
    done
```

### Quality Check Patterns

| Check Type | Pattern to Exclude | Example |
|------------|-------------------|---------|
| Copyright headers | `test_*.cpp` | Test files |
| Copyright headers | `*_data.h` | Auto-generated files |
| Copyright headers | `*.min.js` | Minified files |
| Linting | `vendor/*` | Third-party code |
| Encoding | Accept "ASCII" as valid | ASCII ⊂ UTF-8 |

### Lessons Learned

⚠️ **Not all source files need copyright headers** (tests, generated files)
✅ **Use pattern matching** to exclude special files (`test_*.cpp`, `*_data.h`)
✅ **ASCII is valid UTF-8** - accept both in encoding checks
✅ **Document exclusion patterns** in workflow for maintainability

## Workflow Structure

### Recommended Structure: Separate Build and Quality Workflows

**Why separate?**
- **Faster feedback** - Quality checks (1-2 min) complete before builds (5-10 min)
- **Clear failure reasons** - Know immediately if it's a quality issue or build failure
- **Parallelization** - Both run simultaneously

### Build Workflow Structure

```yaml
jobs:
  build-ubuntu:     # Ubuntu build job
  build-macos:      # macOS build job
  build-status:     # Summary job (runs after both)
```

**Key points:**
- Multi-platform matrix builds run in parallel
- Each platform uploads its binary as artifact
- Summary job aggregates results and fails if any build fails

### Quality Workflow Structure

```yaml
jobs:
  dependency-check:    # Verify submodules, Makefile, scripts
  project-structure:   # Verify required files exist
  copyright-headers:   # Validate GPL v3 headers
```

**Key points:**
- Independent checks run in parallel
- Fast validation (no compilation)
- Clear, actionable error messages

### Artifact Retention

```yaml
- name: Upload binary
  uses: actions/upload-artifact@v4
  with:
    retention-days: 7  # Good default for dev builds
```

**Recommendations:**
- **7 days** - Development branches
- **30 days** - Release candidates
- **90 days** - Tagged releases

## Testing Locally

Before pushing, test CI-like builds locally to catch issues early.

### Simulate Ubuntu CI Build

```bash
cd prototype-ui9

# Clean state
make clean
rm -rf ../libhv/lib

# Build dependencies (as CI does)
cd ../libhv
./configure --with-openssl=no
make -j$(nproc)

# Build project
cd ../prototype-ui9
make -j$(nproc)

# Verify binary
./build/bin/helix-ui-proto --help
```

### Simulate macOS CI Build

```bash
cd prototype-ui9

# Clean state
make clean
rm -rf ../libhv/lib

# Build dependencies
cd ../libhv
./configure --with-openssl=no
make -j$(sysctl -n hw.ncpu)

# Build project
cd ../prototype-ui9
make -j$(sysctl -n hw.ncpu)

# Verify binary
./build/bin/helix-ui-proto --help
```

### Run Quality Checks Locally

```bash
cd prototype-ui9

# Check dependencies
make check-deps

# Validate XML encoding
for xml in ui_xml/*.xml; do
  file "$xml" | grep -qE "UTF-8|ASCII" || echo "❌ $xml"
done

# Check copyright headers (excluding test files)
for file in src/*.cpp include/*.h; do
  basename=$(basename "$file")
  if [[ "$basename" != test_*.cpp ]] && [[ "$basename" != *_data.h ]]; then
    head -n 5 "$file" | grep -q "Copyright" || echo "⚠️  $file"
  fi
done
```

### Docker-Based CI Testing (Advanced)

To exactly match CI environment:

```bash
# Ubuntu 22.04 (matches ubuntu-latest)
docker run -it --rm -v "$PWD:/work" -w /work ubuntu:22.04 bash

# Inside container
apt update
apt install -y libsdl2-dev clang make python3 npm git
cd prototype-ui9
# ... build steps ...
```

## Troubleshooting

### Workflow Not Triggering

**Symptom:** Push to `ui-redesign` doesn't trigger workflow.

**Check:**
```yaml
on:
  push:
    branches: [ main, ui-redesign ]  # Is your branch listed?
    paths:
      - 'prototype-ui9/**'            # Did you modify files in this path?
```

**Debug:** Look at "Actions" tab → "All workflows" (not just "Active workflows").

### Build Fails: "file not found"

**Symptom:**
```
fatal error: 'hv/json.hpp' file not found
```

**Solution:** Add libhv build step **before** main build:
```yaml
- name: Build libhv
  run: |
    cd ../libhv
    ./configure --with-openssl=no
    make -j$(nproc)
```

### Font Generation Fails on Linux

**Symptom:**
```
Cannot read file "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"
```

**Solution:** Use platform-conditional font generation (see [Platform-Specific Dependencies](#platform-specific-dependencies)).

### Artifacts Not Found

**Symptom:** Artifact upload succeeds but download gives "artifact not found".

**Solution:** Artifact paths must include subdirectory when using `working-directory`:
```yaml
# ❌ Wrong (relative to working-directory)
path: build/bin/helix-ui-proto

# ✅ Correct (relative to repo root)
path: prototype-ui9/build/bin/helix-ui-proto
```

### Copyright Check Fails on Test Files

**Symptom:**
```
⚠️  Missing copyright header: src/test_dynamic_cards.cpp
```

**Solution:** Exclude test files and generated files (see [Quality Checks](#quality-checks)).

### Workflow Runs on Unrelated Changes

**Symptom:** Workflow triggers when only parent repo files change.

**Solution:** Add `paths` filter:
```yaml
on:
  push:
    paths:
      - 'prototype-ui9/**'
      - '.github/workflows/prototype-ui9-*.yml'
```

## Summary: CI/CD Best Practices

✅ **Path filtering** - Only trigger workflows when relevant files change
✅ **Platform detection** - Handle platform-specific dependencies gracefully
✅ **Build order** - Build dependencies before main project
✅ **Exclude special files** - Skip test files and generated files in quality checks
✅ **Clear failure messages** - Include helpful context in error output
✅ **Test locally** - Simulate CI builds before pushing
✅ **Separate workflows** - Build and quality checks in parallel
✅ **Document exceptions** - Explain why files are excluded from checks

## See Also

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [CLAUDE.md](../CLAUDE.md) - Project context for AI assistants
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Common development patterns
- [COPYRIGHT_HEADERS.md](COPYRIGHT_HEADERS.md) - GPL v3 header templates
