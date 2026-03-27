#pragma once
#include "cortex/tools/tool.hpp"

namespace cortex {

class FileWriteTool : public ITool {
public:
    ToolDefinition definition() const override;
    ToolResult execute(const Json& arguments) override;
    [[nodiscard]] std::string name() const override { return "file_write"; }
};

} // namespace cortex
