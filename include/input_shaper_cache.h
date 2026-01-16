// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/**
 * @file input_shaper_cache.h
 * @brief Cache for input shaper calibration results (Phase 7)
 *
 * Provides persistent storage for calibration results to avoid
 * re-running expensive resonance tests. Cache is keyed by printer ID
 * and has a configurable TTL (default 30 days).
 *
 * Cache format (JSON):
 * {
 *   "version": 1,
 *   "printer_id": "string",
 *   "timestamp": unix_timestamp,
 *   "noise_level": 22.5,
 *   "x_result": { ... },
 *   "y_result": { ... }
 * }
 */

#include "calibration_types.h"
#include "input_shaper_calibrator.h"

#include <filesystem>
#include <optional>
#include <string>

#include "hv/json.hpp"

namespace helix {
namespace calibration {

// ============================================================================
// JSON Serialization Functions
// ============================================================================

/**
 * @brief Serialize ShaperOption to JSON
 * @param opt Shaper option to serialize
 * @return JSON object
 */
nlohmann::json shaper_option_to_json(const ShaperOption& opt);

/**
 * @brief Deserialize ShaperOption from JSON
 * @param json JSON object to deserialize
 * @return Deserialized shaper option
 */
ShaperOption shaper_option_from_json(const nlohmann::json& json);

/**
 * @brief Serialize InputShaperResult to JSON
 * @param result Result to serialize
 * @return JSON object
 */
nlohmann::json input_shaper_result_to_json(const InputShaperResult& result);

/**
 * @brief Deserialize InputShaperResult from JSON
 * @param json JSON object to deserialize
 * @return Deserialized result
 */
InputShaperResult input_shaper_result_from_json(const nlohmann::json& json);

/**
 * @brief Serialize CalibrationResults to JSON
 * @param results Results to serialize
 * @return JSON object
 */
nlohmann::json
calibration_results_to_json(const InputShaperCalibrator::CalibrationResults& results);

/**
 * @brief Deserialize CalibrationResults from JSON
 * @param json JSON object to deserialize
 * @return Deserialized results
 */
InputShaperCalibrator::CalibrationResults calibration_results_from_json(const nlohmann::json& json);

// ============================================================================
// InputShaperCache Class
// ============================================================================

/**
 * @brief Persistent cache for input shaper calibration results
 *
 * Stores calibration results to disk in JSON format. Cache entries are
 * keyed by printer ID and automatically expire after 30 days.
 *
 * Usage:
 * @code
 *   InputShaperCache cache("/path/to/cache/dir");
 *
 *   // Save results after calibration
 *   cache.save_results(results, printer_id);
 *
 *   // Check for cached results before calibration
 *   if (cache.has_cached_results(printer_id)) {
 *       auto results = cache.load_results(printer_id);
 *       // Use cached results
 *   }
 * @endcode
 */
class InputShaperCache {
  public:
    /// Default cache TTL: 30 days
    static constexpr int DEFAULT_TTL_DAYS = 30;

    /**
     * @brief Construct cache with default path
     *
     * Uses platform-appropriate default cache directory
     * (e.g., ~/.cache/helixscreen on Linux)
     */
    InputShaperCache();

    /**
     * @brief Construct cache with specific directory
     * @param cache_dir Directory for cache files
     */
    explicit InputShaperCache(const std::filesystem::path& cache_dir);

    /**
     * @brief Save calibration results to cache
     * @param results Calibration results to save
     * @param printer_id Unique identifier for the printer
     * @return true if save succeeded, false on error
     */
    bool save_results(const InputShaperCalibrator::CalibrationResults& results,
                      const std::string& printer_id);

    /**
     * @brief Load calibration results from cache
     * @param printer_id Unique identifier for the printer
     * @return Calibration results if cache hit, nullopt if miss or expired
     */
    std::optional<InputShaperCalibrator::CalibrationResults>
    load_results(const std::string& printer_id);

    /**
     * @brief Check if valid cached results exist
     * @param printer_id Unique identifier for the printer
     * @return true if valid cache exists, false otherwise
     */
    bool has_cached_results(const std::string& printer_id);

    /**
     * @brief Clear all cached results
     *
     * Removes the cache file. Safe to call even if no cache exists.
     */
    void clear_cache();

    /**
     * @brief Get the full path to the cache file
     * @return Path to input_shaper_cache.json
     */
    [[nodiscard]] std::filesystem::path get_cache_path() const;

  private:
    std::filesystem::path cache_dir_;

    /**
     * @brief Check if cache timestamp is within TTL
     * @param timestamp Unix timestamp from cache file
     * @return true if not expired, false if expired
     */
    bool is_timestamp_valid(int64_t timestamp) const;
};

} // namespace calibration
} // namespace helix
