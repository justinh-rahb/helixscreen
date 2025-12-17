// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_observer_guard.h"
#include "ui_panel_base.h"

#include "ams_state.h"
#include "ams_types.h"      // For SlotInfo
#include "spoolman_types.h" // For SpoolInfo

/**
 * @file ui_panel_ams.h
 * @brief AMS/Multi-filament panel - slot visualization and operations
 *
 * Displays a Bambu-inspired visualization of multi-filament units (Happy Hare, AFC)
 * with colored slots, status indicators, and load/unload operations.
 *
 * ## UI Layout (480x800 primary target):
 * ```
 * ┌─────────────────────────────────────────┐
 * │ header_bar: "Multi-Filament"            │
 * ├─────────────────────────────────────────┤
 * │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
 * │  │ Slot │ │ Slot │ │ Slot │ │ Slot │   │
 * │  │  0   │ │  1   │ │  2   │ │  3   │   │
 * │  └──────┘ └──────┘ └──────┘ └──────┘   │
 * │                                         │
 * │  [Status: Idle / Loading / etc.]        │
 * │                                         │
 * │  [Action buttons: Unload, Home, etc.]   │
 * └─────────────────────────────────────────┘
 * ```
 *
 * ## Reactive Bindings:
 * - Slot colors: `ams_slot_N_color` (int, RGB packed)
 * - Slot status: `ams_slot_N_status` (int, SlotStatus enum)
 * - Current slot: `ams_current_slot` (int, -1 if none)
 * - Action: `ams_action` (int, AmsAction enum)
 * - Action detail: `ams_action_detail` (string)
 *
 * @see AmsState for subject definitions
 * @see AmsBackend for backend operations
 */
class AmsPanel : public PanelBase {
  public:
    /**
     * @brief Construct AMS panel with dependencies
     * @param printer_state Reference to global PrinterState
     * @param api Pointer to MoonrakerAPI (may be nullptr)
     */
    AmsPanel(PrinterState& printer_state, MoonrakerAPI* api);
    ~AmsPanel() override = default;

    // === PanelBase Interface ===

    void init_subjects() override;
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;
    void on_activate() override;
    void on_deactivate() override;

    [[nodiscard]] const char* get_name() const override {
        return "AMS Panel";
    }

    [[nodiscard]] const char* get_xml_component_name() const override {
        return "ams_panel";
    }

    // === Public API ===

    /**
     * @brief Get the root panel object
     * @return Panel widget, or nullptr if not setup
     */
    [[nodiscard]] lv_obj_t* get_panel() const {
        return panel_;
    }

    /**
     * @brief Refresh slot display from backend state
     *
     * Call this after backend operations complete to update UI.
     * Normally handled automatically via AmsState observer callbacks.
     */
    void refresh_slots();

    /**
     * @brief Clear internal panel reference before UI destruction
     *
     * Called by destroy_ams_panel_ui() before deleting the LVGL object.
     * Clears panel_, slot_widgets_, and other widget references to prevent
     * dangling pointers.
     */
    void clear_panel_reference();

  private:
    // === Slot Management ===

    static constexpr int MAX_VISIBLE_SLOTS =
        16; ///< Max slots displayed (increased for 8+ gate systems)
    lv_obj_t* slot_widgets_[MAX_VISIBLE_SLOTS] = {nullptr};
    lv_obj_t* label_widgets_[MAX_VISIBLE_SLOTS] = {nullptr}; ///< Separate label layer for z-order
    lv_obj_t* labels_layer_ = nullptr; ///< Container for labels (drawn on top of all spools)

    // === Context Menu ===

    lv_obj_t* context_menu_ = nullptr; ///< Active context menu (nullptr if hidden)
    int context_menu_slot_ = -1;       ///< Slot index for active context menu

    // === Spoolman Picker ===

    lv_obj_t* spoolman_picker_ = nullptr;         ///< Spoolman spool picker modal
    int picker_target_slot_ = -1;                 ///< Slot to assign selected spool to
    std::vector<SpoolInfo> picker_spools_;        ///< Cached spools for lookup on selection
    std::shared_ptr<bool> picker_callback_guard_; ///< RAII guard for async picker callbacks

