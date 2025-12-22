// Copyright 2025 HelixScreen
// SPDX-License-Identifier: GPL-3.0-or-later

#include "print_start_analyzer.h"

#include "moonraker_api.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <regex>
#include <sstream>

namespace helix {

// ============================================================================
// Category Helpers
// ============================================================================

const char* category_to_string(PrintStartOpCategory category) {
    switch (category) {
    case PrintStartOpCategory::BED_LEVELING:
        return "bed_leveling";
    case PrintStartOpCategory::QGL:
        return "qgl";
    case PrintStartOpCategory::Z_TILT:
        return "z_tilt";
    case PrintStartOpCategory::NOZZLE_CLEAN:
        return "nozzle_clean";
    case PrintStartOpCategory::HOMING:
        return "homing";
    case PrintStartOpCategory::CHAMBER_SOAK:
        return "chamber_soak";
    case PrintStartOpCategory::UNKNOWN:
    default:
        return "unknown";
    }
}

// ============================================================================
// PrintStartAnalysis Methods
// ============================================================================

bool PrintStartAnalysis::has_operation(PrintStartOpCategory category) const {
    return std::any_of(operations.begin(), operations.end(),
                       [category](const PrintStartOperation& op) { return op.category == category; });
}

const PrintStartOperation* PrintStartAnalysis::get_operation(PrintStartOpCategory category) const {
    auto it = std::find_if(operations.begin(), operations.end(),
                           [category](const PrintStartOperation& op) { return op.category == category; });
    return (it != operations.end()) ? &(*it) : nullptr;
}

std::vector<const PrintStartOperation*> PrintStartAnalysis::get_uncontrollable_operations() const {
    std::vector<const PrintStartOperation*> result;
    for (const auto& op : operations) {
        if (!op.has_skip_param && op.category != PrintStartOpCategory::HOMING) {
            result.push_back(&op);
        }
    }
    return result;
}

std::string PrintStartAnalysis::summary() const {
    if (!found) {
        return "No print start macro found";
    }

    std::ostringstream ss;
    ss << macro_name << ": " << total_ops_count << " operations detected";
    if (controllable_count > 0) {
        ss << " (" << controllable_count << " controllable)";
    }

    if (!operations.empty()) {
        ss << " [";
        bool first = true;
        for (const auto& op : operations) {
            if (!first) ss << ", ";
            first = false;
            ss << op.name;
            if (op.has_skip_param) {
                ss << "(skip:" << op.skip_param_name << ")";
            }
        }
        ss << "]";
    }

    return ss.str();
}

// ============================================================================
// Operation Detection Patterns
// ============================================================================

// Operation patterns with their categories
struct OperationPattern {
    const char* pattern;
    PrintStartOpCategory category;
    const char* suggested_skip_param;
};

static const OperationPattern OPERATION_PATTERNS[] = {
    // Bed leveling
    {"BED_MESH_CALIBRATE", PrintStartOpCategory::BED_LEVELING, "SKIP_BED_MESH"},
    {"G29", PrintStartOpCategory::BED_LEVELING, "SKIP_BED_MESH"},

    // Quad Gantry Level
    {"QUAD_GANTRY_LEVEL", PrintStartOpCategory::QGL, "SKIP_QGL"},

    // Z Tilt
    {"Z_TILT_ADJUST", PrintStartOpCategory::Z_TILT, "SKIP_Z_TILT"},

    // Nozzle cleaning
    {"CLEAN_NOZZLE", PrintStartOpCategory::NOZZLE_CLEAN, "SKIP_NOZZLE_CLEAN"},
    {"NOZZLE_CLEAN", PrintStartOpCategory::NOZZLE_CLEAN, "SKIP_NOZZLE_CLEAN"},
    {"PURGE_LINE", PrintStartOpCategory::NOZZLE_CLEAN, "SKIP_PURGE"},
    {"PRIME_LINE", PrintStartOpCategory::NOZZLE_CLEAN, "SKIP_PURGE"},
    {"PURGE_NOZZLE", PrintStartOpCategory::NOZZLE_CLEAN, "SKIP_PURGE"},

    // Homing
    {"G28", PrintStartOpCategory::HOMING, "SKIP_HOMING"},

    // Chamber soak
    {"HEAT_SOAK", PrintStartOpCategory::CHAMBER_SOAK, "SKIP_SOAK"},
    {"CHAMBER_SOAK", PrintStartOpCategory::CHAMBER_SOAK, "SKIP_SOAK"},
    {"TEMPERATURE_WAIT", PrintStartOpCategory::CHAMBER_SOAK, "SKIP_SOAK"},
};

static constexpr size_t OPERATION_PATTERNS_COUNT =
    sizeof(OPERATION_PATTERNS) / sizeof(OPERATION_PATTERNS[0]);

// Skip parameter pattern variations (for detection)
static const std::vector<std::string> SKIP_PARAM_VARIATIONS[] = {
    // BED_LEVELING
    {"SKIP_BED_MESH", "SKIP_MESH", "SKIP_BED_LEVELING", "NO_BED_MESH", "SKIP_LEVEL"},
    // QGL
    {"SKIP_QGL", "SKIP_GANTRY", "NO_QGL", "SKIP_QUAD_GANTRY_LEVEL"},
    // Z_TILT
    {"SKIP_Z_TILT", "SKIP_TILT", "NO_Z_TILT", "SKIP_Z_TILT_ADJUST"},
    // NOZZLE_CLEAN
    {"SKIP_NOZZLE_CLEAN", "SKIP_CLEAN", "SKIP_PURGE", "NO_PURGE", "SKIP_PRIME"},
    // HOMING
    {"SKIP_HOMING", "SKIP_HOME", "NO_HOME"},
    // CHAMBER_SOAK
    {"SKIP_SOAK", "SKIP_HEAT_SOAK", "NO_SOAK", "SKIP_CHAMBER"},
};

// ============================================================================
// PrintStartAnalyzer Implementation
// ============================================================================

void PrintStartAnalyzer::analyze(MoonrakerAPI* api, AnalysisCallback on_complete,
                                  ErrorCallback on_error) {
    if (!api) {
        if (on_error) {
            MoonrakerError err;
            err.type = MoonrakerErrorType::VALIDATION_ERROR;
            err.message = "API not initialized";
            on_error(err);
        }
        return;
    }

    spdlog::debug("[PrintStartAnalyzer] Fetching printer configuration...");

    // Query the configfile object to get all macro definitions
    // We need "config" (raw strings) not "settings" (parsed values)
    api->query_configfile(
        [on_complete](const json& config) {
            PrintStartAnalysis result;

            // Search for print start macros in priority order
            for (size_t i = 0; i < MACRO_NAMES_COUNT; ++i) {
                std::string section_name = std::string("gcode_macro ") + MACRO_NAMES[i];

                // Convert to lowercase for case-insensitive matching
                // (Klipper config sections are case-insensitive)
                std::string section_lower = section_name;
                std::transform(section_lower.begin(), section_lower.end(), section_lower.begin(),
                               ::tolower);

                // Search through config keys
                for (auto& [key, value] : config.items()) {
                    std::string key_lower = key;
                    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);

                    if (key_lower == section_lower && value.contains("gcode")) {
                        std::string gcode = value["gcode"].get<std::string>();
                        spdlog::info("[PrintStartAnalyzer] Found macro '{}' ({} chars)", MACRO_NAMES[i],
                                     gcode.size());

                        result = parse_macro(MACRO_NAMES[i], gcode);
                        result.found = true;
                        result.macro_name = MACRO_NAMES[i];

                        if (on_complete) {
                            on_complete(result);
                        }
                        return;
                    }
                }
            }

            // No print start macro found
            spdlog::info("[PrintStartAnalyzer] No PRINT_START macro found in config");
            result.found = false;
            if (on_complete) {
                on_complete(result);
            }
        },
        on_error);
}

