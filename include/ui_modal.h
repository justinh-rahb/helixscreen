// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lvgl/lvgl.h"

#include <memory>
#include <string>
#include <vector>

// Forward declaration
class Modal;

// ============================================================================
// CONFIGURATION TYPES
// ============================================================================

/**
 * @brief Modal positioning configuration
 *
 * Modals are positioned using LVGL alignment. Default is centered.
 * The x/y offsets are applied relative to the alignment point.
 */
struct ModalPosition {
    lv_align_t alignment = LV_ALIGN_CENTER;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
};

/**
 * @brief Complete modal configuration
 *
 * For keyboard support, call ui_modal_register_keyboard(dialog, textarea)
 * after showing the modal.
 */
struct ModalConfig {
    ModalPosition position;
    bool persistent = false; /**< true = hide only, false = delete on close */
    lv_event_cb_t on_close = nullptr;
};

/**
 * @brief Severity levels for modal dialogs
 */
enum class ModalSeverity {
    Info = 0,
    Warning = 1,
    Error = 2,
};

// ============================================================================
// MODAL CLASS
// ============================================================================

/**
 * @brief Unified modal dialog system
 *
 * Combines the functionality of ModalBase (OOP hooks) and ModalManager
 * (stack tracking, animations). Provides:
 *
 * - RAII lifecycle (destructor auto-hides if visible)
 * - Backdrop created in C++ (not in XML)
 * - Modal stacking with proper z-order
 * - Backdrop click-to-close and ESC handling
 * - Standard Ok/Cancel button wiring
 * - Move semantics support
 *
 * ## Usage - Simple Modals (no subclass):
 * @code
 * ModalConfig config{};
 * lv_obj_t* modal = Modal::show("print_cancel_confirm_modal", config);
 * // ...later...
 * Modal::hide(modal);
 * @endcode
 *
 * ## Usage - Complex Modals (subclass):
 * @code
 * class AmsEditModal : public Modal {
 * public:
 *     const char* get_name() const override { return "AMS Edit"; }
 *     const char* component_name() const override { return "ams_edit_modal"; }
 *
 * protected:
 *     void on_ok() override {
 *         save_changes();
 *         Modal::on_ok();
 *     }
 * };
 * @endcode
 */
class Modal {
  public:
    Modal();
    virtual ~Modal();

    // Non-copyable
    Modal(const Modal&) = delete;
    Modal& operator=(const Modal&) = delete;

    // Movable
    Modal(Modal&& other) noexcept;
    Modal& operator=(Modal&& other) noexcept;

    // ========================================================================
    // STATIC FACTORY API (for simple modals)
    // ========================================================================

    /**
     * @brief Show a simple modal (no subclass needed)
     *
     * Creates and displays a modal from the specified XML component.
     * Backdrop is created in C++ and XML content is placed inside it.
     *
     * @param component_name XML component name
     * @param config Modal configuration
     * @param attrs Optional XML attributes (NULL-terminated)
     * @return Pointer to the modal's dialog object (for button wiring etc.)
     */
    static lv_obj_t* show(const char* component_name, const ModalConfig& config,
                          const char** attrs = nullptr);

    /**
     * @brief Hide a modal by its dialog pointer
     * @param dialog Dialog object returned by show()
     */
    static void hide(lv_obj_t* dialog);

    /**
     * @brief Get the topmost modal's dialog
     * @return Dialog object, or nullptr if no modals visible
     */
    static lv_obj_t* get_top();

    /**
     * @brief Check if any modals are visible
     */
    static bool any_visible();

    // ========================================================================
    // INSTANCE API (for subclassed modals)
    // ========================================================================

    /**
     * @brief Show this modal instance
     * @param parent Parent object (usually lv_screen_active())
     * @param attrs Optional XML attributes
     * @return true if shown successfully
     *
     * Note: This overloads the static show() - use modal.show(parent) for instances,
     * Modal::show("name", config) for simple modals.
     */
    bool show(lv_obj_t* parent, const char** attrs = nullptr);

    /**
     * @brief Hide this modal instance
     *
     * Note: This overloads the static hide() - use modal.hide() for instances,
     * Modal::hide(dialog) for simple modals.
     */
    void hide();

    /**
     * @brief Check if this modal is currently visible
     */
    bool is_visible() const {
        return backdrop_ != nullptr;
    }

    /**
     * @brief Get this modal's dialog object
     */
    lv_obj_t* dialog() const {
        return dialog_;
    }

    /**
     * @brief Get this modal's backdrop object
     */
    lv_obj_t* backdrop() const {
        return backdrop_;
    }

    // ========================================================================
    // PURE VIRTUAL (must implement in subclass)
    // ========================================================================

    /**
     * @brief Human-readable name for logging
     */
    virtual const char* get_name() const = 0;

    /**
     * @brief XML component name for lv_xml_create()
     */
    virtual const char* component_name() const = 0;

    // ========================================================================
    // HOOKS (override in subclass)
    // ========================================================================

    /**
     * @brief Called after modal is created and visible
     */
    virtual void on_show() {}

    /**
     * @brief Called before modal is destroyed
     */
    virtual void on_hide() {}

    /**
     * @brief Called when Ok button is clicked (default: hides)
     */
    virtual void on_ok() {
        hide();
    }

