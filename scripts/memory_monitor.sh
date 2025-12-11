#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Background memory monitor for HelixScreen
# Usage: ./scripts/memory_monitor.sh [interval_sec] [output_file]
# Runs in background, polls memory every N seconds
# Output: CSV with timestamp,rss_kb,pss_kb,private_dirty_kb,anon_kb

INTERVAL="${1:-5}"
OUTPUT="${2:-/tmp/helix_memory_monitor.csv}"
PROCESS_NAME="helix-screen"

echo "Memory monitor started: interval=${INTERVAL}s output=${OUTPUT}"
echo "timestamp,rss_kb,pss_kb,private_dirty_kb,anon_kb" > "$OUTPUT"

while true; do
    # Find PID (may change if app restarts)
    PID=$(pidof "$PROCESS_NAME" 2>/dev/null)
    if [ -z "$PID" ]; then
        PID=$(pidof "helix-pi.screen" 2>/dev/null)
    fi

    if [ -n "$PID" ]; then
        TIMESTAMP=$(date +%s)

        # Use smaps_rollup for efficient aggregated stats (kernel 4.14+)
        if [ -f /proc/$PID/smaps_rollup ]; then
            STATS=$(awk '
                /^Rss:/           { rss=$2 }
                /^Pss:/           { pss=$2 }
                /^Private_Dirty:/ { pd+=$2 }
                /^Anonymous:/     { anon+=$2 }
                END { print rss","pss","pd","anon }
            ' /proc/$PID/smaps_rollup 2>/dev/null)
        else
            # Fallback: parse full smaps
            STATS=$(awk '
                /^Rss:/           { rss+=$2 }
                /^Pss:/           { pss+=$2 }
                /^Private_Dirty:/ { pd+=$2 }
                /^Anonymous:/     { anon+=$2 }
                END { print rss","pss","pd","anon }
            ' /proc/$PID/smaps 2>/dev/null)
        fi

        if [ -n "$STATS" ] && [ "$STATS" != ",,," ]; then
            echo "$TIMESTAMP,$STATS" >> "$OUTPUT"
        fi
    fi

    sleep "$INTERVAL"
done
