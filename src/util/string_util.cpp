#include "cortex/util/string_util.hpp"
#include <algorithm>
#include <sstream>

namespace cortex {

std::string replace_all(const std::string& input,
                        const std::string& from,
                        const std::string& to) {
    if (from.empty()) return input;
    std::string result = input;
    std::string::size_type pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

std::string render_template(
    const std::string& tmpl,
    const std::vector<std::pair<std::string, std::string>>& vars) {
    std::string result = tmpl;
    for (const auto& [key, value] : vars) {
        result = replace_all(result, key, value);
    }
    return result;
}

std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream stream(s);
    std::string token;
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace cortex
