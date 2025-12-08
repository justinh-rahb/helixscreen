// Copyright 2025 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui_panel_history_list.h"

#include "ui_fonts.h"
#include "ui_nav.h"
#include "ui_panel_common.h"

#include "moonraker_api.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <lvgl.h>

// MDI chevron-down symbol for dropdown arrows (replaces FontAwesome LV_SYMBOL_DOWN)
static const char* MDI_CHEVRON_DOWN = "\xF3\xB0\x85\x80"; // F0140

// Global instance (singleton pattern)
static std::unique_ptr<HistoryListPanel> g_history_list_panel;

HistoryListPanel& get_global_history_list_panel() {
    if (!g_history_list_panel) {
        spdlog::error("get_global_history_list_panel() called before initialization!");
        throw std::runtime_error("HistoryListPanel not initialized");
    }
    return *g_history_list_panel;
}

void init_global_history_list_panel(PrinterState& printer_state, MoonrakerAPI* api) {
    g_history_list_panel = std::make_unique<HistoryListPanel>(printer_state, api);

    // Register XML event callbacks (must be done BEFORE XML is created)
    lv_xml_register_event_cb(nullptr, "history_search_changed",
                             HistoryListPanel::on_search_changed_static);
    lv_xml_register_event_cb(nullptr, "history_filter_status_changed",
                             HistoryListPanel::on_status_filter_changed_static);
    lv_xml_register_event_cb(nullptr, "history_sort_changed",
                             HistoryListPanel::on_sort_changed_static);

    spdlog::debug("[History List] Global instance and event callbacks initialized");
}

// ============================================================================
// Constructor
// ============================================================================

HistoryListPanel::HistoryListPanel(PrinterState& printer_state, MoonrakerAPI* api)
    : PanelBase(printer_state, api) {
    spdlog::debug("[{}] Constructed", get_name());
}

// ============================================================================
// PanelBase Implementation
// ============================================================================

void HistoryListPanel::init_subjects() {
    // Initialize subject for empty state binding
    lv_subject_init_int(&subject_has_jobs_, 0);
    lv_xml_register_subject(nullptr, "history_list_has_jobs", &subject_has_jobs_);

    spdlog::debug("[{}] Subjects initialized", get_name());
}

void HistoryListPanel::setup(lv_obj_t* panel, lv_obj_t* parent_screen) {
    panel_ = panel;
    parent_screen_ = parent_screen;

    // Get widget references - list containers
    list_content_ = lv_obj_find_by_name(panel_, "list_content");
    list_rows_ = lv_obj_find_by_name(panel_, "list_rows");
    empty_state_ = lv_obj_find_by_name(panel_, "empty_state");
    empty_message_ = lv_obj_find_by_name(panel_, "empty_message");
    empty_hint_ = lv_obj_find_by_name(panel_, "empty_hint");

    // Get widget references - filter controls
    search_box_ = lv_obj_find_by_name(panel_, "search_box");
    filter_status_ = lv_obj_find_by_name(panel_, "filter_status");
    sort_dropdown_ = lv_obj_find_by_name(panel_, "sort_dropdown");

    spdlog::debug("[{}] Widget refs - content: {}, rows: {}, empty: {}", get_name(),
                  list_content_ != nullptr, list_rows_ != nullptr, empty_state_ != nullptr);
    spdlog::debug("[{}] Filter refs - search: {}, status: {}, sort: {}", get_name(),
                  search_box_ != nullptr, filter_status_ != nullptr, sort_dropdown_ != nullptr);

    // Set MDI chevron icons for dropdowns (Noto Sans doesn't have LV_SYMBOL_DOWN)
    // Must set BOTH the symbol AND the indicator font to MDI for the symbol to render
    const char* icon_font_name = lv_xml_get_const(NULL, "icon_font_md");
    const lv_font_t* icon_font =
        icon_font_name ? lv_xml_get_font(NULL, icon_font_name) : &mdi_icons_24;

    if (filter_status_) {
        lv_dropdown_set_symbol(filter_status_, MDI_CHEVRON_DOWN);
        lv_obj_set_style_text_font(filter_status_, icon_font, LV_PART_INDICATOR);
    }
    if (sort_dropdown_) {
        lv_dropdown_set_symbol(sort_dropdown_, MDI_CHEVRON_DOWN);
        lv_obj_set_style_text_font(sort_dropdown_, icon_font, LV_PART_INDICATOR);
    }

    // Note: XML event callbacks are registered in init_global_history_list_panel()
    // BEFORE the XML is created - that's when lv_xml_register_event_cb must be called

    // Wire up back button to navigation system
    ui_panel_setup_back_button(panel_);

    spdlog::info("[{}] Setup complete", get_name());
}

