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

#include "lvgl.h"

/**
 * @brief Global keyboard management for HelixScreen
 *
 * Provides a single shared keyboard instance that automatically shows/hides
 * when textareas receive focus. This avoids creating multiple keyboard instances
 * and provides consistent keyboard behavior across the application.
 *
 * Usage:
 * 1. Call ui_keyboard_init() once at application startup
 * 2. For each textarea, call ui_keyboard_register_textarea() to enable auto-show/hide
 * 3. Optionally use ui_keyboard_show()/ui_keyboard_hide() for manual control
 */

/**
 * @brief Initialize the global keyboard instance
 *
 * Creates a keyboard widget at the bottom of the screen, initially hidden.
 * Should be called once during application initialization.
 *
 * @param parent Parent object (typically lv_screen_active())
 */
void ui_keyboard_init(lv_obj_t* parent);

/**
 * @brief Register a textarea with the keyboard system
 *
 * Adds event handlers to the textarea so the keyboard automatically shows
 * when focused and hides when defocused.
 *
 * @param textarea The textarea widget to register
 */
void ui_keyboard_register_textarea(lv_obj_t* textarea);

/**
 * @brief Manually show the keyboard for a specific textarea
 *
 * @param textarea The textarea to assign to the keyboard (NULL to clear)
 */
void ui_keyboard_show(lv_obj_t* textarea);

/**
 * @brief Manually hide the keyboard
 */
void ui_keyboard_hide();

/**
 * @brief Check if the keyboard is currently visible
 *
 * @return true if keyboard is visible, false otherwise
 */
bool ui_keyboard_is_visible();

/**
 * @brief Get the global keyboard instance
 *
 * @return Pointer to the keyboard widget, or NULL if not initialized
 */
lv_obj_t* ui_keyboard_get_instance();

/**
 * @brief Set the keyboard mode
 *
 * @param mode Keyboard mode (text_lower, text_upper, special, number)
 */
void ui_keyboard_set_mode(lv_keyboard_mode_t mode);

/**
 * @brief Set keyboard position
 *
 * By default, keyboard is positioned at BOTTOM_MID. Use this to override.
 *
 * @param align Alignment (e.g., LV_ALIGN_BOTTOM_MID, LV_ALIGN_RIGHT_MID)
 * @param x_ofs X offset from alignment point
 * @param y_ofs Y offset from alignment point
 */
void ui_keyboard_set_position(lv_align_t align, int32_t x_ofs, int32_t y_ofs);
