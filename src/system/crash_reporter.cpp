// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "system/crash_reporter.h"

#include "helix_version.h"
#include "hv/requests.h"
#include "platform_capabilities.h"
#include "system/crash_handler.h"
#include "system/update_checker.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// =============================================================================
// Singleton
// =============================================================================

CrashReporter& CrashReporter::instance() {
    static CrashReporter instance;
    return instance;
}

// =============================================================================
// Lifecycle
// =============================================================================

void CrashReporter::init(const std::string& config_dir) {
    if (initialized_) {
        shutdown();
    }
    config_dir_ = config_dir;
    initialized_ = true;
    spdlog::debug("[CrashReporter] Initialized with config dir: {}", config_dir_);
}

void CrashReporter::shutdown() {
    config_dir_.clear();
    initialized_ = false;
}

// =============================================================================
// Detection
// =============================================================================

bool CrashReporter::has_crash_report() const {
    if (!initialized_) {
        return false;
    }
    return crash_handler::has_crash_file(crash_file_path());
}

std::string CrashReporter::crash_file_path() const {
    return config_dir_ + "/crash.txt";
}

std::string CrashReporter::report_file_path() const {
    return config_dir_ + "/crash_report.txt";
}

// =============================================================================
// Report Collection
// =============================================================================

CrashReporter::CrashReport CrashReporter::collect_report() {
    CrashReport report;

    // Parse crash.txt via existing crash_handler
    auto crash_data = crash_handler::read_crash_file(crash_file_path());
    if (crash_data.is_null()) {
        spdlog::warn("[CrashReporter] Failed to parse crash file");
        return report;
    }

    // Extract crash data fields
    report.signal = crash_data.value("signal", 0);
    report.signal_name = crash_data.value("signal_name", "UNKNOWN");
    report.app_version = crash_data.value("app_version", "unknown");
    report.timestamp = crash_data.value("timestamp", "");
    report.uptime_sec = crash_data.value("uptime_sec", 0);

    if (crash_data.contains("backtrace") && crash_data["backtrace"].is_array()) {
        for (const auto& addr : crash_data["backtrace"]) {
            report.backtrace.push_back(addr.get<std::string>());
        }
    }

    // Collect additional system context
    report.platform = UpdateChecker::get_platform_key();

    auto caps = helix::PlatformCapabilities::detect();
    report.ram_total_mb = static_cast<int>(caps.total_ram_mb);
    report.cpu_cores = caps.cpu_cores;

    // Log tail
    report.log_tail = get_log_tail(50);

    // Printer/Klipper info — these may not be available at startup
    // (no Moonraker connection yet), so left empty until connected
    // The modal or caller can populate these later if Moonraker is available

    spdlog::info(
        "[CrashReporter] Collected report: {} (signal {}), platform={}, RAM={}MB, cores={}",
        report.signal_name, report.signal, report.platform, report.ram_total_mb, report.cpu_cores);

    return report;
}

// =============================================================================
// Log Tail
// =============================================================================

std::string CrashReporter::get_log_tail(int num_lines) {
    // Try common log locations
    std::vector<std::string> log_paths = {
        "/var/log/helix-screen.log",
    };

    // Also try XDG data home
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        log_paths.push_back(std::string(xdg) + "/helix-screen/helix-screen.log");
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        log_paths.push_back(std::string(home) + "/.local/share/helix-screen/helix-screen.log");
    }

    // Also check config dir (for tests)
    log_paths.push_back(config_dir_ + "/helix-screen.log");

    for (const auto& path : log_paths) {
        std::ifstream file(path);
        if (!file.good()) {
            continue;
        }

        // Read all lines into a deque, keeping only the last N
        std::deque<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(std::move(line));
            if (static_cast<int>(lines.size()) > num_lines) {
                lines.pop_front();
            }
        }

        if (lines.empty()) {
            return {};
        }

        std::ostringstream result;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) {
                result << '\n';
            }
            result << lines[i];
        }

        spdlog::debug("[CrashReporter] Read {} log lines from {}", lines.size(), path);
        return result.str();
    }

    spdlog::debug("[CrashReporter] No log file found for log tail");
    return {};
}

// =============================================================================
// Report Formatting
// =============================================================================

nlohmann::json CrashReporter::report_to_json(const CrashReport& report) {
    json j;
    j["signal"] = report.signal;
    j["signal_name"] = report.signal_name;
    j["app_version"] = report.app_version;
    j["timestamp"] = report.timestamp;
    j["uptime_seconds"] = report.uptime_sec;
    j["backtrace"] = report.backtrace;
    j["platform"] = report.platform;
    j["printer_model"] = report.printer_model;
    j["klipper_version"] = report.klipper_version;
    j["display_backend"] = report.display_info;
    j["ram_mb"] = report.ram_total_mb;
    j["cpu_cores"] = report.cpu_cores;

    // Worker expects log_tail as an array of lines
    if (!report.log_tail.empty()) {
        json lines = json::array();
        std::istringstream stream(report.log_tail);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
        j["log_tail"] = lines;
    }

    return j;
}

