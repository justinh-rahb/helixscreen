// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
//
// HelixScreen - Linux DRM/KMS Display Backend Implementation

#ifdef HELIX_DISPLAY_DRM

#include "display_backend_drm.h"

#include "config.h"
#include "display/lv_display_private.h"
#include "draw/sw/lv_draw_sw_utils.h"
#include "drm_rotation_strategy.h"

#include <spdlog/spdlog.h>

#include <lvgl.h>

// System includes for device access checks and DRM capability detection
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace {

/**
 * @brief Check if a DRM device supports dumb buffers and has a connected display
 *
 * Pi 5 has multiple DRM cards:
 * - card0: v3d (3D only, no display output)
 * - card1: drm-rp1-dsi (DSI touchscreen)
 * - card2: vc4-drm (HDMI output)
 *
 * We need to find one that supports dumb buffers for framebuffer allocation.
 */
bool drm_device_supports_display(const std::string& device_path) {
    int fd = open(device_path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    // Check for dumb buffer support
    uint64_t has_dumb = 0;
    if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
        close(fd);
        spdlog::debug("[DRM Backend] {}: no dumb buffer support", device_path);
        return false;
    }

    // Check if there's at least one connected connector
    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources) {
        close(fd);
        spdlog::debug("[DRM Backend] {}: failed to get DRM resources", device_path);
        return false;
    }

    bool has_connected = false;
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector* connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector) {
            if (connector->connection == DRM_MODE_CONNECTED) {
                has_connected = true;
                spdlog::debug("[DRM Backend] {}: found connected connector type {}", device_path,
                              connector->connector_type);
            }
            drmModeFreeConnector(connector);
            if (has_connected)
                break;
        }
    }

    drmModeFreeResources(resources);
    close(fd);

    if (!has_connected) {
        spdlog::debug("[DRM Backend] {}: no connected displays", device_path);
    }

    return has_connected;
}

/**
 * @brief Check if a path points to a valid DRM device (exists and responds to DRM ioctls)
 */
bool is_valid_drm_device(const std::string& path) {
    int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    drmVersionPtr version = drmGetVersion(fd);
    close(fd);
    if (!version) {
        return false;
    }
    drmFreeVersion(version);
    return true;
}

/**
 * @brief Auto-detect the best DRM device
 *
 * Priority order for device selection:
 * 1. Environment variable HELIX_DRM_DEVICE (for debugging/testing)
 * 2. Config file /display/drm_device (user preference)
 * 3. Auto-detection: scan /dev/dri/card* for first with dumb buffers + connected display
 *
 * Pi 5 has multiple DRM cards: card0 (v3d, 3D only), card1 (DSI), card2 (vc4/HDMI)
 */
std::string auto_detect_drm_device() {
    // Priority 1: Environment variable override (for debugging/testing)
    const char* env_device = std::getenv("HELIX_DRM_DEVICE");
    if (env_device && env_device[0] != '\0') {
        if (is_valid_drm_device(env_device)) {
            spdlog::info("[DRM Backend] Using DRM device from HELIX_DRM_DEVICE: {}", env_device);
            return env_device;
        }
        spdlog::warn("[DRM Backend] HELIX_DRM_DEVICE='{}' is not a valid DRM device, "
                     "falling back to auto-detection",
                     env_device);
    }

    // Priority 2: Config file override
    helix::Config* cfg = helix::Config::get_instance();
    std::string config_device = cfg->get<std::string>("/display/drm_device", "");
    if (!config_device.empty()) {
        if (is_valid_drm_device(config_device)) {
            spdlog::info("[DRM Backend] Using DRM device from config: {}", config_device);
            return config_device;
        }
        spdlog::warn("[DRM Backend] Config drm_device '{}' is not a valid DRM device, "
                     "falling back to auto-detection",
                     config_device);
    }

    // Priority 3: Auto-detection
    spdlog::info("[DRM Backend] Auto-detecting DRM device...");

    // Scan /dev/dri/card* in order
    DIR* dir = opendir("/dev/dri");
    if (!dir) {
        spdlog::info("[DRM Backend] /dev/dri not found, DRM not available");
        return {};
    }

    std::vector<std::string> candidates;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "card", 4) == 0) {
            candidates.push_back(std::string("/dev/dri/") + entry->d_name);
        }
    }
    closedir(dir);

    // Sort to ensure consistent order (card0, card1, card2...)
    std::sort(candidates.begin(), candidates.end());

    for (const auto& candidate : candidates) {
        spdlog::debug("[DRM Backend] Checking DRM device: {}", candidate);
        if (drm_device_supports_display(candidate)) {
            spdlog::info("[DRM Backend] Auto-detected DRM device: {}", candidate);
            return candidate;
        }
    }

    spdlog::info("[DRM Backend] No suitable DRM device found");
    return {};
}

} // namespace

