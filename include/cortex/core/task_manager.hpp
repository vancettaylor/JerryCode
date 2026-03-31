/**
 * @file task_manager.hpp
 * @brief Hierarchical task management with numbered sub-items and completion metadata.
 *
 * Tasks are organized hierarchically: 1, 1.1, 1.2, 2, 2.1, etc.
 * The model uses @markoff(number) to mark tasks complete, which triggers
 * a metadata collection prompt for tracking what was done and learned.
 */
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace cortex {

using Json = nlohmann::json;

/// Metadata collected when a task is marked off.
struct TaskCompletion {
    std::string what_was_done;       ///< Brief description of actual work performed.
    std::vector<std::string> files_changed;  ///< Files that were modified.
    std::string key_findings;        ///< Anything important learned.
    std::string issues;              ///< Problems encountered, if any.
    std::string next_recommended;    ///< What should happen next.
};

/// A hierarchical task item (supports numbering like 1, 1.1, 1.2, 2).
struct Task {
    std::string number;              ///< Hierarchical number: "1", "1.1", "1.2", "2".
    std::string title;               ///< Short title.
    std::string description;         ///< What to do.
    std::string type;                ///< "read", "write", "bash", "research", "review".
    std::string status = "pending";  ///< "pending", "active", "done", "failed", "blocked".
    std::vector<std::string> files;  ///< Files involved.
    std::string result;              ///< Outcome after completion.
    int attempts = 0;                ///< Retry count.
    int max_attempts = 3;            ///< Max retries.
    std::optional<TaskCompletion> completion;  ///< Metadata from @markoff.
    std::vector<Task> subtasks;      ///< Child tasks (1.1, 1.2 under 1).
};

/**
 * @brief Manages a hierarchical task tree with status tracking and rendering.
 */
class TaskManager {
public:
    /// Load from JSON (flat or hierarchical).
    void load_from_json(const Json& j);

    /// Add a top-level task.
    void add_task(const std::string& number, const std::string& title,
                  const std::string& description = "", const std::string& type = "write");

    /// Update status of a task by number (e.g., "1.2").
    void update_status(const std::string& number, const std::string& status,
                       const std::string& result = "");

    /// Record completion metadata for a task.
    void set_completion(const std::string& number, const TaskCompletion& completion);

    /// Find task by number.
    Task* find(const std::string& number);
    const Task* find(const std::string& number) const;

    /// Get next pending task (depth-first).
    Task* next_pending();

    /// Check if all tasks are done or failed.
    bool all_done() const;

    /// Count tasks by status.
    int completed_count() const;
    int total_count() const;
    int failed_count() const;

    /// Render the full hierarchical task list for prompt inclusion.
    std::string render() const;

    /// Render a compact summary line.
    std::string render_compact() const;

    /// Serialize to JSON.
    Json to_json() const;

    /// Get all top-level tasks.
    const std::vector<Task>& tasks() const { return tasks_; }

private:
    void render_task(const Task& t, std::ostringstream& ss, int indent) const;
    int count_recursive(const std::vector<Task>& tasks, const std::string& status) const;
    int count_all_recursive(const std::vector<Task>& tasks) const;
    Task* find_recursive(std::vector<Task>& tasks, const std::string& number);
    const Task* find_recursive(const std::vector<Task>& tasks, const std::string& number) const;
    Task* next_pending_recursive(std::vector<Task>& tasks);
    bool all_done_recursive(const std::vector<Task>& tasks) const;

    std::vector<Task> tasks_;
};

} // namespace cortex
