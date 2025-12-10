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

#include "ui_temp_graph.h"

#include "ui_theme.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper: Find series metadata by ID
static ui_temp_series_meta_t* find_series(ui_temp_graph_t* graph, int series_id) {
    if (!graph || series_id < 0 || series_id >= UI_TEMP_GRAPH_MAX_SERIES) {
        return nullptr;
    }

    for (int i = 0; i < graph->series_count; i++) {
        if (graph->series_meta[i].id == series_id &&
            graph->series_meta[i].chart_series != nullptr) {
            return &graph->series_meta[i];
        }
    }
    return nullptr;
}

// Helper: Create a muted (reduced opacity) version of a color
// Since LVGL chart cursors don't support opacity, we blend toward the background
static lv_color_t mute_color(lv_color_t color, lv_opa_t opa) {
    // Blend toward dark gray (chart background) based on opacity
    // opa=255 = full color, opa=0 = full background
    lv_color_t bg = lv_color_hex(0x2D2D2D); // Dark chart background
    uint8_t r = (color.red * opa + bg.red * (255 - opa)) / 255;
    uint8_t g = (color.green * opa + bg.green * (255 - opa)) / 255;
    uint8_t b = (color.blue * opa + bg.blue * (255 - opa)) / 255;
    return lv_color_make(r, g, b);
}

// Helper: Convert temperature value to pixel Y coordinate
// LVGL chart cursors use content-relative coordinates (same as lv_chart_get_point_pos_by_id).
// The draw_cursors function adds obj->coords.y1 internally to convert to screen coords.
// So we return content-relative Y position (0 = top of content area).
static int32_t temp_to_pixel_y(ui_temp_graph_t* graph, float temp) {
    int32_t chart_height = lv_obj_get_content_height(graph->chart);
    if (chart_height <= 0) {
        return 0; // Chart not laid out yet
    }

    // Map temperature to pixel position within content area (inverted for Y axis)
    // temp=max_temp → Y=0 (top), temp=min_temp → Y=chart_height (bottom)
    // This matches LVGL's lv_chart_get_point_pos_by_id calculation:
    //   temp_y = lv_map(v, ymin, ymax, 0, h)
    //   p_out->y = h - temp_y
    int32_t content_y = chart_height - lv_map((int32_t)temp, (int32_t)graph->min_temp,
                                              (int32_t)graph->max_temp, 0, chart_height);

    return content_y;
}

// Helper: Update all cursor positions (called on resize)
static void update_all_cursor_positions(ui_temp_graph_t* graph) {
    if (!graph)
        return;

    for (int i = 0; i < UI_TEMP_GRAPH_MAX_SERIES; i++) {
        ui_temp_series_meta_t* meta = &graph->series_meta[i];
        if (meta->chart_series && meta->target_cursor && meta->show_target) {
            int32_t pixel_y = temp_to_pixel_y(graph, meta->target_temp);
            lv_chart_set_cursor_pos_y(graph->chart, meta->target_cursor, pixel_y);
        }
    }
}

// Event callback: Recalculate cursor positions when chart is resized
static void chart_resize_cb(lv_event_t* e) {
    lv_obj_t* chart = lv_event_get_target_obj(e);
    ui_temp_graph_t* graph = static_cast<ui_temp_graph_t*>(lv_obj_get_user_data(chart));
    if (graph) {
        update_all_cursor_positions(graph);
    }
}

// Helper: Find series metadata by color (for draw task matching)
static ui_temp_series_meta_t* find_series_by_color(ui_temp_graph_t* graph, lv_color_t color) {
    if (!graph)
        return nullptr;

    for (int i = 0; i < graph->series_count; i++) {
        if (graph->series_meta[i].chart_series &&
            lv_color_to_u32(graph->series_meta[i].color) == lv_color_to_u32(color)) {
            return &graph->series_meta[i];
        }
    }
    return nullptr;
}

