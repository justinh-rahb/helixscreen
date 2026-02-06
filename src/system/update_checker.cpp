// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file update_checker.cpp
 * @brief Async update checker implementation
 *
 * SAFETY:
 * - Downloads and installs require explicit user confirmation
 * - Downloads are blocked while a print is in progress
 * - All errors are caught and logged, never thrown
 * - Network failures are gracefully handled
 * - Rate limited to avoid hammering GitHub API
 */

#include "system/update_checker.h"

#include "ui_update_queue.h"

#include "app_globals.h"
#include "hv/requests.h"
#include "printer_state.h"
#include "spdlog/spdlog.h"
#include "version.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sys/statvfs.h>
#include <unistd.h>

#include "hv/json.hpp"

using json = nlohmann::json;

namespace {

/// GitHub API URL for latest release
constexpr const char* GITHUB_API_URL =
    "https://api.github.com/repos/prestonbrown/helixscreen/releases/latest";

/// HTTP request timeout in seconds
constexpr int HTTP_TIMEOUT_SECONDS = 30;

/**
 * @brief Strip 'v' or 'V' prefix from version tag
 *
 * GitHub releases use "v1.2.3" format, but version comparison needs "1.2.3"
 */
std::string strip_version_prefix(const std::string& tag) {
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) {
        return tag.substr(1);
    }
    return tag;
}

/**
 * @brief Safely get string value from JSON, handling null
 */
std::string json_string_or_empty(const json& j, const std::string& key) {
    if (!j.contains(key)) {
        return "";
    }
    const auto& val = j[key];
    if (val.is_null()) {
        return "";
    }
    if (val.is_string()) {
        return val.get<std::string>();
    }
    return "";
}

/**
 * @brief Parse ReleaseInfo from GitHub API JSON response
 *
 * @param json_str JSON response body
 * @param[out] info Parsed release info
 * @param[out] error Error message if parsing fails
 * @return true if parsing succeeded
 */
bool parse_github_release(const std::string& json_str, UpdateChecker::ReleaseInfo& info,
                          std::string& error) {
    try {
        auto j = json::parse(json_str);

        info.tag_name = json_string_or_empty(j, "tag_name");
        info.release_notes = json_string_or_empty(j, "body");
        info.published_at = json_string_or_empty(j, "published_at");

        // Strip 'v' prefix for version comparison
        info.version = strip_version_prefix(info.tag_name);

        if (info.version.empty()) {
            error = "Invalid release format: missing tag_name";
            return false;
        }

        // Validate version can be parsed
        if (!helix::version::parse_version(info.version).has_value()) {
            error = "Invalid version format: " + info.tag_name;
            return false;
        }

        // Find binary asset URL (look for .tar.gz)
        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                std::string name = asset.value("name", "");
                if (name.find(".tar.gz") != std::string::npos) {
                    info.download_url = asset.value("browser_download_url", "");
                    break;
                }
            }
        }

        return true;

    } catch (const json::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    } catch (const std::exception& e) {
        error = std::string("Parse error: ") + e.what();
        return false;
    }
}

/**
 * @brief Check if update is available by comparing versions
 *
 * @param current_version Current installed version
 * @param latest_version Latest release version
 * @return true if latest > current
 */
bool is_update_available(const std::string& current_version, const std::string& latest_version) {
    auto current = helix::version::parse_version(current_version);
    auto latest = helix::version::parse_version(latest_version);

    if (!current || !latest) {
        return false; // Can't determine, assume no update
    }

    return *latest > *current;
}

} // anonymous namespace

// ============================================================================
// Singleton Instance
// ============================================================================

UpdateChecker& UpdateChecker::instance() {
    static UpdateChecker instance;
    return instance;
}

