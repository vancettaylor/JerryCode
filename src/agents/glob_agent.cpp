#include "cortex/agents/glob_agent.hpp"

namespace cortex {

GlobAgent::GlobAgent(std::shared_ptr<ITool> tool) : tool_(std::move(tool)) {}

AgentResult GlobAgent::execute(const AgentContext& ctx,
                                IProvider& provider,
                                StreamCallback on_stream) {
    auto result = tool_->execute(ctx.plan.tool_arguments);

    if (on_stream && result.success) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Glob search",
        .tokens_used = {}
    };
}

} // namespace cortex
