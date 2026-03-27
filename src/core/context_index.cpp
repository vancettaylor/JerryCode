#include "cortex/core/context_index.hpp"
#include <sstream>
#include <algorithm>

namespace cortex {

void ContextIndex::set_project_root(const std::string& path) { project_root_ = path; }
void ContextIndex::set_active_task(const std::string& task) { active_task_ = task; }

void ContextIndex::update_file_tree(const std::vector<std::string>& paths) {
    file_tree_ = paths;
}

void ContextIndex::add_action_summary(const TimelineEntry& entry) {
    recent_.push_back(entry);
    if (static_cast<int>(recent_.size()) > max_recent_) {
        recent_.erase(recent_.begin());
    }
}

void ContextIndex::add_pointer(const ContextPointer& pointer) {
    pointers_.push_back(pointer);
    if (static_cast<int>(pointers_.size()) > max_pointers_) {
        // Remove oldest non-error pointer
        auto it = std::find_if(pointers_.begin(), pointers_.end(),
            [](const ContextPointer& p) { return p.kind != "error"; });
        if (it != pointers_.end()) pointers_.erase(it);
    }
}

void ContextIndex::remove_pointer(const std::string& label) {
    pointers_.erase(
        std::remove_if(pointers_.begin(), pointers_.end(),
            [&](const ContextPointer& p) { return p.label == label; }),
        pointers_.end());
}

void ContextIndex::mark_expanded(const std::string& label, bool expanded) {
    for (auto& p : pointers_) {
        if (p.label == label) { p.currently_expanded = expanded; break; }
    }
}

std::string ContextIndex::render() const {
    std::ostringstream ss;
    ss << "## Project: " << project_root_ << "\n";
    if (!active_task_.empty()) ss << "## Active Task: " << active_task_ << "\n";

    if (!file_tree_.empty()) {
        ss << "\n### File Tree\n";
        for (const auto& f : file_tree_) ss << "  " << f << "\n";
    }

    if (!recent_.empty()) {
        ss << "\n### Recent Actions\n";
        for (const auto& a : recent_) {
            ss << "  #" << a.sequence_number << " "
               << (a.success ? "OK" : "FAIL") << " ["
               << a.tool_used << "] " << a.one_line_summary
               << " (" << a.token_cost << " tokens)\n";
        }
    }

    ss << render_pointers();
    return ss.str();
}

std::string ContextIndex::render_pointers() const {
    if (pointers_.empty()) return "";
    std::ostringstream ss;
    ss << "\n### Expandable Sections\n";
    for (const auto& p : pointers_) {
        ss << "  [" << p.kind << "] " << p.label
           << " (~" << p.estimated_tokens << " tokens)"
           << (p.currently_expanded ? " [EXPANDED]" : "") << "\n";
    }
    return ss.str();
}

int ContextIndex::estimated_tokens() const {
    return static_cast<int>(render().size()) / 4;
}

const std::vector<TimelineEntry>& ContextIndex::recent_actions() const { return recent_; }
const std::vector<ContextPointer>& ContextIndex::pointers() const { return pointers_; }

} // namespace cortex
