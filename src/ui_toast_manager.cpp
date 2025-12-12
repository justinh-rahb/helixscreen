// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_toast_manager.h"

#include "ui_notification_history.h"
#include "ui_status_bar.h"

#include <spdlog/spdlog.h>

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

ToastManager& ToastManager::instance() {
    static ToastManager instance;
    return instance;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Convert ToastSeverity enum to string for logging
static const char* severity_to_string(ToastSeverity severity) {
    switch (severity) {
    case ToastSeverity::ERROR:
        return "error";
    case ToastSeverity::WARNING:
        return "warning";
    case ToastSeverity::SUCCESS:
        return "success";
    case ToastSeverity::INFO:
    default:
        return "info";
    }
}

// Convert ToastSeverity enum to int for subject binding (0=info, 1=success, 2=warning, 3=error)
static int severity_to_int(ToastSeverity severity) {
    switch (severity) {
    case ToastSeverity::INFO:
        return 0;
    case ToastSeverity::SUCCESS:
        return 1;
    case ToastSeverity::WARNING:
        return 2;
    case ToastSeverity::ERROR:
        return 3;
    default:
        return 0;
    }
}

static NotificationStatus severity_to_notification_status(ToastSeverity severity) {
    switch (severity) {
    case ToastSeverity::INFO:
        return NotificationStatus::INFO;
    case ToastSeverity::SUCCESS:
        return NotificationStatus::INFO; // Treat success as info in status bar
    case ToastSeverity::WARNING:
        return NotificationStatus::WARNING;
    case ToastSeverity::ERROR:
        return NotificationStatus::ERROR;
    default:
        return NotificationStatus::NONE;
    }
}

// ============================================================================
// TOAST MANAGER IMPLEMENTATION
// ============================================================================

void ToastManager::init() {
    if (initialized_) {
        spdlog::warn("[ToastManager] Already initialized - skipping");
        return;
    }

    // Action button subjects
    lv_subject_init_int(&action_visible_subject_, 0);
    lv_xml_register_subject(NULL, "toast_action_visible", &action_visible_subject_);

    lv_subject_init_pointer(&action_text_subject_, action_text_buf_);
    lv_xml_register_subject(NULL, "toast_action_text", &action_text_subject_);

    // Severity subject (0=info, 1=success, 2=warning, 3=error)
    lv_subject_init_int(&severity_subject_, 0);
    lv_xml_register_subject(NULL, "toast_severity", &severity_subject_);

    // Register callback for XML event_cb to work
    lv_xml_register_event_cb(nullptr, "toast_close_btn_clicked", close_btn_clicked);

    initialized_ = true;
    spdlog::debug("[ToastManager] Toast notification system initialized");
}

void ToastManager::show(ToastSeverity severity, const char* message, uint32_t duration_ms) {
    create_toast_internal(severity, message, duration_ms, false);
}

void ToastManager::show_with_action(ToastSeverity severity, const char* message,
                                    const char* action_text, toast_action_callback_t callback,
                                    void* user_data, uint32_t duration_ms) {
    if (!action_text || !callback) {
        spdlog::warn("[ToastManager] Toast action requires action_text and callback");
        show(severity, message, duration_ms);
        return;
    }

    // Store callback for when action button is clicked
    action_callback_ = callback;
    action_user_data_ = user_data;

    // Update action button text and visibility via subjects
    snprintf(action_text_buf_, sizeof(action_text_buf_), "%s", action_text);
    lv_subject_set_pointer(&action_text_subject_, action_text_buf_);
    lv_subject_set_int(&action_visible_subject_, 1);

    create_toast_internal(severity, message, duration_ms, true);
}

void ToastManager::hide() {
    if (!active_toast_) {
        return;
    }

    // Cancel dismiss timer if active
    if (dismiss_timer_) {
        lv_timer_delete(dismiss_timer_);
        dismiss_timer_ = nullptr;
    }

    // Clear action state
    action_callback_ = nullptr;
    action_user_data_ = nullptr;
    lv_subject_set_int(&action_visible_subject_, 0);

    // Delete toast widget
    lv_obj_delete(active_toast_);
    active_toast_ = nullptr;

    // Update bell color based on highest unread severity in history
    ToastSeverity highest = NotificationHistory::instance().get_highest_unread_severity();
    size_t unread = NotificationHistory::instance().get_unread_count();

    if (unread == 0) {
        ui_status_bar_update_notification(NotificationStatus::NONE);
    } else {
        ui_status_bar_update_notification(severity_to_notification_status(highest));
    }

    spdlog::debug("[ToastManager] Toast hidden");
}

bool ToastManager::is_visible() const {
    return active_toast_ != nullptr;
}

void ToastManager::create_toast_internal(ToastSeverity severity, const char* message,
                                         uint32_t duration_ms, bool with_action) {
    if (!message) {
        spdlog::warn("[ToastManager] Attempted to show toast with null message");
        return;
    }

    // Hide existing toast if any
    if (active_toast_) {
        hide();
    }

    // Clear action state for basic toasts, keep for action toasts
    if (!with_action) {
        action_callback_ = nullptr;
        action_user_data_ = nullptr;
        lv_subject_set_int(&action_visible_subject_, 0);
    }

    // Set severity subject BEFORE creating toast (XML bindings read it during creation)
    lv_subject_set_int(&severity_subject_, severity_to_int(severity));

    // Create toast via XML component
    const char* attrs[] = {"message", message, nullptr};

    lv_obj_t* screen = lv_screen_active();
    active_toast_ = static_cast<lv_obj_t*>(lv_xml_create(screen, "toast_notification", attrs));

    if (!active_toast_) {
        spdlog::error("[ToastManager] Failed to create toast notification widget");
        return;
    }

    // Wire up action button callback (if showing action toast)
    if (with_action) {
        lv_obj_t* action_btn = lv_obj_find_by_name(active_toast_, "toast_action_btn");
        if (action_btn) {
            lv_obj_add_event_cb(action_btn, action_btn_clicked, LV_EVENT_CLICKED, nullptr);
        }
    }

    // Create auto-dismiss timer
    dismiss_timer_ = lv_timer_create(dismiss_timer_cb, duration_ms, nullptr);
    lv_timer_set_repeat_count(dismiss_timer_, 1); // Run once then stop

    // Update status bar notification icon
    ui_status_bar_update_notification(severity_to_notification_status(severity));

    spdlog::debug("[ToastManager] Toast shown: [{}] {} ({}ms, action={})",
                  severity_to_string(severity), message, duration_ms, with_action);
}

void ToastManager::dismiss_timer_cb(lv_timer_t* timer) {
    (void)timer;
    ToastManager::instance().hide();
}

void ToastManager::close_btn_clicked(lv_event_t* e) {
    (void)e;
    ToastManager::instance().hide();
}

void ToastManager::action_btn_clicked(lv_event_t* e) {
    (void)e;

    auto& mgr = ToastManager::instance();

    // Store callback before hiding (hide clears action_callback)
    toast_action_callback_t cb = mgr.action_callback_;
    void* data = mgr.action_user_data_;

    // Hide the toast first
    mgr.hide();

    // Then invoke the callback
    if (cb) {
        spdlog::debug("[ToastManager] Toast action button clicked - invoking callback");
        cb(data);
    }
}

// ============================================================================
// LEGACY API (forwards to ToastManager)
// ============================================================================

void ui_toast_init() {
    ToastManager::instance().init();
}

void ui_toast_show(ToastSeverity severity, const char* message, uint32_t duration_ms) {
    ToastManager::instance().show(severity, message, duration_ms);
}

void ui_toast_show_with_action(ToastSeverity severity, const char* message, const char* action_text,
                               toast_action_callback_t action_callback, void* user_data,
                               uint32_t duration_ms) {
    ToastManager::instance().show_with_action(severity, message, action_text, action_callback,
                                              user_data, duration_ms);
}

void ui_toast_hide() {
    ToastManager::instance().hide();
}

bool ui_toast_is_visible() {
    return ToastManager::instance().is_visible();
}
