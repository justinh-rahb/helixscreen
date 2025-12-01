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

#include "ui_wizard_helpers.h"

#include "config.h"
#include "moonraker_api.h"

#include <spdlog/spdlog.h>

namespace WizardHelpers {

std::string build_dropdown_options(const std::vector<std::string>& items,
                                   std::function<bool(const std::string&)> filter,
                                   bool include_none) {
    std::string options_str;

    // Add filtered items
    for (const auto& item : items) {
        // Apply filter if provided
        if (filter && !filter(item)) {
            continue;
        }

        if (!options_str.empty()) {
            options_str += "\n";
        }
        options_str += item;
    }

    // Optionally append "None" option
    if (include_none) {
        if (!options_str.empty()) {
            options_str += "\n";
        }
        options_str += "None";
    }

    return options_str;
}

int find_item_index(const std::vector<std::string>& items, const std::string& name,
                    int default_index) {
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i] == name) {
            return static_cast<int>(i);
        }
    }
    return default_index;
}

int restore_dropdown_selection(lv_obj_t* dropdown, lv_subject_t* subject,
                               const std::vector<std::string>& items, const char* config_path,
                               MoonrakerAPI* api,
                               std::function<std::string(MoonrakerAPI*)> guess_method_fn,
                               const char* log_prefix) {
    int selected_index = 0; // Default to first option

    Config* config = Config::get_instance();
    if (config) {
        // Try to restore from saved config
        std::string saved_item = config->get<std::string>(config_path, "");
        if (!saved_item.empty()) {
            // Search for saved name in items
            selected_index = find_item_index(items, saved_item, 0);
            if (selected_index > 0 || (!items.empty() && items[0] == saved_item)) {
                spdlog::debug("{} Restored selection: {}", log_prefix, saved_item);
            }
        } else if (api && guess_method_fn) {
            // No saved config, try guessing via MoonrakerAPI
            std::string guessed = guess_method_fn(api);
            if (!guessed.empty()) {
                selected_index = find_item_index(items, guessed, 0);
                if (selected_index > 0 || (!items.empty() && items[0] == guessed)) {
                    spdlog::debug("{} Auto-selected: {}", log_prefix, guessed);
                }
            }
        }
    }

    // Update dropdown and subject
    if (dropdown) {
        lv_dropdown_set_selected(dropdown, selected_index);
    }
    if (subject) {
        lv_subject_set_int(subject, selected_index);
    }

    spdlog::debug("{} Configured dropdown with {} options, selected index: {}", log_prefix,
                  items.size(), selected_index);

    return selected_index;
}

bool save_dropdown_selection(lv_subject_t* subject, const std::vector<std::string>& items,
                             const char* config_path, const char* log_prefix) {
    if (!subject) {
        spdlog::warn("{} Cannot save selection: null subject", log_prefix);
        return false;
    }

    Config* config = Config::get_instance();
    if (!config) {
        spdlog::warn("{} Cannot save selection: config not available", log_prefix);
        return false;
    }

    // Get selection index from subject
    int index = lv_subject_get_int(subject);
    if (index < 0 || index >= static_cast<int>(items.size())) {
        spdlog::warn("{} Cannot save selection: index {} out of range (0-{})", log_prefix, index,
                     items.size() - 1);
        return false;
    }

    // Save item name (not index) to config
    const std::string& item_name = items[index];
    config->set(config_path, item_name);
    spdlog::debug("{} Saved selection: {}", log_prefix, item_name);

    return true;
}

void init_int_subject(lv_subject_t* subject, int32_t initial_value, const char* subject_name) {
    lv_subject_init_int(subject, initial_value);
    lv_xml_register_subject(nullptr, subject_name, subject);
}

} // namespace WizardHelpers
