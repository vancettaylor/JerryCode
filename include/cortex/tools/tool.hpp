/**
 * @file tool.hpp
 * @brief Abstract tool interface and result/definition types.
 */

#pragma once
#include "cortex/core/types.hpp"

namespace cortex {

/** @brief Result returned after executing a tool. */
struct ToolResult {
    bool success = false;                       ///< Whether the tool execution succeeded.
    std::string output;                         ///< Human-readable output text.
    Json structured_data = Json::object();      ///< Optional structured JSON output.
    std::vector<Trigger> triggers;              ///< Side-effect triggers produced by the tool.
};

/** @brief Schema definition describing a tool for LLM function calling. */
struct ToolDefinition {
    std::string name;                           ///< Unique tool name identifier.
    std::string description;                    ///< Human-readable tool description.
    Json input_schema = Json::object();         ///< JSON Schema for the tool's input parameters.
};

/**
 * @brief Abstract interface for executable tools.
 *
 * Each tool exposes a definition (for LLM function-calling) and an
 * execute method that performs the tool's action.
 */
class ITool {
public:
    virtual ~ITool() = default;

    /**
     * @brief Get the tool's schema definition.
     * @return ToolDefinition describing this tool's name, description, and input schema.
     */
    virtual ToolDefinition definition() const = 0;

    /**
     * @brief Execute the tool with the given arguments.
     * @param arguments JSON object matching the tool's input_schema.
     * @return ToolResult containing output and status.
     */
    virtual ToolResult execute(const Json& arguments) = 0;

    /**
     * @brief Get the unique name of this tool.
     * @return Tool name string.
     */
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace cortex