    // === Edit Modal ===

    lv_obj_t* edit_modal_ = nullptr;      ///< Edit filament modal overlay
    int edit_slot_index_ = -1;            ///< Slot being edited
    SlotInfo edit_original_slot_info_{};  ///< Original slot info (for reset)
    SlotInfo edit_slot_info_{};           ///< Working copy of slot info being edited
    int edit_remaining_pre_edit_pct_ = 0; ///< Remaining % before edit mode (for cancel)

    // === Color Picker ===

    lv_obj_t* color_picker_ = nullptr;   ///< Color picker modal overlay
    uint32_t picker_selected_color_ = 0; ///< Currently selected color in picker (RGB)

    // === Observers (RAII cleanup via ObserverGuard) ===

    ObserverGuard slots_version_observer_;
    ObserverGuard action_observer_;
    ObserverGuard current_slot_observer_;
    ObserverGuard slot_count_observer_;
    ObserverGuard path_segment_observer_;
    ObserverGuard path_topology_observer_;
    ObserverGuard dryer_progress_observer_;

    // === Dynamic Slot State ===

    int current_slot_count_ = 0;    ///< Number of slots currently created
    lv_obj_t* slot_grid_ = nullptr; ///< Container for dynamically created slots

    // === Filament Path Canvas ===

    lv_obj_t* path_canvas_ = nullptr; ///< Filament path visualization widget

    // === Dryer Card ===

    lv_obj_t* dryer_progress_fill_ = nullptr; ///< Dryer progress bar fill element
    lv_obj_t* dryer_modal_ = nullptr;         ///< Dryer presets modal overlay

    // === Edit Modal Subjects ===

    lv_subject_t edit_slot_indicator_subject_; ///< Subject for "Slot X" text
    lv_subject_t edit_color_name_subject_;     ///< Subject for color name text
    lv_subject_t edit_temp_nozzle_subject_;    ///< Subject for nozzle temp text
    lv_subject_t edit_temp_bed_subject_;       ///< Subject for bed temp text
    lv_subject_t edit_remaining_pct_subject_;  ///< Subject for remaining % text

    char edit_slot_indicator_buf_[32]; ///< Buffer for slot indicator text
    char edit_color_name_buf_[32];     ///< Buffer for color name text
    char edit_temp_nozzle_buf_[16];    ///< Buffer for nozzle temp text
    char edit_temp_bed_buf_[16];       ///< Buffer for bed temp text
    char edit_remaining_pct_buf_[16];  ///< Buffer for remaining % text

    // === Spoolman Picker Subjects ===

    lv_subject_t picker_slot_indicator_subject_; ///< Subject for "Assigning to Slot X" text
    char picker_slot_indicator_buf_[48];         ///< Buffer for picker slot indicator text

    // === Color Picker Subjects ===

    lv_subject_t color_hex_subject_;  ///< Subject for hex color text
    lv_subject_t color_name_subject_; ///< Subject for color name text
    char color_hex_buf_[16];          ///< Buffer for hex color text
    char color_name_buf_[32];         ///< Buffer for color name text

    // === Setup Helpers ===

    void setup_system_header();
    void setup_slots();
    void setup_action_buttons();
    void setup_status_display();
    void setup_dryer_card();
    void setup_path_canvas();
    void update_path_canvas_from_backend();

    /**
     * @brief Create slot widgets dynamically based on slot count
     * @param count Number of slots to create (0 to max 8)
     *
     * Deletes existing slots and creates new ones. Uses lv_xml_create()
     * to instantiate ams_slot C++ widgets, then sets their slot_index.
     */
    void create_slots(int count);

    // === Slot Count Observer ===

    static void on_slot_count_changed(lv_observer_t* observer, lv_subject_t* subject);

    // === UI Update Handlers ===