// LVGL 9 draw task callback for gradient fills under chart lines
// Called for each draw task when LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS is set
static void draw_task_cb(lv_event_t* e) {
    lv_draw_task_t* draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t* base_dsc =
        static_cast<lv_draw_dsc_base_t*>(lv_draw_task_get_draw_dsc(draw_task));

    // Only process line draws for chart series (LV_PART_ITEMS)
    if (base_dsc->part != LV_PART_ITEMS ||
        lv_draw_task_get_type(draw_task) != LV_DRAW_TASK_TYPE_LINE) {
        return;
    }

    lv_obj_t* chart = lv_event_get_target_obj(e);
    ui_temp_graph_t* graph = static_cast<ui_temp_graph_t*>(lv_event_get_user_data(e));
    if (!graph)
        return;

    // Get line draw descriptor early for debug logging
    lv_draw_line_dsc_t* line_dsc =
        static_cast<lv_draw_line_dsc_t*>(lv_draw_task_get_draw_dsc(draw_task));

    // Get chart coordinates for bottom reference
    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);

    // Filter out garbage/uninitialized data lines:
    // When the chart has sparse data (few real points among LV_CHART_POINT_NONE values),
    // LVGL may emit line segments where one endpoint is at the chart's top edge
    // (from clamped INT32_MAX values). These appear as vertical lines from the top of chart to
    // data. Skip if either point is at/above chart top (clamped garbage value)
    int32_t chart_top = coords.y1;
    if (line_dsc->p1.y <= chart_top || line_dsc->p2.y <= chart_top) {
        spdlog::trace(
            "[TempGraph] Skipping garbage line: ({},{}) to ({},{}) - point at/above chart top {}",
            static_cast<int>(line_dsc->p1.x), static_cast<int>(line_dsc->p1.y),
            static_cast<int>(line_dsc->p2.x), static_cast<int>(line_dsc->p2.y), chart_top);
        return; // Skip gradient fill for this garbage line
    }

    // Find the series this line belongs to (match by color)
    ui_temp_series_meta_t* meta = find_series_by_color(graph, line_dsc->color);
    lv_opa_t top_opa = meta ? meta->gradient_top_opa : UI_TEMP_GRAPH_GRADIENT_TOP_OPA;
    lv_opa_t bottom_opa = meta ? meta->gradient_bottom_opa : UI_TEMP_GRAPH_GRADIENT_BOTTOM_OPA;
    lv_color_t ser_color = line_dsc->color;

    // Get line segment Y coordinates
    int32_t line_y_upper = LV_MIN(line_dsc->p1.y, line_dsc->p2.y);
    int32_t line_y_lower = LV_MAX(line_dsc->p1.y, line_dsc->p2.y);
    int32_t chart_bottom = coords.y2;

    // Calculate gradient span from line to chart bottom
    int32_t gradient_span = chart_bottom - line_y_upper;
    if (gradient_span <= 0)
        gradient_span = 1;

    lv_opa_t opa_upper = top_opa;
    int32_t lower_distance = line_y_lower - line_y_upper;
    lv_opa_t opa_lower =
        static_cast<lv_opa_t>(top_opa - (top_opa - bottom_opa) * lower_distance / gradient_span);

    // Draw triangle from line segment down to the lower point
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.p[0].x = line_dsc->p1.x;
    tri_dsc.p[0].y = line_dsc->p1.y;
    tri_dsc.p[1].x = line_dsc->p2.x;
    tri_dsc.p[1].y = line_dsc->p2.y;
    tri_dsc.p[2].x = line_dsc->p1.y < line_dsc->p2.y ? line_dsc->p1.x : line_dsc->p2.x;
    tri_dsc.p[2].y = LV_MAX(line_dsc->p1.y, line_dsc->p2.y);

    tri_dsc.grad.dir = LV_GRAD_DIR_VER;
    tri_dsc.grad.stops[0].color = ser_color;
    tri_dsc.grad.stops[0].opa = opa_upper;
    tri_dsc.grad.stops[0].frac = 0;
    tri_dsc.grad.stops[1].color = ser_color;
    tri_dsc.grad.stops[1].opa = opa_lower;
    tri_dsc.grad.stops[1].frac = 255;

    lv_draw_triangle(base_dsc->layer, &tri_dsc);

    // Draw rectangle from the lower line point down to chart bottom
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
    rect_dsc.bg_grad.stops[0].color = ser_color;
    rect_dsc.bg_grad.stops[0].opa = opa_lower;
    rect_dsc.bg_grad.stops[0].frac = 0;
    rect_dsc.bg_grad.stops[1].color = ser_color;
    rect_dsc.bg_grad.stops[1].opa = bottom_opa;
    rect_dsc.bg_grad.stops[1].frac = 255;

    lv_area_t rect_area;
    rect_area.x1 = static_cast<int32_t>(LV_MIN(line_dsc->p1.x, line_dsc->p2.x));
    rect_area.x2 = static_cast<int32_t>(LV_MAX(line_dsc->p1.x, line_dsc->p2.x));
    if (rect_area.x2 <= rect_area.x1) {
        rect_area.x2 = rect_area.x1 + 1;
    }
    rect_area.y1 = static_cast<int32_t>(LV_MAX(line_dsc->p1.y, line_dsc->p2.y));
    rect_area.y2 = static_cast<int32_t>(coords.y2);

    lv_draw_rect(base_dsc->layer, &rect_dsc, &rect_area);
}

