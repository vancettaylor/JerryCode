/**
 * @file json_util.hpp
 * @brief Safe JSON parsing utilities.
 *
 * All JSON parsing in JerryCode should go through these helpers
 * to prevent crashes from malformed or invalid UTF-8 input.
 */
#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace cortex {

using Json = nlohmann::json;

/**
 * @brief Safely parse a JSON string. Returns nullopt on any error
 *        (malformed JSON, invalid UTF-8, etc.) instead of throwing.
 * @param input The string to parse.
 * @return Parsed JSON, or nullopt if parsing failed.
 */
inline std::optional<Json> safe_json_parse(const std::string& input) {
    auto result = Json::parse(input, nullptr, false);
    if (result.is_discarded()) return std::nullopt;
    return result;
}

} // namespace cortex
