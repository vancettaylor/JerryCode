#pragma once
#include "cortex/tools/tool.hpp"

namespace cortex {

class BashTool : public ITool {
public:
    ToolDefinition definition() const override;
    ToolResult execute(const Json& arguments) override;
    [[nodiscard]] std::string name() const override { return "bash"; }
};

} // namespace cortex