DisplayBackendDRM::DisplayBackendDRM() : drm_device_(auto_detect_drm_device()) {}

DisplayBackendDRM::DisplayBackendDRM(const std::string& drm_device) : drm_device_(drm_device) {}

DisplayBackendDRM::~DisplayBackendDRM() {
    for (auto& buf : shadow_bufs_) {
        if (buf) {
            free(buf);
            buf = nullptr;
        }
    }
}

// Rotation-aware flush callback using shadow buffers.
// LVGL renders into cached system-memory shadow buffers (DIRECT mode).
// On the last flush, we rotate the full shadow buffer into the DRM dumb
// buffer (one uncached write pass) and page-flip.
void DisplayBackendDRM::rotation_flush_cb(lv_display_t* disp, const lv_area_t* area,
                                          uint8_t* px_map) {
    auto* self = static_cast<DisplayBackendDRM*>(lv_display_get_user_data(disp));
    if (!self || !self->original_flush_cb_) {
        lv_display_flush_ready(disp);
        return;
    }

    const lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    if (rotation != LV_DISPLAY_ROTATION_0 && lv_display_flush_is_last(disp) &&
        self->shadow_bufs_[0] != nullptr) {
        // Shadow buffer path: px_map is our cached shadow buffer.
        // Rotate the full frame into the DRM dumb buffer, then page-flip.
        const lv_color_format_t cf = lv_display_get_color_format(disp);
        const int32_t hor_res = lv_display_get_horizontal_resolution(disp);
        const int32_t ver_res = lv_display_get_vertical_resolution(disp);

        // Source dimensions = what LVGL rendered (logical/rotated coordinates).
        // lv_draw_sw_rotate uses these to compute destination positions.
        // For 180°: src and dest have the same dimensions.
        // For 90/270°: lv_draw_sw_rotate swaps dimensions internally, so
        //   src_w/src_h must be the LVGL logical resolution (hor_res x ver_res).
        const int32_t src_w = hor_res;
        const int32_t src_h = ver_res;

        const uint32_t drm_stride = lv_linux_drm_get_buf_stride(disp);

        // Source stride matches shadow buffer (allocated with drm_stride).
        // Dest stride matches DRM dumb buffer (same physical pitch).
        const uint32_t src_stride = drm_stride;
        const uint32_t dest_stride = drm_stride;

        // Get the back DRM buffer to write into
        uint8_t* drm_buf = lv_linux_drm_get_buf_map(disp, self->back_drm_buf_idx_);
        if (!drm_buf) {
            spdlog::error("[DRM Backend] Failed to get DRM buffer map for index {}",
                          self->back_drm_buf_idx_);
            lv_display_flush_ready(disp);
            return;
        }

        // Rotate: cached shadow (fast read) → DRM buffer (one uncached write pass)
        uint32_t t0 = lv_tick_get();
        lv_draw_sw_rotate(px_map, drm_buf, src_w, src_h, src_stride, dest_stride, rotation, cf);
        uint32_t t1 = lv_tick_get();

        // Performance sampling every 120 frames
        self->rotation_frame_count_++;
        self->rotation_time_accum_ms_ += (t1 - t0);
        if (self->rotation_frame_count_ >= 120) {
            spdlog::trace("[DRM Backend] Shadow rotation: {:.1f}ms avg over {} frames",
                          static_cast<float>(self->rotation_time_accum_ms_) /
                              static_cast<float>(self->rotation_frame_count_),
                          self->rotation_frame_count_);
            self->rotation_frame_count_ = 0;
            self->rotation_time_accum_ms_ = 0;
        }

        // Tell DRM driver to page-flip this buffer
        lv_linux_drm_set_active_buf(disp, self->back_drm_buf_idx_);
        self->back_drm_buf_idx_ ^= 1; // Alternate for next frame
    }

    // Call original DRM flush (page flip)
    self->original_flush_cb_(disp, area, px_map);
}

