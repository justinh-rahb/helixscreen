// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2025 HelixScreen

#include "static_panel_registry.h"

#include <spdlog/spdlog.h>

namespace {
bool g_registry_destroyed = false;
}

StaticPanelRegistry& StaticPanelRegistry::instance() {
    static StaticPanelRegistry registry;
    return registry;
}

bool StaticPanelRegistry::is_destroyed() {
    return g_registry_destroyed;
}

StaticPanelRegistry::~StaticPanelRegistry() {
    g_registry_destroyed = true;
    // Note: If we get here during static destruction and panels weren't
    // explicitly destroyed via destroy_all(), they'll be destroyed by
    // their own static unique_ptr destructors. We just mark ourselves
    // as destroyed so guards in panel destructors can check.
}

void StaticPanelRegistry::register_destroy(const char* name, std::function<void()> destroy_fn) {
    destroyers_.push_back({name, std::move(destroy_fn)});
    spdlog::trace("[StaticPanelRegistry] Registered: {} (total: {})", name, destroyers_.size());
}

void StaticPanelRegistry::destroy_all() {
    if (destroyers_.empty()) {
        spdlog::debug("[StaticPanelRegistry] No panels registered, nothing to destroy");
        return;
    }

    spdlog::debug("[StaticPanelRegistry] Destroying {} panels in reverse order...",
                  destroyers_.size());

    // Destroy in reverse registration order (LIFO)
    // This ensures dependencies are respected: panels created later
    // (which may depend on earlier ones) are destroyed first
    for (auto it = destroyers_.rbegin(); it != destroyers_.rend(); ++it) {
        spdlog::debug("[StaticPanelRegistry] Destroying: {}", it->name);
        if (it->destroy_fn) {
            it->destroy_fn();
        }
    }

    destroyers_.clear();
    spdlog::debug("[StaticPanelRegistry] All panels destroyed");
}
