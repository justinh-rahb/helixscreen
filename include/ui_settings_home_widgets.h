// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_settings_home_widgets.h
 * @brief Home Widgets settings overlay - toggle status widgets on/off
 *
 * This overlay allows users to enable/disable individual home panel widgets.
 * Accessible from Settings > Appearance > Home Widgets.
 *
 * @pattern Overlay (lazy init)
 * @threading Main thread only
 *
 * @see HomeWidgetConfig for persistence
 * @see HomeWidgetRegistry for widget definitions
 */

#pragma once

#include "home_widget_config.h"
#include "home_widget_registry.h"
#include "lvgl/lvgl.h"
#include "overlay_base.h"

#include <memory>
#include <string>

namespace helix::settings {

/**
 * @class HomeWidgetsOverlay
 * @brief Overlay for toggling home panel widgets on/off
 *
 * Dynamically creates toggle rows for each registered widget.
 * Hardware-gated widgets that aren't detected are shown as disabled.
 * Changes are saved on deactivate and trigger home panel rebuild.
 */
class HomeWidgetsOverlay : public OverlayBase {
  public:
    HomeWidgetsOverlay();
    ~HomeWidgetsOverlay() override;

    //
    // === OverlayBase Interface ===
    //

    void init_subjects() override;
    void register_callbacks() override;

    const char* get_name() const override {
        return "Home Widgets";
    }

    void on_activate() override;
    void on_deactivate() override;

    //
    // === UI Creation ===
    //

    lv_obj_t* create(lv_obj_t* parent) override;

    /**
     * @brief Show the overlay (lazy-creates if needed)
     * @param parent_screen The parent screen for overlay creation
     */
    void show(lv_obj_t* parent_screen);

    bool is_created() const {
        return overlay_root_ != nullptr;
    }

    /**
     * @brief Handle a widget toggle change from the UI
     * @param index Config entry index
     * @param enabled New enabled state
     * @return true if toggle was accepted, false if rejected (e.g. max limit)
     */
    bool handle_widget_toggled(size_t index, bool enabled);

    /**
     * @brief Finalize drag after drop animation completes
     *
     * Settles the row back into flex layout, applies config reorder if
     * position changed, and rebuilds the list. Called from static anim callback.
     */
    void finalize_drag();

  private:
    /**
     * @brief Populate widget toggle list from config + registry
     */
    void populate_widget_list();

    /**
     * @brief Create a single toggle row for a widget entry
     * @param parent Parent container for the row
     * @param entry Widget config entry (id + enabled state)
     * @param def Widget definition (display name, icon, hardware gate)
     * @param index Index in the config entries vector
     */
    void create_widget_row(lv_obj_t* parent, const helix::HomeWidgetEntry& entry,
                           const helix::HomeWidgetDef& def, size_t index);

    /// Own HomeWidgetConfig instance (loads/saves independently)
    std::unique_ptr<helix::HomeWidgetConfig> widget_config_;

    /// Whether any toggle was changed during this activation
    bool changes_made_{false};

    //
    // === Drag-to-Reorder State ===
    //

    /// Cached pointer to the scrollable widget list container
    lv_obj_t* widget_list_{nullptr};

    /// Whether a drag operation is currently in progress
    bool drag_active_{false};

    /// Config index of the row when drag started
    int32_t drag_from_index_{-1};

    /// The row object currently being dragged
    lv_obj_t* drag_row_{nullptr};

    /// Invisible spacer inserted where drag_row_ was
    lv_obj_t* drag_placeholder_{nullptr};

    /// Screen Y coordinate where drag started
    int32_t drag_start_y_{0};

    /// Height of the dragged row (for placeholder sizing)
    int32_t drag_row_height_{0};

    /// Offset from pointer Y to row top edge (prevents jump on grab)
    int32_t drag_offset_y_{0};

    /// Auto-scroll timer for dragging near edges
    lv_timer_t* drag_scroll_timer_{nullptr};

    /// Current auto-scroll speed (pixels per tick, negative = up, positive = down)
    int32_t drag_scroll_speed_{0};

    /// When true, reset_drag_state() skips re-enabling SCROLLABLE (caller handles it)
    bool skip_scroll_restore_{false};

    /// Static event callback for drag handle press/move/release events
    static void on_drag_handle_event(lv_event_t* e);

    /// Handle long-press on a drag handle (enter drag mode)
    void handle_drag_start(lv_obj_t* row);

    /// Handle pointer movement during drag (reorder rows visually)
    void handle_drag_move();

    /// Handle release after drag (animate to final position)
    void handle_drag_end();

    /// Reset all drag state to idle defaults
    void reset_drag_state();

    /// Timer callback for auto-scrolling during drag
    static void drag_scroll_timer_cb(lv_timer_t* timer);

    /// Update auto-scroll speed based on pointer proximity to edges
    void update_drag_auto_scroll();
};

/**
 * @brief Global instance accessor
 * @return Reference to singleton HomeWidgetsOverlay
 */
HomeWidgetsOverlay& get_home_widgets_overlay();

} // namespace helix::settings
