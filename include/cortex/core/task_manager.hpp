#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace cortex {

using Json = nlohmann::json;

struct Task {
    int id = 0;
    std::string title;
    std::string description;
    std::string status = "pending";  // pending, active, done, failed, blocked
    std::vector<std::string> subtasks;
    std::vector<std::string> files_involved;
    std::string result;
    int attempts = 0;
    int max_attempts = 3;
};

class TaskManager {
public:
    // Parse a task breakdown from LLM output
    void load_from_json(const Json& j);

    // Manual task operations
    int add_task(const std::string& title, const std::string& description = "");
    void update_status(int id, const std::string& status, const std::string& result = "");
    void add_subtask(int parent_id, const std::string& subtask);

    // Queries
    std::optional<Task> get(int id) const;
    std::optional<Task> next_pending() const;
    Task& current();
    bool all_done() const;
    int completed_count() const;
    int total_count() const;
    int failed_count() const;

    // Render for prompt inclusion
    std::string render() const;
    std::string render_compact() const;

    // Serialize
    Json to_json() const;

private:
    std::vector<Task> tasks_;
    int next_id_ = 1;
    int current_id_ = -1;
};

} // namespace cortex
