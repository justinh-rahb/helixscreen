// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_panel_base.h"

/**
 * @file ui_panel_filament.h
 * @brief Filament panel - Filament loading/unloading operations with safety checks
 *
 * Provides temperature-controlled filament operations:
 * - Material presets (PLA 210°C, PETG 240°C, ABS 250°C, Custom)
 * - Load/Unload/Purge operations with safety checks
 * - Temperature monitoring with visual feedback
 * - Safety warning when nozzle is too cold (< 170°C)
 *
 * ## Reactive Subjects:
 * - `filament_temp_display` - Temperature string (e.g., "210 / 240°C")
 * - `filament_status` - Status message (e.g., "✓ Ready to load")
 * - `filament_material_selected` - Selected material ID (-1=none, 0-3)
 * - `filament_extrusion_allowed` - Boolean: 1=hot enough, 0=too cold
 * - `filament_safety_warning_visible` - Boolean: 1=show warning, 0=hide
 * - `filament_warning_temps` - Warning card temp text
 *
 * ## Key Features:
 * - Temperature-driven safety logic (not a state machine)
 * - Imperative button enable/disable for performance
 * - Keypad integration for custom temperature input
 * - Visual preset selection feedback (LV_STATE_CHECKED)
 *
 * ## Migration Notes:
 * Phase 4 panel - demonstrates hybrid reactive/imperative state management.
 * Temperature updates are pushed externally via set_temp(), not observed.
 *
 * @see PanelBase for base class documentation
 * @see UITemperatureUtils for safety validation functions
 */

class FilamentPanel : public PanelBase {
  public:
    /**
     * @brief Construct FilamentPanel with injected dependencies
     *
     * @param printer_state Reference to PrinterState
     * @param api Pointer to MoonrakerAPI (for future temp commands)
     */
    FilamentPanel(PrinterState& printer_state, MoonrakerAPI* api);

    ~FilamentPanel() override = default;

    //
    // === PanelBase Implementation ===
    //

    /**
     * @brief Initialize filament subjects for XML binding
     *
     * Registers: filament_temp_display, filament_status, filament_material_selected,
     *            filament_extrusion_allowed, filament_safety_warning_visible,
     *            filament_warning_temps
     */
    void init_subjects() override;

    /**
     * @brief Setup button handlers and initial visual state
     *
     * - Wires preset buttons (PLA, PETG, ABS, Custom)
     * - Wires action buttons (Load, Unload, Purge)
     * - Configures safety warning visibility
     * - Initializes temperature display
     *
     * @param panel Root panel object from lv_xml_create()
     * @param parent_screen Parent screen for navigation
     */
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;

    const char* get_name() const override { return "Filament Panel"; }
    const char* get_xml_component_name() const override { return "filament_panel"; }

    //
    // === Public API ===
    //

    /**
     * @brief Update temperature display and safety state
     *
     * Called externally when temperature updates arrive from printer.
     * Updates subjects and triggers safety state re-evaluation.
     *
     * @param current Current nozzle temperature in °C
     * @param target Target nozzle temperature in °C
     */
    void set_temp(int current, int target);

    /**
     * @brief Get current temperature values
     *
     * @param[out] current Pointer to receive current temp (may be nullptr)
     * @param[out] target Pointer to receive target temp (may be nullptr)
     */
    void get_temp(int* current, int* target) const;

    /**
     * @brief Select a material preset
     *
     * Sets target temperature and updates visual state.
     *
     * @param material_id 0=PLA(210°C), 1=PETG(240°C), 2=ABS(250°C), 3=Custom
     */
    void set_material(int material_id);

    /**
     * @brief Get currently selected material
     * @return Material ID (-1=none, 0=PLA, 1=PETG, 2=ABS, 3=Custom)
     */
    int get_material() const { return selected_material_; }

    /**
     * @brief Check if extrusion operations are safe
     *
     * @return true if nozzle is at or above MIN_EXTRUSION_TEMP (170°C)
     */
    bool is_extrusion_allowed() const;

    /**
     * @brief Set temperature limits from Moonraker heater config
     *
     * @param min_temp Minimum allowed temperature
     * @param max_temp Maximum allowed temperature
     */
    void set_limits(int min_temp, int max_temp);

  private:
    //
    // === Subjects (owned by this panel) ===
    //

    lv_subject_t temp_display_subject_;
    lv_subject_t status_subject_;
    lv_subject_t material_selected_subject_;
    lv_subject_t extrusion_allowed_subject_;
    lv_subject_t safety_warning_visible_subject_;
    lv_subject_t warning_temps_subject_;

