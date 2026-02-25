// SPDX-License-Identifier: GPL-3.0-or-later

#include "grid_edit_mode.h"

#include "app_globals.h"
#include "panel_widget_config.h"

#include <spdlog/spdlog.h>

namespace helix {

GridEditMode::~GridEditMode() {
    if (active_) {
        exit();
    }
}

void GridEditMode::enter(lv_obj_t* container, PanelWidgetConfig* config) {
    if (active_) {
        spdlog::debug("[GridEditMode] Already active, ignoring enter()");
        return;
    }
    active_ = true;
    container_ = container;
    config_ = config;
    lv_subject_set_int(&get_home_edit_mode_subject(), 1);
    spdlog::info("[GridEditMode] Entered edit mode");
}

void GridEditMode::exit() {
    if (!active_) {
        return;
    }
    active_ = false;
    lv_subject_set_int(&get_home_edit_mode_subject(), 0);

    if (config_) {
        config_->save();
    }
    if (save_cb_) {
        save_cb_();
    }

    container_ = nullptr;
    config_ = nullptr;
    spdlog::info("[GridEditMode] Exited edit mode");
}

} // namespace helix
