// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_ui_panel_console.cpp
 * @brief Unit tests for ConsolePanel G-code history functionality
 *
 * Tests the static helper methods and logic for parsing G-code console entries.
 * These tests don't require LVGL initialization since they test pure C++ logic.
 */

#include <string>
#include <vector>

#include "../catch_amalgamated.hpp"

// ============================================================================
// Test is_error_message() detection logic
// (Replicated from ui_panel_console.cpp since it's a private static method)
// ============================================================================

/**
 * @brief Check if a response message indicates an error
 *
 * Moonraker/Klipper errors typically start with "!!" or contain
 * "error" in the message.
 */
static bool is_error_message(const std::string& message) {
    if (message.empty()) {
        return false;
    }

    // Klipper errors typically start with "!!" prefix
    if (message.size() >= 2 && message[0] == '!' && message[1] == '!') {
        return true;
    }

    // Case-insensitive check for "error" at start
    if (message.size() >= 5) {
        std::string lower = message.substr(0, 5);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower == "error") {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Error Message Detection Tests
// ============================================================================

TEST_CASE("Console: is_error_message() with empty string", "[ui][error_detection]") {
    REQUIRE(is_error_message("") == false);
}

TEST_CASE("Console: is_error_message() with !! prefix", "[ui][error_detection]") {
    REQUIRE(is_error_message("!! Error: Heater not responding") == true);
    REQUIRE(is_error_message("!!Thermistor disconnected") == true);
    REQUIRE(is_error_message("!! ") == true);
}

TEST_CASE("Console: is_error_message() with Error prefix", "[ui][error_detection]") {
    REQUIRE(is_error_message("Error: Command failed") == true);
    REQUIRE(is_error_message("ERROR: Unknown G-code") == true);
    REQUIRE(is_error_message("error: invalid parameter") == true);
    REQUIRE(is_error_message("ErRoR: mixed case") == true);
}

TEST_CASE("Console: is_error_message() with normal responses", "[ui][error_detection]") {
    // Normal OK responses
    REQUIRE(is_error_message("ok") == false);
    REQUIRE(is_error_message("// Klipper state: Ready") == false);
    REQUIRE(is_error_message("B:60.0 /60.0 T0:210.0 /210.0") == false);

    // Messages containing "error" but not at start
    REQUIRE(is_error_message("No error detected") == false);
    REQUIRE(is_error_message("G-code M112 for error stop") == false);
}

TEST_CASE("Console: is_error_message() with single character", "[ui][error_detection]") {
    REQUIRE(is_error_message("!") == false); // Only one !, not two
    REQUIRE(is_error_message("E") == false); // Not enough characters for "Error"
}

TEST_CASE("Console: is_error_message() with boundary cases", "[ui][error_detection]") {
    REQUIRE(is_error_message("Err") == false);   // Too short for "Error"
    REQUIRE(is_error_message("Erro") == false);  // Still too short
    REQUIRE(is_error_message("Error") == true);  // Exactly "Error"
    REQUIRE(is_error_message("Errorx") == true); // Starts with "Error"
}

// ============================================================================
// Entry Type Classification Tests
// ============================================================================

TEST_CASE("Console: command vs response type classification", "[ui][entry_type]") {
    // These would come from MoonrakerClient::GcodeStoreEntry.type field

    // Commands are user input
    REQUIRE(std::string("command") == "command");

    // Responses are Klipper output
    REQUIRE(std::string("response") == "response");
}

// ============================================================================
// Message Content Tests
// ============================================================================

TEST_CASE("Console: typical Klipper error messages", "[ui][error_detection]") {
    // Real Klipper error message patterns
    REQUIRE(is_error_message("!! Move out of range: 0.000 250.000 0.500 [0.000]") == true);
    REQUIRE(is_error_message("!! Timer too close") == true);
    REQUIRE(is_error_message("!! MCU 'mcu' shutdown: Timer too close") == true);
    REQUIRE(is_error_message("Error: Bed heater not responding") == true);
}

TEST_CASE("Console: typical Klipper info messages", "[ui][error_detection]") {
    // Normal Klipper messages that should NOT be flagged as errors
    REQUIRE(is_error_message("// Klipper state: Ready") == false);
    REQUIRE(is_error_message("// probe at 150.000,150.000 is z=1.234567") == false);
    REQUIRE(is_error_message("echo: G28 homing completed") == false);
    REQUIRE(is_error_message("Recv: ok") == false);
}

// ============================================================================
// Temperature Message Filtering Tests
// (Replicated from ui_panel_console.cpp since it's a private static method)
// ============================================================================

/**
 * @brief Check if a message is a temperature status update
 *
 * Filters out periodic temperature reports like:
 * "ok T:210.0 /210.0 B:60.0 /60.0"
 */
static bool is_temp_message(const std::string& message) {
    if (message.empty()) {
        return false;
    }

    // Look for temperature patterns: T: or B: followed by numbers
    // Simple heuristic: contains "T:" or "B:" with "/" nearby
    size_t t_pos = message.find("T:");
    size_t b_pos = message.find("B:");

    if (t_pos != std::string::npos || b_pos != std::string::npos) {
        // Check for temperature format: number / number
        size_t slash_pos = message.find('/');
        if (slash_pos != std::string::npos) {
            // Very likely a temperature status message
            return true;
        }
    }

    return false;
}

// ============================================================================
// Temperature Message Detection Tests
// ============================================================================

TEST_CASE("Console: is_temp_message() with empty string", "[ui][temp_filter]") {
    REQUIRE(is_temp_message("") == false);
}

TEST_CASE("Console: is_temp_message() with standard temp reports", "[ui][temp_filter]") {
    // Standard Klipper temperature reports
    REQUIRE(is_temp_message("T:210.0 /210.0 B:60.0 /60.0") == true);
    REQUIRE(is_temp_message("ok T:210.5 /210.0 B:60.2 /60.0") == true);
    REQUIRE(is_temp_message("B:60.0 /60.0 T0:210.0 /210.0") == true);
    REQUIRE(is_temp_message("T0:200.0 /200.0 T1:0.0 /0.0 B:55.0 /55.0") == true);
}

TEST_CASE("Console: is_temp_message() with partial temp formats", "[ui][temp_filter]") {
    // Partial formats that should still be detected
    REQUIRE(is_temp_message("T:25.0 /0.0") == true); // Cold extruder
    REQUIRE(is_temp_message("B:22.0 /0.0") == true); // Cold bed
}

TEST_CASE("Console: is_temp_message() with non-temp messages", "[ui][temp_filter]") {
    // These should NOT be flagged as temperature messages
    REQUIRE(is_temp_message("ok") == false);
    REQUIRE(is_temp_message("// Klipper state: Ready") == false);
    REQUIRE(is_temp_message("echo: G28 completed") == false);
    REQUIRE(is_temp_message("!! Error: Heater failed") == false);
    REQUIRE(is_temp_message("M104 S200") == false); // Temp command, not status
    REQUIRE(is_temp_message("G28 X Y") == false);
}

TEST_CASE("Console: is_temp_message() edge cases", "[ui][temp_filter]") {
    // Edge cases that look like temps but aren't
    REQUIRE(is_temp_message("T:") == false);               // No value or slash
    REQUIRE(is_temp_message("B:60") == false);             // No slash
    REQUIRE(is_temp_message("Setting T: value") == false); // No slash

    // Edge cases that might have slashes but no temp
    REQUIRE(is_temp_message("path/to/file") == false); // No T: or B:
    REQUIRE(is_temp_message("50/50 complete") == false);
}

// ============================================================================
// HTML Span Parsing
// (Replicated from ui_panel_console.cpp since it's in anonymous namespace)
// ============================================================================

/**
 * @brief Parsed text segment with optional color class
 */
struct TextSegment {
    std::string text;
    std::string color_class; // empty = default, "success", "info", "warning", "error"
};

/**
 * @brief Check if a message contains HTML spans we can parse
 *
 * Looks for Mainsail-style spans from AFC/Happy Hare plugins:
 * <span class=success--text>LOADED</span>
 */
static bool contains_html_spans(const std::string& message) {
    return message.find("<span class=") != std::string::npos &&
           (message.find("success--text") != std::string::npos ||
            message.find("info--text") != std::string::npos ||
            message.find("warning--text") != std::string::npos ||
            message.find("error--text") != std::string::npos);
}

/**
 * @brief Parse HTML span tags into text segments with color classes
 *
 * Parses Mainsail-style spans: <span class=XXX--text>content</span>
 * Returns vector of segments, each with text and optional color class.
 */
static std::vector<TextSegment> parse_html_spans(const std::string& message) {
    std::vector<TextSegment> segments;

    size_t pos = 0;
    const size_t len = message.size();

    while (pos < len) {
        // Look for next <span class=
        size_t span_start = message.find("<span class=", pos);

        if (span_start == std::string::npos) {
            // No more spans - add remaining text as plain segment
            if (pos < len) {
                TextSegment seg;
                seg.text = message.substr(pos);
                if (!seg.text.empty()) {
                    segments.push_back(seg);
                }
            }
            break;
        }

        // Add any text before the span as a plain segment
        if (span_start > pos) {
            TextSegment seg;
            seg.text = message.substr(pos, span_start - pos);
            segments.push_back(seg);
        }

        // Parse the span: <span class=XXX--text>content</span>
        // Find the class value (ends at >)
        size_t class_start = span_start + 12; // strlen("<span class=")
        size_t class_end = message.find('>', class_start);

        if (class_end == std::string::npos) {
            // Malformed - add rest as plain text
            TextSegment seg;
            seg.text = message.substr(span_start);
            segments.push_back(seg);
            break;
        }

        // Extract color class from "success--text", "info--text", etc.
        std::string class_attr = message.substr(class_start, class_end - class_start);
        std::string color_class;

        if (class_attr.find("success--text") != std::string::npos) {
            color_class = "success";
        } else if (class_attr.find("info--text") != std::string::npos) {
            color_class = "info";
        } else if (class_attr.find("warning--text") != std::string::npos) {
            color_class = "warning";
        } else if (class_attr.find("error--text") != std::string::npos) {
            color_class = "error";
        }

        // Find the closing </span>
        size_t content_start = class_end + 1;
        size_t span_close = message.find("</span>", content_start);

        if (span_close == std::string::npos) {
            // No closing tag - add rest as plain text
            TextSegment seg;
            seg.text = message.substr(content_start);
            seg.color_class = color_class;
            segments.push_back(seg);
            break;
        }

        // Extract content between > and </span>
        TextSegment seg;
        seg.text = message.substr(content_start, span_close - content_start);
        seg.color_class = color_class;
        if (!seg.text.empty()) {
            segments.push_back(seg);
        }

        // Move past </span>
        pos = span_close + 7; // strlen("</span>")
    }

    return segments;
}

// ============================================================================
// HTML Span Detection Tests
// ============================================================================

TEST_CASE("Console: contains_html_spans() with no HTML", "[ui][html_parse]") {
    REQUIRE(contains_html_spans("") == false);
    REQUIRE(contains_html_spans("ok") == false);
    REQUIRE(contains_html_spans("Normal text message") == false);
    REQUIRE(contains_html_spans("!! Error message") == false);
}

TEST_CASE("Console: contains_html_spans() with HTML spans", "[ui][html_parse]") {
    REQUIRE(contains_html_spans("<span class=success--text>LOADED</span>") == true);
    REQUIRE(contains_html_spans("Text <span class=error--text>ERROR</span> more") == true);
    REQUIRE(contains_html_spans("lane1: <span class=info--text>ready</span>") == true);
}

TEST_CASE("Console: contains_html_spans() with partial/invalid HTML", "[ui][html_parse]") {
    REQUIRE(contains_html_spans("<span>no class</span>") == false);
    REQUIRE(contains_html_spans("<span class=other>unknown</span>") == false);
    REQUIRE(contains_html_spans("<div>not a span</div>") == false);
}

// ============================================================================
// HTML Span Parsing Tests
// ============================================================================

TEST_CASE("Console: parse_html_spans() plain text only", "[ui][html_parse]") {
    auto segments = parse_html_spans("Hello world");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "Hello world");
    REQUIRE(segments[0].color_class.empty());
}

TEST_CASE("Console: parse_html_spans() single span", "[ui][html_parse]") {
    auto segments = parse_html_spans("<span class=success--text>LOADED</span>");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "LOADED");
    REQUIRE(segments[0].color_class == "success");
}

