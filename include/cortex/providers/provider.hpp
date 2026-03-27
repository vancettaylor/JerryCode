#pragma once
#include "cortex/core/types.hpp"
#include <functional>
#include <memory>

namespace cortex {

struct ProviderConfig {
    std::string api_key;
    std::string base_url;
    std::string model;
    int max_tokens = 4096;
    double temperature = 0.0;
    std::optional<Json> tool_definitions;
};

struct CompletionResult {
    std::vector<ContentBlock> content;
    TokenUsage usage;
    std::string stop_reason;
};

using StreamCallback  = std::function<void(const std::string& delta)>;
using ToolUseCallback = std::function<void(const std::string& id,
                                            const std::string& name,
                                            const Json& input)>;

class IProvider {
public:
    virtual ~IProvider() = default;

    virtual CompletionResult complete(
        const std::vector<Message>& messages,
        const ProviderConfig& config) = 0;

    virtual CompletionResult stream(
        const std::vector<Message>& messages,
        const ProviderConfig& config,
        StreamCallback on_text,
        ToolUseCallback on_tool_use = nullptr) = 0;

    virtual int estimate_tokens(const std::string& text) const = 0;

    [[nodiscard]] virtual std::string name() const = 0;
};

} // namespace cortex
