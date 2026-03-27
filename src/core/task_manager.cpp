#include "cortex/core/task_manager.hpp"
#include <sstream>
#include <algorithm>

namespace cortex {

void TaskManager::load_from_json(const Json& j) {
    tasks_.clear();
    next_id_ = 1;
    if (j.is_array()) {
        for (const auto& item : j) {
            Task t;
            t.id = next_id_++;
            t.title = item.value("title", item.value("description", ""));
            t.description = item.value("description", "");
            if (item.contains("subtasks") && item["subtasks"].is_array()) {
                for (const auto& s : item["subtasks"]) {
                    t.subtasks.push_back(s.get<std::string>());
                }
            }
            if (item.contains("files") && item["files"].is_array()) {
                for (const auto& f : item["files"]) {
                    t.files_involved.push_back(f.get<std::string>());
                }
            }
            tasks_.push_back(t);
        }
    }
}

int TaskManager::add_task(const std::string& title, const std::string& description) {
    Task t;
    t.id = next_id_++;
    t.title = title;
    t.description = description;
    tasks_.push_back(t);
    return t.id;
}

void TaskManager::update_status(int id, const std::string& status, const std::string& result) {
    for (auto& t : tasks_) {
        if (t.id == id) {
            t.status = status;
            if (!result.empty()) t.result = result;
            if (status == "active") current_id_ = id;
            return;
        }
    }
}

void TaskManager::add_subtask(int parent_id, const std::string& subtask) {
    for (auto& t : tasks_) {
        if (t.id == parent_id) {
            t.subtasks.push_back(subtask);
            return;
        }
    }
}

std::optional<Task> TaskManager::get(int id) const {
    for (const auto& t : tasks_) {
        if (t.id == id) return t;
    }
    return std::nullopt;
}

std::optional<Task> TaskManager::next_pending() const {
    for (const auto& t : tasks_) {
        if (t.status == "pending") return t;
    }
    return std::nullopt;
}

Task& TaskManager::current() {
    for (auto& t : tasks_) {
        if (t.id == current_id_) return t;
    }
    // Fallback: find first active or pending
    for (auto& t : tasks_) {
        if (t.status == "active") { current_id_ = t.id; return t; }
    }
    for (auto& t : tasks_) {
        if (t.status == "pending") { current_id_ = t.id; return t; }
    }
    static Task empty;
    return empty;
}

bool TaskManager::all_done() const {
    return std::all_of(tasks_.begin(), tasks_.end(),
        [](const Task& t) { return t.status == "done" || t.status == "failed"; });
}

int TaskManager::completed_count() const {
    return std::count_if(tasks_.begin(), tasks_.end(),
        [](const Task& t) { return t.status == "done"; });
}

int TaskManager::total_count() const { return static_cast<int>(tasks_.size()); }

int TaskManager::failed_count() const {
    return std::count_if(tasks_.begin(), tasks_.end(),
        [](const Task& t) { return t.status == "failed"; });
}

std::string TaskManager::render() const {
    std::ostringstream ss;
    ss << "## Task List (" << completed_count() << "/" << total_count() << " done)\n";
    for (const auto& t : tasks_) {
        std::string icon;
        if (t.status == "done") icon = "[x]";
        else if (t.status == "active") icon = "[>]";
        else if (t.status == "failed") icon = "[!]";
        else if (t.status == "blocked") icon = "[-]";
        else icon = "[ ]";

        ss << "  " << icon << " #" << t.id << " " << t.title;
        if (t.status == "active") ss << "  ← CURRENT";
        if (!t.result.empty() && t.result.size() < 60) {
            ss << "  (" << t.result << ")";
        }
        ss << "\n";

        for (const auto& sub : t.subtasks) {
            ss << "      - " << sub << "\n";
        }
    }
    return ss.str();
}

std::string TaskManager::render_compact() const {
    std::ostringstream ss;
    ss << "Tasks: " << completed_count() << "/" << total_count() << " done";
    if (failed_count() > 0) ss << ", " << failed_count() << " failed";
    auto next = next_pending();
    if (next) ss << ". Next: #" << next->id << " " << next->title;
    return ss.str();
}

Json TaskManager::to_json() const {
    Json arr = Json::array();
    for (const auto& t : tasks_) {
        arr.push_back({
            {"id", t.id}, {"title", t.title}, {"status", t.status},
            {"result", t.result}, {"attempts", t.attempts}
        });
    }
    return arr;
}

} // namespace cortex
