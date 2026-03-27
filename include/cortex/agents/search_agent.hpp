#pragma once
#include "cortex/agents/tool_agent.hpp"
#include <memory>

namespace cortex {

class SearchAgent : public IToolAgent {
public:
    explicit SearchAgent(std::shared_ptr<ITool> glob_tool,
                         std::shared_ptr<ITool> grep_tool);

    AgentResult execute(const AgentContext& ctx,
                        IProvider& provider,
                        StreamCallback on_stream) override;

    [[nodiscard]] std::string tool_name() const override { return "search"; }

private:
    std::shared_ptr<ITool> glob_tool_;
    std::shared_ptr<ITool> grep_tool_;
};

} // namespace cortex
