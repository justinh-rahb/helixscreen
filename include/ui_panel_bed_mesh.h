// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

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

#include <vector>

/**
 * @file ui_panel_bed_mesh.h
 * @brief Bed mesh visualization panel for 3D printer bed leveling
 *
 * Provides interactive 3D visualization of printer bed mesh height maps with
 * rotation controls and color-coded height mapping.
 */

/**
 * @brief Initialize bed mesh panel subjects (if needed)
 *
 * Currently no subjects are needed for the bed mesh panel, as it uses
 * direct widget updates rather than reactive data binding. This function
 * is provided for API consistency and future expansion.
 */
void ui_panel_bed_mesh_init_subjects();

/**
 * @brief Setup event handlers after XML creation
 *
 * Initializes the bed mesh renderer, creates the canvas buffer, wires up
 * slider controls for rotation, and renders initial test data.
 *
 * @param panel_obj Root object from lv_xml_create("bed_mesh_panel")
 * @param parent_screen Parent screen for navigation
 */
void ui_panel_bed_mesh_setup(lv_obj_t* panel_obj, lv_obj_t* parent_screen);

/**
 * @brief Load mesh data and render
 *
 * Updates the renderer with new mesh height data and triggers a redraw.
 * Mesh data is expected in row-major format (mesh[row][col]) where:
 * - row = Y-axis (front to back)
 * - col = X-axis (left to right)
 * - values are absolute Z heights in mm
 *
 * @param mesh_data 2D vector of height values (row-major order)
 */
void ui_panel_bed_mesh_set_data(const std::vector<std::vector<float>>& mesh_data);

/**
 * @brief Force redraw of bed mesh visualization
 *
 * Clears the canvas and re-renders the mesh with current rotation angles.
 * Automatically called after rotation changes.
 */
void ui_panel_bed_mesh_redraw();
