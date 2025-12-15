// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace helix {

/**
 * @brief Read current memory stats (cross-platform: Linux + macOS)
 * @param rss_kb Output: Resident Set Size in KB
 * @param hwm_kb Output: High Water Mark (peak RSS) in KB
 * @return true if successful
 */
bool read_memory_stats(int64_t& rss_kb, int64_t& hwm_kb);

/**
 * @brief Read private dirty memory (Linux only)
 * @param private_dirty_kb Output: Private dirty pages in KB
 * @return true if successful (always false on macOS)
 */
bool read_private_dirty(int64_t& private_dirty_kb);

// ============================================================================
// System memory info (for resource management decisions)
// ============================================================================

/**
 * @brief System memory information
 */
struct MemoryInfo {
    size_t total_kb = 0;     ///< Total system memory in KB
    size_t available_kb = 0; ///< Available memory in KB (free + buffers/cache)
    size_t free_kb = 0;      ///< Strictly free memory in KB

    /// Check if this is a memory-constrained device (< 64MB available)
    bool is_constrained() const {
        return available_kb < 64 * 1024;
    }

    /// Get available memory in MB
    size_t available_mb() const {
        return available_kb / 1024;
    }
};

/**
 * @brief Get current system memory information
 *
 * On Linux, reads from /proc/meminfo.
 * On macOS, uses mach APIs (returns zeros for available - use RSS instead).
 *
 * @return MemoryInfo struct with current memory status
 */
MemoryInfo get_system_memory_info();

/**
 * @brief Memory thresholds for G-code 3D rendering decisions
 */
struct GCodeMemoryLimits {
    /// Minimum available RAM (KB) to even attempt 3D rendering
    static constexpr size_t MIN_AVAILABLE_KB = 48 * 1024; // 48MB

    /// Maximum G-code file size (bytes) for 3D rendering on constrained devices
    static constexpr size_t MAX_FILE_SIZE_CONSTRAINED = 2 * 1024 * 1024; // 2MB

    /// Maximum G-code file size (bytes) for 3D rendering on normal devices
    static constexpr size_t MAX_FILE_SIZE_NORMAL = 20 * 1024 * 1024; // 20MB

    /// Memory expansion factor (file size -> parsed geometry size estimate)
    static constexpr size_t EXPANSION_FACTOR = 15;
};

/**
 * @brief Check if G-code 3D rendering is safe for a given file
 *
 * Uses heuristics based on file size and available RAM.
 * G-code parsing expands ~10-20x in memory for geometry data.
 *
 * @param file_size_bytes Size of the G-code file in bytes
 * @return true if rendering is likely safe, false if thumbnail-only recommended
 */
bool is_gcode_3d_render_safe(size_t file_size_bytes);

} // namespace helix
