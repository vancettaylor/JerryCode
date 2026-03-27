#pragma once
#include "cortex/core/types.hpp"

namespace cortex {

struct ToolResult {
    bool success = false;
    std::string output;
    Json structured_data = Json::object();
    std::vector<Trigger> triggers;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    Json input_schema = Json::object();
};

class ITool {
public:
    virtual ~ITool() = default;
    virtual ToolDefinition definition() const = 0;
    virtual ToolResult execute(const Json& arguments) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace cortex
