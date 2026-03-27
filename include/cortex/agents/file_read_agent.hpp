#pragma once
#include "cortex/agents/tool_agent.hpp"
#include <memory>

namespace cortex {

class FileReadTool;

class FileReadAgent : public IToolAgent {
public:
    explicit FileReadAgent(std::shared_ptr<ITool> tool);

    AgentResult execute(const AgentContext& ctx,
                        IProvider& provider,
                        StreamCallback on_stream) override;

    [[nodiscard]] std::string tool_name() const override { return "file_read"; }

private:
    std::shared_ptr<ITool> tool_;
};

} // namespace cortex