UpdateChecker::~UpdateChecker() {
    // NOTE: Don't use spdlog here - during exit(), spdlog may already be destroyed
    // which causes a crash. Just silently clean up.

    // Signal cancellation to any running threads
    cancelled_ = true;
    download_cancelled_ = true;
    shutting_down_ = true;

    // MUST join threads if joinable, regardless of status.
    // A completed check still has a joinable thread.
    // Destroying a joinable std::thread without join() calls std::terminate()!
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void UpdateChecker::init() {
    if (initialized_) {
        return;
    }

    // Reset cancellation flags from any previous shutdown
    shutting_down_ = false;
    cancelled_ = false;
    download_cancelled_ = false;

    init_subjects();

    spdlog::info("[UpdateChecker] Initialized");
    initialized_ = true;
}

void UpdateChecker::shutdown() {
    if (!initialized_) {
        return;
    }

    spdlog::debug("[UpdateChecker] Shutting down");

    // Signal cancellation
    cancelled_ = true;
    download_cancelled_ = true;
    shutting_down_ = true;

    // Wait for worker thread to finish
    if (worker_thread_.joinable()) {
        spdlog::debug("[UpdateChecker] Joining worker thread");
        worker_thread_.join();
    }

    // Wait for download thread to finish
    if (download_thread_.joinable()) {
        spdlog::debug("[UpdateChecker] Joining download thread");
        download_thread_.join();
    }

    // Clear callback to prevent stale references
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_callback_ = nullptr;
    }

    // Cleanup subjects
    if (subjects_initialized_) {
        subjects_.deinit_all();
        subjects_initialized_ = false;
    }

    initialized_ = false;
    spdlog::debug("[UpdateChecker] Shutdown complete");
}

void UpdateChecker::init_subjects() {
    if (subjects_initialized_)
        return;

    UI_MANAGED_SUBJECT_INT(status_subject_, static_cast<int>(Status::Idle), "update_status",
                           subjects_);
    UI_MANAGED_SUBJECT_INT(checking_subject_, 0, "update_checking", subjects_);
    UI_MANAGED_SUBJECT_STRING(version_text_subject_, version_text_buf_, "", "update_version_text",
                              subjects_);
    UI_MANAGED_SUBJECT_STRING(new_version_subject_, new_version_buf_, "", "update_new_version",
                              subjects_);

    // Download subjects
    UI_MANAGED_SUBJECT_INT(download_status_subject_, static_cast<int>(DownloadStatus::Idle),
                           "download_status", subjects_);
    UI_MANAGED_SUBJECT_INT(download_progress_subject_, 0, "download_progress", subjects_);
    UI_MANAGED_SUBJECT_STRING(download_text_subject_, download_text_buf_, "", "download_text",
                              subjects_);

    subjects_initialized_ = true;
    spdlog::debug("[UpdateChecker] LVGL subjects initialized");
}

// ============================================================================
// Subject Accessors
// ============================================================================

lv_subject_t* UpdateChecker::status_subject() {
    return &status_subject_;
}
lv_subject_t* UpdateChecker::checking_subject() {
    return &checking_subject_;
}
lv_subject_t* UpdateChecker::version_text_subject() {
    return &version_text_subject_;
}
lv_subject_t* UpdateChecker::new_version_subject() {
    return &new_version_subject_;
}
lv_subject_t* UpdateChecker::download_status_subject() {
    return &download_status_subject_;
}
lv_subject_t* UpdateChecker::download_progress_subject() {
    return &download_progress_subject_;
}
lv_subject_t* UpdateChecker::download_text_subject() {
    return &download_text_subject_;
}

// ============================================================================
// Download Getters
// ============================================================================

UpdateChecker::DownloadStatus UpdateChecker::get_download_status() const {
    return download_status_.load();
}

int UpdateChecker::get_download_progress() const {
    return download_progress_.load();
}

std::string UpdateChecker::get_download_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return download_error_;
}

// Minimum free space required to attempt download (50 MB)
static constexpr size_t MIN_DOWNLOAD_SPACE_BYTES = 50ULL * 1024 * 1024;

static const char* const DOWNLOAD_FILENAME = "helixscreen-update.tar.gz";

// Check if a directory is writable and return available bytes (0 on failure)
static size_t get_available_space(const std::string& dir) {
    struct statvfs stat{};
    if (statvfs(dir.c_str(), &stat) != 0) {
        return 0;
    }
    // Use f_bavail (blocks available to unprivileged users) * fragment size
    return static_cast<size_t>(stat.f_bavail) * stat.f_frsize;
}

// Check if we can actually write to a directory
static bool is_writable_dir(const std::string& dir) {
    return access(dir.c_str(), W_OK) == 0;
}

