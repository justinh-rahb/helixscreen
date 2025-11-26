// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_panel_base.h"
#include "ui_step_progress.h"

/**
 * @file ui_panel_step_test.h
 * @brief Step progress test panel for demonstrating wizard step indicators
 *
 * A test panel showcasing the ui_step_progress widget in both vertical
 * and horizontal orientations. Provides buttons to navigate through
 * wizard steps for visual testing.
 *
 * ## Key Features:
 * - Vertical step progress widget (retract wizard simulation)
 * - Horizontal step progress widget (leveling wizard simulation)
 * - Prev/Next/Complete buttons to manipulate step state
 * - Demonstrates ui_step_progress API usage
 *
 * ## Migration Notes:
 * Third panel migrated to class-based architecture (Phase 2).
 * First panel with event callbacks - uses static trampolines pattern.
 *
 * @see PanelBase for base class documentation
 * @see ui_step_progress for the widget being tested
 */
class StepTestPanel : public PanelBase {
  public:
    /**
     * @brief Construct StepTestPanel with injected dependencies
     *
     * @param printer_state Reference to PrinterState (not actively used)
     * @param api Pointer to MoonrakerAPI (not actively used)
     *
     * @note Dependencies are passed for interface consistency with PanelBase,
     *       but this panel doesn't require printer connectivity.
     */
    StepTestPanel(PrinterState& printer_state, MoonrakerAPI* api);

    ~StepTestPanel() override = default;

    //
    // === PanelBase Implementation ===
    //

    /**
     * @brief No-op for StepTestPanel (no subjects to initialize)
     */
    void init_subjects() override;

    /**
     * @brief Setup the step test panel with progress widgets and button handlers
     *
     * Creates vertical and horizontal step progress widgets, initializes
     * them to step 1, and wires up prev/next/complete button callbacks.
     *
     * @param panel Root panel object from lv_xml_create()
     * @param parent_screen Parent screen (unused for this panel)
     */
    void setup(lv_obj_t* panel, lv_obj_t* parent_screen) override;

    const char* get_name() const override { return "Step Test Panel"; }
    const char* get_xml_component_name() const override { return "step_progress_test"; }

  private:
    //
    // === Instance State ===
    //

    lv_obj_t* vertical_widget_ = nullptr;
    lv_obj_t* horizontal_widget_ = nullptr;
    int vertical_step_ = 0;
    int horizontal_step_ = 0;

    //
    // === Private Helpers ===
    //

    /**
     * @brief Create and configure the step progress widgets
     */
    void create_progress_widgets();

    /**
     * @brief Wire up button event handlers
     */
    void setup_button_handlers();

    //
    // === Button Handlers ===
    //

    void handle_prev();
    void handle_next();
    void handle_complete();

    //
    // === Static Trampolines ===
    //
    // LVGL callbacks must be static. These trampolines extract the
    // StepTestPanel* from user_data and delegate to instance methods.
    //

    static void on_prev_clicked(lv_event_t* e);
    static void on_next_clicked(lv_event_t* e);
    static void on_complete_clicked(lv_event_t* e);
};

// ============================================================================
// DEPRECATED LEGACY API
// ============================================================================
//
// These functions provide backwards compatibility during the transition.
// New code should use the StepTestPanel class directly.
//
// Clean break: After all callers are updated, remove these wrappers and
// the global instance. See docs/PANEL_MIGRATION.md for procedure.
// ============================================================================

/**
 * @deprecated Use StepTestPanel class directly
 * @brief Legacy wrapper - setup step test panel
 *
 * Creates a global StepTestPanel instance if needed and delegates to setup().
 *
 * @param panel_root Root panel object from lv_xml_create()
 */
[[deprecated("Use StepTestPanel class directly - see docs/PANEL_MIGRATION.md")]]
void ui_panel_step_test_setup(lv_obj_t* panel_root);
