// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fan_stack_widget.h"

#include "app_globals.h"
#include "display_settings_manager.h"
#include "observer_factory.h"
#include "panel_widget_registry.h"
#include "printer_fan_state.h"
#include "printer_state.h"

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

/// Abbreviate a fan display name for compact display.
/// Strips " Fan" suffix, then truncates to max_len chars.
static std::string abbreviate_fan_name(const std::string& display_name, size_t max_len = 6) {
    std::string name = display_name;

    // Strip " Fan" suffix
    const std::string suffix = " Fan";
    if (name.size() > suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
        name.erase(name.size() - suffix.size());
    }

    if (name.size() > max_len) {
        name.resize(max_len);
    }
    return name;
}

/// Minimum spin duration at 100% fan speed (ms per full rotation)
static constexpr uint32_t MIN_SPIN_DURATION_MS = 600;
/// Maximum spin duration at ~1% fan speed (slow crawl)
static constexpr uint32_t MAX_SPIN_DURATION_MS = 6000;

FanStackWidget::FanStackWidget(PrinterState& printer_state) : printer_state_(printer_state) {}

FanStackWidget::~FanStackWidget() {
    detach();
}

void FanStackWidget::attach(lv_obj_t* widget_obj, lv_obj_t* parent_screen) {
    widget_obj_ = widget_obj;
    (void)parent_screen; // Read-only widget, no overlays
    *alive_ = true;

    // Cache label, name, and icon pointers
    part_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_part_speed");
    hotend_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_hotend_speed");
    aux_label_ = lv_obj_find_by_name(widget_obj_, "fan_stack_aux_speed");
    part_name_ = lv_obj_find_by_name(widget_obj_, "fan_stack_part_name");
    hotend_name_ = lv_obj_find_by_name(widget_obj_, "fan_stack_hotend_name");
    aux_name_ = lv_obj_find_by_name(widget_obj_, "fan_stack_aux_name");
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

    widget_obj_ = nullptr;
    part_label_ = nullptr;
    hotend_label_ = nullptr;
    aux_label_ = nullptr;
    part_name_ = nullptr;
    hotend_name_ = nullptr;
    aux_name_ = nullptr;
    aux_row_ = nullptr;
    part_icon_ = nullptr;
    hotend_icon_ = nullptr;
    aux_icon_ = nullptr;

    spdlog::debug("[FanStackWidget] Detached");
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

    // Update name labels with abbreviated display names
    if (part_name_ && !part_display.empty()) {
        lv_label_set_text(part_name_, abbreviate_fan_name(part_display).c_str());
    }
    if (hotend_name_ && !hotend_display.empty()) {
        lv_label_set_text(hotend_name_, abbreviate_fan_name(hotend_display).c_str());
    }
    if (aux_name_ && !aux_display.empty()) {
        lv_label_set_text(aux_name_, abbreviate_fan_name(aux_display).c_str());
    }

    std::weak_ptr<bool> weak_alive = alive_;

    // Bind part fan
    if (!part_fan_name_.empty()) {
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(part_fan_name_);
        if (subject) {
            part_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this, [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->part_speed_ = speed;
                    self->update_label(self->part_label_, speed);
                    self->update_fan_animation(self->part_icon_, speed);
                });
        }
    }

    // Bind hotend fan
    if (!hotend_fan_name_.empty()) {
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(hotend_fan_name_);
        if (subject) {
            hotend_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this, [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->hotend_speed_ = speed;
                    self->update_label(self->hotend_label_, speed);
                    self->update_fan_animation(self->hotend_icon_, speed);
                });
        }
    }

    // Bind aux fan (hide row if none)
    if (!aux_fan_name_.empty()) {
        if (aux_row_) {
            lv_obj_remove_flag(aux_row_, LV_OBJ_FLAG_HIDDEN);
        }
        lv_subject_t* subject = printer_state_.get_fan_speed_subject(aux_fan_name_);
        if (subject) {
            aux_observer_ = helix::ui::observe_int_sync<FanStackWidget>(
                subject, this, [weak_alive](FanStackWidget* self, int speed) {
                    if (weak_alive.expired())
                        return;
                    self->aux_speed_ = speed;
                    self->update_label(self->aux_label_, speed);
                    self->update_fan_animation(self->aux_icon_, speed);
                });
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
        stop_spin(icon);
    } else {
        start_spin(icon, speed_pct);
    }
}

void FanStackWidget::refresh_all_animations() {
    update_fan_animation(part_icon_, part_speed_);
    update_fan_animation(hotend_icon_, hotend_speed_);
    update_fan_animation(aux_icon_, aux_speed_);
}

void FanStackWidget::spin_anim_cb(void* var, int32_t value) {
    lv_obj_set_style_transform_rotation(static_cast<lv_obj_t*>(var), value, 0);
}

void FanStackWidget::stop_spin(lv_obj_t* icon) {
    if (!icon)
        return;
    lv_anim_delete(icon, spin_anim_cb);
    lv_obj_set_style_transform_rotation(icon, 0, 0);
}

void FanStackWidget::start_spin(lv_obj_t* icon, int speed_pct) {
    if (!icon || speed_pct <= 0)
        return;

    // Scale duration inversely with speed: 100% → MIN, 1% → MAX
    uint32_t duration =
        MAX_SPIN_DURATION_MS -
        static_cast<uint32_t>((MAX_SPIN_DURATION_MS - MIN_SPIN_DURATION_MS) * speed_pct / 100);

    // Delete existing animation and restart with new duration
    lv_anim_delete(icon, spin_anim_cb);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, icon);
    lv_anim_set_exec_cb(&anim, spin_anim_cb);
    lv_anim_set_values(&anim, 0, 3600); // 0° to 360° (LVGL uses 0.1° units)
    lv_anim_set_duration(&anim, duration);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_start(&anim);
}