    void update_slot_colors();
    void update_slot_status(int slot_index);
    void update_action_display(AmsAction action);
    void update_current_slot_highlight(int slot_index);
    void update_current_loaded_display(int slot_index);
    void update_tray_size();

    // === Event Callbacks (static trampolines) ===

    static void on_slot_clicked(lv_event_t* e);
    static void on_unload_clicked(lv_event_t* e);
    static void on_reset_clicked(lv_event_t* e);

    // === Observer Callbacks ===

    static void on_slots_version_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void on_action_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void on_current_slot_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void on_path_state_changed(lv_observer_t* observer, lv_subject_t* subject);

    // === Path Canvas Callback ===

    static void on_path_slot_clicked(int slot_index, void* user_data);

    // === Context Menu Callbacks (static trampolines) ===

    static void on_context_backdrop_clicked(lv_event_t* e);
    static void on_context_load_clicked(lv_event_t* e);
    static void on_context_unload_clicked(lv_event_t* e);
    static void on_context_edit_clicked(lv_event_t* e);

    // === Context Menu Management ===

    void show_context_menu(int slot_index, lv_obj_t* near_widget);

    // === Spoolman Integration ===

    void sync_spoolman_active_spool();

    // === Spoolman Picker Management ===

    void show_spoolman_picker(int slot_index);
    void hide_spoolman_picker();
    void populate_spoolman_picker();

    // === Action Handlers (public for XML event callbacks) ===
  public:
    void handle_slot_tap(int slot_index);
    void handle_unload();
    void handle_reset();
    void handle_bypass_toggle();

    // Dryer handlers
    void handle_dryer_preset(float temp_c, int duration_min, int fan_pct);
    void handle_dryer_stop();

    // Context menu handlers (public for XML event callbacks)
    void hide_context_menu();
    void handle_context_load();
    void handle_context_unload();
    void handle_context_edit();
    void handle_context_spoolman();

    // Spoolman picker handlers (public for XML event callbacks)
    void handle_picker_close();
    void handle_picker_unlink();
    void handle_picker_spool_selected(int spool_id);

    // Edit modal handlers (public for XML event callbacks)
    void handle_edit_modal_close();
    void handle_edit_vendor_changed(int vendor_index);
    void handle_edit_material_changed(int material_index);
    void handle_edit_color_clicked();
    void handle_edit_remaining_changed(int percent);
    void handle_edit_remaining_edit();   ///< Enter edit mode for remaining weight
    void handle_edit_remaining_accept(); ///< Accept edited remaining weight
    void handle_edit_remaining_cancel(); ///< Cancel edit mode, revert to original
    void handle_edit_sync_spoolman();
    void handle_edit_reset();
    void handle_edit_save();

    // Color picker handlers (public for XML event callbacks)
    void handle_color_picker_close();
    void handle_color_swatch_clicked(lv_obj_t* swatch);
    void handle_color_picker_cancel();
    void handle_color_picker_select();

  private:
    // Edit modal helpers
    void show_edit_modal(int slot_index);
    void hide_edit_modal();
    void update_edit_modal_ui();
    void update_edit_temp_display();
    bool is_edit_dirty() const;      ///< Check if edit data differs from original
    void update_sync_button_state(); ///< Enable/disable sync button based on dirty state

    // Color picker helpers
    void show_color_picker();
    void hide_color_picker();
    void update_color_picker_selection(uint32_t color_rgb, bool from_hsv_picker = false);
};

/**
 * @brief Get global AMS panel singleton
 *
 * Creates the panel on first call, returns cached instance thereafter.
 * Panel is lazily initialized - widgets registered and XML created on first access.
 *
 * @return Reference to global AmsPanel instance
 */
AmsPanel& get_global_ams_panel();

/**
 * @brief Destroy the AMS panel UI to free memory
 *
 * Deletes the LVGL panel object and canvas buffers. The C++ AmsPanel
 * object and widget registrations remain for quick recreation.
 * Call this when the panel is closed to free memory on embedded systems.
 */
void destroy_ams_panel_ui();
