#include "cortex/tools/grep.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace cortex {

namespace fs = std::filesystem;

ToolDefinition GrepTool::definition() const {
    return ToolDefinition{
        .name = "grep",
        .description = "Search file contents for a pattern",
        .input_schema = Json{
            {"type", "object"},
            {"required", {"pattern"}},
            {"properties", {
                {"pattern", {{"type", "string"}}},
                {"path", {{"type", "string"}}},
                {"include", {{"type", "string"}}}
            }}
        }
    };
}

ToolResult GrepTool::execute(const Json& arguments) {
    auto pattern = arguments.at("pattern").get<std::string>();
    auto base_path = arguments.value("path", ".");

    std::regex re(pattern);
    std::ostringstream ss;
    int match_count = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(base_path,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::ifstream file(entry.path());
            std::string line;
            int line_num = 0;
            while (std::getline(file, line)) {
                line_num++;
                if (std::regex_search(line, re)) {
                    ss << fs::relative(entry.path(), base_path).string()
                       << ":" << line_num << ": " << line << "\n";
                    match_count++;
                    if (match_count >= 100) goto done;
                }
            }
        }
    } catch (const fs::filesystem_error&) {}

done:
    return ToolResult{true, ss.str(), {{"matches", match_count}}, {}};
}

} // namespace cortex
