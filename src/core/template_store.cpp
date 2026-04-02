/**
 * @file template_store.cpp
 * @brief Runtime prompt template loading and variable substitution.
 */
#include "cortex/core/template_store.hpp"
#include "cortex/util/log.hpp"
#include <fstream>

namespace cortex {

bool TemplateStore::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        log::error("Cannot open template file: " + path);
        return false;
    }
    try {
        f >> templates_;
        loaded_ = true;
        log::info("Loaded templates from " + path);
        return true;
    } catch (const std::exception& e) {
        log::error("Template parse error: " + std::string(e.what()));
        return false;
    }
}

void TemplateStore::load_json(const Json& j) {
    templates_ = j;
    loaded_ = true;
}

const Json* TemplateStore::navigate(const std::string& path) const {
    if (!loaded_) return nullptr;

    const Json* node = &templates_;
    std::string segment;
    std::istringstream ss(path);

    while (std::getline(ss, segment, '.')) {
        if (!node->is_object() || !node->contains(segment)) {
            return nullptr;
        }
        node = &(*node)[segment];
    }
    return node;
}

std::string TemplateStore::get(const std::string& path) const {
    auto* node = navigate(path);
    if (!node || !node->is_string()) {
        log::warn("Template not found: " + path);
        return "";
    }
    return node->get<std::string>();
}

std::vector<std::string> TemplateStore::get_array(const std::string& path) const {
    auto* node = navigate(path);
    if (!node || !node->is_array()) return {};
    std::vector<std::string> result;
    for (const auto& item : *node) {
        if (item.is_string()) result.push_back(item.get<std::string>());
    }
    return result;
}

std::string TemplateStore::substitute(const std::string& tmpl,
                                       const std::unordered_map<std::string, std::string>& vars) {
    std::string result = tmpl;
    for (const auto& [key, value] : vars) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), value);
            pos += value.size();
        }
    }
    return result;
}

std::string TemplateStore::render(const std::string& path,
                                   const std::unordered_map<std::string, std::string>& vars) const {
    return substitute(get(path), vars);
}

bool TemplateStore::has(const std::string& path) const {
    return navigate(path) != nullptr;
}

std::string TemplateStore::get_reminder(int round_number) const {
    auto reminders = get_array("reminders");
    if (reminders.empty()) return "";
    return reminders[round_number % reminders.size()];
}

std::string TemplateStore::assemble(const std::string& format,
                                     const std::unordered_map<std::string, std::string>& vars) const {
    // Read the assembly rule: "assembly.{format}" → array of block references
    auto block_refs = get_array("assembly." + format);
    if (block_refs.empty()) {
        log::warn("No assembly rule for format: " + format);
        return get("blocks.identity");
    }

    // Resolve each block reference and join
    std::string result;
    for (const auto& ref : block_refs) {
        auto block = get(ref);
        if (!block.empty()) {
            if (!result.empty()) result += "\n";
            result += block;
        }
    }

    // Substitute variables ({{topic}}, {{goal}}, etc.)
    result = substitute(result, vars);

    return result;
}

} // namespace cortex
