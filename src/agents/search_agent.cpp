#include "cortex/agents/search_agent.hpp"

namespace cortex {

SearchAgent::SearchAgent(std::shared_ptr<ITool> glob_tool,
                         std::shared_ptr<ITool> grep_tool)
    : glob_tool_(std::move(glob_tool)), grep_tool_(std::move(grep_tool)) {}

AgentResult SearchAgent::execute(const AgentContext& ctx,
                                  IProvider& provider,
                                  StreamCallback on_stream) {
    // Determine which tool based on arguments
    ITool* tool = ctx.plan.tool_arguments.contains("pattern") &&
                  ctx.plan.tool_arguments.contains("content")
                  ? grep_tool_.get() : glob_tool_.get();

    auto result = tool->execute(ctx.plan.tool_arguments);

    if (on_stream) {
        on_stream(result.output);
    }

    return AgentResult{
        .tool_result = result,
        .agent_reasoning = "Search via " + tool->name(),
        .tokens_used = {}
    };
}

} // namespace cortex
