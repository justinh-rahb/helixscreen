// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../catch_amalgamated.hpp"

#include "print_start_analyzer.h"

using namespace helix;

// ============================================================================
// Test Macros (representative samples from real printers)
// ============================================================================

// Basic Voron-style PRINT_START with bed mesh and QGL
static const char* BASIC_PRINT_START = R"(
; Basic PRINT_START with common operations
G28                             ; Home all axes
QUAD_GANTRY_LEVEL               ; Level the gantry
BED_MESH_CALIBRATE              ; Create bed mesh
CLEAN_NOZZLE                    ; Clean the nozzle
M109 S{params.EXTRUDER|default(210)|float}
)";

// Advanced PRINT_START with skip parameters already defined
static const char* CONTROLLABLE_PRINT_START = R"(
{% set BED_TEMP = params.BED|default(60)|float %}
{% set EXTRUDER_TEMP = params.EXTRUDER|default(210)|float %}
{% set SKIP_BED_MESH = params.SKIP_BED_MESH|default(0)|int %}
{% set SKIP_QGL = params.SKIP_QGL|default(0)|int %}

G28                             ; Home all axes

{% if SKIP_QGL == 0 %}
    QUAD_GANTRY_LEVEL           ; Level the gantry
{% endif %}

{% if SKIP_BED_MESH == 0 %}
    BED_MESH_CALIBRATE          ; Create bed mesh
{% endif %}

M190 S{BED_TEMP}
M109 S{EXTRUDER_TEMP}
)";

// PRINT_START with only some operations controllable
static const char* PARTIAL_CONTROLLABLE = R"(
{% set SKIP_MESH = params.SKIP_MESH|default(0)|int %}
{% set BED = params.BED|default(60)|float %}

G28
QUAD_GANTRY_LEVEL               ; Always runs - not controllable

{% if SKIP_MESH == 0 %}
    BED_MESH_CALIBRATE
{% endif %}

CLEAN_NOZZLE                    ; Always runs - not controllable
M109 S{params.EXTRUDER|default(210)|float}
)";

// Empty/minimal macro
static const char* MINIMAL_PRINT_START = R"(
G28
M109 S{params.EXTRUDER}
M190 S{params.BED}
)";

// Macro with alternative parameter patterns
static const char* ALT_PATTERN_PRINT_START = R"(
{% set bed_temp = params.BED_TEMP|default(60)|float %}
{% set nozzle_temp = params.NOZZLE_TEMP|default(210)|float %}
{% set force_level = params.FORCE_LEVEL|default(0)|int %}

G28
{% if not SKIP_GANTRY %}
QUAD_GANTRY_LEVEL
{% endif %}

BED_MESH_CALIBRATE PROFILE=default
M109 S{nozzle_temp}
M190 S{bed_temp}
)";

// ============================================================================
// Tests: Operation Detection
// ============================================================================

TEST_CASE("PrintStartAnalyzer: Basic operation detection", "[print_start][parsing]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", BASIC_PRINT_START);

    REQUIRE(result.found == true);
    REQUIRE(result.macro_name == "PRINT_START");

    SECTION("Detects all operations") {
        REQUIRE(result.total_ops_count >= 4);
        REQUIRE(result.has_operation(PrintStartOpCategory::HOMING));
        REQUIRE(result.has_operation(PrintStartOpCategory::QGL));
        REQUIRE(result.has_operation(PrintStartOpCategory::BED_LEVELING));
        REQUIRE(result.has_operation(PrintStartOpCategory::NOZZLE_CLEAN));
    }

    SECTION("No operations are controllable in basic macro") {
        REQUIRE(result.is_controllable == false);
        REQUIRE(result.controllable_count == 0);
    }

    SECTION("Can get specific operations") {
        auto qgl = result.get_operation(PrintStartOpCategory::QGL);
        REQUIRE(qgl != nullptr);
        REQUIRE(qgl->name == "QUAD_GANTRY_LEVEL");
        REQUIRE(qgl->has_skip_param == false);
    }
}

