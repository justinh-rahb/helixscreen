// SPDX-License-Identifier: GPL-3.0-or-later
#include "layout_manager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <unistd.h>

namespace helix {

LayoutManager& LayoutManager::instance() {
    static LayoutManager instance;
    return instance;
}

void LayoutManager::init(int width, int height) {
    if (!override_name_.empty()) {
        type_ = name_to_type(override_name_);
    } else {
        type_ = detect(width, height);
    }
    name_ = type_to_name(type_);
    initialized_ = true;
    spdlog::info("LayoutManager: {}x{} -> layout '{}'{}", width, height, name_,
                 override_name_.empty() ? "" : " (override)");
}

void LayoutManager::set_override(const std::string& name) {
    if (name.empty()) {
        override_name_.clear();
        return;
    }
    // Validate the name by converting (logs warning if unknown)
    name_to_type(name);
    override_name_ = name;
}

LayoutType LayoutManager::type() const {
    return type_;
}

const std::string& LayoutManager::name() const {
    return name_;
}

bool LayoutManager::is_standard() const {
    return type_ == LayoutType::STANDARD;
}

std::string LayoutManager::resolve_xml_path(const std::string& filename) const {
    if (is_standard()) {
        return "ui_xml/" + filename;
    }

    std::string variant_path = "ui_xml/" + name_ + "/" + filename;
    if (access(variant_path.c_str(), F_OK) == 0) {
        return variant_path;
    }

    return "ui_xml/" + filename;
}

LayoutType LayoutManager::detect(int width, int height) const {
    float ratio = static_cast<float>(width) / static_cast<float>(height);
    int max_dim = std::max(width, height);

    if (max_dim <= 480) {
        return (width >= height) ? LayoutType::TINY : LayoutType::TINY_PORTRAIT;
    }
    if (ratio > 2.5f) {
        return LayoutType::ULTRAWIDE;
    }
    if (ratio < 0.8f) {
        return LayoutType::PORTRAIT;
    }
    return LayoutType::STANDARD;
}

const char* LayoutManager::type_to_name(LayoutType type) {
    switch (type) {
    case LayoutType::STANDARD:
        return "standard";
    case LayoutType::ULTRAWIDE:
        return "ultrawide";
    case LayoutType::PORTRAIT:
        return "portrait";
    case LayoutType::TINY:
        return "tiny";
    case LayoutType::TINY_PORTRAIT:
        return "tiny_portrait";
    }
    return "standard";
}

LayoutType LayoutManager::name_to_type(const std::string& name) {
    if (name == "standard")
        return LayoutType::STANDARD;
    if (name == "ultrawide")
        return LayoutType::ULTRAWIDE;
    if (name == "portrait")
        return LayoutType::PORTRAIT;
    if (name == "tiny")
        return LayoutType::TINY;
    if (name == "tiny_portrait")
        return LayoutType::TINY_PORTRAIT;

    spdlog::warn("LayoutManager: unknown layout name '{}', defaulting to standard", name);
    return LayoutType::STANDARD;
}

} // namespace helix
