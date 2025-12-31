// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file xml_registration.h
 * @brief XML component registration in correct dependency order
 *
 * @threading Main thread only; must complete before UI creation
 * @note Fonts and images are registered via AssetManager::register_fonts/images()
 */

#pragma once

namespace helix {

/**
 * @brief Register XML components from ui_xml/ directory
 *
 * Registers all XML component definitions in dependency order.
 * Must be called after AssetManager initialization and theme init.
 */
void register_xml_components();

} // namespace helix