// Draw X-axis time labels (rendered directly on graph canvas)
// Uses LV_EVENT_DRAW_POST to draw after chart content is rendered
static void draw_x_axis_labels_cb(lv_event_t* e) {
    lv_obj_t* chart = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    ui_temp_graph_t* graph = static_cast<ui_temp_graph_t*>(lv_event_get_user_data(e));

    if (!layer || !graph || graph->visible_point_count == 0) {
        spdlog::trace("[TempGraph] X-axis skip: layer={}, graph={}, points={}", layer != nullptr,
                      graph != nullptr, graph ? graph->visible_point_count : -1);
        return; // No data to label yet
    }

    spdlog::debug("[TempGraph] Drawing X-axis labels: {} points, first={}ms, latest={}ms",
                  graph->visible_point_count, graph->first_point_time_ms,
                  graph->latest_point_time_ms);

    // Get chart bounds
    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);
    int32_t content_width = lv_obj_get_content_width(chart);

    // Calculate content area (inside padding)
    int32_t pad_left = lv_obj_get_style_pad_left(chart, LV_PART_MAIN);
    int32_t pad_right = lv_obj_get_style_pad_right(chart, LV_PART_MAIN);
    int32_t content_x1 = coords.x1 + pad_left;
    int32_t content_x2 = coords.x2 - pad_right;

    // Setup label descriptor - match Y-axis label style exactly
    // Y-axis labels use UI_FONT_SMALL and get their color from LVGL's theme default
    // We get the text color from the chart's LV_PART_TICKS style (used for axis labels)
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_obj_get_style_text_color(chart, LV_PART_MAIN); // Use chart's text color
    label_dsc.font = UI_FONT_SMALL; // Same font as Y-axis labels (noto_sans_16)
    label_dsc.align = LV_TEXT_ALIGN_CENTER;
    label_dsc.opa = lv_obj_get_style_text_opa(chart, LV_PART_MAIN); // Use chart's text opacity

    // The chart has a fixed number of points (1200 by default = 20 minutes at 1 sample/sec)
    // Each data point represents 1 second, so the total time span is fixed
    int64_t total_display_time_ms =
        static_cast<int64_t>(graph->point_count) * 1000; // 20 min = 1,200,000 ms

    // The "now" time is always at the rightmost edge
    int64_t latest_ms = graph->latest_point_time_ms;

    // Calculate what time corresponds to the leftmost edge of the graph
    // This is "now - total_display_time"
    int64_t leftmost_ms = latest_ms - total_display_time_ms;

    // Label positioning: Y is aligned with bottom Y-axis label (0° baseline)
    // The Y-axis labels use space_between layout, with 0° at the chart content bottom
    int32_t pad_bottom = lv_obj_get_style_pad_bottom(chart, LV_PART_MAIN);
    int32_t label_height =
        ui_theme_get_font_height(UI_FONT_SMALL); // Get actual font height dynamically
    // Add small gap between chart content and X-axis labels
    int32_t space_xs = ui_theme_get_spacing("space_xs"); // 4/5/6px gap
    // Position below chart content with responsive gap
    int32_t label_y = coords.y2 - pad_bottom + space_xs;

    // Determine label interval based on total display time (fixed)
    // For 20 minutes (1200s), show labels every 5 minutes
    int64_t label_interval_ms = 5 * 60 * 1000; // 5 minutes default
    if (total_display_time_ms < 2 * 60 * 1000) {
        label_interval_ms = 30 * 1000; // 30 seconds for < 2 min
    } else if (total_display_time_ms < 10 * 60 * 1000) {
        label_interval_ms = 2 * 60 * 1000; // 2 minutes for < 10 min
    }

    // Track previous label to skip duplicates
    char prev_label[8] = "";

    // Draw labels at regular time intervals
    // Start from the first time that's on a nice boundary after the left edge
    int64_t first_label_ms = (leftmost_ms / label_interval_ms) * label_interval_ms;
    if (first_label_ms < leftmost_ms) {
        first_label_ms += label_interval_ms;
    }

    for (int64_t label_time_ms = first_label_ms; label_time_ms <= latest_ms;
         label_time_ms += label_interval_ms) {
        // Calculate X position for this time
        // Position is proportional: (time - leftmost) / total_display_time * width
        int64_t time_offset = label_time_ms - leftmost_ms;
        int32_t label_x = content_x1 + static_cast<int32_t>((time_offset * content_width) /
                                                            total_display_time_ms);

        // Skip if outside chart bounds
        if (label_x < content_x1 || label_x > content_x2) {
            continue;
        }

        // Format time string (HH:MM)
        time_t time_sec = static_cast<time_t>(label_time_ms / 1000);
        struct tm* tm_info = localtime(&time_sec);
        // Use static buffer array - LVGL may defer draw and need persistent strings
        static char time_str_buf[8][8]; // 8 labels max, 8 chars each
        static int time_str_idx = 0;
        char* time_str = time_str_buf[time_str_idx++ % 8];
        strftime(time_str, 8, "%H:%M", tm_info);

        // Skip duplicate labels (same HH:MM)
        if (strcmp(time_str, prev_label) == 0) {
            continue;
        }
        strncpy(prev_label, time_str, sizeof(prev_label) - 1);

        // Create label area (centered on label_x)
        lv_area_t label_area;
        label_area.x1 = label_x - 20; // 40px width, centered
        label_area.y1 = label_y;
        label_area.x2 = label_x + 20;
        label_area.y2 = label_y + label_height;

        label_dsc.text = time_str;
        lv_draw_label(layer, &label_dsc, &label_area);
    }

    // Always show "now" label at rightmost edge (most recent data)
    {
        time_t now_sec = static_cast<time_t>(latest_ms / 1000);
        struct tm* tm_info = localtime(&now_sec);
        // Use static buffer for the "now" label
        static char now_str[8];
        strftime(now_str, sizeof(now_str), "%H:%M", tm_info);

        // Only draw if different from last label
        if (strcmp(now_str, prev_label) != 0) {
            lv_area_t label_area;
            label_area.x1 = content_x2 - 20;
            label_area.y1 = label_y;
            label_area.x2 = content_x2 + 20;
            label_area.y2 = label_y + label_height;

            label_dsc.text = now_str;
            label_dsc.align = LV_TEXT_ALIGN_RIGHT; // Right-align the "now" label
            lv_draw_label(layer, &label_dsc, &label_area);
        }
    }
}

