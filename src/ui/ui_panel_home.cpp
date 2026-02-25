// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_home.h"

#include "ui_callback_helpers.h"
#include "ui_error_reporting.h"
#include "ui_event_safety.h"
#include "ui_icon.h"
#include "ui_modal.h"
#include "ui_nav_manager.h"
#include "ui_overlay_network_settings.h"
#include "ui_panel_ams.h"
#include "ui_panel_power.h"
#include "ui_panel_temp_control.h"
#include "ui_subject_registry.h"
#include "ui_update_queue.h"
#include "ui_utils.h"

#include "ams_state.h"
#include "app_globals.h"
#include "config.h"
#include "display_settings_manager.h"
#include "ethernet_manager.h"
#include "favorite_macro_widget.h"
#include "led/led_controller.h"
#include "led/ui_led_control_overlay.h"
#include "moonraker_api.h"
#include "observer_factory.h"
#include "panel_widget_config.h"
#include "panel_widget_manager.h"
#include "panel_widgets/fan_stack_widget.h"
#include "panel_widgets/network_widget.h"
#include "panel_widgets/power_widget.h"
#include "panel_widgets/print_status_widget.h"
#include "panel_widgets/printer_image_widget.h"
#include "panel_widgets/temp_stack_widget.h"
#include "panel_widgets/thermistor_widget.h"
#include "printer_state.h"
#include "runtime_config.h"
#include "static_panel_registry.h"
#include "theme_manager.h"
#include "tool_state.h"
#include "wifi_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>

using namespace helix;

// Signal polling interval (5 seconds)
static constexpr uint32_t SIGNAL_POLL_INTERVAL_MS = 5000;

HomePanel::HomePanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    // Subscribe to PrinterState subjects (ObserverGuard handles cleanup)
    using helix::ui::observe_int_sync;

    extruder_temp_observer_ = observe_int_sync<HomePanel>(
        printer_state_.get_active_extruder_temp_subject(), this,
        [](HomePanel* self, int temp) { self->on_extruder_temp_changed(temp); });
    extruder_target_observer_ = observe_int_sync<HomePanel>(
        printer_state_.get_active_extruder_target_subject(), this,
        [](HomePanel* self, int target) { self->on_extruder_target_changed(target); });

    spdlog::debug("[{}] Subscribed to PrinterState extruder temperature and target", get_name());

    // LED observers are set up lazily via ensure_led_observers() when strips become available.
    // At construction time, hardware discovery may not have completed yet, so
    // selected_strips() could be empty. The observers will be created on first
    // reload_from_config() or handle_light_toggle() when strips are available.
    //
    // LED visibility on the home panel is controlled by the printer_has_led subject
    // (set via set_printer_capabilities after hardware discovery).
}

HomePanel::~HomePanel() {
    // Deinit subjects FIRST - disconnects observers before subject memory is freed
    deinit_subjects();

    // Gate observers watch external subjects (capabilities, klippy_state) that may
    // already be freed. Clear unconditionally.
    helix::PanelWidgetManager::instance().clear_gate_observers("home");
    helix::PanelWidgetManager::instance().unregister_rebuild_callback("home");

    // Detach active PanelWidget instances
    for (auto& w : active_widgets_) {
        w->detach();
    }
    active_widgets_.clear();

    // Clean up timers and animations
    if (lv_is_initialized()) {
        // Stop light-flash animation (var=light_icon_, not this)
        if (light_icon_) {
            lv_anim_delete(light_icon_, nullptr);
        }

        if (signal_poll_timer_) {
            lv_timer_delete(signal_poll_timer_);
            signal_poll_timer_ = nullptr;
        }
    }
}

