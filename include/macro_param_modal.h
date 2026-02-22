// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ui_modal.h"

#include "favorite_macro_widget.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace helix {

/// Callback invoked when user confirms macro execution with parameters
using MacroExecuteCallback = std::function<void(const std::map<std::string, std::string>& params)>;

/// Modal dialog that prompts for macro parameter values before execution.
/// Dynamically creates labeled textarea fields for each detected parameter.
class MacroParamModal : public Modal {
  public:
    MacroParamModal() = default;
    ~MacroParamModal() override = default;

    const char* get_name() const override {
        return "Macro Parameters";
    }
    const char* component_name() const override {
        return "macro_param_modal";
    }

    /// Show the modal for a specific macro with its detected parameters.
    /// @param parent Parent object (usually lv_screen_active())
    /// @param macro_name Display name for the subtitle
    /// @param params Detected parameters with defaults
    /// @param on_execute Called when user clicks Run with collected values
    void show_for_macro(lv_obj_t* parent, const std::string& macro_name,
                        const std::vector<MacroParam>& params, MacroExecuteCallback on_execute);

    // Static callbacks for button wiring
    static void run_cb(lv_event_t* e);
    static void cancel_cb(lv_event_t* e);

  protected:
    void on_show() override;
    void on_ok() override;
    void on_cancel() override;

  private:
    std::string macro_name_;
    std::vector<MacroParam> params_;
    MacroExecuteCallback on_execute_;
    std::vector<lv_obj_t*> textareas_; ///< One textarea per param, in order

    void populate_param_fields();
    std::map<std::string, std::string> collect_values() const;

    static MacroParamModal* s_active_instance_;
};

} // namespace helix