// ============================================================================
// Lifecycle Hooks
// ============================================================================

void HistoryListPanel::on_activate() {
    spdlog::debug("[{}] Activated - jobs_received: {}, job_count: {}", get_name(), jobs_received_,
                  jobs_.size());

    if (!jobs_received_) {
        // Jobs weren't set by dashboard, fetch from API
        refresh_from_api();
    } else {
        // Jobs were provided, apply filters and populate the list
        apply_filters_and_sort();
    }
}

void HistoryListPanel::on_deactivate() {
    spdlog::debug("[{}] Deactivated", get_name());

    // Cancel any pending search timer
    if (search_timer_) {
        lv_timer_delete(search_timer_);
        search_timer_ = nullptr;
    }

    // Reset filter state for fresh start on next activation
    search_query_.clear();
    status_filter_ = HistoryStatusFilter::ALL;
    sort_column_ = HistorySortColumn::DATE;
    sort_direction_ = HistorySortDirection::DESC;

    // Reset filter control widgets if available
    if (search_box_) {
        lv_textarea_set_text(search_box_, "");
    }
    if (filter_status_) {
        lv_dropdown_set_selected(filter_status_, 0);
    }
    if (sort_dropdown_) {
        lv_dropdown_set_selected(sort_dropdown_, 0);
    }

    // Clear the received flag so next activation will refresh
    jobs_received_ = false;
}

// ============================================================================
// Public API
// ============================================================================

void HistoryListPanel::set_jobs(const std::vector<PrintHistoryJob>& jobs) {
    jobs_ = jobs;
    jobs_received_ = true;
    spdlog::debug("[{}] Jobs set: {} items", get_name(), jobs_.size());
}

void HistoryListPanel::refresh_from_api() {
    if (!api_) {
        spdlog::warn("[{}] Cannot refresh: API not set", get_name());
        return;
    }

    spdlog::debug("[{}] Fetching history from API", get_name());

    api_->get_history_list(
        200, // limit
        0,   // start
        0.0, // since (no filter)
        0.0, // before (no filter)
        [this](const std::vector<PrintHistoryJob>& jobs, uint64_t total) {
            spdlog::info("[{}] Received {} jobs (total: {})", get_name(), jobs.size(), total);
            jobs_ = jobs;
            apply_filters_and_sort();
        },
        [this](const MoonrakerError& error) {
            spdlog::error("[{}] Failed to fetch history: {}", get_name(), error.message);
            jobs_.clear();
            apply_filters_and_sort();
        });
}

// ============================================================================
// Internal Methods
// ============================================================================