// Draw Y-axis temperature labels (rendered directly on graph canvas)
// Uses LV_EVENT_DRAW_POST to draw after chart content is rendered
static void draw_y_axis_labels_cb(lv_event_t* e) {
    lv_obj_t* chart = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    ui_temp_graph_t* graph = static_cast<ui_temp_graph_t*>(lv_event_get_user_data(e));

    if (!layer || !graph || !graph->show_y_axis || graph->y_axis_increment <= 0) {
        return; // Y-axis labels disabled or invalid config
    }

    // Get chart bounds and content area
    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);
    int32_t pad_top = lv_obj_get_style_pad_top(chart, LV_PART_MAIN);
    int32_t space_xs = ui_theme_get_spacing("space_xs"); // Gap between labels and chart

    // Chart content area (where data is drawn)
    // Account for extra X-axis label space in bottom padding
    int32_t x_axis_label_height = ui_theme_get_font_height(UI_FONT_SMALL);
    int32_t content_top = coords.y1 + pad_top;
    int32_t content_bottom = coords.y2 - (space_xs + x_axis_label_height + space_xs);
    int32_t content_height = content_bottom - content_top;

    // Setup label descriptor - same style as X-axis
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = lv_obj_get_style_text_color(chart, LV_PART_MAIN);
    label_dsc.font = UI_FONT_SMALL;
    label_dsc.align = LV_TEXT_ALIGN_RIGHT; // Right-align Y-axis labels
    label_dsc.opa = lv_obj_get_style_text_opa(chart, LV_PART_MAIN);

    // Y-axis label width (for positioning)
    int32_t label_height = ui_theme_get_font_height(UI_FONT_SMALL);
    int32_t label_width = 40; // Fixed width for Y-axis labels (fits "320°")

    // Temperature range
    float temp_range = graph->max_temp - graph->min_temp;
    if (temp_range <= 0)
        return;

    // Draw labels at each temperature increment
    // Use static buffer array - LVGL may defer draw and need persistent strings
    static char temp_str_buf[8][8]; // 8 labels max (0, 80, 160, 240, 320 = 5 for nozzle)
    static int temp_str_idx = 0;
    temp_str_idx = 0; // Reset each draw cycle

    for (float temp = graph->min_temp; temp <= graph->max_temp; temp += graph->y_axis_increment) {
        // Calculate Y position: (max_temp - temp) / range * height
        // Top = max_temp, Bottom = min_temp
        float temp_fraction = (graph->max_temp - temp) / temp_range;
        int32_t label_y = content_top + static_cast<int32_t>(temp_fraction * content_height);

        // Center label vertically on the temperature line
        label_y -= label_height / 2;

        // Format temperature string into persistent buffer
        char* temp_str = temp_str_buf[temp_str_idx++ % 8];
        snprintf(temp_str, 8, "%d°", static_cast<int>(temp));

        // Draw label in left padding area (to the left of chart content)
        lv_area_t label_area;
        label_area.x1 = coords.x1;
        label_area.y1 = label_y;
        label_area.x2 = coords.x1 + label_width;
        label_area.y2 = label_y + label_height;

        label_dsc.text = temp_str;
        lv_draw_label(layer, &label_dsc, &label_area);
    }
}

