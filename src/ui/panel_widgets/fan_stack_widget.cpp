// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fan_stack_widget.h"

#include "ui_event_safety.h"
#include "ui_fan_control_overlay.h"
#include "ui_nav_manager.h"

#include "app_globals.h"
#include "display_settings_manager.h"
#include "moonraker_api.h"
#include "observer_factory.h"
#include "panel_widget_registry.h"
#include "printer_fan_state.h"
#include "printer_state.h"
#include "theme_manager.h"
#include "ui/fan_spin_animation.h"

#include <spdlog/spdlog.h>

#include <cstdio>

namespace {
const bool s_registered = [] {
    helix::register_widget_factory("fan_stack", []() {
        auto& ps = get_printer_state();
        return std::make_unique<helix::FanStackWidget>(ps);
    });
    return true;
}();
} // namespace

using namespace helix;

FanStackWidget::FanStackWidget(PrinterState& printer_state) : printer_state_(printer_state) {}

FanStackWidget::~FanStackWidget() {
    detach();
}

void FanStackWidget::attach(lv_obj_t* widget_obj, lv_obj_t* parent_screen) {
    widget_obj_ = widget_obj;
    parent_screen_ = parent_screen;
    *alive_ = true;
    lv_obj_set_user_data(widget_obj_, this);

    // Cache label, name, and icon pointers
    part_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_part_speed");
    hotend_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_hotend_speed");
    aux_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_aux_speed");
    aux_row_ = lv_obj_find_by_name(widget_obj_, "fan_stack_aux_row");
    part_icon_ = lv_obj_find_by_name(widget_obj_, "fan_stack_part_icon");
    hotend_icon_ = lv_obj_find_by_name(widget_obj_, "fan_stack_hotend_icon");
    aux_icon_ = lv_obj_find_by_name(widget_obj_, "fan_stack_aux_icon");

    // Set rotation pivots on icons (center of 16px icon)
    for (auto* icon : {part_icon_, hotend_icon_, aux_icon_}) {
        if (icon) {
            lv_obj_set_style_transform_pivot_x(icon, LV_PCT(50), 0);
            lv_obj_set_style_transform_pivot_y(icon, LV_PCT(50), 0);
        }
    }

    // Read initial animation setting
    auto& dsm = DisplaySettingsManager::instance();
    animations_enabled_ = dsm.get_animations_enabled();

    // Observe animation setting changes
    std::weak_ptr<bool> weak_alive = alive_;
    anim_settings_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
        DisplaySettingsManager::instance().subject_animations_enabled(), this,
        [weak_alive](FanStackWidget* self, int enabled) {
            if (weak_alive.expired())
                return;
            self->animations_enabled_ = (enabled != 0);
            self->refresh_all_animations();
        });

    // Observe fans_version to re-bind when fans are discovered
    version_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
        printer_state_.get_fans_version_subject(), this,
        [weak_alive](FanStackWidget* self, int /*version*/) {
            if (weak_alive.expired())
                return;
            self->bind_fans();
        });

    spdlog::debug("[FanStackWidget] Attached (animations={})", animations_enabled_);
}

void FanStackWidget::detach() {
    *alive_ = false;
    part_observer_.reset();
    hotend_observer_.reset();
    aux_observer_.reset();
    version_observer_.reset();
    anim_settings_observer_.reset();

    // Stop any running animations before clearing pointers
    if (part_icon_)
        stop_spin(part_icon_);
    if (hotend_icon_)
        stop_spin(hotend_icon_);
    if (aux_icon_)
        stop_spin(aux_icon_);

    if (widget_obj_)
        lv_obj_set_user_data(widget_obj_, nullptr);
    widget_obj_ = nullptr;
    parent_screen_ = nullptr;
    fan_control_panel_ = nullptr;
    part_label_ = nullptr;
    hotend_label_ = nullptr;
    aux_label_ = nullptr;
    aux_row_ = nullptr;
    part_icon_ = nullptr;
    hotend_icon_ = nullptr;
    aux_icon_ = nullptr;

    spdlog::debug("[FanStackWidget] Detached");
}

void FanStackWidget::set_row_density(size_t widgets_in_row) {
    if (!widget_obj_)
        return;

    // Use larger font when row has more space (≤4 widgets)
    const char* font_token = (widgets_in_row <= 4) ? "font_small" : "font_xs";
    const lv_font_t* font = theme_manager_get_font(font_token);
    if (!font)
        return;

    // Apply to all speed labels and name labels
    for (auto* label : {part_label_, hotend_label_, aux_label_}) {
        if (label)
            lv_obj_set_style_text_font(label, font, 0);
    }

    // Name labels — use fuller abbreviations when space allows
    bool spacious = (widgets_in_row <= 4);
    struct NameMapping {
        const char* obj_name;
        const char* compact_key;  // translation key for 5+ widgets per row
        const char* spacious_key; // translation key for ≤4 widgets per row
    };
    static constexpr NameMapping name_map[] = {
        {"fan_stack_part_name", "P", "Part"},
        {"fan_stack_hotend_name", "H", "HE"},
        {"fan_stack_aux_name", "C", "Chm"},
    };
    for (const auto& m : name_map) {
        lv_obj_t* lbl = lv_obj_find_by_name(widget_obj_, m.obj_name);
        if (lbl) {
            lv_obj_set_style_text_font(lbl, font, 0);
            lv_label_set_text(lbl, lv_tr(spacious ? m.spacious_key : m.compact_key));
        }
    }

    spdlog::debug("[FanStackWidget] Row density {} -> font {}", widgets_in_row, font_token);
}

