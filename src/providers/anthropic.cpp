#include "cortex/providers/anthropic.hpp"
#include "cortex/providers/sse_parser.hpp"
#include "cortex/util/log.hpp"
#include <httplib.h>

namespace cortex {

Json AnthropicProvider::messages_to_json(const std::vector<Message>& messages) const {
    Json arr = Json::array();
    for (const auto& msg : messages) {
        if (msg.role == MessageRole::System) continue; // System handled separately
        Json j;
        j["role"] = msg.role == MessageRole::User ? "user" : "assistant";
        if (msg.content.size() == 1 && msg.content[0].type == "text") {
            j["content"] = msg.content[0].text;
        } else {
            Json content = Json::array();
            for (const auto& block : msg.content) {
                if (block.type == "text") {
                    content.push_back({{"type", "text"}, {"text", block.text}});
                }
            }
            j["content"] = content;
        }
        arr.push_back(j);
    }
    return arr;
}

Json AnthropicProvider::build_request_body(const std::vector<Message>& messages,
                                            const ProviderConfig& config,
                                            bool streaming) const {
    Json body;
    body["model"] = config.model;
    body["max_tokens"] = config.max_tokens;
    body["temperature"] = config.temperature;
    body["messages"] = messages_to_json(messages);

    // Extract system message
    for (const auto& msg : messages) {
        if (msg.role == MessageRole::System && !msg.content.empty()) {
            body["system"] = msg.content[0].text;
            break;
        }
    }

    if (streaming) body["stream"] = true;

    return body;
}

CompletionResult AnthropicProvider::complete(
    const std::vector<Message>& messages,
    const ProviderConfig& config) {
    auto base_url = config.base_url.empty() ? "https://api.anthropic.com" : config.base_url;

    httplib::Client cli(base_url);
    cli.set_read_timeout(120, 0);

    auto body = build_request_body(messages, config, false);
    httplib::Headers headers = {
        {"x-api-key", config.api_key},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"}
    };

    auto res = cli.Post("/v1/messages", headers, body.dump(), "application/json");

    CompletionResult result;
    if (!res || res->status != 200) {
        std::string err = res ? res->body : "Connection failed";
        log::error("Anthropic API error: " + err);
        result.content.push_back({.type = "text", .text = "API Error: " + err});
        result.stop_reason = "error";
        return result;
    }

    auto json = Json::parse(res->body);
    for (const auto& block : json["content"]) {
        if (block["type"] == "text") {
            result.content.push_back({.type = "text", .text = block["text"]});
        }
    }

    if (json.contains("usage")) {
        result.usage.input_tokens = json["usage"].value("input_tokens", 0);
        result.usage.output_tokens = json["usage"].value("output_tokens", 0);
    }
    result.stop_reason = json.value("stop_reason", "end_turn");

    return result;
}

CompletionResult AnthropicProvider::stream(
    const std::vector<Message>& messages,
    const ProviderConfig& config,
    StreamCallback on_text,
    ToolUseCallback on_tool_use) {

    auto base_url = config.base_url.empty() ? "https://api.anthropic.com" : config.base_url;

    httplib::Client cli(base_url);
    cli.set_read_timeout(120, 0);

    auto body = build_request_body(messages, config, true);
    httplib::Headers headers = {
        {"x-api-key", config.api_key},
        {"anthropic-version", "2023-06-01"},
        {"content-type", "application/json"}
    };

    CompletionResult result;
    std::string accumulated_text;

    SseParser parser([&](const SseEvent& event) {
        if (event.event == "content_block_delta") {
            auto data = Json::parse(event.data);
            if (data.contains("delta") && data["delta"].contains("text")) {
                auto text = data["delta"]["text"].get<std::string>();
                accumulated_text += text;
                if (on_text) on_text(text);
            }
        } else if (event.event == "message_stop") {
            // Done
        } else if (event.event == "message_delta") {
            auto data = Json::parse(event.data);
            if (data.contains("usage")) {
                result.usage.output_tokens = data["usage"].value("output_tokens", 0);
            }
            result.stop_reason = data.value("stop_reason", "end_turn");
        } else if (event.event == "message_start") {
            auto data = Json::parse(event.data);
            if (data.contains("message") && data["message"].contains("usage")) {
                result.usage.input_tokens = data["message"]["usage"].value("input_tokens", 0);
            }
        }
    });

    // For streaming, we do a regular Post and parse SSE from the full response.
    // True incremental streaming would require a lower-level socket approach.
    // This is sufficient for the initial implementation.
    auto res = cli.Post("/v1/messages", headers, body.dump(), "application/json");

    if (!res || (res->status != 200 && res->status != 0)) {
        std::string err = res ? res->body : "Connection failed";
        log::error("Anthropic streaming API error: " + err);
        result.content.push_back({.type = "text", .text = "API Error: " + err});
        result.stop_reason = "error";
        return result;
    }

    // Parse the SSE response body
    parser.feed(res->body);

    if (!accumulated_text.empty()) {
        result.content.push_back({.type = "text", .text = accumulated_text});
    }

    return result;
}

int AnthropicProvider::estimate_tokens(const std::string& text) const {
    return static_cast<int>(text.size()) / 4;
}

} // namespace cortex