std::string UpdateChecker::get_download_path() const {
    // Candidate directories, checked exhaustively — we pick the one with
    // the MOST free space so we don't fill up a tiny tmpfs or crowd out
    // gcode storage on an embedded device.
    std::vector<std::string> candidates;

    // Environment variables first
    for (const char* env_name : {"TMPDIR", "TMP", "TEMP"}) {
        const char* val = std::getenv(env_name);
        if (val != nullptr && val[0] != '\0') {
            candidates.emplace_back(val);
        }
    }

    // Home directory
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        candidates.emplace_back(home);
    }

    // Standard temp locations
    candidates.emplace_back("/tmp");
    candidates.emplace_back("/var/tmp");
    candidates.emplace_back("/mnt/tmp");

    // Persistent storage (embedded devices often have more room here)
    candidates.emplace_back("/data");
    candidates.emplace_back("/mnt/data");
    candidates.emplace_back("/usr/data");

    // Home variants (embedded devices with root user)
    candidates.emplace_back("/root");
    candidates.emplace_back("/home/root");

    // Evaluate all candidates — pick the one with the most free space
    std::string best_dir;
    size_t best_space = 0;

    for (const auto& dir : candidates) {
        if (!is_writable_dir(dir)) {
            continue;
        }

        auto space = get_available_space(dir);
        if (space < MIN_DOWNLOAD_SPACE_BYTES) {
            spdlog::debug("[UpdateChecker] Skipping {} ({:.1f} MB free, need {:.0f} MB)", dir,
                          static_cast<double>(space) / (1024.0 * 1024.0),
                          static_cast<double>(MIN_DOWNLOAD_SPACE_BYTES) / (1024.0 * 1024.0));
            continue;
        }

        if (space > best_space) {
            best_space = space;
            best_dir = dir;
        }
    }

    if (best_dir.empty()) {
        spdlog::error("[UpdateChecker] No writable directory with {} MB free space",
                      MIN_DOWNLOAD_SPACE_BYTES / (1024 * 1024));
        return {}; // Caller must handle empty path
    }

    spdlog::info("[UpdateChecker] Download directory: {} ({:.0f} MB free)", best_dir,
                 static_cast<double>(best_space) / (1024.0 * 1024.0));

    // Ensure trailing slash
    if (best_dir.back() != '/') {
        best_dir += '/';
    }
    return best_dir + DOWNLOAD_FILENAME;
}

std::string UpdateChecker::get_platform_asset_name() const {
    std::string version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        version = cached_info_ ? cached_info_->tag_name : "";
    }
#ifdef HELIX_PLATFORM_AD5M
    return "helixscreen-ad5m-" + version + ".tar.gz";
#elif defined(HELIX_PLATFORM_K1)
    return "helixscreen-k1-" + version + ".tar.gz";
#else
    return "helixscreen-pi-" + version + ".tar.gz";
#endif
}

void UpdateChecker::report_download_status(DownloadStatus status, int progress,
                                           const std::string& text, const std::string& error) {
    if (shutting_down_.load())
        return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        download_status_ = status;
        download_progress_ = progress;
        download_error_ = error;
    }

    ui_queue_update([this, status, progress, text]() {
        if (subjects_initialized_) {
            lv_subject_set_int(&download_status_subject_, static_cast<int>(status));
            lv_subject_set_int(&download_progress_subject_, progress);
            lv_subject_copy_string(&download_text_subject_, text.c_str());
        }
    });
}

// ============================================================================
// Download and Install
// ============================================================================

void UpdateChecker::start_download() {
    if (shutting_down_.load())
        return;

    // Safety: refuse download while printing
    auto job_state = get_printer_state().get_print_job_state();
    if (job_state == PrintJobState::PRINTING || job_state == PrintJobState::PAUSED) {
        spdlog::warn("[UpdateChecker] Cannot download update while printing");
        report_download_status(DownloadStatus::Error, 0, "Error: Cannot update while printing",
                               "Stop the print before installing updates");
        return;
    }

    // Must have a cached update to download
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cached_info_ || cached_info_->download_url.empty()) {
        spdlog::error("[UpdateChecker] start_download() called without cached update info");
        // Unlock before report_download_status (it also acquires mutex_)
        lock.unlock();
        report_download_status(DownloadStatus::Error, 0, "Error: No update available",
                               "No update information cached");
        return;
    }

    // Don't start if already downloading
    auto current = download_status_.load();
    if (current == DownloadStatus::Downloading || current == DownloadStatus::Installing) {
        spdlog::warn("[UpdateChecker] Download already in progress");
        return;
    }

    // Join previous download thread (must release lock first to prevent deadlock)
    lock.unlock();
    if (download_thread_.joinable()) {
        download_thread_.join();
    }

    download_cancelled_ = false;
    report_download_status(DownloadStatus::Downloading, 0, "Starting download...");

    download_thread_ = std::thread(&UpdateChecker::do_download, this);
}

