#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# AFC live smoke test — queries all AFC objects and verifies expected fields exist.
# Usage: ./scripts/afc-test.sh [host:port]  (default: 192.168.1.112:7125)

set -euo pipefail

HOST="${1:-192.168.1.112:7125}"
BASE="http://${HOST}"
PASS=0
FAIL=0
WARN=0

green() { printf '\033[32m%s\033[0m\n' "$*"; }
red()   { printf '\033[31m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[33m%s\033[0m\n' "$*"; }

query() {
    local obj="$1"
    local encoded
    encoded=$(python3 -c "import urllib.parse; print(urllib.parse.quote('$obj'))")
    curl -sf "${BASE}/printer/objects/query?${encoded}" | python3 -c "
import sys, json
data = json.load(sys.stdin)
# Extract the object's status dict
status = data.get('result', {}).get('status', {})
obj_data = status.get('$obj', {})
json.dump(obj_data, sys.stdout, indent=2)
"
}

query_objects_list() {
    curl -sf "${BASE}/printer/objects/list" | python3 -c "
import sys, json
data = json.load(sys.stdin)
objects = data.get('result', {}).get('objects', [])
for obj in objects:
    print(obj)
"
}

check_fields() {
    local label="$1"
    local json_data="$2"
    shift 2
    local expected_fields=("$@")

    local present=()
    local missing=()
    local extra=()

    # Get actual keys from JSON
    local actual_keys
    actual_keys=$(echo "$json_data" | python3 -c "
import sys, json
data = json.load(sys.stdin)
for k in sorted(data.keys()):
    print(k)
" 2>/dev/null || true)

    # Check expected fields
    for field in "${expected_fields[@]}"; do
        if echo "$actual_keys" | grep -qx "$field"; then
            present+=("$field")
        else
            missing+=("$field")
        fi
    done

    # Find unexpected fields (potential drift)
    while IFS= read -r key; do
        [[ -z "$key" ]] && continue
        local found=false
        for field in "${expected_fields[@]}"; do
            if [[ "$key" == "$field" ]]; then
                found=true
                break
            fi
        done
        if ! $found; then
            extra+=("$key")
        fi
    done <<< "$actual_keys"

    # Report
    if [[ ${#missing[@]} -eq 0 ]]; then
        green "  ✓ ${label}: all ${#present[@]} expected fields present"
        ((PASS++))
    else
        red "  ✗ ${label}: missing fields: ${missing[*]}"
        ((FAIL++))
    fi
    if [[ ${#extra[@]} -gt 0 ]]; then
        yellow "  ⚠ ${label}: unknown fields (drift?): ${extra[*]}"
        ((WARN++))
    fi
}

echo "=== AFC Live Smoke Test ==="
echo "Host: ${HOST}"
echo ""

# 1. Discover AFC objects
echo "--- Discovering AFC objects ---"
ALL_OBJECTS=$(query_objects_list)
AFC_OBJECTS=$(echo "$ALL_OBJECTS" | grep -i "^AFC" || true)
echo "Found AFC objects:"
echo "$AFC_OBJECTS" | sed 's/^/  /'
echo ""

# 2. Check AFC global state
echo "--- AFC Global State ---"
AFC_STATE=$(query "AFC")
check_fields "AFC" "$AFC_STATE" \
    current_load current_lane next_lane current_state \
    current_toolchange number_of_toolchanges spoolman \
    error_state bypass_state quiet_mode position_saved \
    units lanes extruders hubs buffers message led_state

# 3. Check AFC steppers (lanes)
LANES=$(echo "$AFC_STATE" | python3 -c "
import sys, json
data = json.load(sys.stdin)
for lane in data.get('lanes', []):
    print(lane)
" 2>/dev/null || true)

echo ""
echo "--- AFC Steppers (Lanes) ---"
while IFS= read -r lane; do
    [[ -z "$lane" ]] && continue
    STEPPER_DATA=$(query "AFC_stepper ${lane}")
    check_fields "AFC_stepper ${lane}" "$STEPPER_DATA" \
        name unit hub extruder buffer buffer_status \
        lane map load prep tool_loaded loaded_to_hub \
        material spool_id color weight extruder_temp \
        runout_lane filament_status filament_status_led \
        status dist_hub
done <<< "$LANES"

# 4. Check AFC hubs
HUBS=$(echo "$AFC_STATE" | python3 -c "
import sys, json
data = json.load(sys.stdin)
for hub in data.get('hubs', []):
    print(hub)
" 2>/dev/null || true)

echo ""
echo "--- AFC Hubs ---"
while IFS= read -r hub; do
    [[ -z "$hub" ]] && continue
    HUB_DATA=$(query "AFC_hub ${hub}")
    check_fields "AFC_hub ${hub}" "$HUB_DATA" \
        state cut cut_cmd cut_dist cut_clear cut_min_length \
        cut_servo_pass_angle cut_servo_clip_angle cut_servo_prep_angle \
        lanes afc_bowden_length
done <<< "$HUBS"

# 5. Check AFC extruders
EXTRUDERS=$(echo "$AFC_STATE" | python3 -c "
import sys, json
data = json.load(sys.stdin)
for ext in data.get('extruders', []):
    print(ext)
" 2>/dev/null || true)

echo ""
echo "--- AFC Extruders ---"
while IFS= read -r ext; do
    [[ -z "$ext" ]] && continue
    EXT_DATA=$(query "AFC_extruder ${ext}")
    check_fields "AFC_extruder ${ext}" "$EXT_DATA" \
        tool_stn tool_stn_unload tool_sensor_after_extruder \
        tool_unload_speed tool_load_speed buffer lane_loaded \
        tool_start tool_start_status tool_end tool_end_status lanes
done <<< "$EXTRUDERS"

# 6. Check AFC buffers
BUFFERS=$(echo "$AFC_STATE" | python3 -c "
import sys, json
data = json.load(sys.stdin)
for buf in data.get('buffers', []):
    print(buf)
" 2>/dev/null || true)

echo ""
echo "--- AFC Buffers ---"
while IFS= read -r buf; do
    [[ -z "$buf" ]] && continue
    BUF_DATA=$(query "AFC_buffer ${buf}")
    check_fields "AFC_buffer ${buf}" "$BUF_DATA" \
        state lanes enabled
done <<< "$BUFFERS"

# Summary
echo ""
echo "=== Summary ==="
green "Passed: ${PASS}"
[[ $FAIL -gt 0 ]] && red "Failed: ${FAIL}" || echo "Failed: 0"
[[ $WARN -gt 0 ]] && yellow "Warnings: ${WARN}" || echo "Warnings: 0"
echo ""

[[ $FAIL -eq 0 ]] && green "All checks passed!" || { red "Some checks failed!"; exit 1; }
