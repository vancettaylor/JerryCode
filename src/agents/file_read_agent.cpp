#include "cortex/agents/file_read_agent.hpp"

namespace cortex {

FileReadAgent::FileReadAgent(std::shared_ptr<ITool> tool) : tool_(std::move(tool)) {}

AgentResult FileReadAgent::execute(const AgentContext& ctx,
                                    IProvider& provider,
                                    StreamCallback on_stream) {
    // Direct tool execution for reads (no LLM mediation needed)
    auto result = tool_->execute(ctx.plan.tool_arguments);

    if (on_stream && result.success) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Direct file read",
        .tokens_used = {}
    };
}

} // namespace cortex
