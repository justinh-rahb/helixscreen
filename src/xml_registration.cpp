// SPDX-License-Identifier: GPL-3.0-or-later

#include "xml_registration.h"

#include "ui_ams_current_tool.h"
#include "ui_fonts.h"
#include "ui_gcode_viewer.h"
#include "ui_hsv_picker.h"
#include "ui_spinner.h"
#include "ui_spool_canvas.h"
#include "ui_switch.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "ui_theme.h"

#include <spdlog/spdlog.h>

#include <lvgl.h>

namespace helix {

/**
 * No-op callback for optional event handlers in XML components.
 * When a component has an optional callback prop with default="",
 * LVGL tries to find a callback named "" which doesn't exist.
 * Registering this no-op callback silences those warnings.
 */
static void noop_event_callback(lv_event_t* /*e*/) {
    // Intentionally empty - used for optional callbacks that weren't provided
}

/**
 * No-op subject for optional subject bindings in XML components.
 * When a component has an optional subject prop with default="",
 * LVGL tries to find a subject named "" which doesn't exist.
 * Registering this no-op subject silences those warnings.
 */
static lv_subject_t s_noop_subject;

/**
 * Register responsive constants for color picker sizing based on screen dimensions
 * Call this BEFORE registering XML components that use the color picker
 */
static void register_color_picker_responsive_constants() {
    lv_display_t* display = lv_display_get_default();
    int32_t hor_res = lv_display_get_horizontal_resolution(display);
    int32_t ver_res = lv_display_get_vertical_resolution(display);
    int32_t greater_res = LV_MAX(hor_res, ver_res);

    // Preview swatch size and text height scale with screen
    const char* preview_size;
    const char* text_height;
    if (greater_res <= UI_BREAKPOINT_SMALL_MAX) {
        preview_size = "40";
        text_height = "52";
    } else if (greater_res <= UI_BREAKPOINT_MEDIUM_MAX) {
        preview_size = "48";
        text_height = "60";
    } else {
        preview_size = "56";
        text_height = "68";
    }

    lv_xml_component_scope_t* scope = lv_xml_component_get_scope("globals");
    if (scope) {
        lv_xml_register_const(scope, "color_preview_size", preview_size);
        lv_xml_register_const(scope, "color_text_height", text_height);
        spdlog::debug(
            "[Color Picker] Registered color_preview_size={}, color_text_height={} for screen {}px",
            preview_size, text_height, greater_res);
    }
}

void register_fonts_and_images() {
    spdlog::debug("[XML Registration] Registering fonts and images...");

    // Material Design Icons (various sizes for different UI elements)
    // Source: https://pictogrammers.com/library/mdi/
    lv_xml_register_font(NULL, "mdi_icons_64", &mdi_icons_64);
    lv_xml_register_font(NULL, "mdi_icons_48", &mdi_icons_48);
    lv_xml_register_font(NULL, "mdi_icons_32", &mdi_icons_32);
    lv_xml_register_font(NULL, "mdi_icons_24", &mdi_icons_24);
    lv_xml_register_font(NULL, "mdi_icons_16", &mdi_icons_16);

    // Montserrat text fonts - used by semantic text components:
    // - text_heading uses font_heading (20/26/28 for small/medium/large breakpoints)
    // - text_body uses font_body (14/18/20 for small/medium/large breakpoints)
    // - text_small uses font_small (12/16/18 for small/medium/large breakpoints)
    // ALL sizes used by the responsive typography system MUST be registered here!
    // NOTE: Registering as "montserrat_*" for XML compatibility but using noto_sans_* fonts
    lv_xml_register_font(NULL, "montserrat_10", &noto_sans_10);
    lv_xml_register_font(NULL, "montserrat_12", &noto_sans_12); // text_small (small)
    lv_xml_register_font(NULL, "montserrat_14", &noto_sans_14); // text_body (small)
    lv_xml_register_font(NULL, "montserrat_16", &noto_sans_16); // text_small (medium)
    lv_xml_register_font(NULL, "montserrat_18",
                         &noto_sans_18); // text_body (medium), text_small (large)
    lv_xml_register_font(NULL, "montserrat_20",
                         &noto_sans_20); // text_heading (small), text_body (large)
    lv_xml_register_font(NULL, "montserrat_24", &noto_sans_24);
    lv_xml_register_font(NULL, "montserrat_26", &noto_sans_26); // text_heading (medium)
    lv_xml_register_font(NULL, "montserrat_28",
                         &noto_sans_28); // text_heading (large), numeric displays

    // Noto Sans fonts - same sizes as Montserrat, with extended Unicode support
    // (includes ©®™€£¥°±•… and other symbols)
    lv_xml_register_font(NULL, "noto_sans_10", &noto_sans_10);
    lv_xml_register_font(NULL, "noto_sans_12", &noto_sans_12);
    lv_xml_register_font(NULL, "noto_sans_14", &noto_sans_14);
    lv_xml_register_font(NULL, "noto_sans_16", &noto_sans_16);
    lv_xml_register_font(NULL, "noto_sans_18", &noto_sans_18);
    lv_xml_register_font(NULL, "noto_sans_20", &noto_sans_20);
    lv_xml_register_font(NULL, "noto_sans_24", &noto_sans_24);
    lv_xml_register_font(NULL, "noto_sans_26", &noto_sans_26);
    lv_xml_register_font(NULL, "noto_sans_28", &noto_sans_28);

    // Noto Sans Bold fonts (for future use)
    lv_xml_register_font(NULL, "noto_sans_bold_14", &noto_sans_bold_14);
    lv_xml_register_font(NULL, "noto_sans_bold_16", &noto_sans_bold_16);
    lv_xml_register_font(NULL, "noto_sans_bold_18", &noto_sans_bold_18);
    lv_xml_register_font(NULL, "noto_sans_bold_20", &noto_sans_bold_20);
    lv_xml_register_font(NULL, "noto_sans_bold_24", &noto_sans_bold_24);
    lv_xml_register_font(NULL, "noto_sans_bold_28", &noto_sans_bold_28);

    lv_xml_register_image(NULL, "A:assets/images/printer_400.png",
                          "A:assets/images/printer_400.png");
    lv_xml_register_image(NULL, "filament_spool", "A:assets/images/filament_spool.png");
    lv_xml_register_image(NULL, "A:assets/images/placeholder_thumb_centered.png",
                          "A:assets/images/placeholder_thumb_centered.png");
    lv_xml_register_image(NULL, "A:assets/images/thumbnail-gradient-bg.png",
                          "A:assets/images/thumbnail-gradient-bg.png");
    lv_xml_register_image(NULL, "A:assets/images/thumbnail-placeholder-160.png",
                          "A:assets/images/thumbnail-placeholder-160.png");
    lv_xml_register_image(NULL, "A:assets/images/large-extruder-icon.svg",
                          "A:assets/images/large-extruder-icon.svg");
    lv_xml_register_image(NULL, "A:assets/images/benchy_thumbnail_white.png",
                          "A:assets/images/benchy_thumbnail_white.png");
}

void register_xml_components() {
    spdlog::debug("[XML Registration] Registering XML components...");

    // Register responsive constants (AFTER globals, BEFORE components that use them)
    ui_switch_register_responsive_constants();
    register_color_picker_responsive_constants();

    // Register semantic text widgets (AFTER theme init, BEFORE components that use them)
    ui_text_init();
    ui_text_input_init(); // <text_input> with bind_text support
    ui_spinner_init();    // <spinner> with responsive sizing

    // Register no-op callback and subject for optional handlers in XML components
    // This silences warnings when components use callback/subject props with default=""
    lv_xml_register_event_cb(nullptr, "", noop_event_callback);
    lv_subject_init_int(&s_noop_subject, 0);
    lv_xml_register_subject(nullptr, "", &s_noop_subject);

    // Register custom widgets (BEFORE components that use them)
    ui_gcode_viewer_register();
    ui_spool_canvas_register(); // Needed by Spoolman panel (and AMS panel)
    ui_hsv_picker_register();   // HSV color picker for edit filament modal
    ui_ams_current_tool_init(); // AMS current tool indicator callbacks
    // NOTE: Other AMS widgets (ams_slot, filament_path_canvas) are
    // registered lazily in ui_panel_ams.cpp when the AMS panel is first accessed

    // Spoolman components (MUST be after spool_canvas registration)
    lv_xml_register_component_from_file("A:ui_xml/spoolman_spool_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/spoolman_panel.xml");

    // Core UI components
    lv_xml_register_component_from_file("A:ui_xml/icon.xml");
    lv_xml_register_component_from_file("A:ui_xml/filament_sensor_indicator.xml");
    lv_xml_register_component_from_file("A:ui_xml/filament_sensor_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/temp_display.xml");
    lv_xml_register_component_from_file("A:ui_xml/header_bar.xml");
    lv_xml_register_component_from_file("A:ui_xml/overlay_backdrop.xml");
    lv_xml_register_component_from_file("A:ui_xml/overlay_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/toast_notification.xml");
    // emergency_stop_button.xml removed - E-Stop buttons are now embedded in panels
    lv_xml_register_component_from_file("A:ui_xml/estop_confirmation_dialog.xml");
    lv_xml_register_component_from_file("A:ui_xml/klipper_recovery_dialog.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_cancel_confirm_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_completion_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/save_z_offset_modal.xml");

    // Notification history
    lv_xml_register_component_from_file("A:ui_xml/notification_history_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/notification_history_item.xml");

    // Modal dialogs
    lv_xml_register_component_from_file("A:ui_xml/modal_dialog.xml");
    lv_xml_register_component_from_file("A:ui_xml/numeric_keypad_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/runout_guidance_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/plugin_install_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/macro_enhance_modal.xml");

    // Print file components
    lv_xml_register_component_from_file("A:ui_xml/print_file_card.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_file_list_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_file_detail.xml");

    // Main navigation and panels
    lv_xml_register_component_from_file("A:ui_xml/navigation_bar.xml");
    lv_xml_register_component_from_file("A:ui_xml/home_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/controls_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/motion_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/nozzle_temp_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/bed_temp_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/extrusion_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/fan_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/ams_current_tool.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_status_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_tune_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/filament_panel.xml");

    // NOTE: AMS panel (ams_panel.xml) is registered lazily in ui_panel_ams.cpp

    // Feature parity panels
    lv_xml_register_component_from_file("A:ui_xml/macro_card.xml");
    lv_xml_register_component_from_file("A:ui_xml/macro_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/console_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/power_device_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/power_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/screws_tilt_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/input_shaper_panel.xml");

    // Print history panels
    lv_xml_register_component_from_file("A:ui_xml/history_list_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/history_list_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/history_detail_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/history_dashboard_panel.xml");

    // Settings components (must be registered before settings_panel)
    lv_xml_register_component_from_file("A:ui_xml/setting_section_header.xml");
    lv_xml_register_component_from_file("A:ui_xml/setting_toggle_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/setting_dropdown_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/setting_action_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/setting_info_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/setting_slider_row.xml");
    lv_xml_register_component_from_file("A:ui_xml/settings_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/restart_prompt_dialog.xml");
    lv_xml_register_component_from_file("A:ui_xml/theme_restart_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/factory_reset_modal.xml");

    // Calibration panels (overlays launched from settings)
    lv_xml_register_component_from_file("A:ui_xml/calibration_zoffset_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/calibration_pid_panel.xml");

    // Bed mesh modals (must be registered before bed_mesh_panel which uses them)
    lv_xml_register_component_from_file("A:ui_xml/bed_mesh_calibrate_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/bed_mesh_rename_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/bed_mesh_save_config_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/bed_mesh_panel.xml");

    // Settings overlay panels
    lv_xml_register_component_from_file("A:ui_xml/display_settings_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/filament_sensors_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/macro_buttons_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/network_settings_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/retraction_settings_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/timelapse_settings_overlay.xml");
    lv_xml_register_component_from_file("A:ui_xml/hidden_network_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/network_test_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/filament_preset_edit_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/wifi_network_item.xml");

    // Development tools
    lv_xml_register_component_from_file("A:ui_xml/memory_stats_overlay.xml");

    // Additional panels
    lv_xml_register_component_from_file("A:ui_xml/advanced_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/test_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/print_select_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/gcode_test_panel.xml");
    lv_xml_register_component_from_file("A:ui_xml/glyphs_panel.xml");

    // App layout
    lv_xml_register_component_from_file("A:ui_xml/app_layout.xml");

    // Wizard components
    lv_xml_register_component_from_file("A:ui_xml/wizard_header_bar.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_container.xml");
    lv_xml_register_component_from_file("A:ui_xml/network_list_item.xml");
    lv_xml_register_component_from_file("A:ui_xml/wifi_password_modal.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_wifi_setup.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_connection.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_printer_identify.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_heater_select.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_fan_select.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_led_select.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_filament_sensor_select.xml");
    lv_xml_register_component_from_file("A:ui_xml/wizard_summary.xml");

    spdlog::debug("[XML Registration] XML component registration complete");
}

} // namespace helix
