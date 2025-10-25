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
 * @brief Controls Panel - Launcher and sub-screen management
 *
 * The controls panel provides a card-based launcher menu for accessing
 * different manual printer control screens (motion, temperature, extrusion).
 */

/**
 * @brief Initialize controls panel subjects
 *
 * Must be called BEFORE creating XML components.
 */
void ui_panel_controls_init_subjects();

/**
 * @brief Setup event handlers for controls panel launcher cards
 *
 * Wires click handlers to each launcher card.
 *
 * @param panel_obj The controls panel object returned from lv_xml_create()
 * @param screen The screen object (parent for overlay panels)
 */
void ui_panel_controls_wire_events(lv_obj_t* panel_obj, lv_obj_t* screen);

/**
 * @brief Get the controls panel object
 *
 * @return The controls panel object, or NULL if not created yet
 */
lv_obj_t* ui_panel_controls_get();

/**
 * @brief Set the controls panel object
 *
 * Called after XML creation to store reference.
 *
 * @param panel_obj The controls panel object
 */
void ui_panel_controls_set(lv_obj_t* panel_obj);
