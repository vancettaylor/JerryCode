/**
 * @file provider.hpp
 * @brief Abstract LLM provider interface and associated types.
 */

#pragma once
#include "cortex/core/types.hpp"
#include <functional>
#include <memory>

namespace cortex {

/** @brief Configuration for an LLM provider request. */
struct ProviderConfig {
    std::string api_key;                    ///< Authentication key for the provider API.
    std::string base_url;                   ///< Base URL of the provider endpoint.
    std::string model;                      ///< Model identifier (e.g. "claude-3-opus").
    int max_tokens = 4096;                  ///< Maximum tokens in the completion response.
    double temperature = 0.0;               ///< Sampling temperature (0.0 = deterministic).
    std::optional<Json> tool_definitions;   ///< Optional JSON tool/function definitions.
};

/** @brief Result returned from a completion or streaming request. */
struct CompletionResult {
    std::vector<ContentBlock> content;  ///< Content blocks in the response.
    TokenUsage usage;                   ///< Token usage statistics.
    std::string stop_reason;            ///< Reason the model stopped generating.
};

/// Callback invoked with each text delta during streaming.
using StreamCallback  = std::function<void(const std::string& delta)>;
/// Callback invoked when the model requests a tool use during streaming.
using ToolUseCallback = std::function<void(const std::string& id,
                                            const std::string& name,
                                            const Json& input)>;

/**
 * @brief Abstract interface for LLM providers (e.g. Anthropic, OpenAI).
 *
 * Implementations handle HTTP communication, request formatting, and
 * response parsing for a specific provider API.
 */
class IProvider {
public:
    virtual ~IProvider() = default;

    /**
     * @brief Send a blocking completion request.
     * @param messages Conversation history to send.
     * @param config   Provider and model configuration.
     * @return CompletionResult containing the model response.
     */
    virtual CompletionResult complete(
        const std::vector<Message>& messages,
        const ProviderConfig& config) = 0;

    /**
     * @brief Send a streaming completion request.
     * @param messages    Conversation history to send.
     * @param config      Provider and model configuration.
     * @param on_text     Callback for each text delta chunk.
     * @param on_tool_use Optional callback for tool-use events.
     * @return CompletionResult with the aggregated response.
     */
    virtual CompletionResult stream(
        const std::vector<Message>& messages,
        const ProviderConfig& config,
        StreamCallback on_text,
        ToolUseCallback on_tool_use = nullptr) = 0;

    /**
     * @brief Estimate the token count for a given text string.
     * @param text The input text to estimate.
     * @return Approximate number of tokens.
     */
    virtual int estimate_tokens(const std::string& text) const = 0;

    /**
     * @brief Get the human-readable name of this provider.
     * @return Provider name string.
     */
    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace cortex
