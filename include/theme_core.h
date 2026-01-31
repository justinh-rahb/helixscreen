// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 16-color semantic palette for theme initialization
 *
 * Consolidates all theme colors into a single struct, replacing the long
 * parameter lists in theme_core_init/update/preview functions.
 *
 * Index mapping matches ModePalette in theme_loader.h:
 *   0: screen_bg (screen_bg)      8:  primary
 *   1: overlay_bg                9:  secondary
 *   2: card_bg                 10: tertiary
 *   3: elevated_bg         11: info
 *   4: border                  12: success
 *   5: text                    13: warning
 *   6: text_muted              14: danger
 *   7: text_subtle             15: focus
 */
typedef struct {
    lv_color_t screen_bg;   // 0: Main app background
    lv_color_t overlay_bg;  // 1: Sidebar/panel background
    lv_color_t card_bg;     // 2: Card surfaces
    lv_color_t elevated_bg; // 3: Elevated/control surfaces (buttons, inputs)
    lv_color_t border;      // 4: Borders and dividers
    lv_color_t text;        // 5: Primary text
    lv_color_t text_muted;  // 6: Secondary text
    lv_color_t text_subtle; // 7: Hint/tertiary text
    lv_color_t primary;     // 8: Primary accent
    lv_color_t secondary;   // 9: Secondary accent
    lv_color_t tertiary;    // 10: Tertiary accent
    lv_color_t info;        // 11: Info states
    lv_color_t success;     // 12: Success states
    lv_color_t warning;     // 13: Warning states
    lv_color_t danger;      // 14: Error/danger states
    lv_color_t focus;       // 15: Focus ring color
} theme_palette_t;

/**
 * @brief Initialize HelixScreen custom theme
 *
 * Creates a wrapper theme that delegates to LVGL default theme but overrides
 * input widget backgrounds to use a different color than cards. This gives
 * input widgets (textarea, dropdown) visual distinction from card backgrounds.
 *
 * @param display LVGL display to apply theme to
 * @param palette Pointer to 16-color semantic palette
 * @param is_dark Dark mode flag (true = dark mode)
 * @param base_font Base font for theme
 * @param border_radius Border radius for buttons/cards
 * @param border_width Border width in pixels (typically 1)
 * @param border_opacity Border opacity (0-255, typically 40)
 * @return Initialized theme, or NULL on failure
 *
 * Example usage:
 * @code
 *   theme_palette_t palette = {
 *       .screen_bg = lv_color_hex(0x2E3440),
 *       .primary = lv_color_hex(0x88C0D0),
 *       // ... other colors
 *   };
 *   lv_theme_t* theme = theme_core_init(display, &palette, true, font, 12, 1, 40);
 *   lv_display_set_theme(display, theme);
 * @endcode
 */
lv_theme_t* theme_core_init(lv_display_t* display, const theme_palette_t* palette, bool is_dark,
                            const lv_font_t* base_font, int32_t border_radius, int32_t border_width,
                            int32_t border_opacity);

/**
 * @brief Update theme colors in-place without recreating the theme
 *
 * Updates all theme style objects with new colors for runtime dark/light mode
 * switching. This modifies existing styles and calls lv_obj_report_style_change()
 * to trigger LVGL's style refresh cascade.
 *
 * Unlike theme_core_init(), this function preserves widget state and avoids
 * the overhead of theme recreation.
 *
 * @param is_dark true for dark mode colors, false for light mode
 * @param palette Pointer to 16-color semantic palette
 * @param border_opacity Border opacity (0-255)
 */
void theme_core_update_colors(bool is_dark, const theme_palette_t* palette, int32_t border_opacity);

/**
 * @brief Update all theme colors for live preview
 *
 * Updates theme styles in-place without requiring restart.
 * Calls lv_obj_report_style_change(NULL) internally to trigger refresh.
 *
 * @param is_dark Dark mode flag
 * @param palette Pointer to 16-color semantic palette
 * @param border_radius Corner radius in pixels
 * @param border_opacity Border opacity (0-255)
 */
void theme_core_preview_colors(bool is_dark, const theme_palette_t* palette, int32_t border_radius,
                               int32_t border_opacity);

