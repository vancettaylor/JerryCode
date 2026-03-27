#include "cortex/tools/file_read.hpp"
#include <fstream>
#include <sstream>

namespace cortex {

ToolDefinition FileReadTool::definition() const {
    return ToolDefinition{
        .name = "file_read",
        .description = "Read a file from disk",
        .input_schema = Json{
            {"type", "object"},
            {"required", {"path"}},
            {"properties", {
                {"path", {{"type", "string"}}},
                {"offset", {{"type", "integer"}}},
                {"limit", {{"type", "integer"}}}
            }}
        }
    };
}

ToolResult FileReadTool::execute(const Json& arguments) {
    auto path = arguments.at("path").get<std::string>();
    int offset = arguments.value("offset", 0);
    int limit = arguments.value("limit", 2000);

    std::ifstream file(path);
    if (!file.is_open()) {
        return ToolResult{false, "Cannot open file: " + path, {}, {}};
    }

    std::ostringstream ss;
    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (line_num < offset) continue;
        if (line_num >= offset + limit) break;
        ss << line_num << "\t" << line << "\n";
    }

    return ToolResult{true, ss.str(), {{"lines", line_num}}, {}};
}

} // namespace cortex
