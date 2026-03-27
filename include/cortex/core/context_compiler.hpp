#pragma once
#include "cortex/core/types.hpp"
#include "cortex/core/context_index.hpp"
#include "cortex/core/token_budget.hpp"
#include <string>

namespace cortex {

class MetadataStore; // forward declaration

struct CompiledPrompt {
    std::vector<Message> messages;
    int estimated_tokens = 0;
    std::vector<std::string> expanded_sections;
};

class ContextCompiler {
public:
    ContextCompiler(const ContextIndex& index, const TokenBudget& budget);

    CompiledPrompt compile_phase1(const std::string& user_input) const;
    CompiledPrompt compile_phase2(const ActionPlan& plan) const;
    CompiledPrompt compile_timeline_review(
        const std::vector<TimelineEntry>& timeline) const;
    CompiledPrompt compile_agent_prompt(
        const ActionPlan& plan,
        const std::string& agent_system_prompt,
        const std::string& expanded_context) const;

private:
    std::string load_template(const std::string& name) const;
    std::string replace_vars(const std::string& tmpl,
                             const std::vector<std::pair<std::string, std::string>>& vars) const;
    std::string render_file_content(const std::string& path, int max_lines = 500) const;

    const ContextIndex& index_;
    TokenBudget budget_;
    std::string prompts_dir_;
};

} // namespace cortex