/**
 * @brief Get the shared card style
 *
 * Returns a pointer to the persistent card style that includes:
 * - bg_color: card_bg token
 * - bg_opa: LV_OPA_COVER
 * - border_color, border_width, border_opa
 * - radius: from border_radius parameter
 *
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to card style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_card_style(void);

/**
 * @brief Get the shared dialog style
 *
 * Returns a pointer to the persistent dialog style that includes:
 * - bg_color: elevated_bg/elevated_bg token
 * - bg_opa: LV_OPA_COVER
 * - radius: from border_radius parameter
 *
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to dialog style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_dialog_style(void);

/**
 * @brief Get the shared primary text style
 *
 * Returns a pointer to the persistent text style that includes:
 * - text_color: text_primary_color token
 *
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to text style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_text_style(void);

/**
 * @brief Get the shared muted text style
 *
 * Returns a pointer to the persistent muted text style that includes:
 * - text_color: text_primary_color with reduced opacity
 * - text_opa: ~70% for muted appearance
 *
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to muted text style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_text_muted_style(void);

/**
 * @brief Get the shared subtle text style
 *
 * Returns a pointer to the persistent subtle text style that includes:
 * - text_color: text_subtle_color (even more muted than text_muted)
 *
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to subtle text style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_text_subtle_style(void);

// ============================================================================
// Icon Style Getters (Phase 2.1)
// ============================================================================
// Icon styles mirror text styles but for icon coloring. Icons in LVGL are
// font-based labels, so they use text_color for their color.

/**
 * @brief Get the shared icon text style
 *
 * Returns a pointer to the persistent icon style using text_primary_color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon text style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_text_style(void);

/**
 * @brief Get the shared icon muted style
 *
 * Returns a pointer to the persistent icon style using text_muted_color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon muted style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_muted_style(void);

/**
 * @brief Get the shared icon primary style
 *
 * Returns a pointer to the persistent icon style using primary_color (accent).
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon primary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_primary_style(void);

/**
 * @brief Get the shared icon secondary style
 *
 * Returns a pointer to the persistent icon style using secondary_color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon secondary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_secondary_style(void);

/**
 * @brief Get the shared icon tertiary style
 *
 * Returns a pointer to the persistent icon style using text_subtle_color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon tertiary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_tertiary_style(void);

/**
 * @brief Get the shared icon success style
 *
 * Returns a pointer to the persistent icon style for success state.
 * Uses a green success color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon success style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_success_style(void);

/**
 * @brief Get the shared icon warning style
 *
 * Returns a pointer to the persistent icon style for warning state.
 * Uses an amber/orange warning color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon warning style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_warning_style(void);

/**
 * @brief Get the shared icon danger style
 *
 * Returns a pointer to the persistent icon style for danger/error state.
 * Uses a red danger color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon danger style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_danger_style(void);

/**
 * @brief Get the shared icon info style
 *
 * Returns a pointer to the persistent icon style for info state.
 * Uses a blue info color.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to icon info style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_icon_info_style(void);

// ============================================================================
// Spinner Style Getters (Phase 2.3)
// ============================================================================

/**
 * @brief Get the shared spinner style
 *
 * Returns a pointer to the persistent spinner style using primary_color for arc.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to spinner style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_spinner_style(void);

// ============================================================================
// Severity Style Getters (Phase 2.3)
// ============================================================================
// Severity styles are for severity_card border colors. Each severity level
// (info, success, warning, danger) has its own style with border_color set.

/**
 * @brief Get the shared severity info style
 * @return Pointer to severity info style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_severity_info_style(void);

/**
 * @brief Get the shared severity success style
 * @return Pointer to severity success style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_severity_success_style(void);

/**
 * @brief Get the shared severity warning style
 * @return Pointer to severity warning style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_severity_warning_style(void);

/**
 * @brief Get the shared severity danger style
 * @return Pointer to severity danger style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_severity_danger_style(void);

// ============================================================================
// Button Style Getters (Phase 2.6a)
// ============================================================================
// Button styles provide reactive background colors for different button types.
// Each style sets bg_color only - text color is handled separately by the
// button widget using contrast text getters.

/**
 * @brief Get the shared button primary style
 *
 * Returns a pointer to the persistent button style using primary_color for bg.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to button primary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_primary_style(void);

/**
 * @brief Get the shared button secondary style
 *
 * Returns a pointer to the persistent button style using elevated_bg for bg.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to button secondary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_secondary_style(void);

/**
 * @brief Get the shared button danger style
 *
 * Returns a pointer to the persistent button style using danger color for bg.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to button danger style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_danger_style(void);

/**
 * @brief Get the shared button ghost style
 *
 * Returns a pointer to the persistent button style with transparent background.
 * The style updates in-place when theme_core_update_colors() is called.
 *
 * @return Pointer to button ghost style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_ghost_style(void);

/**
 * @brief Get the shared button success style
 *
 * Returns a pointer to the persistent button style using success color for bg.
 * The style updates in-place when theme_core_preview_colors() is called.
 *
 * @return Pointer to button success style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_success_style(void);

/**
 * @brief Get button tertiary style
 *
 * Returns a pointer to the persistent button style using tertiary color for bg.
 * The style updates in-place when theme_core_preview_colors() is called.
 *
 * @return Pointer to button tertiary style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_tertiary_style(void);

/**
 * @brief Get button warning style
 *
 * Returns a pointer to the persistent button style using warning color for bg.
 * The style updates in-place when theme_core_preview_colors() is called.
 *
 * @return Pointer to button warning style, or NULL if theme not initialized
 */
lv_style_t* theme_core_get_button_warning_style(void);

// ============================================================================
// Contrast Text Color Getters (Phase 2.6a)
// ============================================================================
// Contrast text getters provide appropriate text colors for dark and light
// backgrounds. These are used by button widgets to pick readable text colors
// based on background luminance.

/**
 * @brief Get text color appropriate for dark backgrounds
 *
 * Returns a light text color (near-white) suitable for display on dark
 * backgrounds. Used by button widgets to ensure text readability.
 *
 * @return Light text color for dark backgrounds (fallback: white 0xFFFFFF)
 */
lv_color_t theme_core_get_text_for_dark_bg(void);

/**
 * @brief Get text color appropriate for light backgrounds
 *
 * Returns a dark text color suitable for display on light backgrounds.
 * Used by button widgets to ensure text readability.
 *
 * @return Dark text color for light backgrounds (fallback: dark gray 0x212121)
 */
lv_color_t theme_core_get_text_for_light_bg(void);

/**
 * @brief Get contrasting text color for a given background color
 *
 * Computes luminance of the background color and returns an appropriate
 * text color for readability. Uses standard luminance formula:
 *   L = (299*R + 587*G + 114*B) / 1000
 *
 * If L < 128 (dark bg): returns light text color
 * If L >= 128 (light bg): returns dark text color
 *
 * @param bg_color Background color to contrast against
 * @return Contrasting text color for readability
 */
lv_color_t theme_core_get_contrast_text_color(lv_color_t bg_color);

#ifdef __cplusplus
}
#endif
