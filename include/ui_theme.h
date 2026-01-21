// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file ui_theme.h
 * @brief Responsive design token system with breakpoints, spacing, and theme colors
 *
 * @pattern Singleton with breakpoint suffixes (_small/_medium/_large) and light/dark variants
 * @threading Main thread only
 * @gotchas ui_theme_get_color() looks up tokens; ui_theme_parse_color() parses hex literals only
 */

#pragma once

#include "ui_fonts.h"

#include "lvgl/lvgl.h"

// Forward declare
namespace helix {
struct ThemeData;
}

// Theme colors: Use ui_theme_get_color() to retrieve from globals.xml
// Available tokens: primary_color, text_primary, text_secondary, success_color, etc.

// Layout constants
#define UI_NAV_WIDTH_PERCENT 10 // Nav bar is 1/10th of screen width
#define UI_NAV_ICON_SIZE 64     // Base icon size for 1024x800
#define UI_NAV_PADDING 16       // Padding between elements

// Calculate nav width based on actual screen
#define UI_NAV_WIDTH(screen_w) ((screen_w) / 10)

// Responsive breakpoints (based on max(width, height))
// Optimized for our target hardware: 480x320, 800x480, 1024x600, 1280x720
#define UI_BREAKPOINT_SMALL_MAX 480 // 480x320 and below → SMALL
#define UI_BREAKPOINT_MEDIUM_MAX                                                                   \
    800 // 481-800: 800x480, up to 800x600 → MEDIUM
        // >800: 1024x600, 1280x720+ → LARGE

// Screen size targets (reference only, use breakpoints above for logic)
#define UI_SCREEN_LARGE_W 1280
#define UI_SCREEN_LARGE_H 720
#define UI_SCREEN_MEDIUM_W 1024
#define UI_SCREEN_MEDIUM_H 600
#define UI_SCREEN_SMALL_W 800
#define UI_SCREEN_SMALL_H 480
#define UI_SCREEN_TINY_W 480
#define UI_SCREEN_TINY_H 320

// Spacing tokens available (use ui_theme_get_spacing() to read values):
//   space_xxs: 2/3/4px  (small/medium/large breakpoints)
//   space_xs:  4/5/6px
//   space_sm:  6/7/8px
//   space_md:  8/10/12px
//   space_lg:  12/16/20px
//   space_xl:  16/20/24px

// Opacity constants (matching globals.xml values)
#define UI_DISABLED_OPA 128 // disabled_opa - 50% opacity for disabled/dimmed elements

// Responsive navigation bar width (applied in C++ based on screen size)
#define UI_NAV_WIDTH_TINY 64   // Tiny screens: 42px button + margins
#define UI_NAV_WIDTH_SMALL 76  // Small screens: 60px button + 8px padding each side
#define UI_NAV_WIDTH_MEDIUM 94 // Medium screens: 70px button + 12px padding each side
#define UI_NAV_WIDTH_LARGE 102 // Large screens: 70px button + 16px padding each side

// Semantic fonts: Use ui_theme_get_font() to retrieve responsive fonts from globals.xml
// Available tokens: font_heading, font_body, font_small

/**
 * @brief Initialize LVGL theme system
 *
 * Creates and applies LVGL theme with light or dark mode.
 * Must be called before creating any widgets.
 *
 * @param display LVGL display instance
 * @param use_dark_mode true for dark theme, false for light theme
 */
void ui_theme_init(lv_display_t* display, bool use_dark_mode);

/**
 * @brief Get breakpoint suffix for a given resolution
 *
 * Returns the suffix string used to select responsive variants from globals.xml.
 * Useful for testing and debugging responsive behavior.
 *
 * @param max_resolution Maximum of horizontal and vertical resolution
 * @return "_small" (≤480), "_medium" (481-800), or "_large" (>800)
 */
const char* ui_theme_get_breakpoint_suffix(int32_t max_resolution);

/**
 * @brief Register responsive spacing tokens (space_* system)
 *
 * Registers the unified spacing scale (space_xxs through space_xl) based on
 * current display resolution. This is the preferred system - use space_*
 * tokens instead of the deprecated padding_* and gap_* tokens.
 *
 * Called automatically by ui_theme_init().
 *
 * @param display LVGL display instance
 */
void ui_theme_register_responsive_spacing(lv_display_t* display);

