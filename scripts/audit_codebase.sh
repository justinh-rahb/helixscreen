#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixScreen Codebase Audit Script
# Checks for violations of coding standards and best practices.
# Run periodically or in CI to catch regressions.
#
# Usage:
#   ./scripts/audit_codebase.sh [--strict]
#
# Exit codes:
#   0 = All checks passed (or only warnings)
#   1 = Critical violations found (with --strict, any warnings also fail)

set -euo pipefail

# Colors for output
RED='\033[0;31m'
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Parse arguments
STRICT_MODE=false
if [[ "${1:-}" == "--strict" ]]; then
    STRICT_MODE=true
fi

# Counters
ERRORS=0
WARNINGS=0

# Helper functions
error() {
    echo -e "${RED}ERROR:${NC} $1"
    ((ERRORS++)) || true
}

warning() {
    echo -e "${YELLOW}WARNING:${NC} $1"
    ((WARNINGS++)) || true
}

info() {
    echo -e "${CYAN}INFO:${NC} $1"
}

success() {
    echo -e "${GREEN}✓${NC} $1"
}

section() {
    echo ""
    echo -e "${CYAN}=== $1 ===${NC}"
}

# Change to repo root
cd "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"

echo "========================================"
echo "HelixScreen Codebase Audit"
echo "Date: $(date)"
echo "========================================"

#
# === P1: Memory Safety (Critical) ===
#
section "P1: Memory Safety (Critical)"

# Timer leak detection
timer_creates=$(grep -rn 'lv_timer_create' src/ --include='*.cpp' 2>/dev/null | wc -l | tr -d ' ')
timer_deletes=$(grep -rn 'lv_timer_del' src/ --include='*.cpp' 2>/dev/null | wc -l | tr -d ' ')
echo "Timer creates: $timer_creates"
echo "Timer deletes: $timer_deletes"

