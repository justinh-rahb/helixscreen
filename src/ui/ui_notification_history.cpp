// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_notification_history.h"

#include "ui_error_reporting.h"

#include <hv/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

NotificationHistory& NotificationHistory::instance() {
    static NotificationHistory instance;
    return instance;
}

void NotificationHistory::add(const NotificationHistoryEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Reserve space if needed
    if (entries_.empty()) {
        entries_.reserve(MAX_ENTRIES);
    }

    // Add entry to circular buffer
    if (entries_.size() < MAX_ENTRIES) {
        // Buffer not full yet - just append
        entries_.push_back(entry);
        // head_index_ tracks next write position (wraps to 0 when at capacity)
        head_index_ = entries_.size() % MAX_ENTRIES;
    } else {
        // Buffer is full - overwrite oldest entry
        if (!buffer_full_) {
            buffer_full_ = true;
        }
        entries_[head_index_] = entry;
        head_index_ = (head_index_ + 1) % MAX_ENTRIES;
    }

    spdlog::trace("[Notification History] Added notification to history: severity={}, message='{}'",
                  static_cast<int>(entry.severity), entry.message);
}

std::vector<NotificationHistoryEntry> NotificationHistory::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (entries_.empty()) {
        return {};
    }

    std::vector<NotificationHistoryEntry> result;
    result.reserve(entries_.size());

    if (!buffer_full_) {
        // Buffer not full - entries are in order, just reverse
        result.assign(entries_.rbegin(), entries_.rend());
    } else {
        // Buffer is full - reconstruct newest-first order
        // head_index_ points to oldest, so start from head_index_-1 (newest)
        size_t idx = (head_index_ == 0) ? MAX_ENTRIES - 1 : head_index_ - 1;
        for (size_t i = 0; i < entries_.size(); i++) {
            result.push_back(entries_[idx]);
            idx = (idx == 0) ? MAX_ENTRIES - 1 : idx - 1;
        }
    }

    return result;
}

std::vector<NotificationHistoryEntry> NotificationHistory::get_filtered(int severity) const {
    // Don't hold lock while calling get_all() - it has its own lock
    auto all_entries = get_all();

    if (severity < 0) {
        return all_entries;
    }

    std::vector<NotificationHistoryEntry> result;
    std::copy_if(all_entries.begin(), all_entries.end(), std::back_inserter(result),
                 [severity](const NotificationHistoryEntry& e) {
                     return static_cast<int>(e.severity) == severity;
                 });

    return result;
}

size_t NotificationHistory::get_unread_count() const {
    std::lock_guard<std::mutex> lock(mutex_);

    return static_cast<size_t>(
        std::count_if(entries_.begin(), entries_.end(),
                      [](const NotificationHistoryEntry& e) { return !e.was_read; }));
}

ToastSeverity NotificationHistory::get_highest_unread_severity() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Severity priority: ERROR > WARNING > SUCCESS > INFO
    ToastSeverity highest = ToastSeverity::INFO;

    for (const auto& entry : entries_) {
        if (!entry.was_read) {
            if (entry.severity == ToastSeverity::ERROR) {
                return ToastSeverity::ERROR; // Can't get higher than this
            }
            if (entry.severity == ToastSeverity::WARNING && highest != ToastSeverity::ERROR) {
                highest = ToastSeverity::WARNING;
            }
        }
    }

    return highest;
}

void NotificationHistory::mark_all_read() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : entries_) {
        entry.was_read = true;
    }

    spdlog::debug("[Notification History] Marked all {} notifications as read", entries_.size());
}

void NotificationHistory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    entries_.clear();
    head_index_ = 0;
    buffer_full_ = false;

    spdlog::debug("[Notification History] Cleared notification history");
}

size_t NotificationHistory::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

bool NotificationHistory::save_to_disk(const char* path) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        json j;
        j["version"] = 1;
        j["entries"] = json::array();

        // Get entries in newest-first order
        auto all_entries = get_all();

        // Limit to last 50 entries to keep file size reasonable
        size_t save_count = std::min(all_entries.size(), size_t(50));

        for (size_t i = 0; i < save_count; i++) {
            const auto& entry = all_entries[i];

            // Convert severity to string
            std::string severity_str;
            switch (entry.severity) {
            case ToastSeverity::INFO:
                severity_str = "INFO";
                break;
            case ToastSeverity::SUCCESS:
                severity_str = "SUCCESS";
                break;
            case ToastSeverity::WARNING:
                severity_str = "WARNING";
                break;
            case ToastSeverity::ERROR:
                severity_str = "ERROR";
                break;
            default:
                severity_str = "UNKNOWN";
                break;
            }

            json entry_json;
            entry_json["timestamp"] = entry.timestamp_ms;
            entry_json["severity"] = severity_str;
            entry_json["title"] = entry.title;
            entry_json["message"] = entry.message;
            entry_json["was_modal"] = entry.was_modal;
            entry_json["was_read"] = entry.was_read;

            j["entries"].push_back(entry_json);
        }

        // Write to file
        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_WARN_INTERNAL("Failed to open notification history file for writing: {}", path);
            return false;
        }

        file << j.dump(2); // Pretty-print with 2-space indent
        file.close();

        spdlog::info("[Notification History] Saved {} notification entries to {}", save_count,
                     path);
        return true;

    } catch (const std::exception& e) {
        LOG_WARN_INTERNAL("Failed to save notification history: {}", e.what());
        return false;
    }
}