bool DisplayBackendDRM::is_available() const {
    if (drm_device_.empty()) {
        spdlog::debug("[DRM Backend] No DRM device configured");
        return false;
    }

    struct stat st;

    // Check if DRM device exists
    if (stat(drm_device_.c_str(), &st) != 0) {
        spdlog::debug("[DRM Backend] DRM device {} not found", drm_device_);
        return false;
    }

    // Check if we can access it
    if (access(drm_device_.c_str(), R_OK | W_OK) != 0) {
        spdlog::debug(
            "[DRM Backend] DRM device {} not accessible (need R/W permissions, check video group)",
            drm_device_);
        return false;
    }

    return true;
}

DetectedResolution DisplayBackendDRM::detect_resolution() const {
    int fd = open(drm_device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        spdlog::debug("[DRM Backend] Cannot open {} for resolution detection: {}", drm_device_,
                      strerror(errno));
        return {};
    }

    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources) {
        spdlog::debug("[DRM Backend] Failed to get DRM resources for resolution detection");
        close(fd);
        return {};
    }

    DetectedResolution result;

    // Find first connected connector and get its preferred mode
    for (int i = 0; i < resources->count_connectors && !result.valid; i++) {
        drmModeConnector* connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!connector) {
            continue;
        }

        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            // Find preferred mode, or use first mode as fallback
            drmModeModeInfo* preferred = nullptr;
            for (int m = 0; m < connector->count_modes; m++) {
                if (connector->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                    preferred = &connector->modes[m];
                    break;
                }
            }

            // Use preferred mode if found, otherwise first mode
            drmModeModeInfo* mode = preferred ? preferred : &connector->modes[0];
            result.width = static_cast<int>(mode->hdisplay);
            result.height = static_cast<int>(mode->vdisplay);
            result.valid = true;

            spdlog::info("[DRM Backend] Detected resolution: {}x{} ({})", result.width,
                         result.height, mode->name);
        }

        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);
    close(fd);

    if (!result.valid) {
        spdlog::debug("[DRM Backend] No connected display found for resolution detection");
    }

    return result;
}

lv_display_t* DisplayBackendDRM::create_display(int width, int height) {
    LV_UNUSED(width);
    LV_UNUSED(height);

    spdlog::info("[DRM Backend] Creating DRM display on {}", drm_device_);

    display_ = lv_linux_drm_create();
    if (display_ == nullptr) {
        spdlog::error("[DRM Backend] Failed to create DRM display");
        return nullptr;
    }

    lv_result_t result = lv_linux_drm_set_file(display_, drm_device_.c_str(), -1);
    if (result != LV_RESULT_OK) {
        spdlog::error("[DRM Backend] Failed to initialize DRM on {}", drm_device_);
        lv_display_delete(display_); // NOLINT(helix-shutdown) init error path, not shutdown
        display_ = nullptr;
        return nullptr;
    }

#ifdef HELIX_ENABLE_OPENGLES
    using_egl_ = true;
    spdlog::info("[DRM Backend] GPU-accelerated display active (EGL/OpenGL ES)");
#else
    spdlog::info("[DRM Backend] DRM display active (dumb buffers, CPU rendering)");
#endif

    return display_;
}