    /**
     * @brief Called when Cancel button is clicked (default: hides)
     */
    virtual void on_cancel() {
        hide();
    }

  protected:
    // Modal state
    lv_obj_t* backdrop_ = nullptr;
    lv_obj_t* dialog_ = nullptr;
    lv_obj_t* parent_ = nullptr;

    // Configuration (set before show_instance())
    ModalConfig config_;

    // Helpers
    lv_obj_t* find_widget(const char* name);
    void wire_ok_button(const char* name = "btn_ok");
    void wire_cancel_button(const char* name = "btn_cancel");

  private:
    // Internal implementation
    bool create_and_show(lv_obj_t* parent, const char* comp_name, const char** attrs);
    void destroy();

    // Static event handlers
    static void backdrop_click_cb(lv_event_t* e);
    static void esc_key_cb(lv_event_t* e);
    static void ok_button_cb(lv_event_t* e);
    static void cancel_button_cb(lv_event_t* e);
};

// ============================================================================
// MODAL MANAGER (internal stack tracking)
// ============================================================================

/**
 * @brief Internal singleton for modal stack management
 *
 * Not meant to be used directly - use Modal::show() instead.
 */
class ModalStack {
  public:
    static ModalStack& instance();

    // Track a modal (called by Modal::create_and_show)
    void push(lv_obj_t* backdrop, lv_obj_t* dialog, const std::string& component_name,
              bool persistent = false);

    // Untrack a modal (called by Modal::destroy)
    void remove(lv_obj_t* backdrop);

    // Get topmost dialog
    lv_obj_t* top_dialog() const;

    // Get backdrop for a dialog
    lv_obj_t* backdrop_for(lv_obj_t* dialog) const;

    // Check if any modals are visible (not counting those in exit animation)
    bool empty() const;

    // Check if stack is completely empty (including exiting)
    bool stack_empty() const {
        return stack_.empty();
    }

    // Check if a modal is persistent (returns false if not found)
    bool is_persistent(lv_obj_t* backdrop) const;

    // Mark a modal as exiting (animation in progress, ignore further hide() calls)
    // Returns true if found and marked, false if not found or already exiting
    bool mark_exiting(lv_obj_t* backdrop);

    // Check if a modal is currently in exit animation
    bool is_exiting(lv_obj_t* backdrop) const;

    // Animation helpers
    void animate_entrance(lv_obj_t* dialog);
    void animate_exit(lv_obj_t* backdrop, lv_obj_t* dialog, bool persistent);

  private:
    ModalStack() = default;

    struct ModalEntry {
        lv_obj_t* backdrop;
        lv_obj_t* dialog;
        std::string component_name;
        bool persistent; /**< true = hide only, false = delete on close */
        bool exiting;    /**< true = exit animation in progress, ignore hide() calls */
    };

    std::vector<ModalEntry> stack_;

    static void exit_animation_done(lv_anim_t* anim);
};

// ============================================================================
// MODAL DIALOG SUBJECTS (for modal_dialog.xml)
// ============================================================================

/**
 * @brief Initialize subjects for modal_dialog.xml bindings
 *
 * Call once during app startup before any modal_dialog is shown.
 */
void modal_init_subjects();

/**
 * @brief Configure modal_dialog before showing
 */
void modal_configure(ModalSeverity severity, bool show_cancel, const char* primary_text,
                     const char* cancel_text);

// Subject accessors
lv_subject_t* modal_severity_subject();
lv_subject_t* modal_show_cancel_subject();
lv_subject_t* modal_primary_text_subject();
lv_subject_t* modal_cancel_text_subject();

// ============================================================================
// LEGACY API WRAPPERS
// ============================================================================

// Note: These wrappers provide the old ui_modal_*() function names
// for backward compatibility with existing code.

inline lv_obj_t* ui_modal_show(const char* name, const ModalConfig* config,
                               const char** attrs = nullptr) {
    return Modal::show(name, config ? *config : ModalConfig{}, attrs);
}

inline void ui_modal_hide(lv_obj_t* dialog) {
    Modal::hide(dialog);
}

inline lv_obj_t* ui_modal_get_top() {
    return Modal::get_top();
}

inline bool ui_modal_is_visible() {
    return Modal::any_visible();
}

inline void ui_modal_init_subjects() {
    modal_init_subjects();
}

inline void ui_modal_configure(ModalSeverity severity, bool show_cancel, const char* primary_text,
                               const char* cancel_text) {
    modal_configure(severity, show_cancel, primary_text, cancel_text);
}

inline lv_subject_t* ui_modal_get_severity_subject() {
    return modal_severity_subject();
}

inline lv_subject_t* ui_modal_get_show_cancel_subject() {
    return modal_show_cancel_subject();
}

inline lv_subject_t* ui_modal_get_primary_text_subject() {
    return modal_primary_text_subject();
}

inline lv_subject_t* ui_modal_get_cancel_text_subject() {
    return modal_cancel_text_subject();
}

/**
 * @brief Register a textarea for keyboard display within a modal
 *
 * Positions the keyboard at bottom-center and registers the textarea.
 * Automatically detects password mode for masking.
 *
 * @param modal The modal dialog (used for logging only)
 * @param textarea The textarea widget to register
 */
void ui_modal_register_keyboard(lv_obj_t* modal, lv_obj_t* textarea);