void UpdateChecker::cancel_download() {
    download_cancelled_ = true;
}

void UpdateChecker::do_download() {
    std::string url;
    std::string version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!cached_info_)
            return;
        url = cached_info_->download_url;
        version = cached_info_->version;
    }

    auto download_path = get_download_path();
    if (download_path.empty()) {
        report_download_status(DownloadStatus::Error, 0, "Error: No space for download",
                               "Could not find a writable directory with enough free space");
        return;
    }
    spdlog::info("[UpdateChecker] Downloading {} to {}", url, download_path);

    // Progress callback -- dispatches to LVGL thread
    auto progress_cb = [this](size_t received, size_t total) {
        if (download_cancelled_.load())
            return;

        int percent = 0;
        if (total > 0) {
            percent = static_cast<int>((100 * received) / total);
        }

        // Throttle UI updates to every 2%
        int current = download_progress_.load();
        if (percent - current >= 2 || percent == 100) {
            auto mb_received = static_cast<double>(received) / (1024.0 * 1024.0);
            auto mb_total = static_cast<double>(total) / (1024.0 * 1024.0);
            auto text = fmt::format("Downloading... {:.1f}/{:.1f} MB", mb_received, mb_total);
            report_download_status(DownloadStatus::Downloading, percent, text);
        }
    };

    // Download the file using libhv
    size_t result = requests::downloadFile(url.c_str(), download_path.c_str(), progress_cb);

    if (download_cancelled_.load()) {
        spdlog::info("[UpdateChecker] Download cancelled");
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Idle, 0, "");
        return;
    }

    if (result == 0) {
        spdlog::error("[UpdateChecker] Download failed from {}", url);
        report_download_status(DownloadStatus::Error, 0, "Error: Download failed",
                               "Failed to download update file");
        return;
    }

    // Verify file size sanity (reject < 1MB or > 50MB)
    if (result < 1024 * 1024) {
        spdlog::error("[UpdateChecker] Downloaded file too small: {} bytes", result);
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Invalid download",
                               "Downloaded file is too small");
        return;
    }
    if (result > 50 * 1024 * 1024) {
        spdlog::error("[UpdateChecker] Downloaded file too large: {} bytes", result);
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Invalid download",
                               "Downloaded file is too large");
        return;
    }

    spdlog::info("[UpdateChecker] Download complete: {} bytes", result);
    report_download_status(DownloadStatus::Verifying, 100, "Verifying download...");

    // Verify gzip integrity
    auto verify_cmd = "gunzip -t " + download_path + " 2>&1";
    auto ret = std::system(verify_cmd.c_str());
    if (ret != 0) {
        spdlog::error("[UpdateChecker] Tarball verification failed");
        std::remove(download_path.c_str());
        report_download_status(DownloadStatus::Error, 0, "Error: Corrupt download",
                               "Downloaded file failed integrity check");
        return;
    }

    spdlog::info("[UpdateChecker] Tarball verified OK, proceeding to install");
    do_install(download_path);
}

void UpdateChecker::do_install(const std::string& tarball_path) {
    if (download_cancelled_.load()) {
        std::remove(tarball_path.c_str());
        report_download_status(DownloadStatus::Idle, 0, "");
        return;
    }

    report_download_status(DownloadStatus::Installing, 100, "Installing update...");

    // Find install.sh — production installs put it in the install root,
    // not in scripts/. Development builds use scripts/install.sh.
    std::string install_script;
    const std::vector<std::string> search_paths = {
        "/opt/helixscreen/install.sh",
        "/root/printer_software/helixscreen/install.sh",
        "/usr/data/helixscreen/install.sh",
        "scripts/install.sh", // development fallback
    };

    for (const auto& path : search_paths) {
        if (access(path.c_str(), X_OK) == 0) {
            install_script = path;
            break;
        }
    }

    if (install_script.empty()) {
        spdlog::error("[UpdateChecker] Cannot find install.sh");
        report_download_status(DownloadStatus::Error, 0, "Error: Installer not found",
                               "Cannot locate install.sh script");
        return;
    }

    spdlog::info("[UpdateChecker] Running: {} --local {} --update", install_script, tarball_path);

    auto cmd = install_script + " --local " + tarball_path + " --update 2>&1";
    auto ret = std::system(cmd.c_str());

    // Clean up tarball regardless of result
    std::remove(tarball_path.c_str());

    if (ret != 0) {
        spdlog::error("[UpdateChecker] Install script failed with code {}", ret);
        report_download_status(DownloadStatus::Error, 0, "Error: Installation failed",
                               "install.sh returned error code " + std::to_string(ret));
        return;
    }

    spdlog::info("[UpdateChecker] Update installed successfully!");

    std::string version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        version = cached_info_ ? cached_info_->version : "unknown";
    }

    report_download_status(DownloadStatus::Complete, 100,
                           "v" + version + " installed! Restart to apply.");
}

