// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_settings_display.h
 * @brief Display Settings overlay - brightness, sleep timeout, render modes
 *
 * This overlay allows users to configure:
 * - Dark mode toggle
 * - Theme colors action (preset, preview)
 * - Screen brightness (when hardware backlight available)
 * - Display sleep timeout
 * - Bed mesh render mode (Auto/3D/2D)
 * - Time format (12H/24H)
 *
 * @pattern Overlay (lazy init)
 * @threading Main thread only
 *
 * @see SettingsManager for persistence
 */

#pragma once

#include "lvgl/lvgl.h"

namespace helix::settings {

/**
 * @class DisplaySettingsOverlay
 * @brief Overlay for configuring display-related settings
 *
 * This overlay provides controls for:
     * - Theme colors action row

 * - Brightness slider with percentage label
 * - Sleep timeout dropdown (Never/1m/5m/10m/30m)
 * - Bed mesh and G-code render mode dropdowns
 * - Time format selection (12H/24H)
 *
 * ## Usage:
 *
 * @code
 * auto& overlay = helix::settings::get_display_settings_overlay();
 * overlay.show(parent_screen);  // Creates overlay if needed, initializes widgets, shows
 * @endcode
 */
class DisplaySettingsOverlay {
  public:
    /**
     * @brief Default constructor
     */
    DisplaySettingsOverlay();

    /**
     * @brief Destructor
     */
    ~DisplaySettingsOverlay();

    // Non-copyable
    DisplaySettingsOverlay(const DisplaySettingsOverlay&) = delete;
    DisplaySettingsOverlay& operator=(const DisplaySettingsOverlay&) = delete;

    //
    // === Initialization ===
    //

    /**
     * @brief Initialize subjects for reactive binding
     *
     * Must be called before overlay creation.
     */
    void init_subjects();

    /**
     * @brief Register event callbacks with lv_xml system
     *
     * Registers callbacks for brightness slider and dropdown changes.
     */
    void register_callbacks();

    //
    // === UI Creation ===
    //

    /**
     * @brief Create the overlay UI (called lazily)
     *
     * @param parent Parent widget to attach overlay to (usually screen)
     * @return Root object of overlay, or nullptr on failure
     */
    lv_obj_t* create(lv_obj_t* parent);

    /**
     * @brief Show the overlay
     *
     * This method:
     * 1. Ensures overlay is created
     * 2. Initializes all widget values from SettingsManager
     * 3. Pushes overlay onto navigation stack
     *
     * @param parent_screen The parent screen for overlay creation
     */
    void show(lv_obj_t* parent_screen);

    //
    // === Accessors ===
    //

    /**
     * @brief Get root overlay widget
     * @return Root widget, or nullptr if not created
     */
    lv_obj_t* get_root() const {
        return overlay_;
    }

    /**
     * @brief Check if overlay has been created
     * @return true if create() was called successfully
     */
    bool is_created() const {
        return overlay_ != nullptr;
    }

    /**
     * @brief Get human-readable overlay name
     * @return "Display Settings"
     */
    const char* get_name() const {
        return "Display Settings";
    }

    //
    // === Event Handlers (public for static callbacks) ===
    //

    /**
     * @brief Handle brightness slider change
     * @param value New brightness value (10-100)
     */
    void handle_brightness_changed(int value);

    /**
     * @brief Handle theme preset dropdown change
     * @param index Selected preset index
     */
    void handle_theme_preset_changed(int index);

    /**
     * @brief Handle theme preview action click
     */
    void handle_theme_preview_clicked();

    /**
     * @brief Handle theme settings action click
     */
    void handle_theme_settings_clicked();

  private:
    //
    // === Internal Methods ===
    //

    /**
     * @brief Initialize brightness slider and label
     */
    void init_brightness_controls();

    /**
     * @brief Initialize sleep timeout dropdown
     */
    void init_sleep_dropdown();

    /**
     * @brief Initialize bed mesh mode dropdown
     */
    void init_bed_mesh_dropdown();

    /**
     * @brief Initialize G-code mode dropdown
     */
    void init_gcode_dropdown();

    /**
     * @brief Initialize theme preset dropdown
     */
    void init_theme_preset_dropdown(lv_obj_t* root);

    /**
     * @brief Initialize time format dropdown
     */
    void init_time_format_dropdown();

    //
    // === State ===
    //

    lv_obj_t* overlay_{nullptr};
    lv_obj_t* parent_screen_{nullptr};
    lv_obj_t* theme_settings_overlay_{nullptr};
    lv_obj_t* theme_preview_overlay_{nullptr};

    /// Subject for brightness value label binding
    lv_subject_t brightness_value_subject_;
    char brightness_value_buf_[8]; // e.g., "100%"

    bool subjects_initialized_{false};

    //
    // === Static Callbacks ===
    //

    static void on_brightness_changed(lv_event_t* e);
    static void on_theme_preset_changed(lv_event_t* e);
    static void on_theme_preview_clicked(lv_event_t* e);
    static void on_theme_settings_clicked(lv_event_t* e);
};

/**
 * @brief Global instance accessor
 *
 * Creates the overlay on first access and registers it for cleanup
 * with StaticPanelRegistry.
 *
 * @return Reference to singleton DisplaySettingsOverlay
 */
DisplaySettingsOverlay& get_display_settings_overlay();

} // namespace helix::settings
