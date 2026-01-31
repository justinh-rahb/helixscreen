#!/bin/bash
# Copyright (C) 2025-2026 356C LLC
# SPDX-License-Identifier: GPL-3.0-or-later
# Initialize a git worktree for HelixScreen development
#
# Usage: ./scripts/init-worktree.sh <worktree-path>
#
# This script handles the git worktree + submodule dance:
# 1. Symlinks submodules from main repo (fast! no re-clone or re-patch)
# 2. Copies pre-built static libraries from main repo
# 3. Sets up npm/python dependencies
#
# Example:
#   ./scripts/init-worktree.sh ../helixscreen-feature-parity

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MAIN_REPO="$(dirname "$SCRIPT_DIR")"
WORKTREE_PATH="$1"

if [ -z "$WORKTREE_PATH" ]; then
    echo "Usage: $0 <worktree-path>"
    echo "Example: $0 ../helixscreen-my-feature"
    exit 1
fi

# Resolve to absolute path
WORKTREE_PATH="$(cd "$(dirname "$WORKTREE_PATH")" 2>/dev/null && pwd)/$(basename "$WORKTREE_PATH")"

if [ ! -d "$WORKTREE_PATH" ]; then
    echo "Error: Worktree path does not exist: $WORKTREE_PATH"
    echo "Create it first with: git worktree add -b <branch> $WORKTREE_PATH main"
    exit 1
fi

echo "Initializing worktree: $WORKTREE_PATH"
echo "Main repo: $MAIN_REPO"
echo ""

cd "$WORKTREE_PATH"

# Step 1: Verify main repo submodules are initialized and patched
echo "→ Checking main repo submodules..."
SUBMODULES="lib/lvgl lib/spdlog lib/libhv lib/glm lib/cpp-terminal lib/wpa_supplicant"
for sub in $SUBMODULES; do
    if [ ! -d "$MAIN_REPO/$sub/.git" ] && [ ! -f "$MAIN_REPO/$sub/.git" ]; then
        echo "  ✗ Main repo submodule not initialized: $sub"
        echo "    Run 'git submodule update --init' in main repo first."
        exit 1
    fi
done
echo "  ✓ All submodules present in main repo"

# Step 2: Symlink submodules from main repo (much faster than cloning)
# This works because submodules are read-only dependencies with patches already applied
echo "→ Symlinking submodules from main repo..."
for sub in $SUBMODULES; do
    # Clean up any existing submodule state
    git submodule deinit -f "$sub" 2>/dev/null || true
    rm -rf "$sub" 2>/dev/null || true
    # Create symlink to main repo's submodule
    ln -s "$MAIN_REPO/$sub" "$sub"
    echo "  ✓ $sub → main repo"
done

# Step 2b: Handle SDL2 (we use system SDL2, just need empty dir)
echo "→ Skipping SDL2 submodule (using system SDL2)..."
git submodule deinit lib/sdl2 2>/dev/null || true
rm -rf lib/sdl2 2>/dev/null || true
mkdir -p lib/sdl2

# Note: Patches are already applied in main repo, no need to re-apply

# Step 3: libhv headers are now available via symlink (no copy needed)

# Step 4: Copy pre-built static libraries (saves significant build time)
# Note: We exclude libTinyGL.a because it builds in-tree and may have architecture
# mismatches (e.g., copying macOS build to a worktree that will cross-compile for Pi).
# TinyGL will be built on first `make` with the correct architecture.
if ls "$MAIN_REPO/build/lib/"*.a 1>/dev/null 2>&1; then
    echo "→ Copying pre-built static libraries (excluding TinyGL)..."
    mkdir -p build/lib
    for lib in "$MAIN_REPO/build/lib/"*.a; do
        if [[ "$(basename "$lib")" != "libTinyGL.a" ]]; then
            cp "$lib" build/lib/
        fi
    done
    echo "  ✓ Copied $(ls build/lib/*.a 2>/dev/null | wc -l | tr -d ' ') libraries"
    echo "  ℹ TinyGL will be built on first 'make' (ensures correct architecture)"
fi

# Step 5: Run npm install for font tools (if npm available)
if command -v npm >/dev/null 2>&1; then
    echo "→ Installing npm packages..."
    npm install --silent 2>/dev/null
    if [ -x "node_modules/.bin/lv_font_conv" ]; then
        echo "  ✓ lv_font_conv installed"
    else
        echo "  ⚠ Warning: lv_font_conv not found after npm install"
    fi
else
    echo "  ⚠ Warning: npm not found - font regeneration will not work"
    echo "    Install Node.js: brew install node (macOS) or apt install nodejs npm (Linux)"
fi

# Step 6: Set up Python venv (if python3 available)
if command -v python3 >/dev/null 2>&1; then
    echo "→ Setting up Python virtual environment..."
    if [ ! -d ".venv" ]; then
        python3 -m venv .venv
    fi
    .venv/bin/pip install -q -r requirements.txt 2>/dev/null
    echo "  ✓ Python venv ready (activate with: source .venv/bin/activate)"
else
    echo "  ⚠ Warning: python3 not found - some scripts may not work"
fi

# Step 7: Generate compile_commands.json for LSP/clangd support
echo "→ Generating compile_commands.json for LSP support..."
if [ -f ".venv/bin/compiledb" ]; then
    .venv/bin/compiledb make -n -B all test-build 2>/dev/null
    echo "  ✓ compile_commands.json generated (LSP ready)"
elif command -v compiledb >/dev/null 2>&1; then
    compiledb make -n -B all test-build 2>/dev/null
    echo "  ✓ compile_commands.json generated (LSP ready)"
else
    echo "  ⚠ Warning: compiledb not found. Run 'make compile_commands' manually for LSP support."
fi

echo ""
echo "✓ Worktree initialized!"
echo ""
echo "Next steps:"
echo "  cd $WORKTREE_PATH"
echo "  make -j"
echo "  ./build/bin/helix-screen --test -vv"
