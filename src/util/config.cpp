#include "cortex/util/config.hpp"
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace cortex {

namespace fs = std::filesystem;

static Json default_config_json() {
    return Json{
        {"provider", "atm-ai"},
        {"model", "qwen3-coder-next-80b"},
        {"providers", {
            {
                {"id", "atm-ai"},
                {"name", "ATM-AI LLM Server"},
                {"base_url", "http://192.168.11.25:8080"},
                {"requires_auth", false},
                {"models", {
                    {{"id", "gpt-oss-120b"}, {"name", "GPT-OSS 120B"}, {"context_window", 131072}, {"supports_tools", true}},
                    {{"id", "qwen3-coder-next-80b"}, {"name", "Qwen3-Coder-Next 80B"}, {"context_window", 131072}, {"supports_tools", true}},
                    {{"id", "qwen3-coder-30b"}, {"name", "Qwen3-Coder 30B"}, {"context_window", 131072}, {"supports_tools", true}},
                    {{"id", "qwen3-next-80b"}, {"name", "Qwen3-Next 80B"}, {"context_window", 131072}, {"supports_tools", true}},
                    {{"id", "deepresearch-30b"}, {"name", "DeepResearch 30B"}, {"context_window", 131072}, {"supports_tools", true}},
                    {{"id", "qwen3-32b-uncensored"}, {"name", "Qwen3 32B Uncensored"}, {"context_window", 131072}, {"supports_tools", true}}
                }}
            },
            {
                {"id", "anthropic"},
                {"name", "Anthropic Claude"},
                {"base_url", "https://api.anthropic.com"},
                {"requires_auth", true},
                {"models", {
                    {{"id", "claude-sonnet-4-20250514"}, {"name", "Claude Sonnet 4"}, {"context_window", 200000}, {"supports_tools", true}}
                }}
            },
            {
                {"id", "openai"},
                {"name", "OpenAI"},
                {"base_url", "https://api.openai.com"},
                {"requires_auth", true},
                {"models", {
                    {{"id", "gpt-4o"}, {"name", "GPT-4o"}, {"context_window", 128000}, {"supports_tools", true}}
                }}
            }
        }}
    };
}

static void parse_config_json(const Json& j, AppConfig& config) {
    if (j.contains("provider")) config.provider = j["provider"].get<std::string>();
    if (j.contains("model")) config.model = j["model"].get<std::string>();
    if (j.contains("api_key")) config.api_key = j["api_key"].get<std::string>();
    if (j.contains("base_url")) config.base_url = j["base_url"].get<std::string>();
    if (j.contains("max_auto_cycles")) config.max_auto_cycles = j["max_auto_cycles"].get<int>();
    if (j.contains("max_tokens")) config.max_tokens = j["max_tokens"].get<int>();
    if (j.contains("timeline_review_interval")) config.timeline_review_interval = j["timeline_review_interval"].get<int>();

    if (j.contains("providers")) {
        config.providers.clear();
        for (const auto& pj : j["providers"]) {
            ProviderEntry entry;
            entry.id = pj.value("id", "");
            entry.name = pj.value("name", "");
            entry.base_url = pj.value("base_url", "");
            entry.requires_auth = pj.value("requires_auth", false);
            if (pj.contains("models")) {
                for (const auto& mj : pj["models"]) {
                    ModelConfig mc;
                    mc.id = mj.value("id", "");
                    mc.name = mj.value("name", "");
                    mc.context_window = mj.value("context_window", 128000);
                    mc.supports_tools = mj.value("supports_tools", true);
                    entry.models.push_back(mc);
                }
            }
            config.providers.push_back(entry);
        }
    }
}

static void resolve_provider(AppConfig& config) {
    // If base_url is already set explicitly, use it
    if (!config.base_url.empty()) return;

    // Find the provider entry and resolve base_url + context_window
    for (const auto& p : config.providers) {
        if (p.id == config.provider) {
            config.base_url = p.base_url;
            for (const auto& m : p.models) {
                if (m.id == config.model) {
                    config.context_window = m.context_window;
                    break;
                }
            }
            break;
        }
    }
}

AppConfig load_config(const std::string& project_root) {
    AppConfig config;

    // Start with defaults
    auto defaults = default_config_json();
    parse_config_json(defaults, config);

    // Layer 1: Global config ~/.config/cortex/config.json
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!home.empty()) {
        auto global_path = home + "/.config/cortex/config.json";
        if (fs::exists(global_path)) {
            std::ifstream f(global_path);
            if (f.is_open()) {
                Json j;
                f >> j;
                parse_config_json(j, config);
            }
        }
    }

    // Layer 2: Project config cortex.json (in project root)
    auto project_path = project_root + "/cortex.json";
    if (fs::exists(project_path)) {
        std::ifstream f(project_path);
        if (f.is_open()) {
            Json j;
            f >> j;
            parse_config_json(j, config);
        }
    }

    // Layer 3: Environment variables override everything
    auto env = load_config_from_env();
    if (!env.provider.empty()) config.provider = env.provider;
    if (!env.model.empty()) config.model = env.model;
    if (!env.api_key.empty()) config.api_key = env.api_key;
    if (!env.base_url.empty()) config.base_url = env.base_url;

    // Resolve provider -> base_url mapping
    resolve_provider(config);

    return config;
}

AppConfig load_config_from_env() {
    AppConfig config;
    config.provider = "";  // Empty = don't override
    config.model = "";
    config.base_url = "";

    if (auto* key = std::getenv("ANTHROPIC_API_KEY")) {
        config.api_key = key;
        config.provider = "anthropic";
    }
    if (auto* key = std::getenv("OPENAI_API_KEY")) {
        config.api_key = key;
    }
    if (auto* v = std::getenv("CORTEX_MODEL")) config.model = v;
    if (auto* v = std::getenv("CORTEX_PROVIDER")) config.provider = v;
    if (auto* v = std::getenv("CORTEX_BASE_URL")) config.base_url = v;
    return config;
}

ProviderConfig to_provider_config(const AppConfig& app_config) {
    ProviderConfig config;
    config.api_key = app_config.api_key;
    config.base_url = app_config.base_url;
    config.model = app_config.model;
    config.max_tokens = app_config.max_tokens;
    return config;
}

void write_default_config(const std::string& path) {
    auto j = default_config_json();
    std::ofstream f(path);
    f << j.dump(2) << "\n";
}

} // namespace cortex
