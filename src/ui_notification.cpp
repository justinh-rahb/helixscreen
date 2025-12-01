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

#include "ui_notification.h"

#include "ui_modal.h"
#include "ui_notification_history.h"
#include "ui_status_bar.h"
#include "ui_toast.h"

#include "app_globals.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstring>
#include <thread>

// Forward declarations
static void notification_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
static void modal_ok_btn_clicked(lv_event_t* e);

// Thread tracking for auto-detection
static std::thread::id g_main_thread_id;
static std::atomic<bool> g_main_thread_id_initialized{false};

// Check if we're on the LVGL main thread
static bool is_main_thread() {
    if (!g_main_thread_id_initialized.load()) {
        return true; // Before initialization, assume main thread
    }
    return std::this_thread::get_id() == g_main_thread_id;
}

// ============================================================================
// Helper structures and callbacks for background thread marshaling
// ============================================================================

struct AsyncMessageData {
    char* message;
    ToastSeverity severity;
    uint32_t duration_ms;
};

struct AsyncErrorData {
    char* title;
    char* message;
    bool modal;
};

// Async callbacks for lv_async_call (called on main thread)
static void async_message_callback(void* user_data) {
    AsyncMessageData* data = (AsyncMessageData*)user_data;
    if (data && data->message) {
        ui_toast_show(data->severity, data->message, data->duration_ms);

        // Add to history (title is always null for these)
        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = data->severity;
        entry.was_modal = false;
        entry.was_read = false;
        entry.title[0] = '\0';
        strncpy(entry.message, data->message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());

        free(data->message);
    }
    free(data);
}

static void async_error_callback(void* user_data) {
    AsyncErrorData* data = (AsyncErrorData*)user_data;
    if (data && data->message) {
        if (data->modal && data->title) {
            // Show modal dialog for critical errors
            ui_modal_config_t config = {
                .position = {.use_alignment = true, .alignment = LV_ALIGN_CENTER, .x = 0, .y = 0},
                .backdrop_opa = 180,
                .keyboard = nullptr,
                .persistent = false,
                .on_close = nullptr};

            const char* attrs[] = {"title", data->title, "message", data->message, nullptr};

            lv_obj_t* modal = ui_modal_show("error_dialog", &config, attrs);

            if (modal) {
                lv_obj_t* ok_btn = lv_obj_find_by_name(modal, "dialog_ok_btn");
                if (ok_btn) {
                    lv_obj_add_event_cb(ok_btn, modal_ok_btn_clicked, LV_EVENT_CLICKED, modal);
                }
            }

            ui_status_bar_update_notification(NotificationStatus::ERROR);
        } else {
            // Show toast for non-critical errors
            ui_toast_show(ToastSeverity::ERROR, data->message, 6000);
        }

        // Add to history
        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = ToastSeverity::ERROR;
        entry.was_modal = data->modal;
        entry.was_read = false;

        if (data->title) {
            strncpy(entry.title, data->title, sizeof(entry.title) - 1);
            entry.title[sizeof(entry.title) - 1] = '\0';
        } else {
            entry.title[0] = '\0';
        }

        strncpy(entry.message, data->message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());

        free(data->title);
        free(data->message);
    }
    free(data);
}

// ============================================================================
// Public API functions (thread-safe with auto-detection)
// ============================================================================

void ui_notification_init() {
    // Capture main thread ID for thread-safety detection
    g_main_thread_id = std::this_thread::get_id();
    g_main_thread_id_initialized.store(true);

    // Add observer to handle notification emissions
    // (Subject itself is initialized in app_globals_init_subjects())
    lv_subject_add_observer(&get_notification_subject(), notification_observer_cb, nullptr);

    spdlog::debug("Notification system initialized (main thread ID captured)");
}

void ui_notification_info(const char* message) {
    if (!message) {
        spdlog::warn("Attempted to show info notification with null message");
        return;
    }

    if (is_main_thread()) {
        // Main thread: call LVGL directly
        ui_toast_show(ToastSeverity::INFO, message, 4000);

        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = ToastSeverity::INFO;
        entry.was_modal = false;
        entry.was_read = false;
        entry.title[0] = '\0';
        strncpy(entry.message, message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());
    } else {
        // Background thread: marshal to main thread
        AsyncMessageData* data = (AsyncMessageData*)malloc(sizeof(AsyncMessageData));
        if (!data) {
            spdlog::error("Failed to allocate memory for async notification");
            return;
        }

        data->message = strdup(message);
        if (!data->message) {
            free(data);
            spdlog::error("Failed to duplicate message for async notification");
            return;
        }

        data->severity = ToastSeverity::INFO;
        data->duration_ms = 4000;

        lv_async_call(async_message_callback, data);
    }
}

void ui_notification_success(const char* message) {
    if (!message) {
        spdlog::warn("Attempted to show success notification with null message");
        return;
    }

    if (is_main_thread()) {
        // Main thread: call LVGL directly
        ui_toast_show(ToastSeverity::SUCCESS, message, 4000);

        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = ToastSeverity::SUCCESS;
        entry.was_modal = false;
        entry.was_read = false;
        entry.title[0] = '\0';
        strncpy(entry.message, message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());
    } else {
        // Background thread: marshal to main thread
        AsyncMessageData* data = (AsyncMessageData*)malloc(sizeof(AsyncMessageData));
        if (!data) {
            spdlog::error("Failed to allocate memory for async notification");
            return;
        }

        data->message = strdup(message);
        if (!data->message) {
            free(data);
            spdlog::error("Failed to duplicate message for async notification");
            return;
        }

        data->severity = ToastSeverity::SUCCESS;
        data->duration_ms = 4000;

        lv_async_call(async_message_callback, data);
    }
}