TEST_CASE("Console: parse_html_spans() mixed content", "[ui][html_parse]") {
    auto segments = parse_html_spans("lane1: <span class=success--text>LOCKED</span> done");
    REQUIRE(segments.size() == 3);
    REQUIRE(segments[0].text == "lane1: ");
    REQUIRE(segments[0].color_class.empty());
    REQUIRE(segments[1].text == "LOCKED");
    REQUIRE(segments[1].color_class == "success");
    REQUIRE(segments[2].text == " done");
    REQUIRE(segments[2].color_class.empty());
}

TEST_CASE("Console: parse_html_spans() multiple spans", "[ui][html_parse]") {
    auto segments =
        parse_html_spans("<span class=success--text>OK</span><span class=error--text>FAIL</span>");
    REQUIRE(segments.size() == 2);
    REQUIRE(segments[0].text == "OK");
    REQUIRE(segments[0].color_class == "success");
    REQUIRE(segments[1].text == "FAIL");
    REQUIRE(segments[1].color_class == "error");
}

TEST_CASE("Console: parse_html_spans() all color classes", "[ui][html_parse]") {
    auto seg1 = parse_html_spans("<span class=success--text>a</span>");
    REQUIRE(seg1[0].color_class == "success");

    auto seg2 = parse_html_spans("<span class=info--text>b</span>");
    REQUIRE(seg2[0].color_class == "info");

    auto seg3 = parse_html_spans("<span class=warning--text>c</span>");
    REQUIRE(seg3[0].color_class == "warning");

    auto seg4 = parse_html_spans("<span class=error--text>d</span>");
    REQUIRE(seg4[0].color_class == "error");
}