void HomePanel::init_subjects() {
    using helix::ui::observe_int_sync;

    if (subjects_initialized_) {
        spdlog::warn("[{}] init_subjects() called twice - ignoring", get_name());
        return;
    }

    spdlog::debug("[{}] Initializing subjects", get_name());

    // Network subjects (home_network_icon_state, network_label) are owned by
    // NetworkWidget and initialized via PanelWidgetManager::init_widget_subjects()
    // before this function runs. HomePanel looks them up by name when needed.

    // Register event callbacks BEFORE loading XML
    register_xml_callbacks({
        {"light_toggle_cb", light_toggle_cb},
        {"light_long_press_cb", light_long_press_cb},
        {"power_toggle_cb", power_toggle_cb},
        {"power_long_press_cb", power_long_press_cb},
        {"temp_clicked_cb", temp_clicked_cb},
        {"printer_status_clicked_cb", printer_status_clicked_cb},
        {"network_clicked_cb", network_clicked_cb},
        {"ams_clicked_cb", ams_clicked_cb},
        {"on_fan_stack_clicked", helix::FanStackWidget::on_fan_stack_clicked},
        {"fan_stack_long_press_cb", helix::FanStackWidget::fan_stack_long_press_cb},
        {"fan_carousel_long_press_cb", helix::FanStackWidget::fan_carousel_long_press_cb},
        {"temp_stack_nozzle_cb", helix::TempStackWidget::temp_stack_nozzle_cb},
        {"temp_stack_bed_cb", helix::TempStackWidget::temp_stack_bed_cb},
        {"temp_stack_chamber_cb", helix::TempStackWidget::temp_stack_chamber_cb},
        {"temp_stack_long_press_cb", helix::TempStackWidget::temp_stack_long_press_cb},
        {"temp_carousel_long_press_cb", helix::TempStackWidget::temp_carousel_long_press_cb},
        {"temp_carousel_page_cb", helix::TempStackWidget::temp_carousel_page_cb},
        {"thermistor_clicked_cb", helix::ThermistorWidget::thermistor_clicked_cb},
        {"thermistor_picker_backdrop_cb", helix::ThermistorWidget::thermistor_picker_backdrop_cb},
        {"favorite_macro_1_clicked_cb", helix::FavoriteMacroWidget::clicked_1_cb},
        {"favorite_macro_1_long_press_cb", helix::FavoriteMacroWidget::long_press_1_cb},
        {"favorite_macro_2_clicked_cb", helix::FavoriteMacroWidget::clicked_2_cb},
        {"favorite_macro_2_long_press_cb", helix::FavoriteMacroWidget::long_press_2_cb},
        {"fav_macro_picker_backdrop_cb", helix::FavoriteMacroWidget::picker_backdrop_cb},
        {"on_home_grid_long_press", on_home_grid_long_press},
    });

    // Subscribe to AmsState slot_count for AMS widget visibility
    ams_slot_count_observer_ =
        observe_int_sync<HomePanel>(AmsState::instance().get_slot_count_subject(), this,
                                    [](HomePanel* /*self*/, int /*slot_count*/) {
                                        // AMS mini status widget auto-updates via observers bound
                                        // to AmsState subjects.
                                    });

    subjects_initialized_ = true;

    // Self-register cleanup — ensures deinit runs before lv_deinit()
    StaticPanelRegistry::instance().register_destroy(
        "HomePanelSubjects", []() { get_global_home_panel().deinit_subjects(); });

    spdlog::debug("[{}] Registered subjects and event callbacks", get_name());
}

void HomePanel::deinit_subjects() {
    if (!subjects_initialized_) {
        return;
    }
    // Release gate observers BEFORE subjects are freed
    helix::PanelWidgetManager::instance().clear_gate_observers("home");

    // SubjectManager handles all lv_subject_deinit() calls via RAII
    subjects_.deinit_all();
    subjects_initialized_ = false;
    spdlog::debug("[{}] Subjects deinitialized", get_name());
}

void HomePanel::setup_widget_gate_observers() {
    auto& mgr = helix::PanelWidgetManager::instance();
    mgr.setup_gate_observers("home", [this]() { populate_widgets(); });
}