TEST_CASE("PrintStartAnalyzer: Controllable operation detection", "[print_start][parsing]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", CONTROLLABLE_PRINT_START);

    SECTION("Detects controllable operations") {
        REQUIRE(result.is_controllable == true);
        REQUIRE(result.controllable_count >= 2);
    }

    SECTION("QGL is controllable via SKIP_QGL") {
        auto qgl = result.get_operation(PrintStartOpCategory::QGL);
        REQUIRE(qgl != nullptr);
        REQUIRE(qgl->has_skip_param == true);
        REQUIRE(qgl->skip_param_name == "SKIP_QGL");
    }

    SECTION("Bed mesh is controllable via SKIP_BED_MESH") {
        auto mesh = result.get_operation(PrintStartOpCategory::BED_LEVELING);
        REQUIRE(mesh != nullptr);
        REQUIRE(mesh->has_skip_param == true);
        REQUIRE(mesh->skip_param_name == "SKIP_BED_MESH");
    }

    SECTION("Homing is always detected but not controllable") {
        auto homing = result.get_operation(PrintStartOpCategory::HOMING);
        REQUIRE(homing != nullptr);
        REQUIRE(homing->has_skip_param == false);
    }

    SECTION("Extracts known parameters") {
        REQUIRE(result.known_params.size() >= 4);
        // Should include BED, EXTRUDER, SKIP_BED_MESH, SKIP_QGL
        auto has_param = [&](const std::string& name) {
            return std::find(result.known_params.begin(), result.known_params.end(), name) !=
                   result.known_params.end();
        };
        REQUIRE(has_param("BED"));
        REQUIRE(has_param("EXTRUDER"));
        REQUIRE(has_param("SKIP_BED_MESH"));
        REQUIRE(has_param("SKIP_QGL"));
    }
}

TEST_CASE("PrintStartAnalyzer: Partial controllability", "[print_start][parsing]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", PARTIAL_CONTROLLABLE);

    SECTION("Detects mixed controllability") {
        REQUIRE(result.is_controllable == true);
        REQUIRE(result.controllable_count == 1);
        REQUIRE(result.total_ops_count >= 3);
    }

    SECTION("Bed mesh is controllable via SKIP_MESH variant") {
        auto mesh = result.get_operation(PrintStartOpCategory::BED_LEVELING);
        REQUIRE(mesh != nullptr);
        REQUIRE(mesh->has_skip_param == true);
        REQUIRE(mesh->skip_param_name == "SKIP_MESH");
    }

    SECTION("QGL is NOT controllable") {
        auto qgl = result.get_operation(PrintStartOpCategory::QGL);
        REQUIRE(qgl != nullptr);
        REQUIRE(qgl->has_skip_param == false);
    }

    SECTION("get_uncontrollable_operations returns QGL and NOZZLE_CLEAN") {
        auto uncontrollable = result.get_uncontrollable_operations();
        // Should include QGL and NOZZLE_CLEAN, but NOT HOMING (excluded by design)
        REQUIRE(uncontrollable.size() >= 2);

        bool has_qgl = false;
        bool has_clean = false;
        for (auto* op : uncontrollable) {
            if (op->category == PrintStartOpCategory::QGL) has_qgl = true;
            if (op->category == PrintStartOpCategory::NOZZLE_CLEAN) has_clean = true;
        }
        REQUIRE(has_qgl);
        REQUIRE(has_clean);
    }
}

TEST_CASE("PrintStartAnalyzer: Minimal macro", "[print_start][parsing]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", MINIMAL_PRINT_START);

    SECTION("Detects only homing") {
        REQUIRE(result.total_ops_count == 1);
        REQUIRE(result.has_operation(PrintStartOpCategory::HOMING));
        REQUIRE_FALSE(result.has_operation(PrintStartOpCategory::BED_LEVELING));
        REQUIRE_FALSE(result.has_operation(PrintStartOpCategory::QGL));
    }

    SECTION("Extracts basic parameters") {
        REQUIRE(result.known_params.size() >= 2);
        auto has_param = [&](const std::string& name) {
            return std::find(result.known_params.begin(), result.known_params.end(), name) !=
                   result.known_params.end();
        };
        REQUIRE(has_param("EXTRUDER"));
        REQUIRE(has_param("BED"));
    }
}

TEST_CASE("PrintStartAnalyzer: Alternative skip parameter patterns",
          "[print_start][parsing]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", ALT_PATTERN_PRINT_START);

    SECTION("Detects QGL with SKIP_GANTRY variant") {
        auto qgl = result.get_operation(PrintStartOpCategory::QGL);
        REQUIRE(qgl != nullptr);
        REQUIRE(qgl->has_skip_param == true);
        REQUIRE(qgl->skip_param_name == "SKIP_GANTRY");
    }

    SECTION("Extracts alternative parameter names") {
        auto has_param = [&](const std::string& name) {
            return std::find(result.known_params.begin(), result.known_params.end(), name) !=
                   result.known_params.end();
        };
        REQUIRE(has_param("BED_TEMP"));
        REQUIRE(has_param("NOZZLE_TEMP"));
        REQUIRE(has_param("FORCE_LEVEL"));
    }
}

// ============================================================================
// Tests: Helper Functions
// ============================================================================