PrintStartAnalysis PrintStartAnalyzer::parse_macro(const std::string& macro_name,
                                                    const std::string& gcode) {
    PrintStartAnalysis result;
    result.found = true;
    result.macro_name = macro_name;
    result.raw_gcode = gcode;

    // Detect operations
    result.operations = detect_operations(gcode);
    result.total_ops_count = result.operations.size();

    // Check each operation for skip conditionals
    for (auto& op : result.operations) {
        std::string param_name;
        if (detect_skip_conditional(gcode, op.name, param_name)) {
            op.has_skip_param = true;
            op.skip_param_name = param_name;
            result.controllable_count++;
        }
    }

    result.is_controllable = (result.controllable_count > 0);

    // Extract known parameters
    result.known_params = extract_parameters(gcode);

    spdlog::debug("[PrintStartAnalyzer] Parsed {}: {} ops, {} controllable, {} params", macro_name,
                  result.total_ops_count, result.controllable_count, result.known_params.size());

    return result;
}

std::string PrintStartAnalyzer::get_suggested_skip_param(const std::string& op_name) {
    for (size_t i = 0; i < OPERATION_PATTERNS_COUNT; ++i) {
        if (op_name == OPERATION_PATTERNS[i].pattern) {
            return OPERATION_PATTERNS[i].suggested_skip_param;
        }
    }
    // Default: SKIP_ + operation name
    return "SKIP_" + op_name;
}

