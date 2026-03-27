#pragma once
#include "cortex/agents/tool_agent.hpp"
#include "cortex/tools/tool.hpp"
#include <unordered_map>
#include <memory>

namespace cortex {

class AgentRegistry {
public:
    void register_agent(std::unique_ptr<IToolAgent> agent);
    IToolAgent* get(const std::string& tool_name) const;
    std::vector<ToolDefinition> all_tool_definitions() const;

private:
    std::unordered_map<std::string, std::unique_ptr<IToolAgent>> agents_;
};

} // namespace cortex