lv_indev_t* DisplayBackendDRM::create_input_pointer() {
    std::string device_override;

    // Priority 1: Environment variable override (for debugging/testing)
    const char* env_device = std::getenv("HELIX_TOUCH_DEVICE");
    if (env_device && env_device[0] != '\0') {
        device_override = env_device;
        spdlog::info("[DRM Backend] Using touch device from HELIX_TOUCH_DEVICE: {}",
                     device_override);
    }

    // Priority 2: Config file override
    if (device_override.empty()) {
        helix::Config* cfg = helix::Config::get_instance();
        device_override = cfg->get<std::string>("/input/touch_device", "");
        if (!device_override.empty()) {
            spdlog::info("[DRM Backend] Using touch device from config: {}", device_override);
        }
    }

    // If we have an explicit device, try it first
    if (!device_override.empty()) {
        pointer_ = lv_libinput_create(LV_INDEV_TYPE_POINTER, device_override.c_str());
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Libinput pointer device created on {}", device_override);
            return pointer_;
        }
        // Try evdev as fallback for the specified device
        pointer_ = lv_evdev_create(LV_INDEV_TYPE_POINTER, device_override.c_str());
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Evdev pointer device created on {}", device_override);
            return pointer_;
        }
        spdlog::warn("[DRM Backend] Could not open specified touch device: {}", device_override);
    }

    // Priority 3: Auto-discover using libinput
    // Look for touch or pointer capability devices
    spdlog::info("[DRM Backend] Auto-detecting touch/pointer device via libinput...");

    // Try to find a touch device first (for touchscreens like DSI displays)
    // Use evdev driver for touch devices — it supports multi-touch gesture
    // recognition (pinch-to-zoom) while the libinput driver does not.
    char* touch_path = lv_libinput_find_dev(LV_LIBINPUT_CAPABILITY_TOUCH, true);
    if (touch_path) {
        spdlog::info("[DRM Backend] Found touch device: {}", touch_path);
        pointer_ = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch_path);
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Evdev touch device created on {} (multi-touch enabled)",
                         touch_path);
#if LV_USE_GESTURE_RECOGNITION
            // Lower pinch thresholds so PINCH recognizes quickly, and disable
            // ROTATE by setting an unreachable threshold.  Without this, ROTATE
            // (default 0.2 rad) wins the race, resets PINCH's cumulative scale
            // to 1.0, and causes visible zoom jumps.
            lv_indev_set_pinch_up_threshold(pointer_, 1.15f);
            lv_indev_set_pinch_down_threshold(pointer_, 0.85f);
            lv_indev_set_rotation_rad_threshold(pointer_, 3.14f);
#endif
            return pointer_;
        }
        // Fall back to libinput if evdev fails
        pointer_ = lv_libinput_create(LV_INDEV_TYPE_POINTER, touch_path);
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Libinput touch device created on {}", touch_path);
            return pointer_;
        }
        spdlog::warn("[DRM Backend] Failed to create input device for: {}", touch_path);
    }

    // Try pointer devices (mouse, trackpad)
    char* pointer_path = lv_libinput_find_dev(LV_LIBINPUT_CAPABILITY_POINTER, false);
    if (pointer_path) {
        spdlog::info("[DRM Backend] Found pointer device: {}", pointer_path);
        pointer_ = lv_libinput_create(LV_INDEV_TYPE_POINTER, pointer_path);
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Libinput pointer device created on {}", pointer_path);
            return pointer_;
        }
        spdlog::warn("[DRM Backend] Failed to create libinput device for: {}", pointer_path);
    }

    // Priority 4: Fallback to evdev on common device paths
    spdlog::warn("[DRM Backend] Libinput auto-detection failed, trying evdev fallback");

    // Try event1 first (common for touchscreens on Pi)
    const char* fallback_devices[] = {"/dev/input/event1", "/dev/input/event0"};
    for (const char* dev : fallback_devices) {
        pointer_ = lv_evdev_create(LV_INDEV_TYPE_POINTER, dev);
        if (pointer_ != nullptr) {
            spdlog::info("[DRM Backend] Evdev pointer device created on {}", dev);
            return pointer_;
        }
    }

    spdlog::error("[DRM Backend] Failed to create any input device");
    return nullptr;
}