PrintStartOpCategory PrintStartAnalyzer::categorize_operation(const std::string& command) {
    // Extract just the command name (before any parameters)
    std::string cmd = command;
    auto space_pos = cmd.find(' ');
    if (space_pos != std::string::npos) {
        cmd = cmd.substr(0, space_pos);
    }

    // Convert to uppercase for matching
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    for (size_t i = 0; i < OPERATION_PATTERNS_COUNT; ++i) {
        if (cmd == OPERATION_PATTERNS[i].pattern) {
            return OPERATION_PATTERNS[i].category;
        }
    }

    return PrintStartOpCategory::UNKNOWN;
}

// ============================================================================
// Parsing Helpers
// ============================================================================

std::vector<PrintStartOperation> PrintStartAnalyzer::detect_operations(const std::string& gcode) {
    std::vector<PrintStartOperation> operations;

    // Split into lines and process each
    std::istringstream stream(gcode);
    std::string line;
    size_t line_num = 0;

    while (std::getline(stream, line)) {
        ++line_num;

        // Skip empty lines and comments
        auto first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) continue;
        if (line[first_non_space] == '#' || line[first_non_space] == ';') continue;

        // Skip Jinja2 control statements ({% ... %})
        if (line.find("{%") != std::string::npos) continue;

        // Extract the command (first word on the line, excluding Jinja2 expressions)
        std::string trimmed = line.substr(first_non_space);

        // Skip lines that are just Jinja2 expressions
        if (trimmed[0] == '{') continue;

        // Get the command name
        auto end_of_cmd = trimmed.find_first_of(" \t{");
        std::string cmd = (end_of_cmd != std::string::npos) ? trimmed.substr(0, end_of_cmd) : trimmed;

        // Check against known patterns
        std::string cmd_upper = cmd;
        std::transform(cmd_upper.begin(), cmd_upper.end(), cmd_upper.begin(), ::toupper);

        for (size_t i = 0; i < OPERATION_PATTERNS_COUNT; ++i) {
            if (cmd_upper == OPERATION_PATTERNS[i].pattern) {
                PrintStartOperation op;
                op.name = OPERATION_PATTERNS[i].pattern;
                op.category = OPERATION_PATTERNS[i].category;
                op.line_number = line_num;

                // Avoid duplicates (same operation appearing multiple times)
                bool duplicate = std::any_of(
                    operations.begin(), operations.end(),
                    [&op](const PrintStartOperation& existing) { return existing.name == op.name; });

                if (!duplicate) {
                    operations.push_back(op);
                    spdlog::trace("[PrintStartAnalyzer] Detected {} at line {}", op.name, line_num);
                }
                break;
            }
        }
    }

    return operations;
}

