#include "cortex/tools/bash.hpp"
#include <cstdio>
#include <array>
#include <memory>

namespace cortex {

ToolDefinition BashTool::definition() const {
    return ToolDefinition{
        .name = "bash",
        .description = "Execute a shell command",
        .input_schema = Json{
            {"type", "object"},
            {"required", {"command"}},
            {"properties", {
                {"command", {{"type", "string"}}},
                {"timeout_ms", {{"type", "integer"}}}
            }}
        }
    };
}

ToolResult BashTool::execute(const Json& arguments) {
    auto command = arguments.at("command").get<std::string>();

    std::array<char, 4096> buffer;
    std::string output;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen((command + " 2>&1").c_str(), "r"), pclose);

    if (!pipe) {
        return ToolResult{false, "Failed to execute command", {}, {
            {TriggerKind::ErrorDetected, "popen failed", std::nullopt, {}}
        }};
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe.release());
    bool success = (status == 0);

    std::vector<Trigger> triggers;
    if (!success) {
        triggers.push_back({TriggerKind::ErrorDetected,
            "Command exited with status " + std::to_string(status),
            std::nullopt, {}});
    }

    return ToolResult{success, output, {{"exit_code", status}}, triggers};
}

} // namespace cortex
