/**
 * @file agent_registry.hpp
 * @brief Central registry for tool agents, mapping tool names to agent instances.
 */

#pragma once
#include "cortex/agents/tool_agent.hpp"
#include "cortex/tools/tool.hpp"
#include <unordered_map>
#include <memory>

namespace cortex {

/**
 * @brief Registry that owns and indexes tool agents by their tool name.
 *
 * Provides lookup by tool name and aggregation of all registered tool
 * definitions for LLM function-calling payloads.
 */
class AgentRegistry {
public:
    /**
     * @brief Register a tool agent, transferring ownership to the registry.
     * @param agent Unique pointer to the agent to register.
     */
    void register_agent(std::unique_ptr<IToolAgent> agent);

    /**
     * @brief Look up a registered agent by tool name.
     * @param tool_name The tool name to search for.
     * @return Pointer to the agent, or nullptr if not found.
     */
    IToolAgent* get(const std::string& tool_name) const;

    /**
     * @brief Collect tool definitions from all registered agents.
     * @return Vector of ToolDefinition for every registered agent.
     */
    std::vector<ToolDefinition> all_tool_definitions() const;

private:
    /// Map from tool name to owning agent pointer.
    std::unordered_map<std::string, std::unique_ptr<IToolAgent>> agents_;
};

} // namespace cortex
