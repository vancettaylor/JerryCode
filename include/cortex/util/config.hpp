#pragma once
#include "cortex/core/types.hpp"
#include "cortex/providers/provider.hpp"
#include <string>
#include <vector>

namespace cortex {

struct ModelConfig {
    std::string id;
    std::string name;
    bool supports_tools = true;
    int context_window = 128000;
};

struct ProviderEntry {
    std::string id;
    std::string name;
    std::string base_url;
    bool requires_auth = false;
    std::vector<ModelConfig> models;
};

struct AppConfig {
    std::string provider;
    std::string model;
    std::string api_key;
    std::string base_url;
    std::string project_root = ".";
    int max_auto_cycles = 5;
    int timeline_review_interval = 10;
    int max_tokens = 4096;
    int context_window = 128000;

    // Loaded from config file
    std::vector<ProviderEntry> providers;
};

// Load config from: project cortex.json -> ~/.config/cortex/config.json -> env vars
// Each layer overrides the previous
AppConfig load_config(const std::string& project_root = ".");
AppConfig load_config_from_env();
ProviderConfig to_provider_config(const AppConfig& app_config);

// Write a default config file
void write_default_config(const std::string& path);

} // namespace cortex
