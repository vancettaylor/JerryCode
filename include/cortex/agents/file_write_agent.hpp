#pragma once
#include "cortex/agents/tool_agent.hpp"
#include <memory>

namespace cortex {

class FileWriteAgent : public IToolAgent {
public:
    explicit FileWriteAgent(std::shared_ptr<ITool> tool);

    AgentResult execute(const AgentContext& ctx,
                        IProvider& provider,
                        StreamCallback on_stream) override;

    [[nodiscard]] std::string tool_name() const override { return "file_write"; }

private:
    std::shared_ptr<ITool> tool_;
};

} // namespace cortex
