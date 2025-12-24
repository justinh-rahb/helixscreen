// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen

#pragma once

#include "ui_observer_guard.h"

#include "runtime_config.h"

#include <functional>
#include <memory>
#include <vector>

// Forward declarations
class MoonrakerAPI;
class PrintSelectPanel;
class PrintStatusPanel;
class MotionPanel;
class ExtrusionPanel;
class BedMeshPanel;
class TempControlPanel;
class UsbManager;

/**
 * @brief Initializes all reactive subjects for LVGL data binding
 *
 * SubjectInitializer orchestrates the initialization of all reactive subjects
 * in the correct dependency order. It manages observer guards for proper cleanup
 * and holds references to panels that need deferred API injection.
 *
 * Initialization order is critical:
 * 1. Core subjects (app_globals, navigation, status bar)
 * 2. PrinterState subjects (must be before panels that observe them)
 * 3. AmsState and FilamentSensorManager subjects
 * 4. Panel subjects (home, controls, filament, settings, etc.)
 * 5. Observers (print completion, print start navigation)
 * 6. Utility subjects (wizard, keypad, notification)
 *
 * Usage:
 *   SubjectInitializer subjects;
 *   subjects.init_all(runtime_config);
 *   // Later, after MoonrakerAPI is ready:
 *   subjects.inject_api(api);
 */
class SubjectInitializer {
  public:
    SubjectInitializer();
    ~SubjectInitializer();

    // Non-copyable, non-movable (owns observer guards)
    SubjectInitializer(const SubjectInitializer&) = delete;
    SubjectInitializer& operator=(const SubjectInitializer&) = delete;
    SubjectInitializer(SubjectInitializer&&) = delete;
    SubjectInitializer& operator=(SubjectInitializer&&) = delete;

    /**
     * @brief Initialize all subjects in dependency order
     * @param runtime_config Runtime configuration for mock modes
     * @return true if initialization succeeded
     */
    bool init_all(const RuntimeConfig& runtime_config);

    /**
     * @brief Inject MoonrakerAPI into panels that need it
     * @param api Pointer to the initialized MoonrakerAPI
     *
     * Called after Moonraker connection is established. Panels stored during
     * init_all() will have their set_api() method called.
     */
    void inject_api(MoonrakerAPI* api);

    /**
     * @brief Check if subjects have been initialized
     */
    bool is_initialized() const {
        return m_initialized;
    }

    /**
     * @brief Get the number of observer guards managed
     */
    size_t observer_count() const {
        return m_observers.size();
    }

    /**
     * @brief Get the USB manager (owned by SubjectInitializer)
     */
    UsbManager* usb_manager() const {
        return m_usb_manager.get();
    }

    /**
     * @brief Get the TempControlPanel (owned by SubjectInitializer)
     */
    TempControlPanel* temp_control_panel() const {
        return m_temp_control_panel.get();
    }

    // Accessors for panels that need API injection
    PrintSelectPanel* print_select_panel() const {
        return m_print_select_panel;
    }
    PrintStatusPanel* print_status_panel() const {
        return m_print_status_panel;
    }
    MotionPanel* motion_panel() const {
        return m_motion_panel;
    }
    ExtrusionPanel* extrusion_panel() const {
        return m_extrusion_panel;
    }
    BedMeshPanel* bed_mesh_panel() const {
        return m_bed_mesh_panel;
    }

  private:
    // Initialization phases (called by init_all in order)
    void init_core_subjects();
    void init_printer_state_subjects();
    void init_ams_subjects();
    void init_panel_subjects();
    void init_observers();
    void init_utility_subjects();
    void init_usb_manager(const RuntimeConfig& runtime_config);

    // Observer guards (RAII cleanup on destruction)
    std::vector<ObserverGuard> m_observers;

    // Owned resources
    std::unique_ptr<UsbManager> m_usb_manager;
    std::unique_ptr<TempControlPanel> m_temp_control_panel;

    // Panels that need deferred API injection (not owned)
    PrintSelectPanel* m_print_select_panel = nullptr;
    PrintStatusPanel* m_print_status_panel = nullptr;
    MotionPanel* m_motion_panel = nullptr;
    ExtrusionPanel* m_extrusion_panel = nullptr;
    BedMeshPanel* m_bed_mesh_panel = nullptr;

    bool m_initialized = false;
};