// Create temperature graph widget
ui_temp_graph_t* ui_temp_graph_create(lv_obj_t* parent) {
    if (!parent) {
        spdlog::error("[TempGraph] NULL parent");
        return nullptr;
    }

    // Allocate graph structure using RAII
    auto graph_ptr = std::make_unique<ui_temp_graph_t>();
    if (!graph_ptr) {
        spdlog::error("[TempGraph] Failed to allocate graph structure");
        return nullptr;
    }

    ui_temp_graph_t* graph = graph_ptr.get();
    memset(graph, 0, sizeof(ui_temp_graph_t));

    // Initialize defaults
    graph->point_count = UI_TEMP_GRAPH_DEFAULT_POINTS;
    graph->min_temp = UI_TEMP_GRAPH_DEFAULT_MIN_TEMP;
    graph->max_temp = UI_TEMP_GRAPH_DEFAULT_MAX_TEMP;
    graph->series_count = 0;
    graph->next_series_id = 0;
    graph->y_axis_increment = 0; // Disabled by default (caller must enable)
    graph->show_y_axis = false;

    // Create LVGL chart
    graph->chart = lv_chart_create(parent);
    if (!graph->chart) {
        spdlog::error("[TempGraph] Failed to create chart widget");
        return nullptr; // graph_ptr auto-freed
    }

    // Configure chart
    lv_chart_set_type(graph->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(graph->chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(graph->chart, static_cast<uint32_t>(graph->point_count));

    // Set Y-axis range
    lv_chart_set_axis_range(graph->chart, LV_CHART_AXIS_PRIMARY_Y,
                            static_cast<int32_t>(graph->min_temp),
                            static_cast<int32_t>(graph->max_temp));

    // Style chart background (theme handles colors)
    lv_obj_set_style_bg_opa(graph->chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(graph->chart, 0, LV_PART_MAIN);
    // Use responsive spacing from theme constants
    int32_t space_md = ui_theme_get_spacing("space_md"); // 8/10/12px
    int32_t space_xs = ui_theme_get_spacing("space_xs"); // 4/5/6px for axis label gaps
    int32_t label_height = ui_theme_get_font_height(UI_FONT_SMALL);
    int32_t y_axis_label_width = 40; // Width for Y-axis labels (fits "320°")

    lv_obj_set_style_pad_top(graph->chart, space_md, LV_PART_MAIN);
    lv_obj_set_style_pad_right(graph->chart, space_md, LV_PART_MAIN);
    // Extra left padding for Y-axis labels: label width + gap
    lv_obj_set_style_pad_left(graph->chart, y_axis_label_width + space_xs, LV_PART_MAIN);
    // Extra bottom padding for X-axis time labels: gap + label height
    lv_obj_set_style_pad_bottom(graph->chart, space_xs + label_height + space_xs, LV_PART_MAIN);

    // Style division lines (theme handles colors)
    lv_obj_set_style_line_width(graph->chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(graph->chart, LV_OPA_30, LV_PART_MAIN); // Subtle - 30% opacity

    // Style data series lines
    lv_obj_set_style_line_width(graph->chart, 2, LV_PART_ITEMS);          // Series line thickness
    lv_obj_set_style_line_opa(graph->chart, LV_OPA_COVER, LV_PART_ITEMS); // Full opacity for series

    // Hide point indicators (circles at each data point)
    lv_obj_set_style_width(graph->chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(graph->chart, 0, LV_PART_INDICATOR);

    // Style target temperature cursor (dashed line, thinner than series)
    // Note: cursor color is set per-cursor in ui_temp_graph_add_series()
    lv_obj_set_style_line_width(graph->chart, 1, LV_PART_CURSOR);      // Thinner than series (2px)
    lv_obj_set_style_line_dash_width(graph->chart, 6, LV_PART_CURSOR); // Dash length
    lv_obj_set_style_line_dash_gap(graph->chart, 4, LV_PART_CURSOR);   // Gap between dashes
    lv_obj_set_style_width(graph->chart, 0, LV_PART_CURSOR);           // No point marker
    lv_obj_set_style_height(graph->chart, 0, LV_PART_CURSOR);          // No point marker

    // Configure division line count
    lv_chart_set_div_line_count(graph->chart, 5, 10); // 5 horizontal, 10 vertical division lines

    // Enable LVGL 9 draw task events for gradient fills under chart lines
    lv_obj_add_flag(graph->chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(graph->chart, draw_task_cb, LV_EVENT_DRAW_TASK_ADDED, graph);

    // Store graph pointer in chart user data for retrieval
    lv_obj_set_user_data(graph->chart, graph);

    // Register resize callback to recalculate value-based cursor positions
    lv_obj_add_event_cb(graph->chart, chart_resize_cb, LV_EVENT_SIZE_CHANGED, nullptr);

    // Register X-axis label draw callback (renders time labels directly on canvas)
    lv_obj_add_event_cb(graph->chart, draw_x_axis_labels_cb, LV_EVENT_DRAW_POST, graph);

    // Register Y-axis label draw callback (renders temperature labels directly on canvas)
    lv_obj_add_event_cb(graph->chart, draw_y_axis_labels_cb, LV_EVENT_DRAW_POST, graph);

    spdlog::info("[TempGraph] Created: {} points, {:.0f}-{:.0f}°C range", graph->point_count,
                 graph->min_temp, graph->max_temp);

    // Transfer ownership to caller
    return graph_ptr.release();
}

// Destroy temperature graph widget
void ui_temp_graph_destroy(ui_temp_graph_t* graph) {
    if (!graph)
        return;

    // Transfer ownership to RAII wrapper - automatic cleanup
    std::unique_ptr<ui_temp_graph_t> graph_ptr(graph);

    // Remove all series (cursors will be cleaned up automatically)
    for (int i = 0; i < graph_ptr->series_count; i++) {
        if (graph_ptr->series_meta[i].chart_series) {
            lv_chart_remove_series(graph_ptr->chart, graph_ptr->series_meta[i].chart_series);
        }
    }

    // Delete chart widget
    if (graph_ptr->chart) {
        lv_obj_del(graph_ptr->chart);
    }

    // graph_ptr automatically freed via ~unique_ptr()
    spdlog::debug("[TempGraph] Destroyed");
}

// Get underlying chart widget
lv_obj_t* ui_temp_graph_get_chart(ui_temp_graph_t* graph) {
    return graph ? graph->chart : nullptr;
}

// Add a new temperature series
int ui_temp_graph_add_series(ui_temp_graph_t* graph, const char* name, lv_color_t color) {
    if (!graph || !name) {
        spdlog::error("[TempGraph] NULL graph or name");
        return -1;
    }

    if (graph->series_count >= UI_TEMP_GRAPH_MAX_SERIES) {
        spdlog::error("[TempGraph] Maximum series count ({}) reached", UI_TEMP_GRAPH_MAX_SERIES);
        return -1;
    }

    // Find next available slot
    int slot = -1;
    for (int i = 0; i < UI_TEMP_GRAPH_MAX_SERIES; i++) {
        if (graph->series_meta[i].chart_series == nullptr) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        spdlog::error("[TempGraph] No available series slots");
        return -1;
    }

    // Create LVGL chart series
    lv_chart_series_t* ser = lv_chart_add_series(graph->chart, color, LV_CHART_AXIS_PRIMARY_Y);
    if (!ser) {
        spdlog::error("[TempGraph] Failed to create chart series");
        return -1;
    }

    // Initialize all points to the minimum temperature (baseline)
    // This prevents the LVGL chart from drawing garbage lines when sparse data exists.
    // LVGL's chart drawing uses start_point unconditionally for the first Y calculation,
    // so if start_point contains LV_CHART_POINT_NONE (INT32_MAX), it produces huge Y values.
    // By initializing to min_temp, the graph starts as a flat line at 0° which looks clean.
    lv_chart_set_all_values(graph->chart, ser, static_cast<int32_t>(graph->min_temp));

    // Initialize series metadata
    ui_temp_series_meta_t* meta = &graph->series_meta[slot];
    meta->id = graph->next_series_id++;
    meta->chart_series = ser;
    meta->color = color;
    strncpy(meta->name, name, sizeof(meta->name) - 1);
    meta->name[sizeof(meta->name) - 1] = '\0';
    meta->visible = true;
    meta->show_target = false;
    meta->target_temp = 0.0f;
    meta->gradient_bottom_opa = UI_TEMP_GRAPH_GRADIENT_BOTTOM_OPA;
    meta->gradient_top_opa = UI_TEMP_GRAPH_GRADIENT_TOP_OPA;

    // Create target temperature cursor (horizontal dashed line, initially hidden)
    // Note: We don't use lv_chart_set_cursor_point because that binds the cursor
    // to a data point which scrolls. Instead we use lv_chart_set_cursor_pos for
    // a fixed Y position representing the target temperature.
    // Use moderately muted color so target line is visible but distinct from actual data
    lv_color_t cursor_color = mute_color(color, LV_OPA_50); // 50% opacity for visibility
    meta->target_cursor = lv_chart_add_cursor(graph->chart, cursor_color, LV_DIR_HOR);

    graph->series_count++;

    spdlog::debug("[TempGraph] Added series {} '{}' (slot {}, color 0x{:06X})", meta->id,
                  meta->name, slot, lv_color_to_u32(color));

    return meta->id;
}

// Remove a temperature series
void ui_temp_graph_remove_series(ui_temp_graph_t* graph, int series_id) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    // Remove cursor (if exists)
    if (meta->target_cursor) {
        // LVGL doesn't have lv_chart_remove_cursor, cursor is auto-freed with chart
        meta->target_cursor = nullptr;
    }

    // Remove chart series
    lv_chart_remove_series(graph->chart, meta->chart_series);

    // Clear metadata
    memset(meta, 0, sizeof(ui_temp_series_meta_t));
    meta->chart_series = nullptr;

    graph->series_count--;

    spdlog::debug("[TempGraph] Removed series {} ({} series remaining)", series_id,
                  graph->series_count);
}

// Show or hide a series
void ui_temp_graph_show_series(ui_temp_graph_t* graph, int series_id, bool visible) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    meta->visible = visible;

    // Use LVGL's public API to hide/show series
    lv_chart_hide_series(graph->chart, meta->chart_series, !visible);

    lv_obj_invalidate(graph->chart);
    spdlog::debug("[TempGraph] Series {} '{}' {}", series_id, meta->name,
                  visible ? "shown" : "hidden");
}

// Add a single temperature point (push mode)
void ui_temp_graph_update_series(ui_temp_graph_t* graph, int series_id, float temp) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    // Add point to series (shifts old data left)
    lv_chart_set_next_value(graph->chart, meta->chart_series, (int32_t)temp);
}

// Add temperature point with timestamp (for X-axis labels)
void ui_temp_graph_update_series_with_time(ui_temp_graph_t* graph, int series_id, float temp,
                                           int64_t timestamp_ms) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    // Debug: Log first few data points to trace garbage data issue
    static int debug_count = 0;
    if (debug_count < 10) {
        spdlog::debug("[TempGraph] Adding point #{} to series {}: temp={:.1f}°C (int32={})",
                      debug_count++, series_id, temp, static_cast<int32_t>(temp));
    }

    // Track timestamp for X-axis label rendering
    graph->latest_point_time_ms = timestamp_ms;
    graph->visible_point_count++;

    // When buffer is full, oldest point scrolls off - update first_point_time_ms
    // First point timestamp is latest - (display period) when full, or the first timestamp received
    if (graph->first_point_time_ms == 0) {
        graph->first_point_time_ms = timestamp_ms;
    } else if (graph->visible_point_count > graph->point_count) {
        // Buffer is full, oldest point scrolled off
        // First visible point is now: latest - (point_count - 1) samples back
        // At 1 sample/sec, that's (point_count - 1) seconds before latest
        graph->first_point_time_ms =
            timestamp_ms - static_cast<int64_t>(graph->point_count - 1) * 1000;
    }

    // Add point to series (shifts old data left)
    lv_chart_set_next_value(graph->chart, meta->chart_series, (int32_t)temp);
}

// Replace all data points (array mode)
void ui_temp_graph_set_series_data(ui_temp_graph_t* graph, int series_id, const float* temps,
                                   int count) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta || !temps || count <= 0) {
        spdlog::error("[TempGraph] Invalid parameters");
        return;
    }

    // Clear existing data using public API (reset to min_temp to avoid garbage lines)
    lv_chart_set_all_values(graph->chart, meta->chart_series,
                            static_cast<int32_t>(graph->min_temp));

    // Convert float array to int32_t array for LVGL API (using RAII)
    int points_to_copy = count > graph->point_count ? graph->point_count : count;
    auto values = std::make_unique<int32_t[]>(static_cast<size_t>(points_to_copy));
    if (!values) {
        spdlog::error("[TempGraph] Failed to allocate conversion buffer");
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(points_to_copy); i++) {
        values[i] = static_cast<int32_t>(temps[i]);
    }

    // Set data using public API
    lv_chart_set_series_values(graph->chart, meta->chart_series, values.get(),
                               static_cast<size_t>(points_to_copy));

    // values automatically freed via ~unique_ptr()

    lv_chart_refresh(graph->chart);
    spdlog::debug("[TempGraph] Series {} '{}' data set ({} points)", series_id, meta->name,
                  points_to_copy);
}