// ============================================================================
// Public API
// ============================================================================

void UpdateChecker::check_for_updates(Callback callback) {
    // Don't start new checks during shutdown
    if (shutting_down_) {
        spdlog::debug("[UpdateChecker] Ignoring check_for_updates during shutdown");
        return;
    }

    // Use mutex for entire operation to prevent race conditions.
    // This is safe because we join the previous thread before spawning a new one,
    // so we won't deadlock with the worker thread.
    std::unique_lock<std::mutex> lock(mutex_);

    // Atomic check if already checking
    if (status_ == Status::Checking) {
        spdlog::debug("[UpdateChecker] Check already in progress, ignoring");
        return;
    }

    // Rate limiting: return cached result if checked recently
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = now - last_check_time_;

    if (last_check_time_.time_since_epoch().count() > 0 && time_since_last < MIN_CHECK_INTERVAL) {
        auto minutes_remaining =
            std::chrono::duration_cast<std::chrono::minutes>(MIN_CHECK_INTERVAL - time_since_last)
                .count();
        spdlog::debug("[UpdateChecker] Rate limited, {} minutes until next check allowed",
                      minutes_remaining);

        // Return cached result via callback
        if (callback) {
            auto cached = cached_info_;
            auto status = status_.load();
            // Release lock before dispatching (callback may call back into UpdateChecker)
            lock.unlock();
            // Dispatch to LVGL thread
            ui_queue_update([callback, status, cached]() { callback(status, cached); });
        }
        return;
    }

    spdlog::info("[UpdateChecker] Starting update check");

    // CRITICAL: Join any previous thread before starting new one.
    // If a previous check completed naturally, the thread is still joinable
    // even though status is not Checking. Assigning to a joinable std::thread
    // causes std::terminate()!
    //
    // We must release the lock before joining to prevent deadlock - the worker
    // thread's report_result() also acquires this mutex.
    lock.unlock();
    if (worker_thread_.joinable()) {
        spdlog::debug("[UpdateChecker] Joining previous worker thread");
        worker_thread_.join();
    }
    lock.lock();

    // Re-check state after reacquiring lock (another thread may have started)
    if (status_ == Status::Checking || shutting_down_) {
        spdlog::debug("[UpdateChecker] State changed while joining, aborting");
        return;
    }

    // Store callback and reset state - all under lock
    pending_callback_ = callback;
    error_message_.clear();
    status_ = Status::Checking;
    cancelled_ = false;

    // Update subjects on LVGL thread (check_for_updates is public, could be called from any thread)
    if (subjects_initialized_) {
        ui_queue_update([this]() {
            lv_subject_set_int(&checking_subject_, 1);
            lv_subject_set_int(&status_subject_, static_cast<int>(Status::Checking));
            lv_subject_copy_string(&version_text_subject_, "Checking...");
        });
    }

    // Spawn worker thread
    worker_thread_ = std::thread(&UpdateChecker::do_check, this);
}

UpdateChecker::Status UpdateChecker::get_status() const {
    return status_.load();
}

std::optional<UpdateChecker::ReleaseInfo> UpdateChecker::get_cached_update() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_info_;
}

bool UpdateChecker::has_update_available() const {
    return status_ == Status::UpdateAvailable && get_cached_update().has_value();
}

std::string UpdateChecker::get_error_message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_message_;
}

void UpdateChecker::clear_cache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cached_info_.reset();
    error_message_.clear();
    status_ = Status::Idle;
    spdlog::debug("[UpdateChecker] Cache cleared");
}

// ============================================================================
// Worker Thread
// ============================================================================

