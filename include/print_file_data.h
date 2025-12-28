// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ctime>
#include <string>
#include <vector>

// Forward declarations for factory methods (avoid header coupling)
struct FileInfo;
struct UsbGcodeFile;

/**
 * @brief Print history status for file list display
 *
 * Status values in priority order (for display):
 * - CURRENTLY_PRINTING: Active print (blue clock icon)
 * - COMPLETED: Last print succeeded (green checkmark with count)
 * - FAILED: Last print failed or cancelled (orange warning triangle)
 * - NEVER_PRINTED: No history record (empty/blank)
 */
enum class FileHistoryStatus {
    NEVER_PRINTED = 0,  ///< No history record
    CURRENTLY_PRINTING, ///< Matches active print filename
    COMPLETED,          ///< Last print completed successfully
    FAILED              ///< Last print failed or cancelled
};

/**
 * @brief File data for print selection
 *
 * Holds file metadata and display strings for print file list/card/detail views.
 */
struct PrintFileData {
    std::string filename;
    std::string thumbnail_path;         ///< Pre-scaled .bin path for cards (fast rendering)
    std::string original_thumbnail_url; ///< Moonraker relative URL (for detail view PNG lookup)
    size_t file_size_bytes;             ///< File size in bytes
    time_t modified_timestamp;          ///< Last modified timestamp
    int print_time_minutes;             ///< Print time in minutes
    float filament_grams;               ///< Filament weight in grams
    std::string filament_type;          ///< Filament type (e.g., "PLA", "PETG", "ABS")
    uint32_t layer_count = 0;           ///< Total layer count from slicer
    double object_height = 0.0;         ///< Object height in mm
    bool is_dir = false;                ///< True if this is a directory
    std::vector<std::string>
        filament_colors; ///< Hex colors per tool (e.g., ["#ED1C24", "#00C1AE"])

    // Formatted strings (cached for performance)
    std::string size_str;
    std::string modified_str;
    std::string print_time_str;
    std::string filament_str;
    std::string layer_count_str;  ///< Formatted layer count string
    std::string print_height_str; ///< Formatted print height string

    // Metadata loading state (travels with file during sorting)
    bool metadata_fetched = false; ///< True if metadata has been loaded

    // Print history status (from PrintHistoryManager)
    FileHistoryStatus history_status = FileHistoryStatus::NEVER_PRINTED;
    int success_count = 0; ///< Number of successful prints (shown as "N ✓")

    // ========================================================================
    // FACTORY METHODS
    // ========================================================================

    /**
     * @brief Create PrintFileData from Moonraker FileInfo
     *
     * Populates basic file info (filename, size, modified time) and sets
     * placeholder values for metadata fields. The thumbnail_path is set to
     * the default placeholder.
     *
     * @param file FileInfo from Moonraker file listing API
     * @param default_thumbnail Path to default/placeholder thumbnail
     * @return Initialized PrintFileData with formatted strings
     */
    static PrintFileData from_moonraker_file(const FileInfo& file,
                                             const std::string& default_thumbnail);

    /**
     * @brief Create PrintFileData from USB G-code file
     *
     * USB files don't have Moonraker metadata, so print_time, filament, etc.
     * are set to defaults. Formatted strings use "--" for unavailable fields.
     *
     * @param file UsbGcodeFile from USB manager scan
     * @param default_thumbnail Path to default/placeholder thumbnail
     * @return Initialized PrintFileData with formatted strings
     */
    static PrintFileData from_usb_file(const UsbGcodeFile& file,
                                       const std::string& default_thumbnail);

    /**
     * @brief Create a directory entry
     *
     * @param name Directory name (e.g., ".." for parent, "folder_name" for subdirs)
     * @param icon_path Path to folder icon
     * @param is_parent True if this is the parent directory entry ".."
     * @return Initialized PrintFileData for directory display
     */
    static PrintFileData make_directory(const std::string& name, const std::string& icon_path,
                                        bool is_parent = false);
};