void HistoryListPanel::populate_list() {
    if (!list_rows_) {
        spdlog::error("[{}] Cannot populate: list_rows container is null", get_name());
        return;
    }

    // Clear existing rows
    clear_list();

    // Update empty state
    update_empty_state();

    if (filtered_jobs_.empty()) {
        spdlog::debug("[{}] No jobs to display after filtering", get_name());
        return;
    }

    spdlog::debug("[{}] Populating list with {} filtered jobs", get_name(), filtered_jobs_.size());

    for (size_t i = 0; i < filtered_jobs_.size(); ++i) {
        const auto& job = filtered_jobs_[i];

        // Get status info
        const char* status_color = get_status_color(job.status);
        const char* status_text = get_status_text(job.status);

        // Build attrs for row creation
        const char* attrs[] = {"filename",
                               job.filename.c_str(),
                               "date",
                               job.date_str.c_str(),
                               "duration",
                               job.duration_str.c_str(),
                               "filament_type",
                               job.filament_type.empty() ? "Unknown" : job.filament_type.c_str(),
                               "status",
                               status_text,
                               "status_color",
                               status_color,
                               NULL};

        lv_obj_t* row =
            static_cast<lv_obj_t*>(lv_xml_create(list_rows_, "history_list_row", attrs));

        if (row) {
            attach_row_click_handler(row, i);
        } else {
            spdlog::warn("[{}] Failed to create row for job {}", get_name(), i);
        }
    }

    spdlog::debug("[{}] List populated with {} rows", get_name(), filtered_jobs_.size());
}

void HistoryListPanel::clear_list() {
    if (!list_rows_)
        return;

    // Remove all children from the list container
    uint32_t child_count = lv_obj_get_child_count(list_rows_);
    for (int32_t i = child_count - 1; i >= 0; --i) {
        lv_obj_t* child = lv_obj_get_child(list_rows_, i);
        if (child) {
            lv_obj_delete(child);
        }
    }
}

void HistoryListPanel::update_empty_state() {
    // Check if there are filtered results
    bool has_filtered_jobs = !filtered_jobs_.empty();
    lv_subject_set_int(&subject_has_jobs_, has_filtered_jobs ? 1 : 0);

    // Update empty state message based on whether filters are active
    if (!has_filtered_jobs && empty_message_ && empty_hint_) {
        bool filters_active = !search_query_.empty() || status_filter_ != HistoryStatusFilter::ALL;

        if (filters_active) {
            // Filters are active but yielded no results
            lv_label_set_text(empty_message_, "No matching prints");
            lv_label_set_text(empty_hint_, "Try adjusting your search or filters");
        } else if (jobs_.empty()) {
            // No jobs at all
            lv_label_set_text(empty_message_, "No print history found");
            lv_label_set_text(empty_hint_, "Completed prints will appear here");
        }
    }

    spdlog::debug("[{}] Empty state updated: has_filtered_jobs={}, total_jobs={}", get_name(),
                  has_filtered_jobs, jobs_.size());
}

const char* HistoryListPanel::get_status_color(PrintJobStatus status) {
    switch (status) {
    case PrintJobStatus::COMPLETED:
        return "#00C853"; // Green
    case PrintJobStatus::CANCELLED:
        return "#FF9800"; // Orange
    case PrintJobStatus::ERROR:
        return "#F44336"; // Red
    case PrintJobStatus::IN_PROGRESS:
        return "#2196F3"; // Blue
    default:
        return "#9E9E9E"; // Gray
    }
}

const char* HistoryListPanel::get_status_text(PrintJobStatus status) {
    switch (status) {
    case PrintJobStatus::COMPLETED:
        return "Completed";
    case PrintJobStatus::CANCELLED:
        return "Cancelled";
    case PrintJobStatus::ERROR:
        return "Failed";
    case PrintJobStatus::IN_PROGRESS:
        return "In Progress";
    default:
        return "Unknown";
    }
}

// ============================================================================
// Click Handlers
// ============================================================================

void HistoryListPanel::attach_row_click_handler(lv_obj_t* row, size_t index) {
    // Store index in user data (cast to void* for LVGL)
    lv_obj_set_user_data(row, reinterpret_cast<void*>(index));

    // Find the actual clickable row element
    lv_obj_t* history_row = lv_obj_find_by_name(row, "history_row");
    if (history_row) {
        lv_obj_set_user_data(history_row, this);
        // Store index as a property we can retrieve
        // Using the row widget's index stored in parent's user_data
        lv_obj_add_event_cb(history_row, on_row_clicked_static, LV_EVENT_CLICKED, row);
    }
}

