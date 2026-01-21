#!/bin/bash
# Copyright (C) 2025-2026 356C LLC
# SPDX-License-Identifier: GPL-3.0-or-later
# Initialize a git worktree for HelixScreen development
#
# Usage: ./scripts/init-worktree.sh <worktree-path>
#
# This script handles the git worktree + submodule dance:
# 1. Initializes all required submodules (skips SDL2 - uses system)
# 2. Copies generated libhv headers (they're built, not in repo)
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

# Step 1: Deinit SDL2 (we use system SDL2 on macOS, and the commit is stale)
echo "→ Skipping SDL2 submodule (using system SDL2)..."
git submodule deinit lib/sdl2 2>/dev/null || true
rm -rf lib/sdl2 2>/dev/null || true
mkdir -p lib/sdl2

# Step 2: Initialize required submodules
SUBMODULES="lib/lvgl lib/spdlog lib/libhv lib/glm lib/cpp-terminal lib/wpa_supplicant"
echo "→ Initializing submodules..."
for sub in $SUBMODULES; do
    echo "  - $sub"
    # Properly deinit before removing to clean up git metadata (fixes worktree issues)
    git submodule deinit -f "$sub" 2>/dev/null || true
    rm -rf "$sub" 2>/dev/null || true
    git submodule update --init --force --depth 1 "$sub"
done

# Step 2b: Apply required patches
echo "→ Applying required patches..."
make apply-patches 2>/dev/null || {
    echo "  ⚠ Warning: Could not apply patches. Run 'make apply-patches' manually."
}

# Step 3: Copy generated libhv headers (they're created during build, not in git)
echo "→ Copying libhv generated headers from main repo..."
if [ -d "$MAIN_REPO/lib/libhv/include/hv" ]; then
    mkdir -p lib/libhv/include
    cp -r "$MAIN_REPO/lib/libhv/include/hv" lib/libhv/include/
    echo "  ✓ Copied $(ls lib/libhv/include/hv | wc -l | tr -d ' ') headers"
else
    echo "  ⚠ Warning: Main repo libhv headers not found. Run 'make' in main repo first."
fi

# Step 4: Copy pre-built static libraries (saves significant build time)
if ls "$MAIN_REPO/build/lib/"*.a 1>/dev/null 2>&1; then
    echo "→ Copying pre-built static libraries..."
    mkdir -p build/lib
    cp "$MAIN_REPO/build/lib/"*.a build/lib/
    echo "  ✓ Copied $(ls build/lib/*.a 2>/dev/null | wc -l | tr -d ' ') libraries"
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
