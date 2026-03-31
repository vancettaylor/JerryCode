/**
 * @file session.hpp
 * @brief Main Session class that orchestrates task breakdown, execution, and context management.
 */

#pragma once
#include "cortex/core/types.hpp"
#include "cortex/core/context_expander.hpp"
#include "cortex/core/task_manager.hpp"
#include "cortex/core/template_store.hpp"
#include "cortex/core/notebook.hpp"
#include "cortex/core/prompt_engine.hpp"
#include "cortex/agents/agent_registry.hpp"
#include "cortex/providers/provider.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace cortex {

/**
 * @brief Callbacks invoked by Session to report progress to the caller.
 */
struct SessionCallbacks {
    std::function<void(const std::string&)> on_phase;   ///< Called when entering a new phase.
    std::function<void(const std::string&)> on_stream;  ///< Called with streaming text chunks.
    std::function<void(const std::string&)> on_status;  ///< Called with status updates.
    std::function<void(const std::string&)> on_error;   ///< Called when an error occurs.
};

/**
 * @brief Aggregate statistics collected over the lifetime of a Session.
 */
struct SessionStats {
    int total_llm_calls = 0;        ///< Number of LLM invocations made.
    int total_tokens_in = 0;        ///< Total input tokens sent to the model.
    int total_tokens_out = 0;       ///< Total output tokens received from the model.
    int tasks_completed = 0;        ///< Number of tasks finished successfully.
    int tasks_failed = 0;           ///< Number of tasks that failed after all retries.
    int errors_fixed = 0;           ///< Number of errors recovered via the meta-agent.
    int context_reads = 0;          ///< Number of context expansion (read) operations.
    int context_hides = 0;          ///< Number of context hide operations.
    int peak_expanded_tokens = 0;   ///< High-water mark of tokens in the expanded context.
    double total_time_ms = 0;       ///< Wall-clock duration of the entire session.
};

/**
 * @brief Orchestrates a complete coding session: breaks a user request into tasks,
 *        executes them sequentially with context expansion, and handles error recovery.
 */
class Session {
public:
    /**
     * @brief Construct a new Session.
     * @param provider LLM provider used for all model calls (ownership transferred).
     * @param agents Registry of available agents.
     * @param project_root Absolute path to the project directory.
     * @param provider_config Configuration for the LLM provider.
     */
    Session(std::unique_ptr<IProvider> provider,
            AgentRegistry& agents,
            const std::string& project_root,
            const ProviderConfig& provider_config);

    /**
     * @brief Run the full session pipeline for a user request.
     * @param user_request The natural-language request to fulfill.
     * @param callbacks Callbacks for progress reporting.
     */
    void run(const std::string& user_request, SessionCallbacks callbacks);

    /**
     * @brief Get the task manager (read-only).
     * @return Const reference to the TaskManager.
     */
    const TaskManager& tasks() const { return tasks_; }

    /**
     * @brief Get session statistics (read-only).
     * @return Const reference to SessionStats.
     */
    const SessionStats& stats() const { return stats_; }

    /**
     * @brief Produce a human-readable summary of session statistics.
     * @return Formatted stats string.
     */
    std::string stats_summary() const;

private:
    /**
     * @brief Phase 1: Break down the request into tasks.
     * @param request The user's original request.
     */
    void phase_breakdown(const std::string& request);

    /**
     * @brief Phase 2: Execute tasks one by one.
     * @param cb Callbacks for progress reporting.
     */
    void phase_execute(SessionCallbacks& cb);

    /**
     * @brief Execute a single task, retrying on failure up to max_attempts.
     * @param task The task to execute.
     * @param cb Callbacks for progress reporting.
     * @return True if the task succeeded.
     */
    bool execute_task(Task& task, SessionCallbacks& cb);

    /**
     * @brief Read a file and add it to the working set.
     * @param path Relative path to the file.
     * @return The file content.
     */
    std::string do_read(const std::string& path);

    /**
     * @brief Write or generate code for a file.
     * @param path Relative path to the file.
     * @param description What to write or change.
     * @return Status or result of the write.
     */
    std::string do_write(const std::string& path, const std::string& description);

    /**
     * @brief Execute a shell command.
     * @param command The command to run.
     * @return Command output (stdout + stderr).
     */
    std::string do_bash(const std::string& command);

    /**
     * @brief Attempt to fix an error using the meta-agent.
     * @param error_output The error text to analyze.
     * @param cb Callbacks for progress reporting.
     * @return True if the error was resolved.
     */
    bool try_fix_error(const std::string& error_output, SessionCallbacks& cb);

    /**
     * @brief Review overall progress via the meta-agent.
     * @param cb Callbacks for progress reporting.
     */
    void review_progress(SessionCallbacks& cb);

    /**
     * @brief Make a single LLM call with the current working-set context.
     * @param system_prompt System prompt to use.
     * @param user_message User message to send.
     * @param max_tokens Maximum output tokens.
     * @return The model's response text.
     */
    std::string llm_call(const std::string& system_prompt,
                         const std::string& user_message,
                         int max_tokens = 4096);

    /**
     * @brief Make an LLM call through the context expansion loop.
     * @param system_prompt System prompt to use.
     * @param task_description Description of the current task.
     * @param max_tokens Maximum output tokens per round.
     * @param max_rounds Maximum expansion rounds.
     * @return The model's final response text.
     */
    std::string llm_with_expansion(const std::string& system_prompt,
                                    const std::string& task_description,
                                    int max_tokens = 4096,
                                    int max_rounds = 6);

    /**
     * @brief Compile a prompt by combining system prompt with current context state.
     * @param system_prompt Base system prompt.
     * @return The fully assembled prompt string.
     */
    std::string compile_prompt(const std::string& system_prompt) const;

    /**
     * @brief Read a file from disk, using the cache if available.
     * @param rel_path Relative path from project root.
     * @return File contents.
     */
    std::string read_file(const std::string& rel_path);

    /**
     * @brief Resolve a relative path to an absolute path under the project root.
     * @param rel_path Relative path.
     * @return Absolute path.
     */
    std::string abs_path(const std::string& rel_path) const;

    /// Scan project environment: languages, build system, structure.
    std::string scan_environment();

    // Members
    std::unique_ptr<IProvider> provider_;    ///< LLM provider for model calls.
    ProviderConfig provider_config_;         ///< Provider configuration.
    AgentRegistry& agents_;                  ///< Registry of available agents.
    std::string project_root_;               ///< Absolute path to the project root.
    std::string original_request_;           ///< The user's original request text.

    ContextExpander expander_;               ///< Manages the working-set context window.
    TaskManager tasks_;                      ///< Manages the task list and statuses.
    TemplateStore templates_;                ///< Runtime-loaded prompt templates.
    Notebook notebook_;                      ///< Session notes for knowledge accumulation.
    SessionStats stats_;                     ///< Accumulated session statistics.

    std::string env_summary_;                ///< Environment scan summary.
    std::unordered_map<std::string, std::string> file_cache_; ///< Cache of file contents by path.
    std::vector<std::string> action_log_;    ///< Log of actions taken during the session.
    int round_counter_ = 0;                  ///< Global round counter across all tasks.
};

} // namespace cortex
