// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen

#include "active_print_media_manager.h"

#include "ui_update_queue.h"
#include "ui_utils.h"

#include "app_globals.h"
#include "thumbnail_cache.h"
#include "thumbnail_processor.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <stdexcept>

namespace helix {

// Singleton storage
static std::unique_ptr<ActivePrintMediaManager> g_instance;

void init_active_print_media_manager() {
    if (g_instance) {
        spdlog::warn("[ActivePrintMediaManager] Already initialized");
        return;
    }
    g_instance = std::make_unique<ActivePrintMediaManager>(::get_printer_state());
    spdlog::info("[ActivePrintMediaManager] Initialized");
}

ActivePrintMediaManager& get_active_print_media_manager() {
    if (!g_instance) {
        throw std::runtime_error("ActivePrintMediaManager not initialized");
    }
    return *g_instance;
}

ActivePrintMediaManager::ActivePrintMediaManager(PrinterState& printer_state)
    : printer_state_(printer_state) {
    // Observe print_filename_ subject to react to filename changes
    print_filename_observer_ =
        ObserverGuard(printer_state_.get_print_filename_subject(), on_print_filename_changed, this);

    spdlog::debug("[ActivePrintMediaManager] Observer attached to print_filename subject");
}

ActivePrintMediaManager::~ActivePrintMediaManager() {
    // ObserverGuard handles cleanup automatically
    // NOTE: No logging here - spdlog may be destroyed before this singleton
}

void ActivePrintMediaManager::set_api(MoonrakerAPI* api) {
    api_ = api;
    spdlog::debug("[ActivePrintMediaManager] API set: {}", api ? "valid" : "nullptr");
}

void ActivePrintMediaManager::set_thumbnail_source(const std::string& original_filename) {
    thumbnail_source_filename_ = original_filename;
    spdlog::debug("[ActivePrintMediaManager] Thumbnail source set to: {}",
                  original_filename.empty() ? "(cleared)" : original_filename);

    // If we have a current print filename, re-process it with the new source
    const char* current = lv_subject_get_string(printer_state_.get_print_filename_subject());
    if (current && current[0] != '\0' && !original_filename.empty()) {
        spdlog::info("[ActivePrintMediaManager] Re-processing with source override: {}",
                     original_filename);
        process_filename(current);
    }
}

void ActivePrintMediaManager::clear_thumbnail_source() {
    thumbnail_source_filename_.clear();
    last_effective_filename_.clear();
    last_loaded_thumbnail_filename_.clear();
    spdlog::debug("[ActivePrintMediaManager] Thumbnail source cleared");
}

void ActivePrintMediaManager::on_print_filename_changed(lv_observer_t* observer,
                                                        lv_subject_t* subject) {
    auto* self = static_cast<ActivePrintMediaManager*>(lv_observer_get_user_data(observer));
    if (!self) {
        spdlog::warn("[ActivePrintMediaManager] Observer callback with null self!");
        return;
    }

    const char* filename = lv_subject_get_string(subject);
    self->process_filename(filename);
}

void ActivePrintMediaManager::process_filename(const char* raw_filename) {
    // Empty filename means print ended or idle
    if (!raw_filename || raw_filename[0] == '\0') {
        // Only clear if we actually had something
        if (!last_effective_filename_.empty()) {
            spdlog::debug("[ActivePrintMediaManager] Filename cleared, clearing print info");
            clear_print_info();
        }
        return;
    }

    std::string filename = raw_filename;

    // Auto-resolve temp file patterns to original filename if no override is set
    std::string resolved = resolve_gcode_filename(filename);
    if (resolved != filename && thumbnail_source_filename_.empty()) {
        spdlog::debug("[ActivePrintMediaManager] Auto-resolved temp filename: {} -> {}", filename,
                      resolved);
        thumbnail_source_filename_ = resolved;
    }

    // Compute effective filename (respects thumbnail_source override)
    std::string effective_filename =
        thumbnail_source_filename_.empty() ? filename : thumbnail_source_filename_;

    // Skip if effective filename hasn't changed (makes processing idempotent)
    if (effective_filename == last_effective_filename_) {
        return;
    }
    last_effective_filename_ = effective_filename;

    // Update display filename subject
    std::string display_name = get_display_filename(effective_filename);
    spdlog::debug("[ActivePrintMediaManager] Display filename: {}", display_name);

    // Thread-safe update to display filename subject (RAII via unique_ptr)
    // Capture printer_state_ reference to avoid using global in tests
    PrinterState* state = &printer_state_;
    ui_queue_update<std::string>(
        std::make_unique<std::string>(display_name),
        [state](std::string* name) { state->set_print_display_filename(*name); });

    // Load thumbnail if filename changed
    if (!effective_filename.empty() && effective_filename != last_loaded_thumbnail_filename_) {
        load_thumbnail_for_file(effective_filename);
        last_loaded_thumbnail_filename_ = effective_filename;
    }
}

void ActivePrintMediaManager::load_thumbnail_for_file(const std::string& filename) {
    // Increment generation to invalidate any in-flight async operations
    ++thumbnail_load_generation_;
    uint32_t current_gen = thumbnail_load_generation_;

    // Skip if no API available
    if (!api_) {
        spdlog::debug("[ActivePrintMediaManager] No API available - skipping thumbnail load");
        return;
    }

    // Resolve to original filename if this is a modified temp file
    // (Moonraker only has metadata for original files, not modified copies)
    std::string metadata_filename = resolve_gcode_filename(filename);

    spdlog::debug("[ActivePrintMediaManager] Loading thumbnail for: {}", metadata_filename);

    // Get file metadata to find thumbnail path
    api_->get_file_metadata(
        metadata_filename,
        [this, current_gen](const FileMetadata& metadata) {
            // Check if this callback is still relevant
            if (current_gen != thumbnail_load_generation_) {
                spdlog::trace(
                    "[ActivePrintMediaManager] Stale metadata callback (gen {} != {}), ignoring",
                    current_gen, thumbnail_load_generation_);
                return;
            }

            // Also set total layer count from metadata while we have it
            if (metadata.layer_count > 0) {
                int layer_count = static_cast<int>(metadata.layer_count);
                PrinterState* state = &printer_state_;
                ui_queue_update<int>(std::make_unique<int>(layer_count),
                                     [state](int* count) { state->set_print_layer_total(*count); });
                spdlog::debug("[ActivePrintMediaManager] Set total layers from metadata: {}",
                              metadata.layer_count);
            }

            // Get the largest thumbnail available
            std::string thumbnail_rel_path = metadata.get_largest_thumbnail();
            if (thumbnail_rel_path.empty()) {
                spdlog::debug("[ActivePrintMediaManager] No thumbnail available in metadata");
                return;
            }

            spdlog::debug("[ActivePrintMediaManager] Found thumbnail: {}", thumbnail_rel_path);

            // Use ThumbnailCache for download and pre-scaling
            ThumbnailTarget target = ThumbnailProcessor::get_target_for_display();
            get_thumbnail_cache().fetch_optimized(
                api_, thumbnail_rel_path, target,
                [this, current_gen](const std::string& lvgl_path) {
                    // Check if this callback is still relevant
                    if (current_gen != thumbnail_load_generation_) {
                        spdlog::trace(
                            "[ActivePrintMediaManager] Stale thumbnail callback, ignoring");
                        return;
                    }

                    // Thread-safe update to thumbnail path subject (RAII via unique_ptr)
                    PrinterState* state = &printer_state_;
                    ui_queue_update<std::string>(
                        std::make_unique<std::string>(lvgl_path), [state](std::string* path) {
                            state->set_print_thumbnail_path(*path);
                            spdlog::info("[ActivePrintMediaManager] Thumbnail path set: {}", *path);
                        });
                },
                [](const std::string& error) {
                    spdlog::warn("[ActivePrintMediaManager] Failed to fetch thumbnail: {}", error);
                });
        },
        [metadata_filename](const MoonrakerError& err) {
            spdlog::debug("[ActivePrintMediaManager] Failed to get file metadata for '{}': {}",
                          metadata_filename, err.message);
        },
        true // silent - don't trigger RPC_ERROR event/toast
    );
}

void ActivePrintMediaManager::clear_print_info() {
    thumbnail_source_filename_.clear();
    last_effective_filename_.clear();
    last_loaded_thumbnail_filename_.clear();

    // Thread-safe clear of shared subjects (capture printer_state_ for testability)
    PrinterState* state = &printer_state_;
    ui_queue_update([state]() {
        state->set_print_thumbnail_path("");
        state->set_print_display_filename("");
        spdlog::debug("[ActivePrintMediaManager] Cleared print info subjects");
    });
}

} // namespace helix