// Clear all data
void ui_temp_graph_clear(ui_temp_graph_t* graph) {
    if (!graph)
        return;

    for (int i = 0; i < graph->series_count; i++) {
        ui_temp_series_meta_t* meta = &graph->series_meta[i];
        if (meta->chart_series) {
            // Reset to min_temp to avoid garbage lines from POINT_NONE
            lv_chart_set_all_values(graph->chart, meta->chart_series,
                                    static_cast<int32_t>(graph->min_temp));
        }
    }

    lv_chart_refresh(graph->chart);
    spdlog::debug("[TempGraph] All data cleared");
}

// Clear data for a specific series
void ui_temp_graph_clear_series(ui_temp_graph_t* graph, int series_id) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    // Reset to min_temp to avoid garbage lines from POINT_NONE (see clear() comment)
    lv_chart_set_all_values(graph->chart, meta->chart_series,
                            static_cast<int32_t>(graph->min_temp));

    lv_chart_refresh(graph->chart);
    spdlog::debug("[TempGraph] Series {} '{}' cleared", series_id, meta->name);
}

// Set target temperature and visibility
void ui_temp_graph_set_series_target(ui_temp_graph_t* graph, int series_id, float target,
                                     bool show) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    // Store the value (used for recalculation on resize)
    meta->target_temp = target;
    meta->show_target = show;

    if (meta->target_cursor && show) {
        // Convert temperature value to pixel coordinate
        // This abstraction allows callers to work with temperatures, not pixels
        lv_obj_update_layout(graph->chart); // Ensure dimensions are current
        int32_t pixel_y = temp_to_pixel_y(graph, target);
        lv_chart_set_cursor_pos_y(graph->chart, meta->target_cursor, pixel_y);

        lv_obj_invalidate(graph->chart);
    }

    spdlog::debug("[TempGraph] Series {} target: {:.1f}°C ({})", series_id, target,
                  show ? "shown" : "hidden");
}

