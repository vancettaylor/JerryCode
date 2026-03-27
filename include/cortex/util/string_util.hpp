#pragma once
#include <string>
#include <vector>
#include <utility>

namespace cortex {

std::string replace_all(const std::string& input,
                        const std::string& from,
                        const std::string& to);

std::string render_template(
    const std::string& tmpl,
    const std::vector<std::pair<std::string, std::string>>& vars);

std::vector<std::string> split(const std::string& s, char delimiter);
std::string trim(const std::string& s);
std::string to_lower(const std::string& s);

} // namespace cortex
