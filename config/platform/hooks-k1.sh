#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Platform hooks: Creality K1 / K1 Max
#
# K1 firmware variants run BusyBox SysV init and may have competing display
# daemons (e.g., display-server/Monitor) that must be stopped for exclusive
# framebuffer access.

detect_fb_resolution() {
    fb_size=""
    width=""
    height=""

    if [ -r /sys/class/graphics/fb0/virtual_size ]; then
        fb_size=$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null)
        width=${fb_size%%,*}
        height=${fb_size##*,}
        if [ -n "$width" ] && [ -n "$height" ]; then
            echo "${width}x${height}"
            return 0
        fi
    fi

    if command -v fbset >/dev/null 2>&1; then
        fb_size=$(fbset 2>/dev/null | sed -n 's/.*geometry[[:space:]]\+\([0-9]\+\)[[:space:]]\+\([0-9]\+\).*/\1x\2/p' | head -1)
        if [ -n "$fb_size" ]; then
            echo "$fb_size"
            return 0
        fi
    fi

    return 1
}

platform_stop_competing_uis() {
    # Stop known init scripts first when present.
    for initscript in /etc/init.d/S*display-server* /etc/init.d/S*Monitor* /etc/init.d/S*KlipperScreen*; do
        if [ -x "$initscript" ] 2>/dev/null; then
            echo "Stopping competing UI service: $initscript"
            "$initscript" stop 2>/dev/null || true
        fi
    done

    # Kill known display/UI processes.
    for proc in display-server Monitor monitor KlipperScreen klipperscreen guppyscreen GuppyScreen; do
        if command -v killall >/dev/null 2>&1; then
            killall "$proc" 2>/dev/null || true
        else
            for pid in $(pidof "$proc" 2>/dev/null); do
                kill "$pid" 2>/dev/null || true
            done
        fi
    done

    # Kill python-based KlipperScreen if running.
    # shellcheck disable=SC2009
    for pid in $(ps 2>/dev/null | grep -E 'python.*screen\.py' | grep -v grep | awk '{print $1}'); do
        kill "$pid" 2>/dev/null || true
    done

    sleep 1
}

platform_enable_backlight() {
    return 0
}

platform_wait_for_services() {
    return 0
}

platform_pre_start() {
    export HELIX_CACHE_DIR="/usr/data/helixscreen/cache"

    # Auto-preset screen size for Nebula Pad class devices (480x272).
    fb_res=""
    fb_res=$(detect_fb_resolution) || fb_res=""
    if [ "$fb_res" = "480x272" ]; then
        export HELIX_SCREEN_SIZE="micro"
        echo "Detected framebuffer ${fb_res} - applying HelixScreen MICRO preset"
    fi
}

platform_post_stop() {
    return 0
}
