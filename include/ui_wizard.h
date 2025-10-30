/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "lvgl/lvgl.h"

/**
 * Wizard Container - Responsive Multi-Step UI Component
 *
 * Clean separation: This component handles ONLY navigation and layout.
 * Screen content and business logic belong in the wizard screen components.
 *
 * Initialization Order (CRITICAL):
 *   1. Register XML components (globals.xml, wizard_container.xml)
 *   2. ui_wizard_init_subjects()
 *   3. ui_wizard_register_event_callbacks()
 *   4. ui_wizard_register_responsive_constants()  <- BEFORE creating XML
 *   5. ui_wizard_create(parent)
 *   6. ui_wizard_navigate_to_step(1)
 */

/**
 * Initialize wizard subjects
 *
 * Creates and registers reactive subjects for wizard state:
 * - current_step (int)
 * - total_steps (int)
 * - wizard_title (string)
 * - wizard_progress (string, e.g. "Step 2 of 7")
 * - wizard_next_button_text (string, "Next" or "Finish")
 *
 * MUST be called BEFORE creating XML components.
 */
void ui_wizard_init_subjects();

/**
 * Register responsive constants based on screen size
 *
 * Detects screen width and overrides globals.xml constants BEFORE widget creation:
 * - TINY (width < 600):   padding=6,  gap=4,  header=28, button=110
 * - SMALL (600-899):      padding=12, gap=8,  header=32, button=140
 * - LARGE (width >= 900): padding=20, gap=12, header=40, button=160
 *
 * Also sets responsive fonts (montserrat_14/16/20 for header, montserrat_16/20/24 for title).
 *
 * MUST be called AFTER ui_wizard_init_subjects() and BEFORE ui_wizard_create().
 */
void ui_wizard_register_responsive_constants();

/**
 * Register event callbacks
 *
 * Registers internal navigation callbacks:
 * - on_back_clicked
 * - on_next_clicked
 *
 * MUST be called BEFORE creating XML components.
 */
void ui_wizard_register_event_callbacks();

/**
 * Create wizard container
 *
 * Creates the wizard UI from wizard_container.xml.
 * Returns the root wizard object.
 *
 * Prerequisites:
 * - ui_wizard_init_subjects() called
 * - ui_wizard_register_event_callbacks() called
 * - ui_wizard_register_responsive_constants() called
 *
 * @param parent Parent object (typically screen root)
 * @return The wizard root object, or NULL on failure
 */
lv_obj_t* ui_wizard_create(lv_obj_t* parent);

/**
 * Navigate to specific step
 *
 * Updates all wizard subjects (title, progress, button text).
 * Handles back button visibility (hidden on step 1).
 *
 * @param step Step number (1-based, e.g. 1 = first step, 7 = last step)
 */
void ui_wizard_navigate_to_step(int step);

/**
 * Set wizard title
 *
 * Updates the wizard_title subject.
 *
 * @param title New title string
 */
void ui_wizard_set_title(const char* title);