void HistoryListPanel::on_row_clicked_static(lv_event_t* e) {
    lv_obj_t* row_container = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));

    // Get the panel instance from the clickable element
    HistoryListPanel* panel = static_cast<HistoryListPanel*>(lv_obj_get_user_data(target));
    if (!panel || !row_container)
        return;

    // Get the index from the row container
    size_t index = reinterpret_cast<size_t>(lv_obj_get_user_data(row_container));
    panel->handle_row_click(index);
}

void HistoryListPanel::handle_row_click(size_t index) {
    if (index >= filtered_jobs_.size()) {
        spdlog::warn("[{}] Invalid row index: {}", get_name(), index);
        return;
    }

    const auto& job = filtered_jobs_[index];
    spdlog::info("[{}] Row clicked: {} ({})", get_name(), job.filename,
                 get_status_text(job.status));

    // TODO: Stage 5 - Open detail overlay
    // For now, just log the click
    spdlog::debug("[{}] Detail overlay not yet implemented (Stage 5)", get_name());
}

// ============================================================================
// Filter/Sort Implementation
// ============================================================================

void HistoryListPanel::apply_filters_and_sort() {
    spdlog::debug("[{}] Applying filters - search: '{}', status: {}, sort: {} {}", get_name(),
                  search_query_, static_cast<int>(status_filter_), static_cast<int>(sort_column_),
                  sort_direction_ == HistorySortDirection::DESC ? "DESC" : "ASC");

    // Chain: search -> status -> sort
    auto result = apply_search_filter(jobs_);
    result = apply_status_filter(result);
    apply_sort(result);

    filtered_jobs_ = std::move(result);

    spdlog::debug("[{}] Filter result: {} jobs -> {} filtered", get_name(), jobs_.size(),
                  filtered_jobs_.size());

    populate_list();
}

std::vector<PrintHistoryJob>
HistoryListPanel::apply_search_filter(const std::vector<PrintHistoryJob>& source) {
    if (search_query_.empty()) {
        return source;
    }

    // Case-insensitive search
    std::string query_lower = search_query_;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<PrintHistoryJob> result;
    result.reserve(source.size());

    for (const auto& job : source) {
        std::string filename_lower = job.filename;
        std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (filename_lower.find(query_lower) != std::string::npos) {
            result.push_back(job);
        }
    }

    return result;
}

std::vector<PrintHistoryJob>
HistoryListPanel::apply_status_filter(const std::vector<PrintHistoryJob>& source) {
    if (status_filter_ == HistoryStatusFilter::ALL) {
        return source;
    }

    std::vector<PrintHistoryJob> result;
    result.reserve(source.size());

    for (const auto& job : source) {
        bool include = false;

        switch (status_filter_) {
        case HistoryStatusFilter::COMPLETED:
            include = (job.status == PrintJobStatus::COMPLETED);
            break;
        case HistoryStatusFilter::FAILED:
            include = (job.status == PrintJobStatus::ERROR);
            break;
        case HistoryStatusFilter::CANCELLED:
            include = (job.status == PrintJobStatus::CANCELLED);
            break;
        default:
            include = true;
            break;
        }

        if (include) {
            result.push_back(job);
        }
    }

    return result;
}

void HistoryListPanel::apply_sort(std::vector<PrintHistoryJob>& jobs) {
    auto sort_col = sort_column_;
    auto sort_dir = sort_direction_;

    std::sort(jobs.begin(), jobs.end(),
              [sort_col, sort_dir](const PrintHistoryJob& a, const PrintHistoryJob& b) {
                  bool result = false;

                  switch (sort_col) {
                  case HistorySortColumn::DATE:
                      result = a.start_time < b.start_time;
                      break;
                  case HistorySortColumn::DURATION:
                      result = a.total_duration < b.total_duration;
                      break;
                  case HistorySortColumn::FILENAME:
                      result = a.filename < b.filename;
                      break;
                  }

                  // For DESC, invert the result
                  if (sort_dir == HistorySortDirection::DESC) {
                      result = !result;
                  }

                  return result;
              });
}

