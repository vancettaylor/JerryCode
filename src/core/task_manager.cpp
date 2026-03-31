/**
 * @file task_manager.cpp
 * @brief Hierarchical task management implementation.
 */
#include "cortex/core/task_manager.hpp"
#include <sstream>
#include <algorithm>

namespace cortex {

// ─── Loading ──────────────────────────────────────────────────

void TaskManager::load_from_json(const Json& j) {
    tasks_.clear();
    if (!j.is_array()) return;

    int top_num = 1;
    for (const auto& item : j) {
        Task t;
        t.number = item.value("number", std::to_string(top_num));
        t.title = item.value("title", item.value("description", ""));
        t.description = item.value("description", "");
        t.type = item.value("type", "write");
        // For bash tasks, store the command in the title if provided
        if (item.contains("command") && !item["command"].get<std::string>().empty()) {
            t.title = item["command"].get<std::string>();
        }
        if (item.contains("files") && item["files"].is_array()) {
            for (const auto& f : item["files"]) t.files.push_back(f.get<std::string>());
        }

        // Load subtasks
        if (item.contains("subtasks") && item["subtasks"].is_array()) {
            int sub_num = 1;
            for (const auto& sub : item["subtasks"]) {
                Task st;
                st.number = sub.value("number", t.number + "." + std::to_string(sub_num));
                st.title = sub.value("title", sub.value("description", ""));
                st.description = sub.value("description", "");
                st.type = sub.value("type", "write");
                if (sub.contains("files") && sub["files"].is_array()) {
                    for (const auto& f : sub["files"]) st.files.push_back(f.get<std::string>());
                }
                t.subtasks.push_back(st);
                sub_num++;
            }
        }

        tasks_.push_back(t);
        top_num++;
    }
}

// ─── Mutation ─────────────────────────────────────────────────

void TaskManager::add_task(const std::string& number, const std::string& title,
                            const std::string& description, const std::string& type) {
    Task t;
    t.number = number;
    t.title = title;
    t.description = description;
    t.type = type;
    tasks_.push_back(t);
}

void TaskManager::update_status(const std::string& number, const std::string& status,
                                 const std::string& result) {
    auto* t = find(number);
    if (t) {
        t->status = status;
        if (!result.empty()) t->result = result;
    }
}

void TaskManager::set_completion(const std::string& number, const TaskCompletion& completion) {
    auto* t = find(number);
    if (t) t->completion = completion;
}

// ─── Lookup ───────────────────────────────────────────────────

Task* TaskManager::find(const std::string& number) {
    return find_recursive(tasks_, number);
}

const Task* TaskManager::find(const std::string& number) const {
    return find_recursive(tasks_, number);
}

Task* TaskManager::find_recursive(std::vector<Task>& tasks, const std::string& number) {
    for (auto& t : tasks) {
        if (t.number == number) return &t;
        auto* sub = find_recursive(t.subtasks, number);
        if (sub) return sub;
    }
    return nullptr;
}

const Task* TaskManager::find_recursive(const std::vector<Task>& tasks, const std::string& number) const {
    for (const auto& t : tasks) {
        if (t.number == number) return &t;
        auto* sub = find_recursive(t.subtasks, number);
        if (sub) return sub;
    }
    return nullptr;
}

Task* TaskManager::next_pending() {
    return next_pending_recursive(tasks_);
}

Task* TaskManager::next_pending_recursive(std::vector<Task>& tasks) {
    for (auto& t : tasks) {
        // Check subtasks first (depth-first)
        if (!t.subtasks.empty()) {
            auto* sub = next_pending_recursive(t.subtasks);
            if (sub) return sub;
        }
        if (t.status == "pending" && t.subtasks.empty()) return &t;
        // If a parent has pending subtasks, it's not itself actionable
        if (t.status == "pending" && !t.subtasks.empty()) {
            // Parent is pending but has subtasks — check if all subtasks done
            if (all_done_recursive(t.subtasks)) {
                t.status = "done";  // Auto-complete parent
            }
        }
    }
    return nullptr;
}

// ─── Status Queries ───────────────────────────────────────────

bool TaskManager::all_done() const {
    return all_done_recursive(tasks_);
}

bool TaskManager::all_done_recursive(const std::vector<Task>& tasks) const {
    for (const auto& t : tasks) {
        if (t.status != "done" && t.status != "failed") {
            if (t.subtasks.empty()) return false;
            if (!all_done_recursive(t.subtasks)) return false;
        }
    }
    return true;
}

int TaskManager::completed_count() const { return count_recursive(tasks_, "done"); }
int TaskManager::failed_count() const { return count_recursive(tasks_, "failed"); }

int TaskManager::total_count() const { return count_all_recursive(tasks_); }

int TaskManager::count_recursive(const std::vector<Task>& tasks, const std::string& status) const {
    int c = 0;
    for (const auto& t : tasks) {
        if (t.status == status && t.subtasks.empty()) c++;
        c += count_recursive(t.subtasks, status);
    }
    return c;
}

int TaskManager::count_all_recursive(const std::vector<Task>& tasks) const {
    int c = 0;
    for (const auto& t : tasks) {
        if (t.subtasks.empty()) c++;  // Only count leaf tasks
        c += count_all_recursive(t.subtasks);
    }
    return c;
}

// ─── Rendering ────────────────────────────────────────────────

std::string TaskManager::render() const {
    std::ostringstream ss;
    ss << "## Task List (" << completed_count() << "/" << total_count() << " done)\n";
    for (const auto& t : tasks_) {
        render_task(t, ss, 0);
    }
    return ss.str();
}

void TaskManager::render_task(const Task& t, std::ostringstream& ss, int indent) const {
    std::string pad(indent * 2, ' ');
    std::string icon;
    if (t.status == "done") icon = "[x]";
    else if (t.status == "active") icon = "[>]";
    else if (t.status == "failed") icon = "[!]";
    else if (t.status == "blocked") icon = "[-]";
    else icon = "[ ]";

    ss << pad << icon << " " << t.number << " " << t.title;
    if (t.status == "active") ss << "  <- CURRENT";
    ss << "\n";

    // Show completion metadata if available
    if (t.completion) {
        if (!t.completion->key_findings.empty()) {
            ss << pad << "     findings: " << t.completion->key_findings << "\n";
        }
    }

    for (const auto& sub : t.subtasks) {
        render_task(sub, ss, indent + 1);
    }
}

std::string TaskManager::render_compact() const {
    std::ostringstream ss;
    ss << "Tasks: " << completed_count() << "/" << total_count() << " done";
    if (failed_count() > 0) ss << ", " << failed_count() << " failed";
    auto* next = const_cast<TaskManager*>(this)->next_pending();
    if (next) ss << ". Next: " << next->number << " " << next->title;
    return ss.str();
}

Json TaskManager::to_json() const {
    Json arr = Json::array();
    for (const auto& t : tasks_) {
        Json j = {
            {"number", t.number}, {"title", t.title}, {"status", t.status},
            {"type", t.type}, {"result", t.result}
        };
        if (t.completion) {
            j["completion"] = {
                {"what_was_done", t.completion->what_was_done},
                {"key_findings", t.completion->key_findings}
            };
        }
        if (!t.subtasks.empty()) {
            Json subs = Json::array();
            for (const auto& s : t.subtasks) {
                subs.push_back({{"number", s.number}, {"title", s.title},
                                {"status", s.status}, {"type", s.type}});
            }
            j["subtasks"] = subs;
        }
        arr.push_back(j);
    }
    return arr;
}

} // namespace cortex
