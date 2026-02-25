// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_heating_animator.h"
#include "ui_observer_guard.h"
#include "ui_panel_base.h"

#include "grid_edit_mode.h"
#include "led/led_controller.h"
#include "panel_widget.h"
#include "subject_managed_panel.h"

#include <memory>
#include <vector>

// Forward declarations
namespace helix {
class WiFiManager;
}
class EthernetManager;
class TempControlPanel;

/**
 * @brief Home panel - Main dashboard showing printer status and quick actions
 *
 * Pure grid container: all visible elements (printer image, tips, print status,
 * temperature, network, LED, power, etc.) are placed as PanelWidgets by
 * PanelWidgetManager. HomePanel retains LED control, temperature icon animation,
 * network detection, and power control logic that widgets delegate back to.
 */

#include "network_type.h"

class HomePanel : public PanelBase {
  public:
    HomePanel(helix::PrinterState& printer_state, MoonrakerAPI* api);
    ~HomePanel() override;

    void init_subjects() override;
    void deinit_subjects();
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    void on_activate() override;
    void on_deactivate() override;
    const char* get_name() const override {
        return "Home Panel";
    }
    const char* get_xml_component_name() const override {
        return "home_panel";
    }

    /**
     * @brief Rebuild the widget list from current PanelWidgetConfig
     *
     * Called from PanelWidgetsOverlay when widget toggles change.
     */
    void populate_widgets();

    /** @brief Set network status display */
    void set_network(helix::NetworkType type);

    /** @brief Set light state (on=gold, off=grey) */
    void set_light(bool is_on);

    bool get_light_state() const {
        return light_on_;
    }

    /**
     * @brief Reload LED visibility from config
     *
     * Called after wizard completion to update LED settings.
     * Printer image reload is handled by PrinterImageWidget.
     */
    void reload_from_config();

    /**
     * @brief Trigger a deferred runout check (used after wizard completes)
     *
     * Delegates to PrintStatusWidget if it is active in the widget grid.
     */
    void trigger_idle_runout_check();

    /**
     * @brief Set reference to TempControlPanel for temperature overlay
     *
     * Must be called before temp icon click handler can work.
     * @param temp_panel Pointer to TempControlPanel instance
     */
    void set_temp_control_panel(TempControlPanel* temp_panel);

    // Transition: widget callbacks delegate to these HomePanel handlers.
    // These will move into widget classes once HomePanel code is removed.
    void handle_light_toggle();
    void handle_light_long_press();
    void handle_power_toggle();
    void handle_power_long_press();
    void handle_temp_clicked();
    void handle_network_clicked();

    /// Exit grid edit mode (called by navbar done button)
    void exit_grid_edit_mode();

  private:
    SubjectManager subjects_;
    TempControlPanel* temp_control_panel_ = nullptr;

    bool light_on_ = false;
    bool light_long_pressed_ = false; // Suppress click after long-press
    bool power_on_ = false;           // Tracks power state for icon
    bool power_long_pressed_ = false; // Suppress click after long-press for power button
    bool populating_widgets_ = false; // Reentrancy guard for populate_widgets()
    helix::NetworkType current_network_ = helix::NetworkType::Wifi;

    lv_timer_t* signal_poll_timer_ = nullptr;           // Polls WiFi signal strength every 5s
    std::shared_ptr<helix::WiFiManager> wifi_manager_;  // For signal strength queries
    std::unique_ptr<EthernetManager> ethernet_manager_; // For Ethernet status queries

    // Light icon for dynamic brightness/color updates
    lv_obj_t* light_icon_ = nullptr;
    lv_obj_t* power_icon_ = nullptr;

    // Lazily-created overlay panels (owned by LVGL parent, not us)
    lv_obj_t* nozzle_temp_panel_ = nullptr;
    lv_obj_t* led_control_panel_ = nullptr;

    void setup_widget_gate_observers();
    void cache_widget_references();
    void detect_network_type();       // Detects WiFi vs Ethernet vs disconnected
    int compute_network_icon_state(); // Maps network type + signal -> 0-5
    void update_network_icon_state(); // Updates the subject
    static void signal_poll_timer_cb(lv_timer_t* timer);

    void flash_light_icon();
    void ensure_led_observers();
    void handle_printer_status_clicked();
    void handle_ams_clicked();
    void on_extruder_temp_changed(int temp);
    void on_extruder_target_changed(int target);
    void on_led_state_changed(int state);
    void update_temp_icon_animation();
    void update_light_icon();
    void update_power_icon(bool is_on);
    void refresh_power_state(); // Query API to sync icon with actual device state

    static void light_toggle_cb(lv_event_t* e);
    static void light_long_press_cb(lv_event_t* e);
    static void power_toggle_cb(lv_event_t* e);
    static void power_long_press_cb(lv_event_t* e);
    static void temp_clicked_cb(lv_event_t* e);
    static void printer_status_clicked_cb(lv_event_t* e);
    static void network_clicked_cb(lv_event_t* e);
    static void ams_clicked_cb(lv_event_t* e);
    static void on_home_grid_long_press(lv_event_t* e);

    ObserverGuard extruder_temp_observer_;
    ObserverGuard extruder_target_observer_;
    ObserverGuard led_state_observer_;
    ObserverGuard led_brightness_observer_;
    ObserverGuard ams_slot_count_observer_;

    // Active PanelWidget instances (factory-created, lifecycle-managed)
    std::vector<std::unique_ptr<helix::PanelWidget>> active_widgets_;

    // Grid edit mode state machine (long-press to rearrange widgets)
    helix::GridEditMode grid_edit_mode_;

    // Heating icon animator (gradient color + pulse while heating)
    HeatingIconAnimator temp_icon_animator_;
    int cached_extruder_temp_ = 25;
    int cached_extruder_target_ = 0;
};

// Global instance accessor (needed by main.cpp)
HomePanel& get_global_home_panel();
