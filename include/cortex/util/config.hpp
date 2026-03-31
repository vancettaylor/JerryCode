/**
 * @file config.hpp
 * @brief Layered configuration system with file, user, and environment sources.
 */

#pragma once
#include "cortex/core/types.hpp"
#include "cortex/providers/provider.hpp"
#include <string>
#include <vector>

namespace cortex {

/** @brief Configuration for a single LLM model. */
struct ModelConfig {
    std::string id;                 ///< Unique model identifier.
    std::string name;               ///< Human-readable model name.
    bool supports_tools = true;     ///< Whether the model supports tool/function calling.
    int context_window = 128000;    ///< Maximum context window size in tokens.
};

/** @brief Configuration entry for a provider and its available models. */
struct ProviderEntry {
    std::string id;                     ///< Unique provider identifier.
    std::string name;                   ///< Human-readable provider name.
    std::string base_url;               ///< API base URL for the provider.
    bool requires_auth = false;         ///< Whether the provider requires an API key.
    std::vector<ModelConfig> models;    ///< Models available from this provider.
};

/** @brief Top-level application configuration. */
struct AppConfig {
    std::string provider;                   ///< Active provider identifier.
    std::string model;                      ///< Active model identifier.
    std::string api_key;                    ///< API key for the active provider.
    std::string base_url;                   ///< Base URL override for the active provider.
    std::string project_root = ".";         ///< Root directory of the current project.
    int max_auto_cycles = 5;                ///< Maximum autonomous agent cycles.
    int timeline_review_interval = 10;      ///< Interval (in steps) between timeline reviews.
    int max_tokens = 4096;                  ///< Default max tokens per completion.
    int context_window = 128000;            ///< Context window size in tokens.

    /// Provider entries loaded from config file.
    std::vector<ProviderEntry> providers;
};

/**
 * @brief Load config with layered overrides: project -> user -> env vars.
 * @param project_root Path to the project root directory.
 * @return Merged AppConfig.
 */
AppConfig load_config(const std::string& project_root = ".");

/**
 * @brief Load configuration exclusively from environment variables.
 * @return AppConfig populated from environment variables.
 */
AppConfig load_config_from_env();

/**
 * @brief Convert an AppConfig into a ProviderConfig for API requests.
 * @param app_config The application configuration to convert.
 * @return ProviderConfig suitable for passing to IProvider methods.
 */
ProviderConfig to_provider_config(const AppConfig& app_config);

/**
 * @brief Write a default configuration file to the specified path.
 * @param path Filesystem path where the config file will be written.
 */
void write_default_config(const std::string& path);

} // namespace cortex
