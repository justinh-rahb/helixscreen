// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file test_thumbnail_path_resolution.cpp
 * @brief Tests for resolve_thumbnail_path() — Moonraker thumbnail path resolution
 *
 * Moonraker's metadata returns thumbnail relative_path values that are relative
 * to the gcode file's parent directory, not the gcodes root. For files in
 * subdirectories, the directory must be prepended so the download URL resolves
 * correctly. This is a known gotcha (see moonraker-home-assistant#116).
 */

#include "moonraker_types.h"

#include "../catch_amalgamated.hpp"

// ============================================================================
// Core Path Resolution
// ============================================================================

TEST_CASE("resolve_thumbnail_path: root directory files unchanged",
          "[thumbnail][path][resolution]") {
    // Files at gcodes root — relative_path is already correct
    REQUIRE(resolve_thumbnail_path(".thumbs/model-300x300.png", "") == ".thumbs/model-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: subdirectory files get prefix",
          "[thumbnail][path][resolution]") {
    // File: prints/model.gcode → thumb relative_path: .thumbs/model-300x300.png
    // Correct URL path: prints/.thumbs/model-300x300.png
    REQUIRE(resolve_thumbnail_path(".thumbs/model-300x300.png", "prints") ==
            "prints/.thumbs/model-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: nested subdirectories", "[thumbnail][path][resolution]") {
    // File: prints/favorites/model.gcode
    REQUIRE(resolve_thumbnail_path(".thumbs/model-300x300.png", "prints/favorites") ==
            "prints/favorites/.thumbs/model-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: deeply nested path", "[thumbnail][path][resolution]") {
    REQUIRE(resolve_thumbnail_path(".thumbs/model-32x32.png", "a/b/c/d") ==
            "a/b/c/d/.thumbs/model-32x32.png");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("resolve_thumbnail_path: empty thumbnail path returns empty",
          "[thumbnail][path][resolution]") {
    // No thumbnail available — should stay empty regardless of directory
    REQUIRE(resolve_thumbnail_path("", "").empty());
    REQUIRE(resolve_thumbnail_path("", "prints").empty());
    REQUIRE(resolve_thumbnail_path("", "prints/favorites").empty());
}

TEST_CASE("resolve_thumbnail_path: empty dir returns path unchanged",
          "[thumbnail][path][resolution]") {
    REQUIRE(resolve_thumbnail_path(".thumbs/file.png", "") == ".thumbs/file.png");
    REQUIRE(resolve_thumbnail_path("some/other/path.png", "") == "some/other/path.png");
}

TEST_CASE("resolve_thumbnail_path: paths with spaces", "[thumbnail][path][resolution]") {
    // Some slicers/users use spaces in filenames
    REQUIRE(resolve_thumbnail_path(".thumbs/My Model-300x300.png", "My Prints") ==
            "My Prints/.thumbs/My Model-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: paths with special characters",
          "[thumbnail][path][resolution]") {
    REQUIRE(resolve_thumbnail_path(".thumbs/benchy_(v2)-300x300.png", "prints+extras") ==
            "prints+extras/.thumbs/benchy_(v2)-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: different thumbnail sizes", "[thumbnail][path][resolution]") {
    // Slicers typically generate multiple sizes
    std::string dir = "prints";

    REQUIRE(resolve_thumbnail_path(".thumbs/model-32x32.png", dir) ==
            "prints/.thumbs/model-32x32.png");
    REQUIRE(resolve_thumbnail_path(".thumbs/model-300x300.png", dir) ==
            "prints/.thumbs/model-300x300.png");
    REQUIRE(resolve_thumbnail_path(".thumbs/model-400x300.png", dir) ==
            "prints/.thumbs/model-400x300.png");
}

TEST_CASE("resolve_thumbnail_path: non-.thumbs relative path", "[thumbnail][path][resolution]") {
    // Some slicers may use different thumbnail directory names
    REQUIRE(resolve_thumbnail_path(".thumbnails/model.png", "prints") ==
            "prints/.thumbnails/model.png");
    REQUIRE(resolve_thumbnail_path("thumbs/model.png", "prints") == "prints/thumbs/model.png");
}

// ============================================================================
// Integration-style: simulates the full metadata → URL construction flow
// ============================================================================

TEST_CASE("resolve_thumbnail_path: simulated URL construction for root file",
          "[thumbnail][path][resolution]") {
    // Simulate: file at root, metadata returns .thumbs/benchy-300x300.png
    std::string current_path = "";
    std::string thumb_relative = ".thumbs/benchy-300x300.png";

    std::string resolved = resolve_thumbnail_path(thumb_relative, current_path);

    // URL would be: /server/files/gcodes/.thumbs/benchy-300x300.png
    std::string url = "/server/files/gcodes/" + resolved;
    REQUIRE(url == "/server/files/gcodes/.thumbs/benchy-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: simulated URL construction for subfolder file",
          "[thumbnail][path][resolution]") {
    // Simulate: file in prints/, metadata returns .thumbs/benchy-300x300.png
    std::string current_path = "prints";
    std::string thumb_relative = ".thumbs/benchy-300x300.png";

    std::string resolved = resolve_thumbnail_path(thumb_relative, current_path);

    // URL would be: /server/files/gcodes/prints/.thumbs/benchy-300x300.png
    std::string url = "/server/files/gcodes/" + resolved;
    REQUIRE(url == "/server/files/gcodes/prints/.thumbs/benchy-300x300.png");
}

TEST_CASE("resolve_thumbnail_path: simulated URL construction for nested subfolder",
          "[thumbnail][path][resolution]") {
    std::string current_path = "prints/favorites";
    std::string thumb_relative = ".thumbs/benchy-300x300.png";

    std::string resolved = resolve_thumbnail_path(thumb_relative, current_path);

    std::string url = "/server/files/gcodes/" + resolved;
    REQUIRE(url == "/server/files/gcodes/prints/favorites/.thumbs/benchy-300x300.png");
}
