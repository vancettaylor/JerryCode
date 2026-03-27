#include "cortex/tools/glob.hpp"
#include <filesystem>
#include <regex>
#include <sstream>

namespace cortex {

namespace fs = std::filesystem;

ToolDefinition GlobTool::definition() const {
    return ToolDefinition{
        .name = "glob",
        .description = "Find files matching a glob pattern",
        .input_schema = Json{
            {"type", "object"},
            {"required", {"pattern"}},
            {"properties", {
                {"pattern", {{"type", "string"}}},
                {"path", {{"type", "string"}}}
            }}
        }
    };
}

ToolResult GlobTool::execute(const Json& arguments) {
    auto pattern = arguments.at("pattern").get<std::string>();
    auto base_path = arguments.value("path", ".");

    // Convert glob to regex (simple conversion)
    std::string regex_str;
    for (char c : pattern) {
        switch (c) {
            case '*': regex_str += ".*"; break;
            case '?': regex_str += "."; break;
            case '.': regex_str += "\\."; break;
            default: regex_str += c;
        }
    }

    std::regex re(regex_str);
    std::vector<std::string> matches;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(base_path,
                fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                auto rel = fs::relative(entry.path(), base_path).string();
                if (std::regex_match(rel, re)) {
                    matches.push_back(rel);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        return ToolResult{false, std::string("Filesystem error: ") + e.what(), {}, {}};
    }

    std::ostringstream ss;
    for (const auto& m : matches) ss << m << "\n";

    return ToolResult{true, ss.str(), {{"count", matches.size()}}, {}};
}

} // namespace cortex