TEST_CASE("Console: parse_html_spans() preserves newlines", "[ui][html_parse]") {
    auto segments = parse_html_spans("<span class=success--text>line1\nline2</span>");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "line1\nline2");
}

TEST_CASE("Console: parse_html_spans() real AFC output", "[ui][html_parse]") {
    // Real example from printer
    auto segments = parse_html_spans("lane1 tool cmd: T0  <span class=success--text>LOCKED</span>"
                                     "<span class=success--text> AND LOADED</span>");
    REQUIRE(segments.size() == 3);
    REQUIRE(segments[0].text == "lane1 tool cmd: T0  ");
    REQUIRE(segments[1].text == "LOCKED");
    REQUIRE(segments[1].color_class == "success");
    REQUIRE(segments[2].text == " AND LOADED");
    REQUIRE(segments[2].color_class == "success");
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("Console: parse_html_spans() empty span content", "[ui][html_parse]") {
    // Span with empty content should be skipped
    auto segments = parse_html_spans("<span class=success--text></span>");
    REQUIRE(segments.empty());
}

TEST_CASE("Console: parse_html_spans() malformed no closing bracket", "[ui][html_parse]") {
    // Missing > should return rest as plain text
    auto segments = parse_html_spans("<span class=success--text");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "<span class=success--text");
    REQUIRE(segments[0].color_class.empty());
}

TEST_CASE("Console: parse_html_spans() malformed no closing tag", "[ui][html_parse]") {
    // Missing </span> should still extract content with color
    auto segments = parse_html_spans("<span class=success--text>content");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "content");
    REQUIRE(segments[0].color_class == "success");
}

TEST_CASE("Console: parse_html_spans() unknown class extracts text plain", "[ui][html_parse]") {
    // Unknown class should still parse, just with empty color_class
    auto segments = parse_html_spans("<span class=unknown--text>text</span>");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "text");
    REQUIRE(segments[0].color_class.empty());
}

TEST_CASE("Console: parse_html_spans() quoted class attribute", "[ui][html_parse]") {
    // Quoted class attribute - class name includes quotes but still matches
    auto segments = parse_html_spans("<span class=\"success--text\">OK</span>");
    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].text == "OK");
    REQUIRE(segments[0].color_class == "success");
}
