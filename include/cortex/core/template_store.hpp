/**
 * @file template_store.hpp
 * @brief Runtime-loaded prompt templates from JSON files.
 *
 * All prompt text is loaded from prompts/templates.json at startup.
 * Templates support {{variable}} substitution.
 * No recompilation needed to change prompt behavior.
 */
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace cortex {

using Json = nlohmann::json;

/**
 * @brief Loads and serves prompt templates from a JSON file.
 *
 * Templates are accessed by dot-separated paths: "code_gen.system",
 * "context_actions.budget_header", etc. Variable substitution is
 * done via render() with a key-value map.
 */
class TemplateStore {
public:
    /// Load templates from a JSON file. Returns false on failure.
    bool load(const std::string& path);

    /// Load from a JSON object directly.
    void load_json(const Json& j);

    /// Get a raw template by dot-path (e.g., "code_gen.system").
    /// Returns empty string if not found.
    std::string get(const std::string& path) const;

    /// Get an array template by dot-path (e.g., "context_reminders").
    /// Returns empty vector if not found.
    std::vector<std::string> get_array(const std::string& path) const;

    /// Render a template with variable substitution.
    /// Variables are {{key}} in the template, replaced by values from vars.
    std::string render(const std::string& path,
                       const std::unordered_map<std::string, std::string>& vars) const;

    /// Render a raw template string with variable substitution.
    static std::string substitute(const std::string& tmpl,
                                   const std::unordered_map<std::string, std::string>& vars);

    /// Check if a template path exists.
    bool has(const std::string& path) const;

    /// Get a cycling reminder (rotates through array by index).
    std::string get_reminder(int round_number) const;

private:
    /// Navigate the JSON tree by dot-separated path.
    const Json* navigate(const std::string& path) const;

    Json templates_;
    bool loaded_ = false;
};

} // namespace cortex
