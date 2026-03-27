#pragma once
#include "cortex/providers/provider.hpp"

namespace cortex {

class OpenAiProvider : public IProvider {
public:
    CompletionResult complete(
        const std::vector<Message>& messages,
        const ProviderConfig& config) override;

    CompletionResult stream(
        const std::vector<Message>& messages,
        const ProviderConfig& config,
        StreamCallback on_text,
        ToolUseCallback on_tool_use) override;

    int estimate_tokens(const std::string& text) const override;

    [[nodiscard]] std::string name() const override { return "openai"; }

private:
    Json build_request_body(const std::vector<Message>& messages,
                            const ProviderConfig& config,
                            bool streaming) const;
};

} // namespace cortex