// ============================================================================
// Filter/Sort Event Handlers
// ============================================================================

void HistoryListPanel::on_search_changed_static(lv_event_t* e) {
    // Get the textarea that fired the event
    lv_obj_t* textarea = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!textarea)
        return;

    // Find the panel instance (singleton pattern)
    try {
        auto& panel = get_global_history_list_panel();
        panel.on_search_changed();
    } catch (const std::exception& ex) {
        spdlog::error("[History List] Search callback error: {}", ex.what());
    }
}

void HistoryListPanel::on_search_changed() {
    // Cancel existing timer if any
    if (search_timer_) {
        lv_timer_delete(search_timer_);
        search_timer_ = nullptr;
    }

    // Create debounce timer (300ms)
    search_timer_ = lv_timer_create(on_search_timer_static, 300, this);
    lv_timer_set_repeat_count(search_timer_, 1); // Fire once
}

void HistoryListPanel::on_search_timer_static(lv_timer_t* timer) {
    auto* panel = static_cast<HistoryListPanel*>(lv_timer_get_user_data(timer));
    if (panel) {
        panel->do_debounced_search();
    }
}

void HistoryListPanel::do_debounced_search() {
    search_timer_ = nullptr; // Timer is auto-deleted after single fire

    if (!search_box_) {
        return;
    }

    const char* text = lv_textarea_get_text(search_box_);
    search_query_ = text ? text : "";

    spdlog::debug("[{}] Search query changed: '{}'", get_name(), search_query_);
    apply_filters_and_sort();
}

void HistoryListPanel::on_status_filter_changed_static(lv_event_t* e) {
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!dropdown)
        return;

    int index = lv_dropdown_get_selected(dropdown);

    try {
        auto& panel = get_global_history_list_panel();
        panel.on_status_filter_changed(index);
    } catch (const std::exception& ex) {
        spdlog::error("[History List] Status filter callback error: {}", ex.what());
    }
}

void HistoryListPanel::on_status_filter_changed(int index) {
    status_filter_ = static_cast<HistoryStatusFilter>(index);
    spdlog::debug("[{}] Status filter changed to: {}", get_name(), index);
    apply_filters_and_sort();
}

void HistoryListPanel::on_sort_changed_static(lv_event_t* e) {
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!dropdown)
        return;

    int index = lv_dropdown_get_selected(dropdown);

    try {
        auto& panel = get_global_history_list_panel();
        panel.on_sort_changed(index);
    } catch (const std::exception& ex) {
        spdlog::error("[History List] Sort callback error: {}", ex.what());
    }
}

void HistoryListPanel::on_sort_changed(int index) {
    // Map dropdown indices to sort settings:
    // 0: Date (newest) -> DATE, DESC
    // 1: Date (oldest) -> DATE, ASC
    // 2: Duration      -> DURATION, DESC
    // 3: Filename      -> FILENAME, ASC

    switch (index) {
    case 0: // Date (newest)
        sort_column_ = HistorySortColumn::DATE;
        sort_direction_ = HistorySortDirection::DESC;
        break;
    case 1: // Date (oldest)
        sort_column_ = HistorySortColumn::DATE;
        sort_direction_ = HistorySortDirection::ASC;
        break;
    case 2: // Duration
        sort_column_ = HistorySortColumn::DURATION;
        sort_direction_ = HistorySortDirection::DESC;
        break;
    case 3: // Filename
        sort_column_ = HistorySortColumn::FILENAME;
        sort_direction_ = HistorySortDirection::ASC;
        break;
    default:
        spdlog::warn("[{}] Unknown sort index: {}", get_name(), index);
        return;
    }

    spdlog::debug("[{}] Sort changed to: column={}, dir={}", get_name(),
                  static_cast<int>(sort_column_),
                  sort_direction_ == HistorySortDirection::DESC ? "DESC" : "ASC");
    apply_filters_and_sort();
}
