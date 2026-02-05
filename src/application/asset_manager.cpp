// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "asset_manager.h"

#include "ui_fonts.h"

#include <spdlog/spdlog.h>

#include <lvgl.h>

// Static member definitions
bool AssetManager::s_fonts_registered = false;
bool AssetManager::s_images_registered = false;

void AssetManager::register_fonts() {
    if (s_fonts_registered) {
        spdlog::debug("[AssetManager] Fonts already registered, skipping");
        return;
    }

    spdlog::debug("[AssetManager] Registering fonts...");

    // Material Design Icons (various sizes for different UI elements)
    // Source: https://pictogrammers.com/library/mdi/
    lv_xml_register_font(nullptr, "mdi_icons_64", &mdi_icons_64);
    lv_xml_register_font(nullptr, "mdi_icons_48", &mdi_icons_48);
    lv_xml_register_font(nullptr, "mdi_icons_32", &mdi_icons_32);
    lv_xml_register_font(nullptr, "mdi_icons_24", &mdi_icons_24);
    lv_xml_register_font(nullptr, "mdi_icons_16", &mdi_icons_16);

    // Montserrat text fonts - used by semantic text components:
    // - text_heading uses font_heading (20/26/28 for small/medium/large breakpoints)
    // - text_body uses font_body (14/18/20 for small/medium/large breakpoints)
    // - text_small uses font_small (12/16/18 for small/medium/large breakpoints)
    // NOTE: Registering as "montserrat_*" for XML compatibility but using noto_sans_* fonts
    lv_xml_register_font(nullptr, "montserrat_10", &noto_sans_10);
    lv_xml_register_font(nullptr, "montserrat_12", &noto_sans_12);
    lv_xml_register_font(nullptr, "montserrat_14", &noto_sans_14);
    lv_xml_register_font(nullptr, "montserrat_16", &noto_sans_16);
    lv_xml_register_font(nullptr, "montserrat_18", &noto_sans_18);
    lv_xml_register_font(nullptr, "montserrat_20", &noto_sans_20);
    lv_xml_register_font(nullptr, "montserrat_24", &noto_sans_24);
    lv_xml_register_font(nullptr, "montserrat_26", &noto_sans_26);
    lv_xml_register_font(nullptr, "montserrat_28", &noto_sans_28);

    // Noto Sans fonts - same sizes as Montserrat, with extended Unicode support
    // (includes ©®™€£¥°±•… and other symbols)
    lv_xml_register_font(nullptr, "noto_sans_10", &noto_sans_10);
    lv_xml_register_font(nullptr, "noto_sans_12", &noto_sans_12);
    lv_xml_register_font(nullptr, "noto_sans_14", &noto_sans_14);
    lv_xml_register_font(nullptr, "noto_sans_16", &noto_sans_16);
    lv_xml_register_font(nullptr, "noto_sans_18", &noto_sans_18);
    lv_xml_register_font(nullptr, "noto_sans_20", &noto_sans_20);
    lv_xml_register_font(nullptr, "noto_sans_24", &noto_sans_24);
    lv_xml_register_font(nullptr, "noto_sans_26", &noto_sans_26);
    lv_xml_register_font(nullptr, "noto_sans_28", &noto_sans_28);

    // Noto Sans Light fonts (for text_small)
    lv_xml_register_font(nullptr, "noto_sans_light_10", &noto_sans_light_10);
    lv_xml_register_font(nullptr, "noto_sans_light_12", &noto_sans_light_12);
    lv_xml_register_font(nullptr, "noto_sans_light_14", &noto_sans_light_14);
    lv_xml_register_font(nullptr, "noto_sans_light_16", &noto_sans_light_16);
    lv_xml_register_font(nullptr, "noto_sans_light_18", &noto_sans_light_18);

    // Noto Sans Bold fonts
    lv_xml_register_font(nullptr, "noto_sans_bold_14", &noto_sans_bold_14);
    lv_xml_register_font(nullptr, "noto_sans_bold_16", &noto_sans_bold_16);
    lv_xml_register_font(nullptr, "noto_sans_bold_18", &noto_sans_bold_18);
    lv_xml_register_font(nullptr, "noto_sans_bold_20", &noto_sans_bold_20);
    lv_xml_register_font(nullptr, "noto_sans_bold_24", &noto_sans_bold_24);
    lv_xml_register_font(nullptr, "noto_sans_bold_28", &noto_sans_bold_28);

    s_fonts_registered = true;
    spdlog::debug("[AssetManager] Fonts registered successfully");
}

