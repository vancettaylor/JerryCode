/**
 * @file prompt_engine.hpp
 * @brief Prompt templates and cycling reminders for context management and task execution.
 */

#pragma once
#include <string>
#include <vector>

namespace cortex {

class ContextExpander;
class TaskManager;

/**
 * @brief Static prompt template factory. Builds system prompts that cycle reminders
 *        about context management, task updating, and efficient work habits.
 */
class PromptEngine {
public:
    /**
     * @brief Build the system identity block (always included in prompts).
     * @return The identity prompt string.
     */
    static std::string identity();

    /**
     * @brief Build the prompt for breaking a user request into tasks.
     * @param project_context Description of the project structure and files.
     * @return The task breakdown prompt string.
     */
    static std::string task_breakdown(const std::string& project_context);

    /**
     * @brief Build the action prompt for executing a specific task.
     * @param task_title Short title of the task.
     * @param task_description Detailed description of what to do.
     * @param original_request The user's original request for reference.
     * @param round_number Current round number (drives reminder cycling).
     * @return The action prompt string.
     */
    static std::string action_prompt(
        const std::string& task_title,
        const std::string& task_description,
        const std::string& original_request,
        int round_number);

    /**
     * @brief Build the prompt for generating or modifying code in a file.
     * @param file_path Path to the target file.
     * @param description What to write or change.
     * @param original_request The user's original request for reference.
     * @return The code generation prompt string.
     */
    static std::string code_gen(
        const std::string& file_path,
        const std::string& description,
        const std::string& original_request);

    /**
     * @brief Build the prompt for fixing an error in a file.
     * @param file_path Path to the file containing the error.
     * @param error_output The error text or compiler output.
     * @return The error fix prompt string.
     */
    static std::string error_fix(
        const std::string& file_path,
        const std::string& error_output);

    /**
     * @brief Build the meta-agent prompt for reviewing overall progress.
     * @return The progress review prompt string.
     */
    static std::string progress_review();

    /**
     * @brief Build the meta-agent prompt for analyzing an error log.
     * @param error_log The accumulated error log text.
     * @return The error analysis prompt string.
     */
    static std::string error_analysis(const std::string& error_log);

    /**
     * @brief Build the meta-agent prompt for updating the task list.
     * @return The task update prompt string.
     */
    static std::string task_update();

    /**
     * @brief Get a context management reminder that rotates based on the round number.
     * @param round_number Current round number (used to select the reminder).
     * @return The context reminder string.
     */
    static std::string context_reminder(int round_number);
};

} // namespace cortex
