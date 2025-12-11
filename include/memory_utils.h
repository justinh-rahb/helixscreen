// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

} // namespace helix
