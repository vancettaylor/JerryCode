#include "cortex/agents/grep_agent.hpp"

namespace cortex {

GrepAgent::GrepAgent(std::shared_ptr<ITool> tool) : tool_(std::move(tool)) {}

AgentResult GrepAgent::execute(const AgentContext& ctx,
                                IProvider& provider,
                                StreamCallback on_stream) {
    auto result = tool_->execute(ctx.plan.tool_arguments);

    if (on_stream && result.success) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Grep search",
        .tokens_used = {}
    };
}

} // namespace cortex