void DisplayBackendDRM::set_display_rotation(lv_display_rotation_t rot, int phys_w, int phys_h) {
    (void)phys_w;
    (void)phys_h;

    if (display_ == nullptr) {
        spdlog::warn("[DRM Backend] Cannot set rotation — display not created");
        return;
    }

    // Map LVGL rotation enum to DRM plane rotation constants
    uint64_t drm_rot = DRM_MODE_ROTATE_0;
    switch (rot) {
    case LV_DISPLAY_ROTATION_0:
        drm_rot = DRM_MODE_ROTATE_0;
        break;
    case LV_DISPLAY_ROTATION_90:
        drm_rot = DRM_MODE_ROTATE_90;
        break;
    case LV_DISPLAY_ROTATION_180:
        drm_rot = DRM_MODE_ROTATE_180;
        break;
    case LV_DISPLAY_ROTATION_270:
        drm_rot = DRM_MODE_ROTATE_270;
        break;
    }

    // Query hardware capabilities and choose strategy.
    // On EGL builds, lv_linux_drm_get_plane_rotation_mask() and
    // lv_linux_drm_set_rotation() do not exist (only in the dumb-buffer
    // driver), so force SOFTWARE fallback.
#ifdef HELIX_ENABLE_OPENGLES
    uint64_t supported_mask = 0;
#else
    uint64_t supported_mask = lv_linux_drm_get_plane_rotation_mask(display_);
#endif
    auto strategy = choose_drm_rotation_strategy(drm_rot, supported_mask);

    switch (strategy) {
    case DrmRotationStrategy::HARDWARE:
#ifndef HELIX_ENABLE_OPENGLES
        lv_linux_drm_set_rotation(display_, drm_rot);
        spdlog::info("[DRM Backend] Hardware plane rotation set to {}°",
                     static_cast<int>(rot) * 90);
#endif
        break;

    case DrmRotationStrategy::SOFTWARE: {
        // Shadow buffer rotation: LVGL renders into cached system memory,
        // flush callback rotates into DRM buffer for page-flip.
        // This avoids FULL render mode (which re-renders the entire screen
        // on every UI change) and halves DRM uncached memory access.

        // Get DRM buffer info for shadow buffer sizing
        uint32_t drm_stride = lv_linux_drm_get_buf_stride(display_);
        uint32_t drm_h = lv_display_get_vertical_resolution(display_);
        // For 180°, physical resolution == logical resolution (no swap)
        if (rot == LV_DISPLAY_ROTATION_90 || rot == LV_DISPLAY_ROTATION_270) {
            drm_h = lv_display_get_horizontal_resolution(display_);
        }
        size_t buf_size = static_cast<size_t>(drm_stride) * drm_h;

        // Allocate cached shadow buffers (page-aligned for potential future DMA).
        // aligned_alloc requires size to be a multiple of alignment (C11/C++17).
        constexpr size_t kAlignment = 4096;
        size_t alloc_size = (buf_size + kAlignment - 1) & ~(kAlignment - 1);
        for (int i = 0; i < 2; i++) {
            if (!shadow_bufs_[i]) {
                shadow_bufs_[i] = static_cast<uint8_t*>(aligned_alloc(kAlignment, alloc_size));
                if (!shadow_bufs_[i]) {
                    spdlog::error("[DRM Backend] Failed to allocate shadow buffer {} ({} bytes)", i,
                                  buf_size);
                    return;
                }
                memset(shadow_bufs_[i], 0, buf_size);
            }
        }
        shadow_buf_size_ = buf_size;
        back_drm_buf_idx_ = 0;

        // Tell LVGL to render into our cached shadow buffers (DIRECT mode)
        lv_display_set_buffers_with_stride(display_, shadow_bufs_[0], shadow_bufs_[1], buf_size,
                                           drm_stride, LV_DISPLAY_RENDER_MODE_DIRECT);

        // Set rotation so LVGL adjusts its coordinate system
        lv_display_set_rotation(display_, rot);

        // Install rotation-aware flush wrapper (only once)
        if (!original_flush_cb_) {
            original_flush_cb_ = display_->flush_cb;
            lv_display_set_user_data(display_, this);
            lv_display_set_flush_cb(display_, rotation_flush_cb);
        }

        spdlog::info("[DRM Backend] Shadow buffer rotation set to {}° "
                     "(cached shadow + DIRECT mode, plane supports 0x{:X})",
                     static_cast<int>(rot) * 90, supported_mask);
        break;
    }

    case DrmRotationStrategy::NONE:
#ifndef HELIX_ENABLE_OPENGLES
        // Explicitly reset in case a previous call set a non-zero rotation
        lv_linux_drm_set_rotation(display_, DRM_MODE_ROTATE_0);
#endif

        // Reset LVGL coordinate system to no rotation
        lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_0);

        // Restore original flush callback if we installed our wrapper
        if (original_flush_cb_) {
            lv_display_set_flush_cb(display_, original_flush_cb_);
            lv_display_set_user_data(display_, nullptr);
            original_flush_cb_ = nullptr;
        }
        // Restore DRM buffers before freeing shadow buffers (LVGL's buf_1/buf_2
        // still point to the shadow memory — must redirect first to avoid
        // use-after-free on next render).
        if (shadow_bufs_[0]) {
            uint8_t* drm_buf0 = lv_linux_drm_get_buf_map(display_, 0);
            uint8_t* drm_buf1 = lv_linux_drm_get_buf_map(display_, 1);
            uint32_t drm_stride = lv_linux_drm_get_buf_stride(display_);
            uint32_t drm_h = lv_display_get_vertical_resolution(display_);
            size_t drm_buf_size = static_cast<size_t>(drm_stride) * drm_h;
            lv_display_set_buffers_with_stride(display_, drm_buf0, drm_buf1, drm_buf_size,
                                               drm_stride, LV_DISPLAY_RENDER_MODE_DIRECT);
        }
        // Now safe to free shadow buffers
        for (auto& buf : shadow_bufs_) {
            if (buf) {
                free(buf);
                buf = nullptr;
            }
        }
        shadow_buf_size_ = 0;
        spdlog::debug("[DRM Backend] No rotation needed");
        break;
    }
}

