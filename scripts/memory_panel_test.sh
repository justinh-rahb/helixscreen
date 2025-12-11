#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Automated panel memory test for HelixScreen
# Opens each panel and records memory usage
# Usage: ./scripts/memory_panel_test.sh [binary_path]

BINARY="${1:-./build/bin/helix-screen}"
if [ ! -f "$BINARY" ]; then
    BINARY="./helix-screen"
fi
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found. Pass path as argument." >&2
    exit 1
fi

OUTPUT="/tmp/helix_panel_memory.csv"
SCRIPT_DIR="$(dirname "$0")"

# All main panels (excluding overlays which need manual nav)
MAIN_PANELS=(
    "home"
    "controls"
    "filament"
    "settings"
    "advanced"
    "print-select"
)

# Overlay panels (opened via -p flag with auto-open)
OVERLAY_PANELS=(
    "motion"
    "nozzle-temp"
    "bed-temp"
    "extrusion"
    "fan"
    "bed-mesh"
    "zoffset"
    "pid"
)

echo "=========================================="
echo "HelixScreen Memory Panel Test"
echo "Binary: $BINARY"
echo "Output: $OUTPUT"
echo "=========================================="

# Write CSV header
echo "timestamp,panel,rss_kb,hwm_kb,vsz_kb,private_dirty_kb,anon_kb" > "$OUTPUT"

get_memory() {
    local pid=$1
    local label=$2

    if [ ! -d /proc/$pid ]; then
        return 1
    fi

    local timestamp=$(date +%s)

    # Read status
    local status=$(awk '
        /^VmRSS:/  { rss=$2 }
        /^VmHWM:/  { hwm=$2 }
        /^VmSize:/ { vsz=$2 }
        END { print rss","hwm","vsz }
    ' /proc/$pid/status 2>/dev/null)

    # Read smaps_rollup
    local smaps=""
    if [ -f /proc/$pid/smaps_rollup ]; then
        smaps=$(awk '
            /^Private_Dirty:/ { pd+=$2 }
            /^Anonymous:/     { anon+=$2 }
            END { print pd","anon }
        ' /proc/$pid/smaps_rollup 2>/dev/null)
    fi

    echo "$timestamp,$label,$status,$smaps" >> "$OUTPUT"
    echo "  $label: RSS=$(echo $status | cut -d, -f1)KB"
}

# Test each main panel
echo ""
echo "Testing main panels..."
for panel in "${MAIN_PANELS[@]}"; do
    echo "→ $panel"

    # Launch with test mode, skip splash, timeout after 4 seconds
    $BINARY --test --skip-splash -p "$panel" --timeout 4 &
    PID=$!

    # Wait for UI init
    sleep 2

    # Capture memory
    get_memory $PID "$panel"

    # Wait for timeout/exit
    wait $PID 2>/dev/null
done

# Test overlay panels (these auto-open from controls)
echo ""
echo "Testing overlay panels..."
for panel in "${OVERLAY_PANELS[@]}"; do
    echo "→ $panel"

    # Launch with test mode, targeting overlay
    $BINARY --test --skip-splash -p "$panel" --timeout 4 &
    PID=$!

    # Wait for UI init
    sleep 2

    # Capture memory
    get_memory $PID "$panel"

    # Wait for timeout/exit
    wait $PID 2>/dev/null
done

echo ""
echo "=========================================="
echo "Test complete! Results saved to: $OUTPUT"
echo ""

# Print summary
echo "Summary:"
echo "--------"
awk -F, 'NR>1 {
    printf "  %-15s RSS: %6d KB  HWM: %6d KB\n", $2, $3, $4
}' "$OUTPUT"

# Calculate stats
echo ""
awk -F, 'NR>1 {
    sum+=$3; count++;
    if($3>max) max=$3;
    if(min=="" || $3<min) min=$3
}
END {
    printf "Min RSS:  %d KB\n", min
    printf "Max RSS:  %d KB\n", max
    printf "Avg RSS:  %d KB\n", sum/count
}' "$OUTPUT"