/**
 * @brief Register responsive font constants
 *
 * Selects font sizes based on screen size breakpoints.
 * Called automatically by ui_theme_init().
 *
 * @param display LVGL display instance
 */
void ui_theme_register_responsive_fonts(lv_display_t* display);

/**
 * @brief Toggle between light and dark themes
 *
 * Switches theme mode, re-registers XML color constants, updates theme
 * styles in-place, and forces a widget tree refresh. All existing widgets
 * will update to the new color scheme without recreation.
 */
void ui_theme_toggle_dark_mode();

/**
 * @brief Force style refresh on widget tree
 *
 * Walks the widget tree starting from root and forces style recalculation
 * on each widget. This is called automatically by ui_theme_toggle_dark_mode()
 * but can be used independently for custom refresh scenarios.
 *
 * @param root Root widget to start refresh from (typically lv_screen_active())
 */
void ui_theme_refresh_widget_tree(lv_obj_t* root);

/**
 * @brief Check if dark mode is currently active
 *
 * @return true if dark mode enabled, false if light mode
 */
bool ui_theme_is_dark_mode();

/**
 * @brief Get currently active theme data
 * @return Reference to active theme (valid after ui_theme_init)
 */
const helix::ThemeData& ui_theme_get_active_theme();

/**
 * @brief Parse hex color string to lv_color_t
 *
 * Supports both "#RRGGBB" and "RRGGBB" formats.
 *
 * @param hex_str Hex color string (e.g., "#FF0000" or "FF0000")
 * @return LVGL color object
 */
lv_color_t ui_theme_parse_hex_color(const char* hex_str);

/**
 * @brief Get themed color by base name
 *
 * Retrieves color from globals.xml with automatic _light/_dark
 * variant selection based on current theme mode.
 *
 * Example: base_name="card_bg" → "card_bg_light" or "card_bg_dark"
 *
 * @param base_name Base color name (without _light/_dark suffix)
 * @return Themed color for current mode
 */
lv_color_t ui_theme_get_color(const char* base_name);

/**
 * @brief Apply themed background color to widget
 *
 * Sets widget background color using theme-aware color lookup.
 * Automatically selects _light or _dark variant.
 *
 * @param obj Widget to style
 * @param base_name Base color name (without _light/_dark suffix)
 * @param part Widget part to style (default: LV_PART_MAIN)
 */
void ui_theme_apply_bg_color(lv_obj_t* obj, const char* base_name, lv_part_t part = LV_PART_MAIN);

/**
 * @brief Get font height in pixels
 *
 * Returns the line height of the font, useful for layout calculations.
 *
 * @param font LVGL font pointer
 * @return Font height in pixels
 */
int32_t ui_theme_get_font_height(const lv_font_t* font);

/**
 * @brief Set overlay widget width to fill space after nav bar
 *
 * Utility for overlay panels/widgets that use x="#nav_width" positioning.
 * Sets width to (screen_width - nav_width).
 *
 * @param obj Widget to resize (typically an overlay panel or detail view)
 * @param screen Parent screen to calculate width from
 */
void ui_set_overlay_width(lv_obj_t* obj, lv_obj_t* screen);

/**
 * @brief Get spacing value from unified space_* system
 *
 * Reads the registered space_* constant value from LVGL's XML constant registry.
 * The value returned is responsive - it depends on what breakpoint was used
 * during theme initialization (small/medium/large).
 *
 * This function is the C++ interface to the unified spacing system. All spacing
 * in C++ code should use this function to stay consistent with XML layouts.
 *
 * Available tokens:
 *   space_xxs: 2/3/4px  (small/medium/large)
 *   space_xs:  4/5/6px
 *   space_sm:  6/7/8px
 *   space_md:  8/10/12px
 *   space_lg:  12/16/20px
 *   space_xl:  16/20/24px
 *
 * @param token Spacing token name (e.g., "space_lg", "space_md", "space_xs")
 * @return Spacing value in pixels, or 0 if token not found
 */
int32_t ui_theme_get_spacing(const char* token);

/**
 * @brief Get responsive font by token name
 *
 * Retrieves font pointer from globals.xml based on current display breakpoint.
 * The font returned is responsive - it depends on the breakpoint used during
 * theme initialization (small/medium/large).
 *
 * Available tokens:
 *   font_heading: 20/26/28px (small/medium/large breakpoints)
 *   font_body:    14/18/20px
 *   font_small:   12/16/18px
 *
 * @param token Font token name (e.g., "font_small", "font_body", "font_heading")
 * @return Font pointer, or nullptr if token not found
 */