void HomePanel::populate_widgets() {
    if (populating_widgets_) {
        spdlog::debug("[{}] populate_widgets: already in progress, skipping", get_name());
        return;
    }
    populating_widgets_ = true;

    lv_obj_t* container = lv_obj_find_by_name(panel_, "widget_container");
    if (!container) {
        spdlog::error("[{}] widget_container not found", get_name());
        populating_widgets_ = false;
        return;
    }

    // Detach active PanelWidget instances before clearing
    for (auto& w : active_widgets_) {
        w->detach();
    }

    // Destroy LVGL children BEFORE destroying C++ widget instances.
    // Pending async_call callbacks capture widget_obj_ as a validity guard —
    // if C++ objects are freed while LVGL objects still exist, the guard passes
    // but the captured `this` is dangling.
    lv_obj_clean(container);
    active_widgets_.clear();

    // Delegate generic widget creation to the manager
    active_widgets_ = helix::PanelWidgetManager::instance().populate_widgets("home", container);

    // HomePanel-specific: cache references for light_icon_, power_icon_, etc.
    cache_widget_references();

    populating_widgets_ = false;
}

void HomePanel::cache_widget_references() {
    // Find light icon for dynamic brightness/color updates
    light_icon_ = lv_obj_find_by_name(panel_, "light_icon");
    if (light_icon_) {
        spdlog::debug("[{}] Found light_icon for dynamic brightness/color", get_name());
        update_light_icon();
    }

    // Find power icon for visual feedback
    power_icon_ = lv_obj_find_by_name(panel_, "power_icon");

    // Attach heating icon animator
    lv_obj_t* temp_icon = lv_obj_find_by_name(panel_, "nozzle_icon_glyph");
    if (temp_icon) {
        temp_icon_animator_.attach(temp_icon);
        cached_extruder_temp_ =
            lv_subject_get_int(printer_state_.get_active_extruder_temp_subject());
        cached_extruder_target_ =
            lv_subject_get_int(printer_state_.get_active_extruder_target_subject());
        temp_icon_animator_.update(cached_extruder_temp_, cached_extruder_target_);
        spdlog::debug("[{}] Heating icon animator attached", get_name());
    }
}

void HomePanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    // Call base class to store panel_ and parent_screen_
    PanelBase::setup(panel, parent_screen);

    if (!panel_) {
        spdlog::error("[{}] NULL panel", get_name());
        return;
    }

    spdlog::debug("[{}] Setting up...", get_name());

    // Dynamically populate grid widgets from PanelWidgetConfig
    populate_widgets();

    // Observe hardware gate subjects so widgets appear/disappear when
    // capabilities change (e.g. power devices discovered after startup).
    setup_widget_gate_observers();

    // Register rebuild callback so settings overlay toggle changes take effect immediately
    helix::PanelWidgetManager::instance().register_rebuild_callback(
        "home", [this]() { populate_widgets(); });

    // Use global WiFiManager for signal strength queries
    if (!wifi_manager_) {
        wifi_manager_ = get_wifi_manager();
    }

    // Initialize EthernetManager for Ethernet status detection
    if (!ethernet_manager_) {
        ethernet_manager_ = std::make_unique<EthernetManager>();
        spdlog::debug("[{}] EthernetManager initialized for connection detection", get_name());
    }

    // Detect actual network type (Ethernet vs WiFi vs disconnected)
    detect_network_type();

    // Start signal polling timer if on WiFi
    if (!signal_poll_timer_ && current_network_ == NetworkType::Wifi) {
        signal_poll_timer_ = lv_timer_create(signal_poll_timer_cb, SIGNAL_POLL_INTERVAL_MS, this);
        spdlog::debug("[{}] Started signal polling timer ({}ms)", get_name(),
                      SIGNAL_POLL_INTERVAL_MS);
    }

    // Load LED configuration from config
    reload_from_config();

    spdlog::debug("[{}] Setup complete!", get_name());
}

void HomePanel::on_activate() {
    // Re-detect network type in case it changed while on another panel
    detect_network_type();

    // Start signal polling timer when panel becomes visible (only for WiFi)
    if (!signal_poll_timer_ && current_network_ == NetworkType::Wifi) {
        signal_poll_timer_ = lv_timer_create(signal_poll_timer_cb, SIGNAL_POLL_INTERVAL_MS, this);
        spdlog::debug("[{}] Started signal polling timer ({}ms interval)", get_name(),
                      SIGNAL_POLL_INTERVAL_MS);
    }

    // Activate behavioral widgets (network polling, power refresh, printer image, etc.)
    for (auto& w : active_widgets_) {
        if (auto* nw = dynamic_cast<helix::NetworkWidget*>(w.get())) {
            nw->on_activate();
        } else if (auto* pw = dynamic_cast<helix::PowerWidget*>(w.get())) {
            pw->refresh_power_state();
        }
    }

    // Start Spoolman polling for AMS mini status updates
    AmsState::instance().start_spoolman_polling();
}

