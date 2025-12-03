// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (C) 2025 356C LLC
 * Author: Preston Brown <pbrown@brown-house.net>
 *
 * This file is part of HelixScreen.
 *
 * HelixScreen is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HelixScreen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HelixScreen. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ethernet_backend.h"

#include "ethernet_backend_mock.h"
#include "runtime_config.h"

#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include "ethernet_backend_macos.h"
#else
#include "ethernet_backend_linux.h"
#endif

std::unique_ptr<EthernetBackend> EthernetBackend::create() {
    // In test mode, always use mock unless --real-ethernet was specified
    if (get_runtime_config().should_mock_ethernet()) {
        spdlog::info("[EthernetBackend] Test mode: using mock backend");
        return std::make_unique<EthernetBackendMock>();
    }

#ifdef __APPLE__
    // macOS: Use native backend (handles missing interface gracefully)
    spdlog::debug("[EthernetBackend] Creating macOS backend");
    auto backend = std::make_unique<EthernetBackendMacOS>();

    if (backend->has_interface()) {
        spdlog::debug("[EthernetBackend] macOS backend initialized (interface found)");
    } else {
        spdlog::info("[EthernetBackend] No Ethernet interface found");
    }
    return backend;
#else
    // Linux: Use native backend (handles missing interface gracefully)
    spdlog::debug("[EthernetBackend] Creating Linux backend");
    auto backend = std::make_unique<EthernetBackendLinux>();

    if (backend->has_interface()) {
        spdlog::debug("[EthernetBackend] Linux backend initialized (interface found)");
    } else {
        spdlog::info("[EthernetBackend] No Ethernet interface found");
    }
    return backend;
#endif
}