bool DisplayBackendDRM::clear_framebuffer(uint32_t color) {
    // For DRM, we can try to clear via /dev/fb0 if it exists (legacy fbdev emulation)
    // Many DRM systems provide /dev/fb0 as a compatibility layer
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        spdlog::debug("[DRM Backend] Cannot open /dev/fb0 for clearing (DRM-only system)");
        return false;
    }

    // Get variable screen info
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        spdlog::warn("[DRM Backend] Cannot get vscreeninfo from /dev/fb0");
        close(fd);
        return false;
    }

    // Get fixed screen info
    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        spdlog::warn("[DRM Backend] Cannot get fscreeninfo from /dev/fb0");
        close(fd);
        return false;
    }

    // Calculate framebuffer size
    size_t screensize = finfo.smem_len;

    // Map framebuffer to memory
    void* fbp = mmap(nullptr, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fbp == MAP_FAILED) {
        spdlog::warn("[DRM Backend] Cannot mmap /dev/fb0 for clearing");
        close(fd);
        return false;
    }

    // Determine bytes per pixel from stride
    uint32_t bpp = 32;
    if (vinfo.xres > 0) {
        bpp = (finfo.line_length * 8) / vinfo.xres;
    }

    // Fill framebuffer with the specified color
    if (bpp == 32) {
        uint32_t* pixels = static_cast<uint32_t*>(fbp);
        size_t pixel_count = screensize / 4;
        for (size_t i = 0; i < pixel_count; i++) {
            pixels[i] = color;
        }
    } else if (bpp == 16) {
        uint16_t r = ((color >> 16) & 0xFF) >> 3;
        uint16_t g = ((color >> 8) & 0xFF) >> 2;
        uint16_t b = (color & 0xFF) >> 3;
        uint16_t rgb565 = (r << 11) | (g << 5) | b;

        uint16_t* pixels = static_cast<uint16_t*>(fbp);
        size_t pixel_count = screensize / 2;
        for (size_t i = 0; i < pixel_count; i++) {
            pixels[i] = rgb565;
        }
    } else {
        memset(fbp, 0, screensize);
    }

    spdlog::info("[DRM Backend] Cleared framebuffer via /dev/fb0 to 0x{:08X}", color);

    munmap(fbp, screensize);
    close(fd);
    return true;
}

#endif // HELIX_DISPLAY_DRM