bool PrintStartAnalyzer::detect_skip_conditional(const std::string& gcode,
                                                  const std::string& op_name,
                                                  std::string& out_param_name) {
    // Get the category to know which skip param variations to look for
    PrintStartOpCategory category = categorize_operation(op_name);
    if (category == PrintStartOpCategory::UNKNOWN) {
        return false;
    }

    // Get index for skip param variations
    size_t category_idx = static_cast<size_t>(category);
    if (category_idx >= sizeof(SKIP_PARAM_VARIATIONS) / sizeof(SKIP_PARAM_VARIATIONS[0])) {
        return false;
    }

    const auto& variations = SKIP_PARAM_VARIATIONS[category_idx];

    // Look for patterns like:
    //   {% if SKIP_BED_MESH == 0 %}
    //   {% if params.SKIP_BED_MESH|default(0)|int == 0 %}
    //   {% if not SKIP_BED_MESH %}
    //
    // We check if any skip parameter variation appears near the operation

    // First, find the operation in the gcode
    auto op_pos = gcode.find(op_name);
    if (op_pos == std::string::npos) {
        return false;
    }

    // Look backwards from the operation for an {% if ... %} block
    // Search up to 500 characters before the operation
    size_t search_start = (op_pos > 500) ? op_pos - 500 : 0;
    std::string context = gcode.substr(search_start, op_pos - search_start);

    // Check for each skip param variation
    for (const auto& param : variations) {
        // Case-insensitive search
        std::string param_lower = param;
        std::transform(param_lower.begin(), param_lower.end(), param_lower.begin(), ::tolower);

        std::string context_lower = context;
        std::transform(context_lower.begin(), context_lower.end(), context_lower.begin(), ::tolower);

        // Look for the param in an if statement
        if (context_lower.find(param_lower) != std::string::npos) {
            // Verify it's in an if statement context
            // Look for patterns like: {% if ... param ...
            std::regex if_pattern(R"(\{%\s*if\s+.*)" + param_lower + R"(.*%\})",
                                  std::regex::icase);
            if (std::regex_search(context, if_pattern)) {
                out_param_name = param;
                spdlog::trace("[PrintStartAnalyzer] {} is controlled by {}", op_name, param);
                return true;
            }

            // Also check for variable assignment: {% set X = params.SKIP_...
            std::regex set_pattern(R"(\{%\s*set\s+\w+\s*=\s*params\.)" + param_lower,
                                   std::regex::icase);
            if (std::regex_search(context, set_pattern)) {
                out_param_name = param;
                spdlog::trace("[PrintStartAnalyzer] {} is controlled by params.{}", op_name, param);
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> PrintStartAnalyzer::extract_parameters(const std::string& gcode) {
    std::vector<std::string> params;

    // Look for patterns like:
    //   params.BED
    //   params.EXTRUDER|default(...)
    //   {% set BED = params.BED|default(60) %}

    std::regex params_pattern(R"(params\.([A-Z_][A-Z0-9_]*))", std::regex::icase);

    std::smatch match;
    std::string::const_iterator search_start = gcode.cbegin();

    while (std::regex_search(search_start, gcode.cend(), match, params_pattern)) {
        std::string param = match[1].str();
        // Convert to uppercase
        std::transform(param.begin(), param.end(), param.begin(), ::toupper);

        // Avoid duplicates
        if (std::find(params.begin(), params.end(), param) == params.end()) {
            params.push_back(param);
        }

        search_start = match.suffix().first;
    }

    return params;
}

} // namespace helix
