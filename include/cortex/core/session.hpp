#pragma once
#include "cortex/core/types.hpp"
#include "cortex/core/context_expander.hpp"
#include "cortex/core/task_manager.hpp"
#include "cortex/core/prompt_engine.hpp"
#include "cortex/agents/agent_registry.hpp"
#include "cortex/providers/provider.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace cortex {

struct SessionCallbacks {
    std::function<void(const std::string&)> on_phase;
    std::function<void(const std::string&)> on_stream;
    std::function<void(const std::string&)> on_status;
    std::function<void(const std::string&)> on_error;
};

struct SessionStats {
    int total_llm_calls = 0;
    int total_tokens_in = 0;
    int total_tokens_out = 0;
    int tasks_completed = 0;
    int tasks_failed = 0;
    int errors_fixed = 0;
    int context_reads = 0;
    int context_hides = 0;
    int peak_expanded_tokens = 0;
    double total_time_ms = 0;
};

class Session {
public:
    Session(std::unique_ptr<IProvider> provider,
            AgentRegistry& agents,
            const std::string& project_root,
            const ProviderConfig& provider_config);

    void run(const std::string& user_request, SessionCallbacks callbacks);

    const TaskManager& tasks() const { return tasks_; }
    const SessionStats& stats() const { return stats_; }
    std::string stats_summary() const;

private:
    // Phase 1: Break down the request into tasks
    void phase_breakdown(const std::string& request);

    // Phase 2: Execute tasks one by one
    void phase_execute(SessionCallbacks& cb);

    // Execute a single task
    bool execute_task(Task& task, SessionCallbacks& cb);

    // Task actions
    std::string do_read(const std::string& path);
    std::string do_write(const std::string& path, const std::string& description);
    std::string do_bash(const std::string& command);

    // Error recovery with meta-agent
    bool try_fix_error(const std::string& error_output, SessionCallbacks& cb);

    // Meta-agent: review progress
    void review_progress(SessionCallbacks& cb);

    // LLM call with the working set context
    std::string llm_call(const std::string& system_prompt,
                         const std::string& user_message,
                         int max_tokens = 4096);

    // LLM call through the expansion loop
    std::string llm_with_expansion(const std::string& system_prompt,
                                    const std::string& task_description,
                                    int max_tokens = 4096,
                                    int max_rounds = 6);

    // Compile a prompt with current context state
    std::string compile_prompt(const std::string& system_prompt) const;

    // File helpers
    std::string read_file(const std::string& rel_path);
    std::string abs_path(const std::string& rel_path) const;

    // Members
    std::unique_ptr<IProvider> provider_;
    ProviderConfig provider_config_;
    AgentRegistry& agents_;
    std::string project_root_;
    std::string original_request_;

    ContextExpander expander_;
    TaskManager tasks_;
    SessionStats stats_;

    std::unordered_map<std::string, std::string> file_cache_;
    std::vector<std::string> action_log_;
    int round_counter_ = 0;
};

} // namespace cortex