void AssetManager::register_images() {
    if (s_images_registered) {
        spdlog::debug("[AssetManager] Images already registered, skipping");
        return;
    }

    spdlog::debug("[AssetManager] Registering images...");

    // Printer and UI images
    lv_xml_register_image(nullptr, "A:assets/images/printer_400.png",
                          "A:assets/images/printer_400.png");
    lv_xml_register_image(nullptr, "filament_spool", "A:assets/images/filament_spool.png");
    lv_xml_register_image(nullptr, "A:assets/images/placeholder_thumb_centered.png",
                          "A:assets/images/placeholder_thumb_centered.png");
    lv_xml_register_image(nullptr, "A:assets/images/thumbnail-gradient-bg.png",
                          "A:assets/images/thumbnail-gradient-bg.png");
    lv_xml_register_image(nullptr, "A:assets/images/thumbnail-placeholder.png",
                          "A:assets/images/thumbnail-placeholder.png");
    lv_xml_register_image(nullptr, "A:assets/images/thumbnail-placeholder-160.png",
                          "A:assets/images/thumbnail-placeholder-160.png");
    lv_xml_register_image(nullptr, "A:assets/images/benchy_thumbnail_white.png",
                          "A:assets/images/benchy_thumbnail_white.png");

    // Pre-rendered gradient backgrounds (LVGL native .bin format for fast blitting)
    // Card gradients (print file cards in grid view)
    lv_xml_register_image(nullptr, "A:assets/images/gradient-card-small.bin",
                          "A:assets/images/gradient-card-small.bin");
    lv_xml_register_image(nullptr, "A:assets/images/gradient-card-medium.bin",
                          "A:assets/images/gradient-card-medium.bin");
    lv_xml_register_image(nullptr, "A:assets/images/gradient-card-large.bin",
                          "A:assets/images/gradient-card-large.bin");
    // Panel gradients (detail overlays: print status, file detail, history)
    lv_xml_register_image(nullptr, "A:assets/images/gradient-panel-medium.bin",
                          "A:assets/images/gradient-panel-medium.bin");
    lv_xml_register_image(nullptr, "A:assets/images/gradient-panel-large.bin",
                          "A:assets/images/gradient-panel-large.bin");
    // Pre-rendered placeholder thumbnails (for file cards without embedded thumbnails)
    lv_xml_register_image(nullptr, "A:assets/images/prerendered/thumbnail-placeholder-160.bin",
                          "A:assets/images/prerendered/thumbnail-placeholder-160.bin");
    lv_xml_register_image(nullptr, "A:assets/images/prerendered/benchy_thumbnail_white.bin",
                          "A:assets/images/prerendered/benchy_thumbnail_white.bin");

    // Flag icons (language chooser wizard) - pre-rendered ARGB8888 32x24
    lv_xml_register_image(nullptr, "flag_en", "A:assets/images/flags/flag_en.bin");
    lv_xml_register_image(nullptr, "flag_de", "A:assets/images/flags/flag_de.bin");
    lv_xml_register_image(nullptr, "flag_fr", "A:assets/images/flags/flag_fr.bin");
    lv_xml_register_image(nullptr, "flag_es", "A:assets/images/flags/flag_es.bin");
    lv_xml_register_image(nullptr, "flag_ru", "A:assets/images/flags/flag_ru.bin");
    lv_xml_register_image(nullptr, "flag_pt", "A:assets/images/flags/flag_pt.bin");
    lv_xml_register_image(nullptr, "flag_it", "A:assets/images/flags/flag_it.bin");
    lv_xml_register_image(nullptr, "flag_zh", "A:assets/images/flags/flag_zh.bin");
    lv_xml_register_image(nullptr, "flag_ja", "A:assets/images/flags/flag_ja.bin");

    s_images_registered = true;
    spdlog::debug("[AssetManager] Images registered successfully");
}

void AssetManager::register_all() {
    register_fonts();
    register_images();
}

bool AssetManager::fonts_registered() {
    return s_fonts_registered;
}

bool AssetManager::images_registered() {
    return s_images_registered;
}