void HomePanel::on_deactivate() {
    // Deactivate behavioral widgets
    for (auto& w : active_widgets_) {
        if (auto* nw = dynamic_cast<helix::NetworkWidget*>(w.get())) {
            nw->on_deactivate();
        }
    }

    AmsState::instance().stop_spoolman_polling();

    // Stop signal polling timer when panel is hidden (saves CPU)
    if (signal_poll_timer_) {
        lv_timer_delete(signal_poll_timer_);
        signal_poll_timer_ = nullptr;
        spdlog::debug("[{}] Stopped signal polling timer", get_name());
    }
}

void HomePanel::detect_network_type() {
    // Priority: Ethernet > WiFi > Disconnected
    // This ensures users on wired connections see the Ethernet icon even if WiFi is also available

    // Check Ethernet first (higher priority - more reliable connection)
    if (ethernet_manager_) {
        EthernetInfo eth_info = ethernet_manager_->get_info();
        if (eth_info.connected) {
            spdlog::debug("[{}] Detected Ethernet connection on {} ({})", get_name(),
                          eth_info.interface, eth_info.ip_address);
            set_network(NetworkType::Ethernet);
            return;
        }
    }

    // Check WiFi second
    if (wifi_manager_ && wifi_manager_->is_connected()) {
        spdlog::info("[{}] Detected WiFi connection ({})", get_name(),
                     wifi_manager_->get_connected_ssid());
        set_network(NetworkType::Wifi);
        return;
    }

    // Neither connected
    spdlog::info("[{}] No network connection detected", get_name());
    set_network(NetworkType::Disconnected);
}

void HomePanel::handle_light_toggle() {
    // Suppress click that follows a long-press gesture
    if (light_long_pressed_) {
        light_long_pressed_ = false;
        spdlog::debug("[{}] Light click suppressed (follows long-press)", get_name());
        return;
    }

    spdlog::info("[{}] Light button clicked", get_name());

    auto& led_ctrl = helix::led::LedController::instance();
    const auto& strips = led_ctrl.selected_strips();
    if (strips.empty()) {
        spdlog::warn("[{}] Light toggle called but no LED configured", get_name());
        return;
    }

    ensure_led_observers();

    led_ctrl.light_toggle();

    if (led_ctrl.light_state_trackable()) {
        light_on_ = led_ctrl.light_is_on();
        update_light_icon();
    } else {
        flash_light_icon();
    }
}

void HomePanel::handle_light_long_press() {
    spdlog::info("[{}] Light long-press: opening LED control overlay", get_name());

    // Lazy-create overlay on first access
    if (!led_control_panel_ && parent_screen_) {
        auto& overlay = get_led_control_overlay();

        if (!overlay.are_subjects_initialized()) {
            overlay.init_subjects();
        }
        overlay.register_callbacks();
        overlay.set_api(api_);

        led_control_panel_ = overlay.create(parent_screen_);
        if (!led_control_panel_) {
            NOTIFY_ERROR("Failed to load LED control overlay");
            return;
        }

        NavigationManager::instance().register_overlay_instance(led_control_panel_, &overlay);
    }

    if (led_control_panel_) {
        light_long_pressed_ = true; // Suppress the click that follows long-press
        get_led_control_overlay().set_api(api_);
        NavigationManager::instance().push_overlay(led_control_panel_);
    }
}

