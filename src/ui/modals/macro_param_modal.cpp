// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "macro_param_modal.h"

#include "ui_event_safety.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <regex>
#include <set>

using namespace helix;

// ============================================================================
// parse_macro_params — extract Klipper gcode_macro parameters from template
// ============================================================================

std::vector<MacroParam> helix::parse_macro_params(const std::string& gcode_template) {
    std::vector<MacroParam> result;
    std::set<std::string> seen;

    // Match params.NAME, params['NAME'], params["NAME"]
    // Optional trailing |default(VALUE) or | default(VALUE)
    std::regex param_re(
        R"RE(params\.([A-Za-z_][A-Za-z0-9_]*)|params\['([A-Za-z_][A-Za-z0-9_]*)'\]|params\["([A-Za-z_][A-Za-z0-9_]*)"\])RE");

    auto it = std::sregex_iterator(gcode_template.begin(), gcode_template.end(), param_re);
    auto end = std::sregex_iterator();

    for (; it != end; ++it) {
        const auto& match = *it;

        // Extract name from whichever group matched
        std::string name;
        if (match[1].matched)
            name = match[1].str();
        else if (match[2].matched)
            name = match[2].str();
        else if (match[3].matched)
            name = match[3].str();

        // Normalize to uppercase
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        // Skip duplicates
        if (seen.count(name)) {
            continue;
        }
        seen.insert(name);

        // Try to extract |default(VALUE) after the match
        std::string default_value;
        auto suffix_start = match.suffix().first;
        auto suffix_end = gcode_template.cend();
        std::string suffix(suffix_start, suffix_end);

        // Look for |default(...) or | default(...) immediately after
        std::regex default_re(R"(^\s*\|\s*default\(([^)]*)\))");
        std::smatch default_match;
        if (std::regex_search(suffix, default_match, default_re)) {
            default_value = default_match[1].str();
            // Strip surrounding quotes from string defaults
            if (default_value.size() >= 2) {
                char first = default_value.front();
                char last = default_value.back();
                if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
                    default_value = default_value.substr(1, default_value.size() - 2);
                }
            }
        }

        result.push_back({name, default_value});
    }

    return result;
}

MacroParamModal* MacroParamModal::s_active_instance_ = nullptr;

void MacroParamModal::show_for_macro(lv_obj_t* parent, const std::string& macro_name,
                                     const std::vector<MacroParam>& params,
                                     MacroExecuteCallback on_execute) {
    macro_name_ = macro_name;
    params_ = params;
    on_execute_ = std::move(on_execute);
    textareas_.clear();

    // Register callbacks before showing (idempotent)
    lv_xml_register_event_cb(nullptr, "macro_param_modal_run_cb", MacroParamModal::run_cb);
    lv_xml_register_event_cb(nullptr, "macro_param_modal_cancel_cb", MacroParamModal::cancel_cb);

    if (!show(parent)) {
        spdlog::error("[MacroParamModal] Failed to show modal");
        return;
    }

    s_active_instance_ = this;
}

void MacroParamModal::on_show() {
    // Set subtitle to macro name
    lv_obj_t* subtitle = find_widget("modal_subtitle");
    if (subtitle) {
        lv_label_set_text(subtitle, macro_name_.c_str());
    }

    populate_param_fields();
}

void MacroParamModal::on_ok() {
    if (on_execute_) {
        auto values = collect_values();
        on_execute_(values);
    }
    textareas_.clear(); // Clear before hide() — widgets are about to be deleted
    s_active_instance_ = nullptr;
    hide();
}

void MacroParamModal::on_cancel() {
    textareas_.clear(); // Clear before hide() — widgets are about to be deleted
    s_active_instance_ = nullptr;
    hide();
}

void MacroParamModal::populate_param_fields() {
    lv_obj_t* param_list = find_widget("param_list");
    if (!param_list) {
        spdlog::error("[MacroParamModal] param_list container not found");
        return;
    }

    textareas_.clear();

    for (const auto& param : params_) {
        // Prettify: lowercase with first letter capitalized
        std::string display_name = param.name;
        std::transform(display_name.begin(), display_name.end(), display_name.begin(), ::tolower);
        if (!display_name.empty()) {
            display_name[0] = static_cast<char>(::toupper(display_name[0]));
        }

        // Create form_field component (label + themed text_input with keyboard wiring)
        const char* attrs[] = {"label",       display_name.c_str(),
                               "placeholder", param.name.c_str(),
                               nullptr,       nullptr};
        lv_obj_t* field =
            static_cast<lv_obj_t*>(lv_xml_create(param_list, "form_field", attrs));
        if (!field) {
            spdlog::warn("[MacroParamModal] Failed to create form_field for {}", param.name);
            continue;
        }

        // Find the text_input inside the form_field to set default value
        lv_obj_t* textarea = lv_obj_find_by_name(field, "field_input");
        if (textarea && !param.default_value.empty()) {
            lv_textarea_set_text(textarea, param.default_value.c_str());
        }

        textareas_.push_back(textarea);
    }

    spdlog::debug("[MacroParamModal] Created {} param fields for {}", params_.size(), macro_name_);
}

std::map<std::string, std::string> MacroParamModal::collect_values() const {
    std::map<std::string, std::string> result;

    for (size_t i = 0; i < params_.size() && i < textareas_.size(); ++i) {
        if (!textareas_[i]) {
            continue;
        }
        const char* text = lv_textarea_get_text(textareas_[i]);
        if (text && text[0] != '\0') {
            result[params_[i].name] = text;
        }
    }

    return result;
}

// Static callbacks
void MacroParamModal::run_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[MacroParamModal] run_cb");
    (void)e;
    if (s_active_instance_) {
        s_active_instance_->on_ok();
    }
    LVGL_SAFE_EVENT_CB_END();
}

void MacroParamModal::cancel_cb(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[MacroParamModal] cancel_cb");
    (void)e;
    if (s_active_instance_) {
        s_active_instance_->on_cancel();
    }
    LVGL_SAFE_EVENT_CB_END();
}
