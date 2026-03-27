#pragma once
#include "cortex/core/types.hpp"
#include "cortex/tools/tool.hpp"
#include "cortex/providers/provider.hpp"
#include <memory>

namespace cortex {

struct AgentContext {
    ActionPlan plan;
    std::vector<ContextPointer> relevant_pointers;
    std::string expanded_context;
    int token_budget = 30000;
};

struct AgentResult {
    ToolResult tool_result;
    std::string agent_reasoning;
    TokenUsage tokens_used;
};

class IToolAgent {
public:
    virtual ~IToolAgent() = default;

    virtual AgentResult execute(
        const AgentContext& ctx,
        IProvider& provider,
        StreamCallback on_stream = nullptr) = 0;

    [[nodiscard]] virtual std::string tool_name() const = 0;
};

} // namespace cortex
