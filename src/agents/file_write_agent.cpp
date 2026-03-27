#include "cortex/agents/file_write_agent.hpp"

namespace cortex {

FileWriteAgent::FileWriteAgent(std::shared_ptr<ITool> tool) : tool_(std::move(tool)) {}

AgentResult FileWriteAgent::execute(const AgentContext& ctx,
                                     IProvider& provider,
                                     StreamCallback on_stream) {
    // TODO: LLM-mediated write with scoped prompt
    // For now, direct execution
    auto result = tool_->execute(ctx.plan.tool_arguments);

    if (on_stream) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Direct file write (LLM mediation TODO)",
        .tokens_used = {}
    };
}

} // namespace cortex