void ui_notification_warning(const char* message) {
    if (!message) {
        spdlog::warn("Attempted to show warning notification with null message");
        return;
    }

    if (is_main_thread()) {
        // Main thread: call LVGL directly
        ui_toast_show(ToastSeverity::WARNING, message, 5000);

        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = ToastSeverity::WARNING;
        entry.was_modal = false;
        entry.was_read = false;
        entry.title[0] = '\0';
        strncpy(entry.message, message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());
    } else {
        // Background thread: marshal to main thread
        AsyncMessageData* data = (AsyncMessageData*)malloc(sizeof(AsyncMessageData));
        if (!data) {
            spdlog::error("Failed to allocate memory for async notification");
            return;
        }

        data->message = strdup(message);
        if (!data->message) {
            free(data);
            spdlog::error("Failed to duplicate message for async notification");
            return;
        }

        data->severity = ToastSeverity::WARNING;
        data->duration_ms = 5000;

        lv_async_call(async_message_callback, data);
    }
}

void ui_notification_error(const char* title, const char* message, bool modal) {
    if (!message) {
        spdlog::warn("Attempted to show error notification with null message");
        return;
    }

    if (is_main_thread()) {
        // Main thread: call LVGL directly
        if (modal && title) {
            // Show modal dialog for critical errors
            ui_modal_config_t config = {
                .position = {.use_alignment = true, .alignment = LV_ALIGN_CENTER, .x = 0, .y = 0},
                .backdrop_opa = 180,
                .keyboard = nullptr,
                .persistent = false,
                .on_close = nullptr};

            const char* attrs[] = {"title", title, "message", message, nullptr};

            lv_obj_t* modal_obj = ui_modal_show("error_dialog", &config, attrs);

            if (modal_obj) {
                lv_obj_t* ok_btn = lv_obj_find_by_name(modal_obj, "dialog_ok_btn");
                if (ok_btn) {
                    lv_obj_add_event_cb(ok_btn, modal_ok_btn_clicked, LV_EVENT_CLICKED, modal_obj);
                }
            }

            ui_status_bar_update_notification(NotificationStatus::ERROR);
        } else {
            // Show toast for non-critical errors
            ui_toast_show(ToastSeverity::ERROR, message, 6000);
        }

        // Add to history
        NotificationHistoryEntry entry = {};
        entry.timestamp_ms = lv_tick_get();
        entry.severity = ToastSeverity::ERROR;
        entry.was_modal = modal;
        entry.was_read = false;

        if (title) {
            strncpy(entry.title, title, sizeof(entry.title) - 1);
            entry.title[sizeof(entry.title) - 1] = '\0';
        } else {
            entry.title[0] = '\0';
        }

        strncpy(entry.message, message, sizeof(entry.message) - 1);
        entry.message[sizeof(entry.message) - 1] = '\0';

        NotificationHistory::instance().add(entry);
        ui_status_bar_update_notification_count(NotificationHistory::instance().get_unread_count());
    } else {
        // Background thread: marshal to main thread
        AsyncErrorData* data = (AsyncErrorData*)malloc(sizeof(AsyncErrorData));
        if (!data) {
            spdlog::error("Failed to allocate memory for async error notification");
            return;
        }

        // Copy title (can be nullptr)
        if (title) {
            data->title = strdup(title);
            if (!data->title) {
                free(data);
                spdlog::error("Failed to duplicate title for async error notification");
                return;
            }
        } else {
            data->title = nullptr;
        }

        // Copy message
        data->message = strdup(message);
        if (!data->message) {
            free(data->title);
            free(data);
            spdlog::error("Failed to duplicate message for async error notification");
            return;
        }

        data->modal = modal;

        lv_async_call(async_error_callback, data);
    }
}

// ============================================================================
// Subject observer and modal callbacks
// ============================================================================

// Subject observer callback - routes notifications to appropriate display
static void notification_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    (void)observer;

    // Get notification data from subject
    NotificationData* data = (NotificationData*)lv_subject_get_pointer(subject);
    if (!data) {
        // Silently ignore - this happens during initialization when subject is nullptr
        return;
    }
    if (!data->message) {
        spdlog::warn("Notification observer received data with null message");
        return;
    }

    // Route to modal or toast based on show_modal flag
    if (data->show_modal) {
        ui_notification_error(data->title, data->message, true);
    } else {
        // Route to toast based on severity
        switch (data->severity) {
        case ToastSeverity::INFO:
            ui_notification_info(data->message);
            break;
        case ToastSeverity::SUCCESS:
            ui_notification_success(data->message);
            break;
        case ToastSeverity::WARNING:
            ui_notification_warning(data->message);
            break;
        case ToastSeverity::ERROR:
            ui_notification_error(nullptr, data->message, false);
            break;
        }
    }

    spdlog::debug("Notification routed: modal={}, severity={}, msg={}", data->show_modal,
                  static_cast<int>(data->severity), data->message);
}

// Modal OK button callback
static void modal_ok_btn_clicked(lv_event_t* e) {
    lv_obj_t* modal = (lv_obj_t*)lv_event_get_user_data(e);
    if (modal) {
        ui_modal_hide(modal);
    }
}