void UpdateChecker::do_check() {
    spdlog::debug("[UpdateChecker] Worker thread started");

    // Record check time at start (under mutex for thread safety)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_check_time_ = std::chrono::steady_clock::now();
    }

    // Check for cancellation before network request
    if (cancelled_) {
        spdlog::debug("[UpdateChecker] Check cancelled before network request");
        return;
    }

    // Make HTTP request to GitHub API
    auto req = std::make_shared<HttpRequest>();
    req->method = HTTP_GET;
    req->url = GITHUB_API_URL;
    req->timeout = HTTP_TIMEOUT_SECONDS;
    req->headers["User-Agent"] = std::string("HelixScreen/") + HELIX_VERSION;
    req->headers["Accept"] = "application/vnd.github.v3+json";

    spdlog::debug("[UpdateChecker] Requesting: {}", GITHUB_API_URL);

    auto resp = requests::request(req);

    // Check for cancellation after network request
    if (cancelled_) {
        spdlog::debug("[UpdateChecker] Check cancelled after network request");
        return;
    }

    // Handle network failure
    if (!resp) {
        spdlog::warn("[UpdateChecker] Network request failed (no response)");
        report_result(Status::Error, std::nullopt, "Network request failed");
        return;
    }

    // Handle HTTP errors
    if (resp->status_code != 200) {
        const char* status_msg = resp->status_message();
        std::string error = "HTTP " + std::to_string(resp->status_code);
        if (status_msg != nullptr && status_msg[0] != '\0') {
            error += ": ";
            error += status_msg;
        }
        spdlog::warn("[UpdateChecker] {}", error);
        report_result(Status::Error, std::nullopt, error);
        return;
    }

    // Parse JSON response
    ReleaseInfo info;
    std::string parse_error;
    if (!parse_github_release(resp->body, info, parse_error)) {
        spdlog::warn("[UpdateChecker] {}", parse_error);
        report_result(Status::Error, std::nullopt, parse_error);
        return;
    }

    // Compare versions
    std::string current_version = HELIX_VERSION;
    spdlog::debug("[UpdateChecker] Current: {}, Latest: {}", current_version, info.version);

    if (is_update_available(current_version, info.version)) {
        spdlog::info("[UpdateChecker] Update available: {} -> {}", current_version, info.version);
        report_result(Status::UpdateAvailable, info, "");
    } else {
        spdlog::info("[UpdateChecker] Already up to date ({})", current_version);
        report_result(Status::UpToDate, std::nullopt, "");
    }

    spdlog::debug("[UpdateChecker] Worker thread finished");
}

void UpdateChecker::report_result(Status status, std::optional<ReleaseInfo> info,
                                  const std::string& error) {
    // Don't report if cancelled
    if (cancelled_ || shutting_down_) {
        spdlog::debug("[UpdateChecker] Skipping result report (cancelled/shutting down)");
        return;
    }

    // Update state under lock
    Callback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status;
        error_message_ = error;

        if (status == Status::UpdateAvailable && info) {
            cached_info_ = info;
        } else if (status == Status::UpToDate) {
            // Clear cached info when up to date
            cached_info_.reset();
        }
        // On Error, keep previous cached_info_ in case it was valid

        callback = pending_callback_;
    }

    // Dispatch to LVGL thread for subject updates and callback
    spdlog::debug("[UpdateChecker] Dispatching to LVGL thread");
    ui_queue_update([this, callback, status, info, error]() {
        spdlog::debug("[UpdateChecker] Executing on LVGL thread");

        // Update LVGL subjects
        if (subjects_initialized_) {
            lv_subject_set_int(&status_subject_, static_cast<int>(status));
            lv_subject_set_int(&checking_subject_, 0); // Done checking

            if (status == Status::UpdateAvailable && info) {
                snprintf(version_text_buf_, sizeof(version_text_buf_), "v%s available",
                         info->version.c_str());
                lv_subject_copy_string(&version_text_subject_, version_text_buf_);
                lv_subject_copy_string(&new_version_subject_, info->version.c_str());
            } else if (status == Status::UpToDate) {
                lv_subject_copy_string(&version_text_subject_, "Up to date");
                lv_subject_copy_string(&new_version_subject_, "");
            } else if (status == Status::Error) {
                snprintf(version_text_buf_, sizeof(version_text_buf_), "Error: %s", error.c_str());
                lv_subject_copy_string(&version_text_subject_, version_text_buf_);
                lv_subject_copy_string(&new_version_subject_, "");
            }
        }

        // Execute callback if present
        if (callback) {
            callback(status, info);
        }
    });
}
