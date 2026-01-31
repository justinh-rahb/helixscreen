// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the notification_badge XML widget
 *
 * Registers the <notification_badge> widget with LVGL's XML system.
 * Creates a circular badge with auto-contrast text color.
 *
 * Attributes:
 * - variant: Badge color (info/warning/error) - default: info
 * - text: Badge text content - default: "0"
 * - bind_text: Subject to bind text to
 *
 * The badge automatically adjusts text color based on background
 * luminance for optimal readability.
 */
void ui_notification_badge_init(void);

#ifdef __cplusplus
}
#endif
