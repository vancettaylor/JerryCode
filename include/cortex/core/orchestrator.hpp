#pragma once
#include "cortex/core/types.hpp"
#include "cortex/core/context_index.hpp"
#include "cortex/core/context_compiler.hpp"
#include "cortex/core/context_expander.hpp"
#include "cortex/core/timeline.hpp"
#include "cortex/agents/agent_registry.hpp"
#include "cortex/providers/provider.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace cortex {

class MetadataStore;

struct OrchestratorCallbacks {
    std::function<void(const std::string&)> on_phase_change;
    StreamCallback on_stream_text;
    std::function<void(const ActionPlan&)> on_plan_ready;
    std::function<void(const MetadataRecord&)> on_action_complete;
    std::function<void(const std::string&)> on_error;
};

struct TaskStep {
    std::string id;
    std::string action;       // "read", "write", "bash"
    std::string target;       // file path or command
    std::string description;  // what to do
    std::string status;       // "pending", "done", "failed"
    std::string result;       // output after execution
};

class Orchestrator {
public:
    Orchestrator(std::unique_ptr<IProvider> provider,
                 AgentRegistry& agents,
                 const std::string& project_root,
                 const ProviderConfig& provider_config = {});

    void handle_user_input(const std::string& input,
                           OrchestratorCallbacks callbacks);

    [[nodiscard]] const ContextIndex& index() const { return index_; }
    [[nodiscard]] const Timeline& timeline() const { return timeline_; }

private:
    // Task decomposition
    std::vector<TaskStep> decompose_task(const std::string& user_input);

    // Step execution
    void execute_step(TaskStep& step, OrchestratorCallbacks& callbacks);
    void execute_read(TaskStep& step, OrchestratorCallbacks& callbacks);
    void execute_write(TaskStep& step, OrchestratorCallbacks& callbacks);
    void execute_bash(TaskStep& step, OrchestratorCallbacks& callbacks);

    // Error recovery
    bool attempt_fix(TaskStep& failed_step, OrchestratorCallbacks& callbacks);

    // LLM helpers
    std::string llm_complete(const std::string& system_prompt,
                             const std::string& user_prompt,
                             int max_tokens = 4096);
    std::string llm_complete_multi(const std::string& system_prompt,
                                    const std::vector<std::pair<std::string, std::string>>& messages,
                                    int max_tokens = 4096);
    std::string llm_generate_code(const std::string& file_path,
                                   const std::string& description,
                                   const std::string& original_request = "");

    // Context expansion
    void setup_expander();

    // File helpers
    std::string read_file(const std::string& rel_path);
    std::string abs_path(const std::string& rel_path);
    std::string build_file_context();

    // Backward compat stubs (unused in new flow)
    MetadataRecord run_cycle(const std::string&, OrchestratorCallbacks&);
    ActionPlan run_phase1(const std::string&);
    AgentResult run_phase2(const ActionPlan&, StreamCallback);
    MetadataRecord finalize_cycle(const ActionPlan&, const AgentResult&);
    bool should_review_timeline() const;
    void trigger_timeline_review(OrchestratorCallbacks);

    std::unique_ptr<IProvider> provider_;
    ProviderConfig provider_config_;
    AgentRegistry& agents_;
    ContextIndex index_;
    ContextCompiler compiler_;
    Timeline timeline_;
    TokenBudget budget_;

    // Context expansion engine
    ContextExpander expander_;

    // Per-session state
    std::string original_request_;
    std::unordered_map<std::string, std::string> file_cache_;
    std::vector<TaskStep> steps_;
};

} // namespace cortex