void HomePanel::handle_power_toggle() {
    // Suppress click that follows a long-press gesture
    if (power_long_pressed_) {
        power_long_pressed_ = false;
        spdlog::debug("[{}] Power click suppressed (follows long-press)", get_name());
        return;
    }

    spdlog::info("[{}] Power button clicked", get_name());

    if (!api_) {
        spdlog::warn("[{}] Power toggle: no API available", get_name());
        return;
    }

    // Get selected devices from power panel config
    auto& power_panel = get_global_power_panel();
    const auto& selected = power_panel.get_selected_devices();
    if (selected.empty()) {
        spdlog::warn("[{}] Power toggle: no devices selected", get_name());
        return;
    }

    // Determine action: if currently on -> turn off, else turn on
    const char* action = power_on_ ? "off" : "on";
    bool new_state = !power_on_;

    for (const auto& device : selected) {
        api_->set_device_power(
            device, action,
            [this, device]() {
                spdlog::debug("[{}] Power device '{}' set successfully", get_name(), device);
            },
            [this, device](const MoonrakerError& err) {
                spdlog::error("[{}] Failed to set power device '{}': {}", get_name(), device,
                              err.message);
                // On error, refresh from actual state
                refresh_power_state();
            });
    }

    // Optimistically update icon state
    power_on_ = new_state;
    update_power_icon(power_on_);
}

void HomePanel::handle_power_long_press() {
    spdlog::info("[{}] Power long-press: opening power panel overlay", get_name());

    auto& panel = get_global_power_panel();
    lv_obj_t* overlay = panel.get_or_create_overlay(parent_screen_);
    if (overlay) {
        power_long_pressed_ = true; // Suppress the click that follows long-press
        NavigationManager::instance().push_overlay(overlay);
    }
}

void HomePanel::update_power_icon(bool is_on) {
    if (!power_icon_)
        return;

    ui_icon_set_variant(power_icon_, is_on ? "danger" : "muted");
}

void HomePanel::refresh_power_state() {
    if (!api_)
        return;

    // Capture selected devices on UI thread before async API call
    auto& power_panel = get_global_power_panel();
    const auto& selected = power_panel.get_selected_devices();
    if (selected.empty())
        return;
    std::set<std::string> selected_set(selected.begin(), selected.end());

    // Query power devices to determine if selected ones are on
    api_->get_power_devices(
        [this, selected_set](const std::vector<PowerDevice>& devices) {
            // Check if any selected device is on
            bool any_on = false;
            for (const auto& dev : devices) {
                if (selected_set.count(dev.device) > 0 && dev.status == "on") {
                    any_on = true;
                    break;
                }
            }

            helix::ui::queue_update([this, any_on]() {
                power_on_ = any_on;
                update_power_icon(power_on_);
                spdlog::debug("[{}] Power state refreshed: {}", get_name(),
                              power_on_ ? "on" : "off");
            });
        },
        [this](const MoonrakerError& err) {
            spdlog::warn("[{}] Failed to refresh power state: {}", get_name(), err.message);
        });
}

void HomePanel::set_temp_control_panel(TempControlPanel* temp_panel) {
    temp_control_panel_ = temp_panel;
    spdlog::trace("[{}] TempControlPanel reference set", get_name());
}

void HomePanel::handle_temp_clicked() {
    spdlog::info("[{}] Temperature icon clicked - opening nozzle temp panel", get_name());

    if (!temp_control_panel_) {
        spdlog::error("[{}] TempControlPanel not initialized", get_name());
        NOTIFY_ERROR("Temperature panel not available");
        return;
    }

    // Create nozzle temp panel on first access (lazy initialization)
    if (!nozzle_temp_panel_ && parent_screen_) {
        spdlog::debug("[{}] Creating nozzle temperature panel...", get_name());

        // Create from XML
        nozzle_temp_panel_ =
            static_cast<lv_obj_t*>(lv_xml_create(parent_screen_, "nozzle_temp_panel", nullptr));
        if (nozzle_temp_panel_) {
            // Setup via injected TempControlPanel
            temp_control_panel_->setup_nozzle_panel(nozzle_temp_panel_, parent_screen_);
            NavigationManager::instance().register_overlay_instance(
                nozzle_temp_panel_, temp_control_panel_->get_nozzle_lifecycle());

            // Initially hidden
            lv_obj_add_flag(nozzle_temp_panel_, LV_OBJ_FLAG_HIDDEN);
            spdlog::info("[{}] Nozzle temp panel created and initialized", get_name());
        } else {
            spdlog::error("[{}] Failed to create nozzle temp panel from XML", get_name());
            NOTIFY_ERROR("Failed to load temperature panel");
            return;
        }
    }

    // Push nozzle temp panel onto navigation history and show it
    if (nozzle_temp_panel_) {
        NavigationManager::instance().push_overlay(nozzle_temp_panel_);
    }
}

