// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_ams_context_menu.h"

#include <spdlog/spdlog.h>

namespace helix::ui {

// Static member initialization
bool AmsContextMenu::callbacks_registered_ = false;

// ============================================================================
// Construction / Destruction
// ============================================================================

AmsContextMenu::AmsContextMenu() {
    spdlog::debug("[AmsContextMenu] Constructed");
}

AmsContextMenu::~AmsContextMenu() {
    hide();
    spdlog::debug("[AmsContextMenu] Destroyed");
}

AmsContextMenu::AmsContextMenu(AmsContextMenu&& other) noexcept
    : menu_(other.menu_), parent_(other.parent_), slot_index_(other.slot_index_),
      action_callback_(std::move(other.action_callback_)) {
    other.menu_ = nullptr;
    other.parent_ = nullptr;
    other.slot_index_ = -1;
}

AmsContextMenu& AmsContextMenu::operator=(AmsContextMenu&& other) noexcept {
    if (this != &other) {
        hide();

        menu_ = other.menu_;
        parent_ = other.parent_;
        slot_index_ = other.slot_index_;
        action_callback_ = std::move(other.action_callback_);

        other.menu_ = nullptr;
        other.parent_ = nullptr;
        other.slot_index_ = -1;
    }
    return *this;
}

// ============================================================================
// Public API
// ============================================================================

void AmsContextMenu::set_action_callback(ActionCallback callback) {
    action_callback_ = std::move(callback);
}

bool AmsContextMenu::show_near_widget(lv_obj_t* parent, int slot_index, lv_obj_t* near_widget) {
    // Hide any existing menu first
    hide();

    if (!parent || !near_widget) {
        spdlog::warn("[AmsContextMenu] Cannot show - missing parent or widget");
        return false;
    }

    // Register callbacks once (idempotent)
    register_callbacks();

    // Store state
    parent_ = parent;
    slot_index_ = slot_index;

    // Create context menu from XML
    menu_ = static_cast<lv_obj_t*>(lv_xml_create(parent, "ams_context_menu", nullptr));
    if (!menu_) {
        spdlog::error("[AmsContextMenu] Failed to create menu from XML");
        return false;
    }

    // Store 'this' in menu's user_data for callback traversal
    lv_obj_set_user_data(menu_, this);

    // Find the menu card to position it
    lv_obj_t* menu_card = lv_obj_find_by_name(menu_, "context_menu");

    if (menu_card) {
        // Update layout to get accurate dimensions
        lv_obj_update_layout(menu_card);

        // Get the position of the slot widget in screen coordinates
        lv_point_t slot_pos;
        lv_obj_get_coords(near_widget, (lv_area_t*)&slot_pos);

        // Calculate positioning
        int32_t screen_width = lv_obj_get_width(parent);
        int32_t menu_width = lv_obj_get_width(menu_card);
        int32_t slot_center_x = slot_pos.x + lv_obj_get_width(near_widget) / 2;
        int32_t slot_center_y = slot_pos.y + lv_obj_get_height(near_widget) / 2;

        // Position to the right of slot, or left if near screen edge
        int32_t menu_x = slot_center_x + 20;
        if (menu_x + menu_width > screen_width - 10) {
            menu_x = slot_center_x - menu_width - 20;
        }

        // Center vertically on the slot
        int32_t menu_y = slot_center_y - lv_obj_get_height(menu_card) / 2;

        // Clamp to screen bounds
        int32_t screen_height = lv_obj_get_height(parent);
        if (menu_y < 10) {
            menu_y = 10;
        }
        if (menu_y + lv_obj_get_height(menu_card) > screen_height - 10) {
            menu_y = screen_height - lv_obj_get_height(menu_card) - 10;
        }

        lv_obj_set_pos(menu_card, menu_x, menu_y);
    }

    spdlog::debug("[AmsContextMenu] Shown for slot {}", slot_index);
    return true;
}

void AmsContextMenu::hide() {
    // Check if LVGL is initialized - may be called from destructor during static destruction
    if (menu_ && lv_is_initialized()) {
        lv_obj_delete(menu_);
        menu_ = nullptr;
        slot_index_ = -1;
        spdlog::debug("[AmsContextMenu] hide()");
    }
}

// ============================================================================
// Event Handlers
// ============================================================================

void AmsContextMenu::handle_backdrop_clicked() {
    int slot = slot_index_; // Capture before hide() resets it
    spdlog::debug("[AmsContextMenu] Backdrop clicked");

    hide(); // Hide before callback (consistent with other handlers)

    if (action_callback_) {
        action_callback_(MenuAction::CANCELLED, slot);
    }
}

void AmsContextMenu::handle_load() {
    int slot = slot_index_;
    spdlog::info("[AmsContextMenu] Load requested for slot {}", slot);

    hide(); // Hide first so slot_index_ is valid in callback

    if (action_callback_) {
        action_callback_(MenuAction::LOAD, slot);
    }
}

void AmsContextMenu::handle_unload() {
    int slot = slot_index_;
    spdlog::info("[AmsContextMenu] Unload requested for slot {}", slot);

    hide();

    if (action_callback_) {
        action_callback_(MenuAction::UNLOAD, slot);
    }
}

void AmsContextMenu::handle_edit() {
    int slot = slot_index_;
    spdlog::info("[AmsContextMenu] Edit requested for slot {}", slot);

    hide();

    if (action_callback_) {
        action_callback_(MenuAction::EDIT, slot);
    }
}

void AmsContextMenu::handle_spoolman() {
    int slot = slot_index_;
    spdlog::info("[AmsContextMenu] Spoolman requested for slot {}", slot);

    hide();

    if (action_callback_) {
        action_callback_(MenuAction::SPOOLMAN, slot);
    }
}

// ============================================================================
// Static Callback Registration
// ============================================================================

void AmsContextMenu::register_callbacks() {
    if (callbacks_registered_) {
        return;
    }

    lv_xml_register_event_cb(nullptr, "ams_context_backdrop_cb", on_backdrop_cb);
    lv_xml_register_event_cb(nullptr, "ams_context_load_cb", on_load_cb);
    lv_xml_register_event_cb(nullptr, "ams_context_unload_cb", on_unload_cb);
    lv_xml_register_event_cb(nullptr, "ams_context_edit_cb", on_edit_cb);
    lv_xml_register_event_cb(nullptr, "ams_context_spoolman_cb", on_spoolman_cb);

    callbacks_registered_ = true;
    spdlog::debug("[AmsContextMenu] Callbacks registered");
}

// ============================================================================
// Static Callbacks (Instance Lookup via User Data)
// ============================================================================

AmsContextMenu* AmsContextMenu::get_instance_from_event(lv_event_t* e) {
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Traverse parent chain to find menu root with user_data
    lv_obj_t* obj = target;
    while (obj) {
        void* user_data = lv_obj_get_user_data(obj);
        if (user_data) {
            return static_cast<AmsContextMenu*>(user_data);
        }
        obj = lv_obj_get_parent(obj);
    }

    spdlog::warn("[AmsContextMenu] Could not find instance from event target");
    return nullptr;
}

void AmsContextMenu::on_backdrop_cb(lv_event_t* e) {
    auto* self = get_instance_from_event(e);
    if (self) {
        self->handle_backdrop_clicked();
    }
}

void AmsContextMenu::on_load_cb(lv_event_t* e) {
    auto* self = get_instance_from_event(e);
    if (self) {
        self->handle_load();
    }
}

void AmsContextMenu::on_unload_cb(lv_event_t* e) {
    auto* self = get_instance_from_event(e);
    if (self) {
        self->handle_unload();
    }
}

void AmsContextMenu::on_edit_cb(lv_event_t* e) {
    auto* self = get_instance_from_event(e);
    if (self) {
        self->handle_edit();
    }
}

void AmsContextMenu::on_spoolman_cb(lv_event_t* e) {
    auto* self = get_instance_from_event(e);
    if (self) {
        self->handle_spoolman();
    }
}

} // namespace helix::ui