# Check for files with more creates than deletes (potential leaks)
echo ""
echo "Checking for unbalanced timer usage:"
unbalanced=0
for f in src/*.cpp; do
    [ -f "$f" ] || continue
    creates=$(grep -c "lv_timer_create" "$f" 2>/dev/null || echo 0)
    deletes=$(grep -c "lv_timer_del" "$f" 2>/dev/null || echo 0)
    if [ "$creates" -gt 0 ] && [ "$creates" -gt "$deletes" ]; then
        echo "  REVIEW: $f (creates: $creates, deletes: $deletes)"
        ((unbalanced++)) || true
    fi
done
if [ "$unbalanced" -eq 0 ]; then
    success "All timer usage appears balanced"
fi

#
# === P2: RAII Compliance ===
#
section "P2: RAII Compliance"

# Manual new/delete in UI files
manual_new=$(grep -rn '\bnew \w\+(' src/ui_*.cpp 2>/dev/null | grep -v 'make_unique\|placement' | wc -l | tr -d ' ')
manual_delete=$(grep -rn '^\s*delete ' src/ --include='*.cpp' 2>/dev/null | grep -v lib/ | wc -l | tr -d ' ')
lv_malloc_count=$(grep -rn 'lv_malloc' src/ --include='*.cpp' 2>/dev/null | grep -v lib/ | wc -l | tr -d ' ')

echo "Manual 'new' in UI code: $manual_new"
echo "Manual 'delete': $manual_delete"
echo "lv_malloc in src/: $lv_malloc_count"

# Thresholds (adjust based on migration progress)
MANUAL_NEW_THRESHOLD=20
MANUAL_DELETE_THRESHOLD=35
LV_MALLOC_THRESHOLD=5

if [ "$manual_new" -gt "$MANUAL_NEW_THRESHOLD" ]; then
    warning "Manual 'new' count ($manual_new) exceeds threshold ($MANUAL_NEW_THRESHOLD)"
fi
if [ "$manual_delete" -gt "$MANUAL_DELETE_THRESHOLD" ]; then
    warning "Manual 'delete' count ($manual_delete) exceeds threshold ($MANUAL_DELETE_THRESHOLD)"
fi
if [ "$lv_malloc_count" -gt "$LV_MALLOC_THRESHOLD" ]; then
    warning "lv_malloc count ($lv_malloc_count) exceeds threshold ($LV_MALLOC_THRESHOLD)"
fi

#
# === P3: Design Tokens ===
#
section "P3: XML Design Token Compliance"

hardcoded_padding=$(grep -rn 'style_pad[^=]*="[1-9]' ui_xml/ --include='*.xml' 2>/dev/null | wc -l | tr -d ' ')
hardcoded_margin=$(grep -rn 'style_margin[^=]*="[1-9]' ui_xml/ --include='*.xml' 2>/dev/null | wc -l | tr -d ' ')
hardcoded_gap=$(grep -rn 'style_gap[^=]*="[1-9]' ui_xml/ --include='*.xml' 2>/dev/null | wc -l | tr -d ' ')

echo "Hardcoded padding values: $hardcoded_padding"
echo "Hardcoded margin values: $hardcoded_margin"
echo "Hardcoded gap values: $hardcoded_gap"

HARDCODED_SPACING_THRESHOLD=100
total_hardcoded=$((hardcoded_padding + hardcoded_margin + hardcoded_gap))
if [ "$total_hardcoded" -gt "$HARDCODED_SPACING_THRESHOLD" ]; then
    warning "Hardcoded spacing values ($total_hardcoded) exceed threshold ($HARDCODED_SPACING_THRESHOLD)"
fi

#
# === P4: Declarative UI Compliance ===
#
section "P4: Declarative UI Compliance"

event_handlers=$(grep -rn 'lv_obj_add_event_cb' src/ui_*.cpp 2>/dev/null | wc -l | tr -d ' ')
text_updates=$(grep -rn 'lv_label_set_text' src/ui_*.cpp 2>/dev/null | wc -l | tr -d ' ')
visibility_toggles=$(grep -rn 'lv_obj_add_flag.*HIDDEN\|lv_obj_clear_flag.*HIDDEN' src/ui_*.cpp 2>/dev/null | wc -l | tr -d ' ')
inline_styles=$(grep -rn 'lv_obj_set_style_' src/ui_*.cpp 2>/dev/null | wc -l | tr -d ' ')

echo "Imperative event handlers: $event_handlers"
echo "Direct text updates: $text_updates"
echo "Visibility toggles: $visibility_toggles"
echo "Inline style setters: $inline_styles"

# Note: These have legitimate exceptions, so just report counts
EVENT_HANDLER_THRESHOLD=200
TEXT_UPDATE_THRESHOLD=200
VISIBILITY_THRESHOLD=150
INLINE_STYLE_THRESHOLD=600

if [ "$event_handlers" -gt "$EVENT_HANDLER_THRESHOLD" ]; then
    warning "Imperative event handlers ($event_handlers) exceed threshold ($EVENT_HANDLER_THRESHOLD)"
fi
if [ "$text_updates" -gt "$TEXT_UPDATE_THRESHOLD" ]; then
    warning "Direct text updates ($text_updates) exceed threshold ($TEXT_UPDATE_THRESHOLD)"
fi
if [ "$visibility_toggles" -gt "$VISIBILITY_THRESHOLD" ]; then
    warning "Visibility toggles ($visibility_toggles) exceed threshold ($VISIBILITY_THRESHOLD)"
fi
if [ "$inline_styles" -gt "$INLINE_STYLE_THRESHOLD" ]; then
    warning "Inline style setters ($inline_styles) exceed threshold ($INLINE_STYLE_THRESHOLD)"
fi

#
# === P5: Code Size ===
#
section "P5: Code Organization (File Size)"

MAX_LINES=2500
echo "Files exceeding $MAX_LINES lines:"
oversized=0
for f in src/ui_panel_*.cpp; do
    [ -f "$f" ] || continue
    lines=$(wc -l < "$f")
    if [ "$lines" -gt "$MAX_LINES" ]; then
        warning "$(basename "$f") has $lines lines (max: $MAX_LINES)"
        ((oversized++)) || true
    fi
done
if [ "$oversized" -eq 0 ]; then
    success "No files exceed $MAX_LINES lines"
fi

#
# === Hardcoded Colors in C++ ===
#
section "C++ Hardcoded Colors"

color_literals=$(grep -rn 'lv_color_hex\|lv_color_make' src/ui_*.cpp 2>/dev/null | grep -v 'theme\|parse' | wc -l | tr -d ' ')
echo "Hardcoded color literals: $color_literals"

COLOR_LITERAL_THRESHOLD=50
if [ "$color_literals" -gt "$COLOR_LITERAL_THRESHOLD" ]; then
    warning "Hardcoded color literals ($color_literals) exceed threshold ($COLOR_LITERAL_THRESHOLD)"
fi

#
# === Summary ===
#
section "Summary"

echo ""
echo "Errors:   $ERRORS"
echo "Warnings: $WARNINGS"
echo ""

if [ "$ERRORS" -gt 0 ]; then
    echo -e "${RED}AUDIT FAILED${NC} - $ERRORS critical error(s) found"
    exit 1
elif [ "$WARNINGS" -gt 0 ] && [ "$STRICT_MODE" = true ]; then
    echo -e "${YELLOW}AUDIT FAILED (strict mode)${NC} - $WARNINGS warning(s) found"
    exit 1
elif [ "$WARNINGS" -gt 0 ]; then
    echo -e "${YELLOW}AUDIT PASSED WITH WARNINGS${NC} - $WARNINGS warning(s) found"
    exit 0
else
    echo -e "${GREEN}AUDIT PASSED${NC} - No issues found"
    exit 0
fi
