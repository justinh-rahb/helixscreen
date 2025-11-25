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

#include "ui_card.h"

#include "ui_theme.h"

#include "lvgl/lvgl.h"
#include "lvgl/src/xml/lv_xml_parser.h"
#include "lvgl/src/xml/lv_xml_widget.h"
#include "lvgl/src/xml/parsers/lv_xml_obj_parser.h"

#include <spdlog/spdlog.h>

/**
 * XML create handler for ui_card
 * Creates an lv_obj widget when <ui_card> is encountered in XML
 * and applies theme-aware defaults. Defaults are set here (not in apply)
 * because create is called exactly once, while apply may be called multiple times.
 */
static void* ui_card_xml_create(lv_xml_parser_state_t* state, const char** attrs) {
    LV_UNUSED(attrs);

    void* parent = lv_xml_state_get_parent(state);
    lv_obj_t* obj = lv_obj_create((lv_obj_t*)parent);

    if (!obj) {
        spdlog::error("[Card] Failed to create lv_obj");
        return NULL;
    }

    // Apply theme-aware defaults (can be overridden by XML attrs in apply handler)
    // 1. Background color (theme-aware: light/dark via ui_theme_get_color)
    lv_color_t bg_color = ui_theme_get_color("card_bg");
    lv_obj_set_style_bg_color(obj, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    // 2. Disabled state: 50% opacity for visual feedback
    lv_obj_set_style_opa(obj, LV_OPA_50, LV_PART_MAIN | LV_STATE_DISABLED);

    // 3. Border: 0 (no border by default)
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);

    // 4. Disable scrolling (cards are fixed containers, not scroll areas)
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    // 5. Shadow: 0 (no shadow)
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);

    // 6. Padding: 16px default
    lv_obj_set_style_pad_all(obj, 16, LV_PART_MAIN);

    spdlog::trace("[Card] Created ui_card with defaults: bg=card_bg, pad=16, border=0, shadow=0");
    return (void*)obj;
}

void ui_card_register(void) {
    // Use standard lv_xml_obj_apply for XML attr processing - defaults are in create handler
    lv_xml_register_widget("ui_card", ui_card_xml_create, lv_xml_obj_apply);
    spdlog::debug("[Card] Registered <ui_card> widget with LVGL XML system");
}
