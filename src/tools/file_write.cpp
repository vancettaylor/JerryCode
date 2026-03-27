#include "cortex/tools/file_write.hpp"
#include <fstream>
#include <filesystem>

namespace cortex {

ToolDefinition FileWriteTool::definition() const {
    return ToolDefinition{
        .name = "file_write",
        .description = "Write content to a file",
        .input_schema = Json{
            {"type", "object"},
            {"required", {"path", "content"}},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"content", {{"type", "string"}}}
            }}
        }
    };
}

ToolResult FileWriteTool::execute(const Json& arguments) {
    auto path = arguments.at("path").get<std::string>();
    auto content = arguments.at("content").get<std::string>();

    // Ensure parent directory exists
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    bool existed = std::filesystem::exists(path);
    std::ofstream file(path);
    if (!file.is_open()) {
        return ToolResult{false, "Cannot write to: " + path, {}, {}};
    }

    file << content;
    file.close();

    TriggerKind kind = existed ? TriggerKind::FileModified : TriggerKind::FileCreated;
    return ToolResult{
        true,
        "Wrote " + std::to_string(content.size()) + " bytes to " + path,
        {{"bytes", content.size()}},
        {{kind, "Wrote " + path, path, {}}}
    };
}

} // namespace cortex
