/**
 * @file task_manager.hpp
 * @brief Task list management: creation, status tracking, and rendering for prompt inclusion.
 */

#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace cortex {

using Json = nlohmann::json;

/**
 * @brief A single task in the session's work plan.
 */
struct Task {
    int id = 0;                              ///< Unique task identifier.
    std::string title;                       ///< Short title of the task.
    std::string description;                 ///< Detailed description of what to do.
    std::string status = "pending";          ///< Status: "pending", "active", "done", "failed", or "blocked".
    std::vector<std::string> subtasks;       ///< List of subtask descriptions.
    std::vector<std::string> files_involved; ///< Files this task will read or modify.
    std::string result;                      ///< Outcome description after completion.
    int attempts = 0;                        ///< Number of execution attempts so far.
    int max_attempts = 3;                    ///< Maximum allowed retry attempts.
};

/**
 * @brief Manages an ordered list of tasks: parsing, status updates, queries, and rendering.
 */
class TaskManager {
public:
    /**
     * @brief Parse and load a task breakdown from JSON (typically LLM output).
     * @param j JSON object containing the task list.
     */
    void load_from_json(const Json& j);

    /**
     * @brief Add a task manually.
     * @param title Short title for the task.
     * @param description Optional detailed description.
     * @return The assigned task ID.
     */
    int add_task(const std::string& title, const std::string& description = "");

    /**
     * @brief Update a task's status and optionally record its result.
     * @param id Task ID to update.
     * @param status New status string.
     * @param result Optional result description.
     */
    void update_status(int id, const std::string& status, const std::string& result = "");

    /**
     * @brief Add a subtask description to an existing task.
     * @param parent_id ID of the parent task.
     * @param subtask Description of the subtask.
     */
    void add_subtask(int parent_id, const std::string& subtask);

    /**
     * @brief Retrieve a task by ID.
     * @param id Task ID to look up.
     * @return The task if found, or nullopt.
     */
    std::optional<Task> get(int id) const;

    /**
     * @brief Get the next task with "pending" status.
     * @return The next pending task, or nullopt if none remain.
     */
    std::optional<Task> next_pending() const;

    /**
     * @brief Get a mutable reference to the currently active task.
     * @return Reference to the current task.
     */
    Task& current();

    /**
     * @brief Check whether all tasks are done or failed.
     * @return True if no tasks are pending or active.
     */
    bool all_done() const;

    /**
     * @brief Count tasks with "done" status.
     * @return Number of completed tasks.
     */
    int completed_count() const;

    /**
     * @brief Get the total number of tasks.
     * @return Total task count.
     */
    int total_count() const;

    /**
     * @brief Count tasks with "failed" status.
     * @return Number of failed tasks.
     */
    int failed_count() const;

    /**
     * @brief Render the full task list as a formatted string for prompt inclusion.
     * @return Multi-line task list string.
     */
    std::string render() const;

    /**
     * @brief Render a compact single-line-per-task summary.
     * @return Compact task list string.
     */
    std::string render_compact() const;

    /**
     * @brief Serialize the task list to JSON.
     * @return JSON representation of all tasks.
     */
    Json to_json() const;

private:
    std::vector<Task> tasks_; ///< Ordered list of tasks.
    int next_id_ = 1;        ///< Next ID to assign.
    int current_id_ = -1;    ///< ID of the currently active task (-1 if none).
};

} // namespace cortex
