// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "moonraker_client_mock_internal.h"

#include <ctime>
#include <spdlog/spdlog.h>

namespace mock_internal {

void register_queue_handlers(std::unordered_map<std::string, MethodHandler>& registry) {
    // server.job_queue.status — return mock queue with 3 jobs
    registry["server.job_queue.status"] =
        []([[maybe_unused]] MoonrakerClientMock* self, [[maybe_unused]] const json& params,
           std::function<void(json)> success_cb,
           [[maybe_unused]] std::function<void(const MoonrakerError&)> error_cb) -> bool {
        json result;
        result["queue_state"] = "ready";
        json jobs = json::array();

        double now = static_cast<double>(time(nullptr));
        jobs.push_back({{"job_id", "0001"},
                        {"filename", "benchy_v2.gcode"},
                        {"time_added", now - 3600},
                        {"time_in_queue", 3600.0}});
        jobs.push_back({{"job_id", "0002"},
                        {"filename", "calibration_cube.gcode"},
                        {"time_added", now - 1800},
                        {"time_in_queue", 1800.0}});
        jobs.push_back({{"job_id", "0003"},
                        {"filename", "phone_stand.gcode"},
                        {"time_added", now - 300},
                        {"time_in_queue", 300.0}});
        result["queued_jobs"] = jobs;

        spdlog::debug("[MoonrakerClientMock] Returning mock job queue: {} jobs", jobs.size());

        if (success_cb) {
            success_cb(json{{"result", result}});
        }
        return true;
    };

    // server.job_queue.start — start processing queue
    registry["server.job_queue.start"] =
        []([[maybe_unused]] MoonrakerClientMock* self, [[maybe_unused]] const json& params,
           std::function<void(json)> success_cb,
           [[maybe_unused]] std::function<void(const MoonrakerError&)> error_cb) -> bool {
        spdlog::info("[MoonrakerClientMock] Job queue started");
        if (success_cb) {
            success_cb(json::object());
        }
        return true;
    };

    // server.job_queue.pause — pause queue processing
    registry["server.job_queue.pause"] =
        []([[maybe_unused]] MoonrakerClientMock* self, [[maybe_unused]] const json& params,
           std::function<void(json)> success_cb,
           [[maybe_unused]] std::function<void(const MoonrakerError&)> error_cb) -> bool {
        spdlog::info("[MoonrakerClientMock] Job queue paused");
        if (success_cb) {
            success_cb(json::object());
        }
        return true;
    };

    // server.job_queue.post_job — add job(s) to queue
    registry["server.job_queue.post_job"] =
        []([[maybe_unused]] MoonrakerClientMock* self, const json& params,
           std::function<void(json)> success_cb,
           [[maybe_unused]] std::function<void(const MoonrakerError&)> error_cb) -> bool {
        if (params.contains("filenames")) {
            for (const auto& f : params["filenames"]) {
                spdlog::info("[MoonrakerClientMock] Added job to queue: {}", f.get<std::string>());
            }
        }
        if (success_cb) {
            success_cb(json::object());
        }
        return true;
    };

    // server.job_queue.delete_job — remove job(s) from queue
    registry["server.job_queue.delete_job"] =
        []([[maybe_unused]] MoonrakerClientMock* self, const json& params,
           std::function<void(json)> success_cb,
           [[maybe_unused]] std::function<void(const MoonrakerError&)> error_cb) -> bool {
        if (params.contains("job_ids")) {
            spdlog::info("[MoonrakerClientMock] Removed {} jobs from queue",
                         params["job_ids"].size());
        }
        if (success_cb) {
            success_cb(json::object());
        }
        return true;
    };
}

} // namespace mock_internal
