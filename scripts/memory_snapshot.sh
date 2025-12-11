#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Memory snapshot script for HelixScreen
# Usage: ./scripts/memory_snapshot.sh [label]
# Outputs: timestamp,label,rss_kb,hwm_kb,vsz_kb,private_dirty_kb,anon_kb

LABEL="${1:-snapshot}"
PROCESS_NAME="helix-screen"

# Find PID
PID=$(pidof "$PROCESS_NAME" 2>/dev/null)
if [ -z "$PID" ]; then
    # Try alternate name
    PID=$(pidof "helix-pi.screen" 2>/dev/null)
fi

if [ -z "$PID" ]; then
    echo "ERROR: $PROCESS_NAME not running" >&2
    exit 1
fi

TIMESTAMP=$(date +%s)

# Read /proc/[pid]/status for RSS, HWM, VSZ
STATUS=$(awk '
    /^VmRSS:/  { rss=$2 }
    /^VmHWM:/  { hwm=$2 }
    /^VmSize:/ { vsz=$2 }
    END { print rss","hwm","vsz }
' /proc/$PID/status 2>/dev/null)

# Read /proc/[pid]/smaps_rollup for Private_Dirty and Anonymous
# smaps_rollup is more efficient than parsing full smaps
if [ -f /proc/$PID/smaps_rollup ]; then
    SMAPS=$(awk '
        /^Private_Dirty:/ { pd+=$2 }
        /^Anonymous:/     { anon+=$2 }
        END { print pd","anon }
    ' /proc/$PID/smaps_rollup 2>/dev/null)
else
    # Fallback: parse full smaps (slower but works on older kernels)
    SMAPS=$(awk '
        /^Private_Dirty:/ { pd+=$2 }
        /^Anonymous:/     { anon+=$2 }
        END { print pd","anon }
    ' /proc/$PID/smaps 2>/dev/null)
fi

# Output CSV line
echo "$TIMESTAMP,$LABEL,$STATUS,$SMAPS"