    // Subject storage buffers
    char temp_display_buf_[32];
    char status_buf_[64];
    char warning_temps_buf_[64];

    //
    // === Instance State ===
    //

    int nozzle_current_ = 25;
    int nozzle_target_ = 0;
    int selected_material_ = -1; // -1=none, 0=PLA, 1=PETG, 2=ABS, 3=Custom
    int nozzle_min_temp_ = 0;
    int nozzle_max_temp_ = 500;

    // Child widgets (for imperative state management)
    lv_obj_t* btn_load_ = nullptr;
    lv_obj_t* btn_unload_ = nullptr;
    lv_obj_t* btn_purge_ = nullptr;
    lv_obj_t* safety_warning_ = nullptr;
    lv_obj_t* preset_buttons_[4] = {nullptr};

    //
    // === Private Helpers ===
    //

    void update_temp_display();
    void update_status();
    void update_warning_text();
    void update_safety_state();
    void update_preset_buttons_visual();

    //
    // === Instance Handlers ===
    //

    void handle_preset_button(int material_id);
    void handle_custom_button();
    void handle_custom_temp_confirmed(float value);
    void handle_load_button();
    void handle_unload_button();
    void handle_purge_button();

    //
    // === Static Trampolines ===
    //

    static void on_preset_button_clicked(lv_event_t* e);
    static void on_custom_button_clicked(lv_event_t* e);
    static void on_load_button_clicked(lv_event_t* e);
    static void on_unload_button_clicked(lv_event_t* e);
    static void on_purge_button_clicked(lv_event_t* e);

    // Keypad callback bridge (different signature - not an LVGL event)
    static void custom_temp_keypad_cb(float value, void* user_data);
};

// ============================================================================
// DEPRECATED LEGACY API
// ============================================================================
//
// These functions provide backwards compatibility during the transition.
// New code should use the FilamentPanel class directly.
//
// Clean break: After all callers are updated, remove these wrappers and
// the global instance. See docs/PANEL_MIGRATION.md for procedure.
// ============================================================================

/**
 * @deprecated Use FilamentPanel class directly
 * @brief Legacy wrapper - initialize filament panel subjects
 */
[[deprecated("Use FilamentPanel class directly - see docs/PANEL_MIGRATION.md")]]
void ui_panel_filament_init_subjects();

/**
 * @deprecated Use FilamentPanel class directly
 * @brief Legacy wrapper - create filament panel from XML
 */
[[deprecated("Use FilamentPanel class directly - see docs/PANEL_MIGRATION.md")]]
lv_obj_t* ui_panel_filament_create(lv_obj_t* parent);

/**
 * @deprecated Use FilamentPanel class directly
 * @brief Legacy wrapper - setup event handlers for filament panel
 */
[[deprecated("Use FilamentPanel class directly - see docs/PANEL_MIGRATION.md")]]
void ui_panel_filament_setup(lv_obj_t* panel, lv_obj_t* parent_screen);

/**
 * @deprecated Use FilamentPanel::set_temp() instead
 * @brief Legacy wrapper - update temperature display
 */
[[deprecated("Use FilamentPanel::set_temp() instead")]]
void ui_panel_filament_set_temp(int current, int target);

/**
 * @deprecated Use FilamentPanel::get_temp() instead
 * @brief Legacy wrapper - get current temperature values
 */
[[deprecated("Use FilamentPanel::get_temp() instead")]]
void ui_panel_filament_get_temp(int* current, int* target);

/**
 * @deprecated Use FilamentPanel::set_material() instead
 * @brief Legacy wrapper - select material preset
 */
[[deprecated("Use FilamentPanel::set_material() instead")]]
void ui_panel_filament_set_material(int material_id);

/**
 * @deprecated Use FilamentPanel::get_material() instead
 * @brief Legacy wrapper - get selected material
 */
[[deprecated("Use FilamentPanel::get_material() instead")]]
int ui_panel_filament_get_material();

/**
 * @deprecated Use FilamentPanel::is_extrusion_allowed() instead
 * @brief Legacy wrapper - check if extrusion is allowed
 */
[[deprecated("Use FilamentPanel::is_extrusion_allowed() instead")]]
bool ui_panel_filament_is_extrusion_allowed();

/**
 * @deprecated Use FilamentPanel::set_limits() instead
 * @brief Legacy wrapper - set temperature limits
 */
[[deprecated("Use FilamentPanel::set_limits() instead")]]
void ui_panel_filament_set_limits(int min_temp, int max_temp);
