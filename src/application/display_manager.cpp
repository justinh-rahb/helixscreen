// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file display_manager.cpp
 * @brief LVGL display and input device lifecycle management
 *
 * @pattern Manager wrapping DisplayBackend with RAII lifecycle
 * @threading Main thread only
 * @gotchas NEVER call lv_display_delete/lv_group_delete manually - lv_deinit() handles all cleanup
 *
 * @see application.cpp
 */

#include "display_manager.h"

#include "ui_fatal_error.h"
#include "ui_update_queue.h"

#include "config.h"
#include "settings_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>

#ifdef HELIX_DISPLAY_SDL
#include <SDL.h>
#endif

#ifndef HELIX_DISPLAY_SDL
#include <time.h>
#endif

// Static instance pointer for global access (e.g., from print_completion)
static DisplayManager* s_instance = nullptr;

DisplayManager::DisplayManager() = default;

DisplayManager::~DisplayManager() {
    shutdown();
}

bool DisplayManager::init(const Config& config) {
    if (m_initialized) {
        spdlog::warn("[DisplayManager] Already initialized, call shutdown() first");
        return false;
    }

    m_width = config.width;
    m_height = config.height;

    // Initialize LVGL library
    lv_init();

    // Create display backend (auto-detects: DRM → framebuffer → SDL)
    m_backend = DisplayBackend::create_auto();
    if (!m_backend) {
        spdlog::error("[DisplayManager] No display backend available");
        lv_deinit();
        return false;
    }

    spdlog::info("[DisplayManager] Using backend: {}", m_backend->name());

    // Create LVGL display
    m_display = m_backend->create_display(m_width, m_height);
    if (!m_display) {
        spdlog::error("[DisplayManager] Failed to create display");
        m_backend.reset();
        lv_deinit();
        return false;
    }

    // Initialize UI update queue for thread-safe async updates
    // Must be done AFTER display is created - registers LV_EVENT_REFR_START handler
    ui_update_queue_init();

    // Create pointer input device (mouse/touch)
    m_pointer = m_backend->create_input_pointer();
    if (!m_pointer) {
#if defined(HELIX_DISPLAY_DRM) || defined(HELIX_DISPLAY_FBDEV)
        if (config.require_pointer) {
            // On embedded platforms, no input device is fatal
            spdlog::error("[DisplayManager] No input device found - cannot operate touchscreen UI");

            static const char* suggestions[] = {
                "Check /dev/input/event* devices exist",
                "Ensure user is in 'input' group: sudo usermod -aG input $USER",
                "Check touchscreen driver is loaded: dmesg | grep -i touch",
                "Set HELIX_TOUCH_DEVICE=/dev/input/eventX to override",
                "Add \"touch_device\": \"/dev/input/event1\" to helixconfig.json",
                nullptr};

            ui_show_fatal_error("No Input Device",
                                "Could not find or open a touch/pointer input device.\n"
                                "The UI requires an input device to function.",
                                suggestions, 30000);

            m_backend.reset();
            lv_deinit();
            return false;
        }
#else
        // On desktop (SDL), continue without pointer - mouse is optional
        spdlog::warn("[DisplayManager] No pointer input device created - touch/mouse disabled");
#endif
    }

    // Configure scroll behavior
    if (m_pointer) {
        configure_scroll(config.scroll_throw, config.scroll_limit);
    }

    // Create keyboard input device (optional)
    m_keyboard = m_backend->create_input_keyboard();
    if (m_keyboard) {
        setup_keyboard_group();
        spdlog::debug("[DisplayManager] Physical keyboard input enabled");
    }

    // Create backlight backend (auto-detects hardware)
    m_backlight = BacklightBackend::create();
    spdlog::info("[DisplayManager] Backlight: {} (available: {})", m_backlight->name(),
                 m_backlight->is_available());

    // Load dim settings from config
    ::Config* cfg = ::Config::get_instance();
    m_dim_timeout_sec = cfg->get<int>("/display_dim_sec", 300);
    m_dim_brightness_percent = std::clamp(cfg->get<int>("/display_dim_brightness", 30), 1, 100);
    spdlog::info("[DisplayManager] Display dim: {}s timeout, {}% brightness", m_dim_timeout_sec,
                 m_dim_brightness_percent);

    spdlog::debug("[DisplayManager] Initialized: {}x{}", m_width, m_height);
    m_initialized = true;
    s_instance = this;
    return true;
}

DisplayManager* DisplayManager::instance() {
    return s_instance;
}

void DisplayManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    s_instance = nullptr;
    spdlog::debug("[DisplayManager] Shutting down");

    // NOTE: We do NOT call lv_group_delete(m_input_group) here because:
    // 1. Objects in the group may already be freed (panels deleted before display)
    // 2. lv_deinit() calls lv_group_deinit() which safely clears the group list
    // 3. lv_group_delete() iterates objects and would crash on dangling pointers
    m_input_group = nullptr;

    // Reset input device pointers (LVGL manages their memory)
    m_keyboard = nullptr;
    m_pointer = nullptr;

    // NOTE: We do NOT call lv_display_delete() here because:
    // lv_deinit() iterates all displays and deletes them.
    // Manually deleting first causes double-free crash.
    m_display = nullptr;

    // Release backends
    m_backlight.reset();
    m_backend.reset();

    // Shutdown UI update queue before LVGL
    ui_update_queue_shutdown();

    // Deinitialize LVGL (guard against static destruction order issues)
    if (lv_is_initialized()) {
        lv_deinit();
    }

    m_width = 0;
    m_height = 0;
    m_initialized = false;
}

void DisplayManager::configure_scroll(int scroll_throw, int scroll_limit) {
    if (!m_pointer) {
        return;
    }

    lv_indev_set_scroll_throw(m_pointer, static_cast<uint8_t>(scroll_throw));
    lv_indev_set_scroll_limit(m_pointer, static_cast<uint8_t>(scroll_limit));
    spdlog::debug("[DisplayManager] Scroll config: throw={}, limit={}", scroll_throw, scroll_limit);
}

void DisplayManager::setup_keyboard_group() {
    if (!m_keyboard) {
        return;
    }

    m_input_group = lv_group_create();
    lv_group_set_default(m_input_group);
    lv_indev_set_group(m_keyboard, m_input_group);
    spdlog::debug("[DisplayManager] Created default input group for keyboard");
}

// ============================================================================
// Static Timing Functions
// ============================================================================

uint32_t DisplayManager::get_ticks() {
#ifdef HELIX_DISPLAY_SDL
    return SDL_GetTicks();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

void DisplayManager::delay(uint32_t ms) {
#ifdef HELIX_DISPLAY_SDL
    SDL_Delay(ms);
#else
    struct timespec ts = {static_cast<time_t>(ms / 1000),
                          static_cast<long>((ms % 1000) * 1000000L)};
    nanosleep(&ts, nullptr);
#endif
}

// ============================================================================
// Display Sleep Management
// ============================================================================

void DisplayManager::check_display_sleep() {
    // Get configured sleep timeout from settings (0 = disabled)
    int sleep_timeout_sec = SettingsManager::instance().get_display_sleep_sec();

    // Get LVGL inactivity time (milliseconds since last touch/input)
    uint32_t inactive_ms = lv_display_get_inactive_time(nullptr);
    uint32_t dim_timeout_ms =
        (m_dim_timeout_sec > 0) ? static_cast<uint32_t>(m_dim_timeout_sec) * 1000U : UINT32_MAX;
    uint32_t sleep_timeout_ms =
        (sleep_timeout_sec > 0) ? static_cast<uint32_t>(sleep_timeout_sec) * 1000U : UINT32_MAX;

    // Check for activity (touch detected within last 500ms)
    bool activity_detected = (inactive_ms < 500);

    if (m_display_sleeping) {
        // Currently sleeping - wake on any touch
        if (activity_detected) {
            wake_display();
        }
    } else if (m_display_dimmed) {
        // Currently dimmed - wake on touch, or go to sleep if timeout exceeded
        if (activity_detected) {
            wake_display();
        } else if (sleep_timeout_sec > 0 && inactive_ms >= sleep_timeout_ms) {
            // Transition from dimmed to sleeping
            m_display_sleeping = true;
            if (m_backlight) {
                m_backlight->set_brightness(0);
            }
            spdlog::info("[DisplayManager] Display sleeping (backlight off) after {}s inactivity",
                         sleep_timeout_sec);
        }
    } else {
        // Currently awake - check if we should dim or sleep
        if (sleep_timeout_sec > 0 && inactive_ms >= sleep_timeout_ms) {
            // Skip dim, go straight to sleep (sleep timeout <= dim timeout)
            m_display_sleeping = true;
            if (m_backlight) {
                m_backlight->set_brightness(0);
            }
            spdlog::info("[DisplayManager] Display sleeping (backlight off) after {}s inactivity",
                         sleep_timeout_sec);
        } else if (m_dim_timeout_sec > 0 && inactive_ms >= dim_timeout_ms) {
            // Dim the display
            m_display_dimmed = true;
            if (m_backlight) {
                m_backlight->set_brightness(m_dim_brightness_percent);
            }
            spdlog::info("[DisplayManager] Display dimmed to {}% after {}s inactivity",
                         m_dim_brightness_percent, m_dim_timeout_sec);
        }
    }
}

void DisplayManager::wake_display() {
    if (!m_display_sleeping && !m_display_dimmed) {
        return; // Already fully awake
    }

    bool was_sleeping = m_display_sleeping;
    m_display_sleeping = false;
    m_display_dimmed = false;

    // Gate input if waking from full sleep (not dim)
    // This prevents the wake touch from triggering UI actions
    if (was_sleeping) {
        disable_input_briefly();
    }

    // Restore configured brightness from settings
    int brightness = SettingsManager::instance().get_brightness();
    brightness = std::clamp(brightness, 10, 100);

    if (m_backlight) {
        m_backlight->set_brightness(brightness);
    }
    spdlog::info("[DisplayManager] Display woken from {}, brightness restored to {}%",
                 was_sleeping ? "sleep" : "dim", brightness);
}

void DisplayManager::ensure_display_on() {
    // Force display awake at startup regardless of previous state
    m_display_sleeping = false;
    m_display_dimmed = false;

    // Get configured brightness (or default to 50%)
    int brightness = SettingsManager::instance().get_brightness();
    brightness = std::clamp(brightness, 10, 100);

    // Apply to hardware - this ensures display is visible
    if (m_backlight) {
        m_backlight->set_brightness(brightness);
    }
    spdlog::info("[DisplayManager] Startup: forcing display ON at {}% brightness", brightness);
}

void DisplayManager::restore_display_on_shutdown() {
    // Ensure display is awake before exiting so next app doesn't start with black screen
    int brightness = SettingsManager::instance().get_brightness();
    brightness = std::clamp(brightness, 10, 100);

    if (m_backlight) {
        m_backlight->set_brightness(brightness);
    }
    m_display_sleeping = false;
    spdlog::info("[DisplayManager] Shutdown: restoring display to {}% brightness", brightness);
}

void DisplayManager::set_backlight_brightness(int percent) {
    percent = std::clamp(percent, 0, 100);
    if (m_backlight) {
        m_backlight->set_brightness(percent);
    }
}

bool DisplayManager::has_backlight_control() const {
    return m_backlight && m_backlight->is_available();
}

// ============================================================================
// Input Gating (Wake-Only First Touch)
// ============================================================================

void DisplayManager::disable_input_briefly() {
    // Disable all pointer input devices
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_enable(indev, false);
        }
        indev = lv_indev_get_next(indev);
    }

    // Schedule re-enable after 200ms via LVGL timer
    lv_timer_create(reenable_input_cb, 200, nullptr);

    spdlog::debug("[DisplayManager] Input disabled for 200ms (wake-only touch)");
}

