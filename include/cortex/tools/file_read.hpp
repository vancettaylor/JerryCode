#pragma once
#include "cortex/tools/tool.hpp"

namespace cortex {

class FileReadTool : public ITool {
public:
    ToolDefinition definition() const override;
    ToolResult execute(const Json& arguments) override;
    [[nodiscard]] std::string name() const override { return "file_read"; }
};

} // namespace cortex