void NotificationHistory::seed_test_data() {
    // Add test notifications with varied severities and timestamps
    // Timestamps are offset from current tick to simulate "time ago" display
    uint64_t now = lv_tick_get();

    // Error from 2 hours ago
    NotificationHistoryEntry error_entry = {};
    error_entry.timestamp_ms = now - (2 * 60 * 60 * 1000); // 2 hours ago
    error_entry.severity = ToastSeverity::ERROR;
    strncpy(error_entry.title, "Thermal Runaway", sizeof(error_entry.title) - 1);
    strncpy(error_entry.message, "Hotend temperature exceeded safety threshold. Heater disabled.",
            sizeof(error_entry.message) - 1);
    error_entry.was_modal = true;
    error_entry.was_read = false;
    add(error_entry);

    // Warning from 45 minutes ago
    NotificationHistoryEntry warning_entry = {};
    warning_entry.timestamp_ms = now - (45 * 60 * 1000); // 45 min ago
    warning_entry.severity = ToastSeverity::WARNING;
    strncpy(warning_entry.title, "Filament Low", sizeof(warning_entry.title) - 1);
    strncpy(warning_entry.message, "AMS slot 1 has less than 10m of filament remaining.",
            sizeof(warning_entry.message) - 1);
    warning_entry.was_modal = false;
    warning_entry.was_read = false;
    add(warning_entry);

    // Success from 20 minutes ago
    NotificationHistoryEntry success_entry = {};
    success_entry.timestamp_ms = now - (20 * 60 * 1000); // 20 min ago
    success_entry.severity = ToastSeverity::SUCCESS;
    strncpy(success_entry.title, "Print Complete", sizeof(success_entry.title) - 1);
    strncpy(success_entry.message, "benchy_v2.gcode finished successfully in 1h 23m.",
            sizeof(success_entry.message) - 1);
    success_entry.was_modal = false;
    success_entry.was_read = false;
    add(success_entry);

    // Info from 5 minutes ago
    NotificationHistoryEntry info_entry = {};
    info_entry.timestamp_ms = now - (5 * 60 * 1000); // 5 min ago
    info_entry.severity = ToastSeverity::INFO;
    strncpy(info_entry.title, "Firmware Update", sizeof(info_entry.title) - 1);
    strncpy(info_entry.message, "Klipper v0.12.1 is available. Current: v0.12.0",
            sizeof(info_entry.message) - 1);
    info_entry.was_modal = false;
    info_entry.was_read = false;
    add(info_entry);

    // Another warning from just now
    NotificationHistoryEntry warning2_entry = {};
    warning2_entry.timestamp_ms = now - (30 * 1000); // 30 sec ago
    warning2_entry.severity = ToastSeverity::WARNING;
    strncpy(warning2_entry.title, "Bed Leveling", sizeof(warning2_entry.title) - 1);
    strncpy(warning2_entry.message, "Bed mesh is outdated. Consider re-calibrating.",
            sizeof(warning2_entry.message) - 1);
    warning2_entry.was_modal = false;
    warning2_entry.was_read = false;
    add(warning2_entry);

    spdlog::info("[Notification History] Seeded {} test notifications", 5);
}

bool NotificationHistory::load_from_disk(const char* path) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::debug("[Notification History] No notification history file found at {}", path);
            return false;
        }

        json j;
        file >> j;
        file.close();

        // Check version
        int version = j.value("version", 0);
        if (version != 1) {
            spdlog::warn("[Notification History] Unsupported notification history version: {}",
                         version);
            return false;
        }

        // Clear existing entries
        entries_.clear();
        head_index_ = 0;
        buffer_full_ = false;

        // Load entries
        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& entry_json : j["entries"]) {
                NotificationHistoryEntry entry = {};

                entry.timestamp_ms = entry_json.value("timestamp", uint64_t(0));
                entry.was_modal = entry_json.value("was_modal", false);
                entry.was_read = entry_json.value("was_read", false);

                // Parse severity
                std::string severity_str = entry_json.value("severity", "INFO");
                if (severity_str == "INFO") {
                    entry.severity = ToastSeverity::INFO;
                } else if (severity_str == "SUCCESS") {
                    entry.severity = ToastSeverity::SUCCESS;
                } else if (severity_str == "WARNING") {
                    entry.severity = ToastSeverity::WARNING;
                } else if (severity_str == "ERROR") {
                    entry.severity = ToastSeverity::ERROR;
                } else {
                    entry.severity = ToastSeverity::INFO;
                }

                // Copy strings
                std::string title = entry_json.value("title", "");
                std::string message = entry_json.value("message", "");

                strncpy(entry.title, title.c_str(), sizeof(entry.title) - 1);
                entry.title[sizeof(entry.title) - 1] = '\0';

                strncpy(entry.message, message.c_str(), sizeof(entry.message) - 1);
                entry.message[sizeof(entry.message) - 1] = '\0';

                entries_.push_back(entry);
            }

            head_index_ = entries_.size();
            if (entries_.size() >= MAX_ENTRIES) {
                buffer_full_ = true;
                head_index_ = 0;
            }

            spdlog::info("[Notification History] Loaded {} notification entries from {}",
                         entries_.size(), path);
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        spdlog::error("[Notification History] Failed to load notification history: {}", e.what());
        return false;
    }
}
