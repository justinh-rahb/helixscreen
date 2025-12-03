// SPDX-License-Identifier: GPL-3.0-or-later
//
// HelixScreen - Display Backend Abstraction
//
// This module provides platform-independent display initialization,
// allowing HelixScreen to run on:
// - Desktop (SDL2) for development
// - Raspberry Pi (DRM/KMS or framebuffer)
// - Adventurer 5M (framebuffer)

#pragma once

#include <lvgl.h>
#include <memory>
#include <string>

/**
 * @brief Display backend types supported by HelixScreen
 */
enum class DisplayBackendType {
    SDL,    ///< SDL2 for desktop development (macOS/Linux with X11/Wayland)
    FBDEV,  ///< Linux framebuffer (/dev/fb0) - works on most embedded Linux
    DRM,    ///< Linux DRM/KMS - modern display API, better for Pi
    AUTO    ///< Auto-detect best available backend
};

/**
 * @brief Convert DisplayBackendType to string for logging
 */
inline const char* display_backend_type_to_string(DisplayBackendType type) {
    switch (type) {
        case DisplayBackendType::SDL:
            return "SDL";
        case DisplayBackendType::FBDEV:
            return "Framebuffer";
        case DisplayBackendType::DRM:
            return "DRM/KMS";
        case DisplayBackendType::AUTO:
            return "Auto";
        default:
            return "Unknown";
    }
}

/**
 * @brief Abstract display backend interface
 *
 * Provides platform-agnostic display and input initialization.
 * Follows the same factory pattern as WifiBackend.
 *
 * Lifecycle:
 * 1. Factory creates backend via DisplayBackend::create() or create_auto()
 * 2. Call create_display() to initialize display hardware
 * 3. Call create_input_pointer() to initialize touch/mouse input
 * 4. Optionally call create_input_keyboard() for keyboard support
 * 5. Backend is destroyed when unique_ptr goes out of scope
 *
 * Thread safety: Backend creation and destruction should be done from
 * the main thread. Display operations are typically single-threaded.
 */
class DisplayBackend {
  public:
    virtual ~DisplayBackend() = default;

    // ========================================================================
    // Display Creation
    // ========================================================================

    /**
     * @brief Initialize the display
     *
     * Creates the LVGL display object for this backend. This allocates
     * display buffers and initializes the underlying display hardware.
     *
     * @param width Display width in pixels
     * @param height Display height in pixels
     * @return LVGL display object, or nullptr on failure
     */
    virtual lv_display_t* create_display(int width, int height) = 0;

    // ========================================================================
    // Input Device Creation
    // ========================================================================

    /**
     * @brief Create pointer input device (mouse/touchscreen)
     *
     * Initializes the primary input device for the display.
     * For desktop: mouse input via SDL
     * For embedded: touchscreen via evdev
     *
     * @return LVGL input device, or nullptr on failure
     */
    virtual lv_indev_t* create_input_pointer() = 0;

    /**
     * @brief Create keyboard input device (optional)
     *
     * Not all backends support keyboard input. Returns nullptr
     * if keyboard is not available or not applicable.
     *
     * @return LVGL input device, or nullptr if not supported
     */
    virtual lv_indev_t* create_input_keyboard() { return nullptr; }

    // ========================================================================
    // Backend Information
    // ========================================================================

    /**
     * @brief Get the backend type
     */
    virtual DisplayBackendType type() const = 0;

    /**
     * @brief Get backend name for logging/display
     */
    virtual const char* name() const = 0;

    /**
     * @brief Check if this backend is available on the current system
     *
     * For SDL: checks if display can be opened
     * For FBDEV: checks if /dev/fb0 exists and is accessible
     * For DRM: checks if /dev/dri/card0 exists and is accessible
     *
     * @return true if backend can be used
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Check if the display is still active/owned by this process
     *
     * Used by the splash screen to detect when the main app takes over
     * the display. For framebuffer/DRM backends, this checks if another
     * process has opened the display device.
     *
     * @return true if display is still active, false if taken over
     */
    virtual bool is_active() const { return true; }

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a specific backend type
     *
     * @param type Backend type to create
     * @return Backend instance, or nullptr if type not available/compiled
     */
    static std::unique_ptr<DisplayBackend> create(DisplayBackendType type);

    /**
     * @brief Auto-detect and create the best available backend
     *
     * Detection order (first available wins):
     * 1. Check HELIX_DISPLAY_BACKEND environment variable override
     * 2. DRM (if compiled and /dev/dri/card0 accessible)
     * 3. Framebuffer (if compiled and /dev/fb0 accessible)
     * 4. SDL (fallback for desktop)
     *
     * @return Backend instance, or nullptr if no backend available
     */
    static std::unique_ptr<DisplayBackend> create_auto();

    /**
     * @brief Convenience: auto-detect and create backend
     *
     * Same as create_auto(), provided for simpler calling code.
     */
    static std::unique_ptr<DisplayBackend> create() { return create_auto(); }
};

// ============================================================================
// Backend-Specific Headers (conditionally included)
// ============================================================================

// These are only available when the corresponding backend is compiled in.
// Check with #ifdef HELIX_DISPLAY_SDL etc.

#ifdef HELIX_DISPLAY_SDL
#include "display_backend_sdl.h"
#endif

#ifdef HELIX_DISPLAY_FBDEV
#include "display_backend_fbdev.h"
#endif

#ifdef HELIX_DISPLAY_DRM
#include "display_backend_drm.h"
#endif
