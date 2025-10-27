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
 */

#pragma once

#include <string>

/**
 * @brief Validate IP address or hostname format
 *
 * Accepts:
 * - Valid IPv4 addresses (e.g., "192.168.1.1")
 * - Valid hostnames (e.g., "printer.local", "my-printer")
 *
 * @param host IP address or hostname string
 * @return true if valid, false otherwise
 */
bool is_valid_ip_or_hostname(const std::string& host);

/**
 * @brief Validate port number
 *
 * Accepts port numbers in range 1-65535 (numeric only)
 *
 * @param port_str Port number as string
 * @return true if valid, false otherwise
 */
bool is_valid_port(const std::string& port_str);