TEST_CASE("PrintStartAnalyzer: categorize_operation", "[print_start][helpers]") {
    REQUIRE(PrintStartAnalyzer::categorize_operation("BED_MESH_CALIBRATE") ==
            PrintStartOpCategory::BED_LEVELING);
    REQUIRE(PrintStartAnalyzer::categorize_operation("G29") == PrintStartOpCategory::BED_LEVELING);
    REQUIRE(PrintStartAnalyzer::categorize_operation("QUAD_GANTRY_LEVEL") ==
            PrintStartOpCategory::QGL);
    REQUIRE(PrintStartAnalyzer::categorize_operation("Z_TILT_ADJUST") ==
            PrintStartOpCategory::Z_TILT);
    REQUIRE(PrintStartAnalyzer::categorize_operation("CLEAN_NOZZLE") ==
            PrintStartOpCategory::NOZZLE_CLEAN);
    REQUIRE(PrintStartAnalyzer::categorize_operation("G28") == PrintStartOpCategory::HOMING);
    REQUIRE(PrintStartAnalyzer::categorize_operation("UNKNOWN_CMD") ==
            PrintStartOpCategory::UNKNOWN);
}

TEST_CASE("PrintStartAnalyzer: get_suggested_skip_param", "[print_start][helpers]") {
    REQUIRE(PrintStartAnalyzer::get_suggested_skip_param("BED_MESH_CALIBRATE") == "SKIP_BED_MESH");
    REQUIRE(PrintStartAnalyzer::get_suggested_skip_param("QUAD_GANTRY_LEVEL") == "SKIP_QGL");
    REQUIRE(PrintStartAnalyzer::get_suggested_skip_param("Z_TILT_ADJUST") == "SKIP_Z_TILT");
    REQUIRE(PrintStartAnalyzer::get_suggested_skip_param("CLEAN_NOZZLE") == "SKIP_NOZZLE_CLEAN");

    // Unknown operation should return SKIP_ + name
    REQUIRE(PrintStartAnalyzer::get_suggested_skip_param("CUSTOM_OP") == "SKIP_CUSTOM_OP");
}

TEST_CASE("PrintStartAnalyzer: category_to_string", "[print_start][helpers]") {
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::BED_LEVELING)) == "bed_leveling");
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::QGL)) == "qgl");
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::Z_TILT)) == "z_tilt");
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::NOZZLE_CLEAN)) == "nozzle_clean");
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::HOMING)) == "homing");
    REQUIRE(std::string(category_to_string(PrintStartOpCategory::UNKNOWN)) == "unknown");
}

TEST_CASE("PrintStartAnalyzer: summary generation", "[print_start][helpers]") {
    SECTION("Found macro summary") {
        auto result = PrintStartAnalyzer::parse_macro("PRINT_START", CONTROLLABLE_PRINT_START);
        auto summary = result.summary();

        REQUIRE(summary.find("PRINT_START") != std::string::npos);
        REQUIRE(summary.find("controllable") != std::string::npos);
    }

    SECTION("Not found summary") {
        PrintStartAnalysis result;
        result.found = false;
        auto summary = result.summary();

        REQUIRE(summary.find("No print start macro found") != std::string::npos);
    }
}

// ============================================================================
// Tests: Edge Cases
// ============================================================================

TEST_CASE("PrintStartAnalyzer: Empty macro", "[print_start][edge]") {
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", "");

    REQUIRE(result.found == true);
    REQUIRE(result.total_ops_count == 0);
    REQUIRE(result.is_controllable == false);
}

TEST_CASE("PrintStartAnalyzer: Comments only", "[print_start][edge]") {
    const char* comments_only = R"(
; This is a comment
# This is also a comment
    ; Indented comment
)";
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", comments_only);

    REQUIRE(result.total_ops_count == 0);
}

TEST_CASE("PrintStartAnalyzer: Operations with parameters", "[print_start][edge]") {
    const char* ops_with_params = R"(
G28 X Y                         ; Home X and Y only
BED_MESH_CALIBRATE PROFILE=default
QUAD_GANTRY_LEVEL RETRIES=5
)";
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", ops_with_params);

    REQUIRE(result.has_operation(PrintStartOpCategory::HOMING));
    REQUIRE(result.has_operation(PrintStartOpCategory::BED_LEVELING));
    REQUIRE(result.has_operation(PrintStartOpCategory::QGL));
}

TEST_CASE("PrintStartAnalyzer: Case insensitive operation detection", "[print_start][edge]") {
    const char* mixed_case = R"(
g28
bed_mesh_calibrate
Quad_Gantry_Level
)";
    auto result = PrintStartAnalyzer::parse_macro("PRINT_START", mixed_case);

    REQUIRE(result.has_operation(PrintStartOpCategory::HOMING));
    REQUIRE(result.has_operation(PrintStartOpCategory::BED_LEVELING));
    REQUIRE(result.has_operation(PrintStartOpCategory::QGL));
}