// Show or hide target temperature line
void ui_temp_graph_show_target(ui_temp_graph_t* graph, int series_id, bool show) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    ui_temp_graph_set_series_target(graph, series_id, meta->target_temp, show);
}

// Set Y-axis temperature range
void ui_temp_graph_set_temp_range(ui_temp_graph_t* graph, float min, float max) {
    if (!graph || min >= max) {
        spdlog::error("[TempGraph] Invalid temperature range");
        return;
    }

    graph->min_temp = min;
    graph->max_temp = max;

    lv_chart_set_axis_range(graph->chart, LV_CHART_AXIS_PRIMARY_Y, static_cast<int32_t>(min),
                            static_cast<int32_t>(max));

    // Recalculate all cursor positions since value-to-pixel mapping changed
    update_all_cursor_positions(graph);

    spdlog::debug("[TempGraph] Temperature range set: {:.0f} - {:.0f}°C", min, max);
}

// Set point count
void ui_temp_graph_set_point_count(ui_temp_graph_t* graph, int count) {
    if (!graph || count <= 0) {
        spdlog::error("[TempGraph] Invalid point count");
        return;
    }

    graph->point_count = count;
    lv_chart_set_point_count(graph->chart, static_cast<uint32_t>(count));

    spdlog::debug("[TempGraph] Point count set: {}", count);
}

// Set gradient opacity for a series
void ui_temp_graph_set_series_gradient(ui_temp_graph_t* graph, int series_id, lv_opa_t bottom_opa,
                                       lv_opa_t top_opa) {
    ui_temp_series_meta_t* meta = find_series(graph, series_id);
    if (!meta) {
        spdlog::error("[TempGraph] Series {} not found", series_id);
        return;
    }

    meta->gradient_bottom_opa = bottom_opa;
    meta->gradient_top_opa = top_opa;

    lv_obj_invalidate(graph->chart);

    spdlog::debug("[TempGraph] Series {} gradient: bottom={}%, top={}%", series_id,
                  (bottom_opa * 100) / 255, (top_opa * 100) / 255);
}

// Set Y-axis label configuration
void ui_temp_graph_set_y_axis(ui_temp_graph_t* graph, float increment, bool show) {
    if (!graph) {
        spdlog::error("[TempGraph] NULL graph in set_y_axis");
        return;
    }

    graph->y_axis_increment = increment;
    graph->show_y_axis = show;

    // Force redraw to apply changes
    lv_obj_invalidate(graph->chart);

    spdlog::debug("[TempGraph] Y-axis config: increment={:.0f}°, show={}", increment, show);
}
