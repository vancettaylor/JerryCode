/**
 * @file tool_agent.hpp
 * @brief Abstract tool-agent interface with LLM-driven tool execution.
 */

#pragma once
#include "cortex/core/types.hpp"
#include "cortex/tools/tool.hpp"
#include "cortex/providers/provider.hpp"
#include <memory>

namespace cortex {

/** @brief Contextual information passed to an agent for execution. */
struct AgentContext {
    ActionPlan plan;                                ///< The action plan the agent should follow.
    std::vector<ContextPointer> relevant_pointers;  ///< Pointers to relevant context items.
    std::string expanded_context;                   ///< Pre-expanded context text for the LLM.
    int token_budget = 30000;                       ///< Maximum tokens the agent may consume.
};

/** @brief Result of an agent execution cycle. */
struct AgentResult {
    ToolResult tool_result;         ///< Underlying tool execution result.
    std::string agent_reasoning;    ///< The agent's chain-of-thought reasoning.
    TokenUsage tokens_used;         ///< Tokens consumed during this execution.
};

/**
 * @brief Abstract interface for LLM-powered tool agents.
 *
 * A tool agent wraps a specific tool with LLM reasoning, allowing the
 * model to plan, execute, and interpret tool results within a token budget.
 */
class IToolAgent {
public:
    virtual ~IToolAgent() = default;

    /**
     * @brief Execute the agent's tool within the given context.
     * @param ctx       Agent context with plan, pointers, and budget.
     * @param provider  LLM provider to use for reasoning.
     * @param on_stream Optional callback for streaming text output.
     * @return AgentResult with tool output, reasoning, and token usage.
     */
    virtual AgentResult execute(
        const AgentContext& ctx,
        IProvider& provider,
        StreamCallback on_stream = nullptr) = 0;

    /**
     * @brief Get the name of the tool this agent manages.
     * @return Tool name string.
     */
    [[nodiscard]] virtual std::string tool_name() const = 0;
};

} // namespace cortex
