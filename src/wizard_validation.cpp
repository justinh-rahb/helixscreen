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

#include "wizard_validation.h"
#include <cctype>

bool is_valid_ip_or_hostname(const std::string& host) {
    if (host.empty()) {
        return false;
    }

    // First check if it looks like an IP address (contains only digits and dots)
    bool looks_like_ip = true;
    for (char c : host) {
        if (!std::isdigit(c) && c != '.') {
            looks_like_ip = false;
            break;
        }
    }

    // If it looks like an IP, validate as IPv4
    if (looks_like_ip) {
        int dot_count = 0;
        size_t last_dot = 0;
        bool valid_ip = true;

        for (size_t i = 0; i < host.length(); i++) {
            if (host[i] == '.') {
                // Check segment between dots
                if (i == last_dot) {  // Empty segment (e.g., "192..1.1")
                    valid_ip = false;
                    break;
                }
                std::string segment = host.substr(last_dot, i - last_dot);
                if (segment.empty() || segment.length() > 3) {
                    valid_ip = false;
                    break;
                }
                try {
                    int num = std::stoi(segment);
                    if (num < 0 || num > 255) {
                        valid_ip = false;
                        break;
                    }
                } catch (...) {
                    valid_ip = false;
                    break;
                }
                dot_count++;
                last_dot = i + 1;
            }
        }

        if (valid_ip && dot_count == 3) {
            // Check last segment
            std::string last_segment = host.substr(last_dot);
            if (!last_segment.empty() && last_segment.length() <= 3) {
                try {
                    int num = std::stoi(last_segment);
                    if (num >= 0 && num <= 255) {
                        return true;
                    }
                } catch (...) {}
            }
        }
        // If it looks like an IP but isn't valid, reject it
        return false;
    }

    // Otherwise, check if it's a valid hostname
    // Hostname rules: alphanumeric, hyphens, dots, underscores
    // Must start with alphanumeric, must not be all numeric
    if (!std::isalnum(host[0])) {
        return false;
    }

    for (size_t i = 0; i < host.length(); i++) {
        char c = host[i];
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') {
            return false;
        }
    }

    return true;
}

bool is_valid_port(const std::string& port_str) {
    if (port_str.empty()) {
        return false;
    }

    // Check all characters are digits
    for (char c : port_str) {
        if (!std::isdigit(c)) {
            return false;
        }
    }

    // Parse and validate range
    try {
        int port = std::stoi(port_str);
        return port > 0 && port <= 65535;
    } catch (...) {
        return false;
    }
}
