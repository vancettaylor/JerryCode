#include "cortex/agents/agent_registry.hpp"

namespace cortex {

void AgentRegistry::register_agent(std::unique_ptr<IToolAgent> agent) {
    auto name = agent->tool_name();
    agents_[name] = std::move(agent);
}

IToolAgent* AgentRegistry::get(const std::string& tool_name) const {
    auto it = agents_.find(tool_name);
    return it != agents_.end() ? it->second.get() : nullptr;
}

std::vector<ToolDefinition> AgentRegistry::all_tool_definitions() const {
    // TODO: Collect definitions from agents' underlying tools
    return {};
}

} // namespace cortex