const lv_font_t* ui_theme_get_font(const char* token);

/**
 * @brief Convert semantic size name to font token
 *
 * Maps semantic size names (xs, sm, md, lg) to font tokens (font_xs, font_small, etc.)
 * This provides a consistent size vocabulary across widgets.
 *
 * Mapping:
 *   "xs" → "font_xs"
 *   "sm" → "font_small"
 *   "md" → "font_body"
 *   "lg" → "font_heading"
 *
 * @param size Size name (xs, sm, md, lg), or NULL for default
 * @param default_size Default size to use if size is NULL (defaults to "sm")
 * @return Font token string (e.g., "font_small"). Never returns NULL.
 */
const char* ui_theme_size_to_font_token(const char* size, const char* default_size = "sm");

// ============================================================================
// Multi-File Responsive Constants API
// ============================================================================
// These functions extend the responsive constant system to work with ALL XML
// files, not just globals.xml. Component-local constants are aggregated with
// last-wins precedence (alphabetical order, so component files can override
// globals.xml values).

#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Parse an XML file and extract constants with a specific suffix
 *
 * Extracts name→value pairs for elements of the given type that end with
 * the specified suffix. The base name (suffix stripped) is used as the key.
 *
 * Example: For suffix="_small", element <px name="button_height_small" value="32"/>
 * produces entry {"button_height", "32"} in the results map.
 *
 * @param filepath Path to the XML file to parse
 * @param element_type Element type to match ("px", "color", "string")
 * @param suffix Suffix to match ("_small", "_medium", "_large", "_light", "_dark")
 * @param token_values Output map: base_name → value (last-wins if duplicate)
 */
void ui_theme_parse_xml_file_for_suffix(const char* filepath, const char* element_type,
                                        const char* suffix,
                                        std::unordered_map<std::string, std::string>& token_values);

/**
 * @brief Find all XML files in a directory
 *
 * Returns a sorted list of *.xml file paths in the given directory.
 * Files are sorted alphabetically to ensure deterministic processing order
 * (important for last-wins precedence).
 *
 * Does NOT recurse into subdirectories.
 *
 * @param directory Directory path to search
 * @return Sorted vector of full file paths, empty if directory doesn't exist
 */
std::vector<std::string> ui_theme_find_xml_files(const char* directory);

/**
 * @brief Parse all XML files in a directory for constants with a specific suffix
 *
 * Aggregates constants from all XML files in the directory. Files are processed
 * in alphabetical order, so later files (by name) override earlier ones.
 * This allows component files to override globals.xml values.
 *
 * @param directory Directory containing XML files
 * @param element_type Element type to match ("px", "color", "string")
 * @param suffix Suffix to match ("_small", "_medium", "_large", "_light", "_dark")
 * @return Map of base_name → value for all matching constants
 */
std::unordered_map<std::string, std::string>
ui_theme_parse_all_xml_for_suffix(const char* directory, const char* element_type,
                                  const char* suffix);

/**
 * @brief Parse all XML files in a directory for ALL elements of a given type
 *
 * Aggregates constants from all XML files in the directory. Files are processed
 * in alphabetical order, so later files (by name) override earlier ones.
 * Unlike ui_theme_parse_all_xml_for_suffix(), this returns ALL elements regardless
 * of suffix, with the full name as the key.
 *
 * @param directory Directory containing XML files
 * @param element_type Element type to match ("px", "color", "string")
 * @return Map of name → value for all matching constants
 */
std::unordered_map<std::string, std::string>
ui_theme_parse_all_xml_for_element(const char* directory, const char* element_type);

/**
 * @brief Validate that responsive/themed constant sets are complete
 *
 * Checks for incomplete sets:
 * - Responsive px: If ANY of foo_small, foo_medium, foo_large exist but NOT ALL -> warn
 * - Themed colors: If ONLY bar_light OR ONLY bar_dark exists -> warn
 *
 * This function is useful for:
 * - Unit tests to catch incomplete constant sets
 * - Pre-commit hooks to validate XML files before committing
 *
 * @param directory Directory containing XML files to validate
 * @return Vector of warning messages (empty if all valid)
 */
std::vector<std::string> ui_theme_validate_constant_sets(const char* directory);
