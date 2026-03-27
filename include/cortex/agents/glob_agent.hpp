#pragma once
#include "cortex/agents/tool_agent.hpp"
#include <memory>

namespace cortex {

class GlobAgent : public IToolAgent {
public:
    explicit GlobAgent(std::shared_ptr<ITool> tool);

    AgentResult execute(const AgentContext& ctx,
                        IProvider& provider,
                        StreamCallback on_stream) override;

    [[nodiscard]] std::string tool_name() const override { return "glob"; }

private:
    std::shared_ptr<ITool> tool_;
};

} // namespace cortex