void FanStackWidget::bind_fans() {
    // Reset existing per-fan observers
    part_observer_.reset();
    hotend_observer_.reset();
    aux_observer_.reset();

    part_fan_name_.clear();
    hotend_fan_name_.clear();
    aux_fan_name_.clear();

    part_speed_ = 0;
    hotend_speed_ = 0;
    aux_speed_ = 0;

    const auto& fans = printer_state_.get_fans();
    if (fans.empty()) {
        spdlog::debug("[FanStackWidget] No fans discovered yet");
        return;
    }

    // Classify fans into our three rows and set name labels
    std::string part_display, hotend_display, aux_display;
    for (const auto& fan : fans) {
        switch (fan.type) {
        case FanType::PART_COOLING:
            if (part_fan_name_.empty()) {
                part_fan_name_ = fan.object_name;
                part_display = fan.display_name;
            }
            break;
        case FanType::HEATER_FAN:
            if (hotend_fan_name_.empty()) {
                hotend_fan_name_ = fan.object_name;
                hotend_display = fan.display_name;
            }
            break;
        case FanType::CONTROLLER_FAN:
        case FanType::GENERIC_FAN:
            if (aux_fan_name_.empty()) {
                aux_fan_name_ = fan.object_name;
                aux_display = fan.display_name;
            }
            break;
        }
    }

    std::weak_ptr<bool> weak_alive = alive_;

    // Bind part fan
    if (!part_fan_name_.empty()) {
        SubjectLifetime lifetime;
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(part_fan_name_, lifetime);
        if (subject) {
            part_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this,
                [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->part_speed_ = speed;
                    self->update_label(self->part_label_, speed);
                    self->update_fan_animation(self->part_icon_, speed);
                },
                lifetime);
        }
    }

    // Bind hotend fan
    if (!hotend_fan_name_.empty()) {
        SubjectLifetime lifetime;
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(hotend_fan_name_, lifetime);
        if (subject) {
            hotend_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this,
                [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->hotend_speed_ = speed;
                    self->update_label(self->hotend_label_, speed);
                    self->update_fan_animation(self->hotend_icon_, speed);
                },
                lifetime);
        }
    }

    // Bind aux fan (hide row if none)
    if (!aux_fan_name_.empty()) {
        if (aux_row_) {
            lv_obj_remove_flag(aux_row_, LV_OBJ_FLAG_HIDDEN);
        }
        SubjectLifetime lifetime;
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(aux_fan_name_, lifetime);
        if (subject) {
            aux_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this,
                [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->aux_speed_ = speed;
                    self->update_label(self->aux_label_, speed);
                    self->update_fan_animation(self->aux_icon_, speed);
                },
                lifetime);
        }
    } else {
        if (aux_row_) {
            lv_obj_add_flag(aux_row_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    spdlog::debug("[FanStackWidget] Bound fans: part='{}' hotend='{}' aux='{}'", part_fan_name_,
                  hotend_fan_name_, aux_fan_name_);
}

void FanStackWidget::update_label(lv_obj_t* label, int speed_pct) {
    if (!label)
        return;

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d%%", speed_pct);
    lv_label_set_text(label, buf);
}

void FanStackWidget::update_fan_animation(lv_obj_t* icon, int speed_pct) {
    if (!icon)
        return;

    if (!animations_enabled_ || speed_pct <= 0) {
        helix::ui::fan_spin_stop(icon);
    } else {
        helix::ui::fan_spin_start(icon, speed_pct);
    }
}

void FanStackWidget::refresh_all_animations() {
    update_fan_animation(part_icon_, part_speed_);
    update_fan_animation(hotend_icon_, hotend_speed_);
    update_fan_animation(aux_icon_, aux_speed_);
}

void FanStackWidget::spin_anim_cb(void* var, int32_t value) {
    helix::ui::fan_spin_anim_cb(var, value);
}

void FanStackWidget::stop_spin(lv_obj_t* icon) {
    helix::ui::fan_spin_stop(icon);
}

void FanStackWidget::start_spin(lv_obj_t* icon, int speed_pct) {
    helix::ui::fan_spin_start(icon, speed_pct);
}

void FanStackWidget::handle_clicked() {
    spdlog::debug("[FanStackWidget] Clicked - opening fan control overlay");

    if (!fan_control_panel_ && parent_screen_) {
        auto& overlay = get_fan_control_overlay();

        if (!overlay.are_subjects_initialized()) {
            overlay.init_subjects();
        }
        overlay.register_callbacks();
        overlay.set_api(get_moonraker_api());

        fan_control_panel_ = overlay.create(parent_screen_);
        if (!fan_control_panel_) {
            spdlog::error("[FanStackWidget] Failed to create fan control overlay");
            return;
        }
        NavigationManager::instance().register_overlay_instance(fan_control_panel_, &overlay);
    }

    if (fan_control_panel_) {
        get_fan_control_overlay().set_api(get_moonraker_api());
        NavigationManager::instance().push_overlay(fan_control_panel_);
    }
}

void FanStackWidget::on_fan_stack_clicked(lv_event_t* e) {
    LVGL_SAFE_EVENT_CB_BEGIN("[FanStackWidget] on_fan_stack_clicked");
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
    auto* self = static_cast<FanStackWidget*>(lv_obj_get_user_data(target));
    if (!self) {
        lv_obj_t* parent = lv_obj_get_parent(target);
        while (parent && !self) {
            self = static_cast<FanStackWidget*>(lv_obj_get_user_data(parent));
            parent = lv_obj_get_parent(parent);
        }
    }
    if (self) {
        self->handle_clicked();
    } else {
        spdlog::warn("[FanStackWidget] on_fan_stack_clicked: could not recover widget instance");
    }
    LVGL_SAFE_EVENT_CB_END();
}
