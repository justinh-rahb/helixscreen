// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "splash_screen_manager.h"

#include <spdlog/spdlog.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace helix::application {

void SplashScreenManager::start(pid_t splash_pid) {
    m_splash_pid = splash_pid;
    m_start_time = std::chrono::steady_clock::now();
    m_signaled = false;
    m_discovery_complete = false;
    m_post_refresh_frames = 0;
}

void SplashScreenManager::on_discovery_complete() {
    m_discovery_complete = true;
}

void SplashScreenManager::check_and_signal() {
    if (m_signaled) {
        return; // Already signaled
    }

    // No splash process
    if (m_splash_pid <= 0) {
        m_signaled = true;
        m_post_refresh_frames = 1;
        return;
    }

    // Wait for discovery completion OR timeout before dismissing splash
    auto elapsed = elapsed_ms();

    if (!m_discovery_complete && elapsed < DISCOVERY_TIMEOUT_MS) {
        return; // Keep splash showing, will retry on next frame
    }

    m_signaled = true;

    if (!m_discovery_complete) {
        spdlog::warn("[SplashManager] Discovery timeout ({}ms elapsed), exiting splash anyway",
                     elapsed);
    } else {
        spdlog::debug("[SplashManager] Discovery complete after {}ms, dismissing splash", elapsed);
    }

    signal_and_wait();

    // Schedule post-splash refresh
    spdlog::info("[SplashManager] Splash exited, scheduling post-splash refresh");
    m_post_refresh_frames = 1;
}

int64_t SplashScreenManager::elapsed_ms() const {
    auto elapsed = std::chrono::steady_clock::now() - m_start_time;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

void SplashScreenManager::mark_refresh_done() {
    if (m_post_refresh_frames > 0) {
        m_post_refresh_frames--;
    }
}

void SplashScreenManager::signal_and_wait() {
    spdlog::info("[SplashManager] Signaling splash process (PID {}) to exit...", m_splash_pid);

    if (kill(m_splash_pid, SIGUSR1) != 0) {
        spdlog::warn("[SplashManager] Failed to signal splash process: {}", strerror(errno));
        m_splash_pid = 0;
        return;
    }

    // Wait for splash to exit
    // Check /proc/<pid>/status for zombie state because kill(pid, 0) returns 0 for zombies
    int wait_attempts = 50;

#ifdef __linux__
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/status", m_splash_pid);

    while (wait_attempts-- > 0) {
        // First check if process exists at all
        if (kill(m_splash_pid, 0) != 0) {
            break; // Process gone
        }

        // Check if it's a zombie (exited but not reaped)
        FILE* f = fopen(proc_path, "r");
        if (f) {
            char line[256];
            bool is_zombie = false;
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "State:", 6) == 0) {
                    is_zombie = (strchr(line, 'Z') != nullptr);
                    break;
                }
            }
            fclose(f);
            if (is_zombie) {
                spdlog::debug("[SplashManager] Splash process exited (zombie, waiting for reap)");
                break;
            }
        }

        usleep(20000); // 20ms
    }
#else
    // macOS/other: just poll with kill()
    while (wait_attempts-- > 0) {
        if (kill(m_splash_pid, 0) != 0) {
            break;
        }
        usleep(20000);
    }
#endif

    if (wait_attempts <= 0) {
        // Escalate: SIGTERM then SIGKILL as defense-in-depth
        spdlog::warn("[SplashManager] Splash did not exit after SIGUSR1, sending SIGTERM");
        kill(m_splash_pid, SIGTERM);
        usleep(200000); // 200ms

        if (kill(m_splash_pid, 0) == 0) {
            spdlog::warn("[SplashManager] Splash still alive, sending SIGKILL");
            kill(m_splash_pid, SIGKILL);
            usleep(50000); // 50ms
        }
    } else {
        spdlog::info("[SplashManager] Splash process exited");
    }

    m_splash_pid = 0;
}

} // namespace helix::application
