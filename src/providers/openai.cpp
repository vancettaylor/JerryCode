#include "cortex/providers/openai.hpp"
#include "cortex/providers/sse_parser.hpp"
#include "cortex/util/log.hpp"
#include <httplib.h>

namespace cortex {

// Sanitize invalid UTF-8 bytes that can crash nlohmann/json
static std::string sanitize_utf8(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = input[i];
        if (c < 0x80) {
            output += c; i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80) {
            output += input[i]; output += input[i+1]; i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80) {
            output += input[i]; output += input[i+1]; output += input[i+2]; i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size() &&
                   (input[i+1] & 0xC0) == 0x80 && (input[i+2] & 0xC0) == 0x80 &&
                   (input[i+3] & 0xC0) == 0x80) {
            output += input[i]; output += input[i+1]; output += input[i+2]; output += input[i+3]; i += 4;
        } else {
            output += '?'; i++;  // Replace invalid byte
        }
    }
    return output;
}

Json OpenAiProvider::build_request_body(const std::vector<Message>& messages,
                                         const ProviderConfig& config,
                                         bool streaming) const {
    Json body;
    body["model"] = config.model;
    body["max_tokens"] = config.max_tokens;
    body["temperature"] = config.temperature;

    // Convert messages to OpenAI format
    Json msgs = Json::array();
    for (const auto& msg : messages) {
        Json j;
        switch (msg.role) {
            case MessageRole::System:    j["role"] = "system"; break;
            case MessageRole::User:      j["role"] = "user"; break;
            case MessageRole::Assistant: j["role"] = "assistant"; break;
            case MessageRole::ToolResult: j["role"] = "tool"; break;
        }

        if (msg.content.size() == 1 && msg.content[0].type == "text") {
            j["content"] = msg.content[0].text;
        } else {
            std::string combined;
            for (const auto& block : msg.content) {
                if (block.type == "text") combined += block.text;
            }
            j["content"] = combined;
        }
        msgs.push_back(j);
    }
    body["messages"] = msgs;

    if (streaming) body["stream"] = true;

    return body;
}

CompletionResult OpenAiProvider::complete(
    const std::vector<Message>& messages,
    const ProviderConfig& config) {

    auto base_url = config.base_url.empty() ? "http://192.168.11.25:8080" : config.base_url;

    httplib::Client cli(base_url);
    cli.set_read_timeout(600, 0);  // 10 min timeout for model loading
    cli.set_connection_timeout(30, 0);

    auto body = build_request_body(messages, config, false);

    log::debug("OpenAI complete request to " + base_url + "/v1/chat/completions");
    log::debug("Model: " + config.model);

    auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");

    CompletionResult result;
    if (!res) {
        std::string err = "Connection failed to " + base_url;
        log::error(err);
        result.content.push_back({.type = "text", .text = "Error: " + err});
        result.stop_reason = "error";
        return result;
    }

    if (res->status != 200) {
        log::error("OpenAI API error " + std::to_string(res->status) + ": " + res->body);
        result.content.push_back({.type = "text", .text = "API Error: " + res->body});
        result.stop_reason = "error";
        return result;
    }

    auto json = Json::parse(sanitize_utf8(res->body));

    if (json.contains("choices") && !json["choices"].empty()) {
        auto& choice = json["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            auto content = choice["message"]["content"];
            if (!content.is_null()) {
                result.content.push_back({.type = "text", .text = content.get<std::string>()});
            }
        }
        result.stop_reason = choice.value("finish_reason", "stop");
    }

    if (json.contains("usage")) {
        result.usage.input_tokens = json["usage"].value("prompt_tokens", 0);
        result.usage.output_tokens = json["usage"].value("completion_tokens", 0);
    }

    return result;
}

CompletionResult OpenAiProvider::stream(
    const std::vector<Message>& messages,
    const ProviderConfig& config,
    StreamCallback on_text,
    ToolUseCallback on_tool_use) {

    auto base_url = config.base_url.empty() ? "http://192.168.11.25:8080" : config.base_url;

    httplib::Client cli(base_url);
    cli.set_read_timeout(600, 0);
    cli.set_connection_timeout(30, 0);

    auto body = build_request_body(messages, config, true);

    log::debug("OpenAI stream request to " + base_url + "/v1/chat/completions");

    CompletionResult result;
    std::string accumulated_text;

    SseParser parser([&](const SseEvent& event) {
        if (event.data == "[DONE]") return;

        try {
            auto data = Json::parse(sanitize_utf8(event.data));
            if (data.contains("choices") && !data["choices"].empty()) {
                auto& choice = data["choices"][0];
                if (choice.contains("delta") && choice["delta"].contains("content")) {
                    auto content = choice["delta"]["content"];
                    if (!content.is_null()) {
                        auto text = content.get<std::string>();
                        accumulated_text += text;
                        if (on_text) on_text(text);
                    }
                }
                if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                    result.stop_reason = choice["finish_reason"].get<std::string>();
                }
            }
            if (data.contains("usage")) {
                result.usage.input_tokens = data["usage"].value("prompt_tokens", 0);
                result.usage.output_tokens = data["usage"].value("completion_tokens", 0);
            }
        } catch (const Json::parse_error& e) {
            log::warn("SSE parse error: " + std::string(e.what()));
        }
    });

    // Post request — httplib accumulates the full response
    auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");

    if (!res) {
        std::string err = "Connection failed to " + base_url;
        log::error(err);
        result.content.push_back({.type = "text", .text = "Error: " + err});
        result.stop_reason = "error";
        return result;
    }

    if (res->status != 200) {
        log::error("OpenAI streaming error " + std::to_string(res->status) + ": " + res->body);
        result.content.push_back({.type = "text", .text = "API Error: " + res->body});
        result.stop_reason = "error";
        return result;
    }

    // Parse SSE events from full response body
    parser.feed(res->body);

    if (!accumulated_text.empty()) {
        result.content.push_back({.type = "text", .text = accumulated_text});
    }

    if (result.stop_reason.empty()) result.stop_reason = "stop";

    return result;
}

int OpenAiProvider::estimate_tokens(const std::string& text) const {
    return static_cast<int>(text.size()) / 4;
}

} // namespace cortex
