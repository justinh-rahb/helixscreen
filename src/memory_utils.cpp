// SPDX-License-Identifier: GPL-3.0-or-later

#include "memory_utils.h"

#include <fstream>
#include <string>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/task_info.h>
#endif

namespace helix {

// Track peak memory on macOS (mach doesn't provide HWM)
static int64_t g_macos_peak_rss_kb = 0;

bool read_memory_stats(int64_t& rss_kb, int64_t& hwm_kb) {
    rss_kb = 0;
    hwm_kb = 0;

#ifdef __linux__
    std::ifstream status("/proc/self/status");
    if (!status.is_open())
        return false;

    std::string line;
    while (std::getline(status, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            rss_kb = std::stoll(line.substr(6));
        } else if (line.compare(0, 6, "VmHWM:") == 0) {
            hwm_kb = std::stoll(line.substr(6));
        }
    }
    return rss_kb > 0;

#elif defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) ==
        KERN_SUCCESS) {
        rss_kb = static_cast<int64_t>(info.resident_size / 1024);
        // Track peak ourselves since macOS doesn't provide HWM
        if (rss_kb > g_macos_peak_rss_kb) {
            g_macos_peak_rss_kb = rss_kb;
        }
        hwm_kb = g_macos_peak_rss_kb;
        return true;
    }
    return false;

#else
    return false;
#endif
}

bool read_private_dirty(int64_t& private_dirty_kb) {
    private_dirty_kb = 0;

#ifdef __linux__
    std::ifstream smaps("/proc/self/smaps_rollup");
    if (!smaps.is_open())
        return false;

    std::string line;
    while (std::getline(smaps, line)) {
        if (line.compare(0, 14, "Private_Dirty:") == 0) {
            private_dirty_kb = std::stoll(line.substr(14));
            return true;
        }
    }
#endif
    // macOS: private dirty not easily available
    return false;
}

} // namespace helix
