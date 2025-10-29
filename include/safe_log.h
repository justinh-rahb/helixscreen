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

#pragma once

#include <spdlog/spdlog.h>

/**
 * @brief Safe logging for destructors and cleanup paths
 *
 * During static destruction, even checking spdlog::default_logger() can crash
 * because spdlog's internal mutexes may be destroyed.
 *
 * **SOLUTION:** Use fprintf(stderr, ...) in destructors and stop() methods.
 * fprintf has no static dependencies and is always safe.
 *
 * Example:
 *   MyClass::~MyClass() {
 *       fprintf(stderr, "[MyClass] Destructor called\n");
 *       cleanup();
 *   }
 *
 * For normal operation (not in destructors), use regular spdlog::* functions.
 */

// NOTE: These macros are NOT safe during static destruction - use fprintf instead
#define SAFE_LOG_DEBUG(...) \
    do { if (spdlog::default_logger()) { spdlog::debug(__VA_ARGS__); } } while(0)

#define SAFE_LOG_INFO(...) \
    do { if (spdlog::default_logger()) { spdlog::info(__VA_ARGS__); } } while(0)

#define SAFE_LOG_WARN(...) \
    do { if (spdlog::default_logger()) { spdlog::warn(__VA_ARGS__); } } while(0)

#define SAFE_LOG_ERROR(...) \
    do { if (spdlog::default_logger()) { spdlog::error(__VA_ARGS__); } } while(0)