void HomePanel::handle_printer_status_clicked() {
    spdlog::info("[{}] Printer status icon clicked - navigating to advanced settings", get_name());

    // Navigate to advanced settings panel
    NavigationManager::instance().set_active(PanelId::Advanced);
}

void HomePanel::handle_network_clicked() {
    spdlog::info("[{}] Network icon clicked - opening network settings directly", get_name());

    // Open Network settings overlay directly (same as Settings panel's Network row)
    auto& overlay = get_network_settings_overlay();

    if (!overlay.is_created()) {
        overlay.init_subjects();
        overlay.register_callbacks();
        overlay.create(parent_screen_);
    }

    overlay.show();
}

void HomePanel::handle_ams_clicked() {
    spdlog::info("[{}] AMS indicator clicked - opening AMS panel overlay", get_name());

    // Open AMS panel overlay for multi-filament management
    auto& ams_panel = get_global_ams_panel();
    if (!ams_panel.are_subjects_initialized()) {
        ams_panel.init_subjects();
    }
    lv_obj_t* panel_obj = ams_panel.get_panel();
    if (panel_obj) {
        NavigationManager::instance().push_overlay(panel_obj);
    }
}

void HomePanel::ensure_led_observers() {
    using helix::ui::observe_int_sync;

    if (!led_state_observer_) {
        led_state_observer_ = observe_int_sync<HomePanel>(
            printer_state_.get_led_state_subject(), this,
            [](HomePanel* self, int state) { self->on_led_state_changed(state); });
    }
    if (!led_brightness_observer_) {
        led_brightness_observer_ = observe_int_sync<HomePanel>(
            printer_state_.get_led_brightness_subject(), this,
            [](HomePanel* self, int /*brightness*/) { self->update_light_icon(); });
    }
}

void HomePanel::on_led_state_changed(int state) {
    auto& led_ctrl = helix::led::LedController::instance();
    if (led_ctrl.light_state_trackable()) {
        light_on_ = (state != 0);
        spdlog::debug("[{}] LED state changed: {} (from PrinterState)", get_name(),
                      light_on_ ? "ON" : "OFF");
        update_light_icon();
    } else {
        spdlog::debug("[{}] LED state changed but not trackable (TOGGLE macro mode)", get_name());
    }
}

void HomePanel::update_light_icon() {
    if (!light_icon_) {
        return;
    }

    // Get current brightness
    int brightness = lv_subject_get_int(printer_state_.get_led_brightness_subject());

    // Set icon based on brightness level
    const char* icon_name = ui_brightness_to_lightbulb_icon(brightness);
    ui_icon_set_source(light_icon_, icon_name);

    // Calculate icon color from LED RGBW values
    if (brightness == 0) {
        // OFF state - use muted gray from design tokens
        ui_icon_set_color(light_icon_, theme_manager_get_color("light_icon_off"), LV_OPA_COVER);
    } else {
        // Get RGB values from PrinterState
        int r = lv_subject_get_int(printer_state_.get_led_r_subject());
        int g = lv_subject_get_int(printer_state_.get_led_g_subject());
        int b = lv_subject_get_int(printer_state_.get_led_b_subject());
        int w = lv_subject_get_int(printer_state_.get_led_w_subject());

        lv_color_t icon_color;
        // If white channel dominant or RGB near white, use gold from design tokens
        if (w > std::max({r, g, b}) || (r > 200 && g > 200 && b > 200)) {
            icon_color = theme_manager_get_color("light_icon_on");
        } else {
            // Use actual LED color, boost if too dark for visibility
            int max_val = std::max({r, g, b});
            if (max_val < 128 && max_val > 0) {
                float scale = 128.0f / static_cast<float>(max_val);
                icon_color =
                    lv_color_make(static_cast<uint8_t>(std::min(255, static_cast<int>(r * scale))),
                                  static_cast<uint8_t>(std::min(255, static_cast<int>(g * scale))),
                                  static_cast<uint8_t>(std::min(255, static_cast<int>(b * scale))));
            } else {
                icon_color = lv_color_make(static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                           static_cast<uint8_t>(b));
            }
        }

        ui_icon_set_color(light_icon_, icon_color, LV_OPA_COVER);
    }

    spdlog::trace("[{}] Light icon: {} at {}%", get_name(), icon_name, brightness);
}

