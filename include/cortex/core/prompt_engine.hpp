#pragma once
#include <string>
#include <vector>

namespace cortex {

class ContextExpander;
class TaskManager;

// Prompt templates that cycle reminders about context management,
// task updating, and efficient work habits. The idea: the model needs
// regular nudges to manage its context, not just a one-time instruction.
class PromptEngine {
public:
    // Build the system identity block (always included)
    static std::string identity();

    // Build the task breakdown prompt
    static std::string task_breakdown(const std::string& project_context);

    // Build the action prompt for a specific task
    // round_number drives reminder cycling
    static std::string action_prompt(
        const std::string& task_title,
        const std::string& task_description,
        const std::string& original_request,
        int round_number);

    // Build the code generation prompt
    static std::string code_gen(
        const std::string& file_path,
        const std::string& description,
        const std::string& original_request);

    // Build the error fix prompt
    static std::string error_fix(
        const std::string& file_path,
        const std::string& error_output);

    // Build the progress review prompt (meta-agent)
    static std::string progress_review();

    // Build the error analysis prompt (meta-agent)
    static std::string error_analysis(const std::string& error_log);

    // Build the task update prompt (meta-agent)
    static std::string task_update();

    // Context management reminders that rotate every N rounds
    static std::string context_reminder(int round_number);
};

} // namespace cortex
