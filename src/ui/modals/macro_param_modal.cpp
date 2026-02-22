// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "macro_param_modal.h"

#include "ui_event_safety.h"

#include <spdlog/spdlog.h>

using namespace helix;

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
    s_active_instance_ = nullptr;
    hide();
}

void MacroParamModal::on_cancel() {
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
        // Container for label + textarea
        lv_obj_t* field = lv_obj_create(param_list);
        lv_obj_set_width(field, LV_PCT(100));
        lv_obj_set_height(field, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(field, 0, 0);
        lv_obj_set_style_pad_gap(field, 2, 0);
        lv_obj_set_flex_flow(field, LV_FLEX_FLOW_COLUMN);
        lv_obj_remove_flag(field, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(field, 0, 0);
        lv_obj_set_style_border_width(field, 0, 0);

        // Label with param name
        lv_obj_t* label = lv_label_create(field);
        // Prettify: lowercase with first letter capitalized
        std::string display_name = param.name;
        std::transform(display_name.begin(), display_name.end(), display_name.begin(), ::tolower);
        if (!display_name.empty()) {
            display_name[0] = static_cast<char>(::toupper(display_name[0]));
        }
        lv_label_set_text(label, display_name.c_str());
        lv_obj_set_style_text_font(label, lv_font_get_default(), 0);

        // Textarea with default value
        lv_obj_t* textarea = lv_textarea_create(field);
        lv_obj_set_width(textarea, LV_PCT(100));
        lv_obj_set_height(textarea, LV_SIZE_CONTENT);
        lv_textarea_set_one_line(textarea, true);
        lv_textarea_set_placeholder_text(textarea, param.name.c_str());

        if (!param.default_value.empty()) {
            lv_textarea_set_text(textarea, param.default_value.c_str());
        }

        textareas_.push_back(textarea);
    }

    spdlog::debug("[MacroParamModal] Created {} param fields for {}", params_.size(), macro_name_);
}

std::map<std::string, std::string> MacroParamModal::collect_values() const {
    std::map<std::string, std::string> result;

    for (size_t i = 0; i < params_.size() && i < textareas_.size(); ++i) {
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