void HomePanel::flash_light_icon() {
    if (!light_icon_)
        return;

    // Flash gold briefly then fade back to muted
    ui_icon_set_color(light_icon_, theme_manager_get_color("light_icon_on"), LV_OPA_COVER);

    if (!DisplaySettingsManager::instance().get_animations_enabled()) {
        // No animations -- the next status update will restore the icon naturally
        return;
    }

    // Animate opacity 255 -> 0 then restore to muted on completion
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, light_icon_);
    lv_anim_set_values(&anim, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&anim, 300);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, [](void* obj, int32_t value) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(value), 0);
    });
    lv_anim_set_completed_cb(&anim, [](lv_anim_t* a) {
        auto* icon = static_cast<lv_obj_t*>(a->var);
        lv_obj_set_style_opa(icon, LV_OPA_COVER, 0);
        ui_icon_set_color(icon, theme_manager_get_color("light_icon_off"), LV_OPA_COVER);
    });
    lv_anim_start(&anim);

    spdlog::debug("[{}] Flash light icon (TOGGLE macro, state unknown)", get_name());
}

void HomePanel::on_extruder_temp_changed(int temp_centi) {
    cached_extruder_temp_ = temp_centi;
    update_temp_icon_animation();
}

void HomePanel::on_extruder_target_changed(int target_centi) {
    cached_extruder_target_ = target_centi;
    update_temp_icon_animation();
}

void HomePanel::update_temp_icon_animation() {
    temp_icon_animator_.update(cached_extruder_temp_, cached_extruder_target_);
}

void HomePanel::reload_from_config() {
    using helix::ui::observe_int_sync;

    Config* config = Config::get_instance();
    if (!config) {
        spdlog::warn("[{}] reload_from_config: Config not available", get_name());
        return;
    }

    // Reload LED configuration from LedController (single source of truth)
    // LED visibility is controlled by printer_has_led subject set via set_printer_capabilities()
    auto& led_ctrl = helix::led::LedController::instance();
    const auto& strips = led_ctrl.selected_strips();
    if (!strips.empty()) {
        // Set up tracked LED and observers (idempotent)
        printer_state_.set_tracked_led(strips.front());
        ensure_led_observers();
        spdlog::info("[{}] Reloaded LED config: {} LED(s)", get_name(), strips.size());
    } else {
        // No LED configured - clear tracking
        printer_state_.set_tracked_led("");
        spdlog::debug("[{}] LED config cleared", get_name());
    }

    // Delegate printer image reload to PrinterImageWidget if active
    for (auto& w : active_widgets_) {
        if (auto* piw = dynamic_cast<helix::PrinterImageWidget*>(w.get())) {
            piw->reload_from_config();
            break;
        }
    }
}

void HomePanel::set_network(NetworkType type) {
    current_network_ = type;

    // Look up network subjects owned by NetworkWidget
    lv_subject_t* label_subject = lv_xml_get_subject(nullptr, "network_label");
    if (label_subject) {
        switch (type) {
        case NetworkType::Wifi:
            lv_subject_copy_string(label_subject, "WiFi");
            break;
        case NetworkType::Ethernet:
            lv_subject_copy_string(label_subject, "Ethernet");
            break;
        case NetworkType::Disconnected:
            lv_subject_copy_string(label_subject, "Disconnected");
            break;
        }
    }

    // Update the icon state (will query WiFi signal strength if connected)
    update_network_icon_state();

    spdlog::debug("[{}] Network type set to {} (icon state will be computed)", get_name(),
                  static_cast<int>(type));
}

