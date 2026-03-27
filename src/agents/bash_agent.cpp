#include "cortex/agents/bash_agent.hpp"

namespace cortex {

BashAgent::BashAgent(std::shared_ptr<ITool> tool) : tool_(std::move(tool)) {}

AgentResult BashAgent::execute(const AgentContext& ctx,
                                IProvider& provider,
                                StreamCallback on_stream) {
    auto result = tool_->execute(ctx.plan.tool_arguments);

    if (on_stream) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Direct bash execution",
        .tokens_used = {}
    };
}

} // namespace cortex