std::string CrashReporter::report_to_text(const CrashReport& report) {
    std::ostringstream ss;

    ss << "=== HelixScreen Crash Report ===\n\n";

    ss << "--- Crash Summary ---\n";
    ss << "Signal:    " << report.signal << " (" << report.signal_name << ")\n";
    ss << "Version:   " << report.app_version << "\n";
    ss << "Timestamp: " << report.timestamp << "\n";
    ss << "Uptime:    " << report.uptime_sec << " seconds\n\n";

    ss << "--- System Info ---\n";
    ss << "Platform:  " << report.platform << "\n";
    ss << "RAM:       " << report.ram_total_mb << " MB\n";
    ss << "CPU Cores: " << report.cpu_cores << "\n";
    ss << "Display:   " << report.display_info << "\n";
    ss << "Printer:   " << report.printer_model << "\n";
    ss << "Klipper:   " << report.klipper_version << "\n\n";

    if (!report.backtrace.empty()) {
        ss << "--- Backtrace ---\n";
        for (const auto& addr : report.backtrace) {
            ss << addr << "\n";
        }
        ss << "\n";
    }

    if (!report.log_tail.empty()) {
        ss << "--- Log Tail (last 50 lines) ---\n";
        ss << report.log_tail << "\n";
    }

    return ss.str();
}

// =============================================================================
// GitHub URL
// =============================================================================

std::string CrashReporter::generate_github_url(const CrashReport& report) {
    // Build a pre-filled GitHub issue URL
    // Must stay under ~2000 chars for QR code compatibility

    std::string title = "Crash: " + report.signal_name + " in v" + report.app_version;

    std::ostringstream body;
    body << "## Crash Summary\n";
    body << "- **Signal:** " << report.signal << " (" << report.signal_name << ")\n";
    body << "- **Version:** " << report.app_version << "\n";
    body << "- **Platform:** " << report.platform << "\n";
    body << "- **Uptime:** " << report.uptime_sec << "s\n\n";

    if (!report.backtrace.empty()) {
        body << "## Backtrace\n```\n";
        // Limit backtrace entries to keep URL short
        size_t max_bt = std::min(report.backtrace.size(), static_cast<size_t>(10));
        for (size_t i = 0; i < max_bt; ++i) {
            body << report.backtrace[i] << "\n";
        }
        if (report.backtrace.size() > max_bt) {
            body << "... (" << (report.backtrace.size() - max_bt) << " more frames)\n";
        }
        body << "```\n";
    }

    // URL-encode the title and body
    auto url_encode = [](const std::string& str) -> std::string {
        std::ostringstream encoded;
        for (unsigned char c : str) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else if (c == ' ') {
                encoded << '+';
            } else {
                encoded << '%' << "0123456789ABCDEF"[c >> 4] << "0123456789ABCDEF"[c & 0xF];
            }
        }
        return encoded.str();
    };

    std::string url = "https://github.com/" + std::string(GITHUB_REPO) +
                      "/issues/new?title=" + url_encode(title) + "&body=" + url_encode(body.str()) +
                      "&labels=crash,auto-reported";

    // Truncate body if URL exceeds 2000 chars
    if (url.size() > 2000) {
        // Rebuild with minimal body
        std::string minimal_body = "## Crash: " + report.signal_name + " in v" +
                                   report.app_version + " on " + report.platform;
        url = "https://github.com/" + std::string(GITHUB_REPO) +
              "/issues/new?title=" + url_encode(title) + "&body=" + url_encode(minimal_body) +
              "&labels=crash,auto-reported";
    }

    return url;
}

// =============================================================================
// File Save
// =============================================================================

bool CrashReporter::save_to_file(const CrashReport& report) {
    std::string path = report_file_path();

    // Ensure parent directory exists
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }

    std::ofstream ofs(path);
    if (!ofs.good()) {
        spdlog::error("[CrashReporter] Cannot write report file: {}", path);
        return false;
    }

    ofs << report_to_text(report);
    ofs.close();

    if (ofs.fail()) {
        spdlog::error("[CrashReporter] Failed to write report file: {}", path);
        return false;
    }

    spdlog::info("[CrashReporter] Saved crash report to: {}", path);
    return true;
}

// =============================================================================
// Crash File Lifecycle
// =============================================================================

void CrashReporter::consume_crash_file() {
    crash_handler::remove_crash_file(crash_file_path());
    spdlog::debug("[CrashReporter] Consumed crash file");
}

// =============================================================================
// Auto-Send
// =============================================================================

bool CrashReporter::try_auto_send(const CrashReport& report) {
    // Best-effort POST to crash worker — failure falls through to QR/file
    try {
        json payload = report_to_json(report);

        auto req = std::make_shared<HttpRequest>();
        req->method = HTTP_POST;
        req->url = CRASH_WORKER_URL;
        req->timeout = 15; // seconds — don't block UI too long
        req->content_type = APPLICATION_JSON;
        req->headers["User-Agent"] = std::string("HelixScreen/") + HELIX_VERSION;
        req->headers["X-API-Key"] = INGEST_API_KEY;
        req->body = payload.dump();

        auto resp = requests::request(req);
        int status = resp ? static_cast<int>(resp->status_code) : 0;

        if (resp && status >= 200 && status < 300) {
            spdlog::info("[CrashReporter] Crash report sent to worker (HTTP {})", status);
            return true;
        }

        spdlog::warn("[CrashReporter] Worker returned HTTP {} (body: {})", status,
                     resp ? resp->body.substr(0, 200) : "no response");
        return false;
    } catch (const std::exception& e) {
        spdlog::warn("[CrashReporter] Auto-send failed: {}", e.what());
        return false;
    }
}