int HomePanel::compute_network_icon_state() {
    // State values:
    // 0 = Disconnected (wifi_off, disabled variant)
    // 1 = WiFi strength 1 (<=25%, warning variant)
    // 2 = WiFi strength 2 (26-50%, accent variant)
    // 3 = WiFi strength 3 (51-75%, accent variant)
    // 4 = WiFi strength 4 (>75%, accent variant)
    // 5 = Ethernet connected (accent variant)

    if (current_network_ == NetworkType::Disconnected) {
        spdlog::trace("[{}] Network disconnected -> state 0", get_name());
        return 0;
    }

    if (current_network_ == NetworkType::Ethernet) {
        spdlog::trace("[{}] Network ethernet -> state 5", get_name());
        return 5;
    }

    // WiFi - get signal strength from WiFiManager
    int signal = 0;
    if (wifi_manager_) {
        signal = wifi_manager_->get_signal_strength();
        spdlog::trace("[{}] WiFi signal strength: {}%", get_name(), signal);
    } else {
        spdlog::warn("[{}] WiFiManager not available for signal query", get_name());
    }

    // Map signal percentage to icon state (1-4)
    int state;
    if (signal <= 25)
        state = 1; // Weak (warning)
    else if (signal <= 50)
        state = 2; // Fair
    else if (signal <= 75)
        state = 3; // Good
    else
        state = 4; // Strong

    spdlog::trace("[{}] WiFi signal {}% -> state {}", get_name(), signal, state);
    return state;
}

void HomePanel::update_network_icon_state() {
    lv_subject_t* icon_state = lv_xml_get_subject(nullptr, "home_network_icon_state");
    if (!icon_state) {
        return;
    }

    int new_state = compute_network_icon_state();
    int old_state = lv_subject_get_int(icon_state);

    if (new_state != old_state) {
        lv_subject_set_int(icon_state, new_state);
        spdlog::debug("[{}] Network icon state: {} -> {}", get_name(), old_state, new_state);
    }
}

void HomePanel::signal_poll_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<HomePanel*>(lv_timer_get_user_data(timer));
    if (self && self->current_network_ == NetworkType::Wifi) {
        self->update_network_icon_state();
    }
}

void HomePanel::trigger_idle_runout_check() {
    for (auto& w : active_widgets_) {
        if (auto* psw = dynamic_cast<helix::PrintStatusWidget*>(w.get())) {
            psw->trigger_idle_runout_check();
            return;
        }
    }
    spdlog::debug("[{}] PrintStatusWidget not active - skipping runout check", get_name());
}

void HomePanel::set_light(bool is_on) {
    light_on_ = is_on;
    spdlog::debug("[{}] Local light state: {}", get_name(), is_on ? "ON" : "OFF");
}

// ============================================================================
// Static callback trampolines
// ============================================================================

void HomePanel::light_toggle_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] light_toggle_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_light_toggle();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::light_long_press_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] light_long_press_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_light_long_press();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::power_toggle_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] power_toggle_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_power_toggle();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::power_long_press_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] power_long_press_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_power_long_press();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::temp_clicked_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] temp_clicked_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_temp_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::printer_status_clicked_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] printer_status_clicked_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_printer_status_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::network_clicked_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] network_clicked_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_network_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::ams_clicked_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] ams_clicked_cb");
    (void)e;
    extern HomePanel& get_global_home_panel();
    get_global_home_panel().handle_ams_clicked();
    LVGL_SAFE_EVENT_CB_END();
}

void HomePanel::on_home_grid_long_press(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[HomePanel] on_home_grid_long_press");
    (void)e;
    extern HomePanel& get_global_home_panel();
    auto& panel = get_global_home_panel();
    if (!panel.grid_edit_mode_.is_active()) {
        auto* container = lv_obj_find_by_name(panel.panel_, "widget_container");
        auto& config = helix::PanelWidgetManager::instance().get_widget_config("home");
        panel.grid_edit_mode_.enter(container, &config);
    }
    LVGL_SAFE_EVENT_CB_END();
}

static std::unique_ptr<HomePanel> g_home_panel;

HomePanel& get_global_home_panel() {
    if (!g_home_panel) {
        g_home_panel = std::make_unique<HomePanel>(get_printer_state(), nullptr);
        StaticPanelRegistry::instance().register_destroy("HomePanel",
                                                         []() { g_home_panel.reset(); });
    }
    return *g_home_panel;
}