void DisplayManager::reenable_input_cb(lv_timer_t* timer) {
    // Re-enable all pointer input devices
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_enable(indev, true);
        }
        indev = lv_indev_get_next(indev);
    }

    // Delete the one-shot timer
    lv_timer_delete(timer);

    spdlog::debug("[DisplayManager] Input re-enabled after wake");
}

// ============================================================================
// Window Resize Handler (Desktop/SDL)
// ============================================================================

void DisplayManager::resize_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<DisplayManager*>(lv_timer_get_user_data(timer));
    if (!self) {
        return;
    }

    spdlog::debug("[DisplayManager] Resize debounce complete, calling {} registered callbacks",
                  self->m_resize_callbacks.size());

    // Call all registered callbacks
    for (auto callback : self->m_resize_callbacks) {
        if (callback) {
            callback();
        }
    }

    // Delete one-shot timer
    lv_timer_delete(timer);
    self->m_resize_debounce_timer = nullptr;
}

void DisplayManager::resize_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SIZE_CHANGED) {
        auto* self = static_cast<DisplayManager*>(lv_event_get_user_data(e));
        if (!self) {
            return;
        }

        lv_obj_t* screen = static_cast<lv_obj_t*>(lv_event_get_target(e));
        lv_coord_t width = lv_obj_get_width(screen);
        lv_coord_t height = lv_obj_get_height(screen);

        spdlog::debug("[DisplayManager] Screen size changed to {}x{}, resetting debounce timer",
                      width, height);

        // Reset or create debounce timer
        if (self->m_resize_debounce_timer) {
            lv_timer_reset(self->m_resize_debounce_timer);
        } else {
            self->m_resize_debounce_timer =
                lv_timer_create(resize_timer_cb, RESIZE_DEBOUNCE_MS, self);
            lv_timer_set_repeat_count(self->m_resize_debounce_timer, 1); // One-shot
        }
    }
}

void DisplayManager::init_resize_handler(lv_obj_t* screen) {
    if (!screen) {
        spdlog::error("[DisplayManager] Cannot init resize handler: screen is null");
        return;
    }

    // Add SIZE_CHANGED event listener to screen
    lv_obj_add_event_cb(screen, resize_event_cb, LV_EVENT_SIZE_CHANGED, this);

    spdlog::debug("[DisplayManager] Resize handler initialized on screen");
}

void DisplayManager::register_resize_callback(ResizeCallback callback) {
    if (!callback) {
        spdlog::warn("[DisplayManager] Attempted to register null resize callback");
        return;
    }

    m_resize_callbacks.push_back(callback);
    spdlog::debug("[DisplayManager] Registered resize callback ({} total)",
                  m_resize_callbacks.size());
}
