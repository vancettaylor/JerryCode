#include "cortex/core/orchestrator.hpp"
#include "cortex/agents/agent_registry.hpp"
#include "cortex/agents/file_read_agent.hpp"
#include "cortex/agents/file_write_agent.hpp"
#include "cortex/agents/bash_agent.hpp"
#include "cortex/agents/search_agent.hpp"
#include "cortex/agents/glob_agent.hpp"
#include "cortex/agents/grep_agent.hpp"
#include "cortex/tools/file_read.hpp"
#include "cortex/tools/file_write.hpp"
#include "cortex/tools/bash.hpp"
#include "cortex/tools/glob.hpp"
#include "cortex/tools/grep.hpp"
#include "cortex/providers/anthropic.hpp"
#include "cortex/providers/openai.hpp"
#include "cortex/storage/project_state.hpp"
#include "cortex/tui/app.hpp"
#include "cortex/util/config.hpp"
#include "cortex/util/log.hpp"
#include <iostream>
#include <filesystem>
#include <memory>

int main(int argc, char* argv[]) {
    using namespace cortex;

    std::string project_root = std::filesystem::current_path().string();
    bool init_config = false;

    // Pre-parse for --project and --init so we know where to load config from
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) project_root = argv[++i];
        else if (arg == "--init") init_config = true;
    }

    // Handle --init: write default config and exit
    if (init_config) {
        auto path = project_root + "/cortex.json";
        write_default_config(path);
        std::cout << "Wrote default config to " << path << "\n";
        return 0;
    }

    // Load config from files + env (layered: defaults -> global -> project -> env)
    auto config = load_config(project_root);

    // CLI args override config
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            i++; // Already handled above
        } else if (arg == "--model" && i + 1 < argc) {
            config.model = argv[++i];
        } else if (arg == "--provider" && i + 1 < argc) {
            config.provider = argv[++i];
        } else if (arg == "--base-url" && i + 1 < argc) {
            config.base_url = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Cortex - Dual-prompt context-managed coding agent\n\n"
                      << "Usage: cortex [options]\n\n"
                      << "Options:\n"
                      << "  --project <path>      Set project root (default: cwd)\n"
                      << "  --model <model>       Set model name\n"
                      << "  --provider <name>     Set provider ID from config\n"
                      << "  --base-url <url>      Override API base URL\n"
                      << "  --init                Write default cortex.json to project root\n"
                      << "  --help                Show this help\n\n"
                      << "Config precedence: defaults -> ~/.config/cortex/config.json -> ./cortex.json -> env vars -> CLI\n\n"
                      << "Environment variables:\n"
                      << "  CORTEX_PROVIDER       Provider ID\n"
                      << "  CORTEX_MODEL          Model ID\n"
                      << "  CORTEX_BASE_URL       API base URL\n"
                      << "  ANTHROPIC_API_KEY     Anthropic API key\n"
                      << "  OPENAI_API_KEY        OpenAI API key\n";

            // List configured providers
            if (!config.providers.empty()) {
                std::cout << "\nConfigured providers:\n";
                for (const auto& p : config.providers) {
                    std::cout << "  " << p.id << " (" << p.name << ") " << p.base_url;
                    if (p.requires_auth) std::cout << " [auth required]";
                    std::cout << "\n";
                    for (const auto& m : p.models) {
                        std::cout << "    - " << m.id << " (" << m.name << ", "
                                  << m.context_window / 1024 << "K context)\n";
                    }
                }
            }
            return 0;
        }
    }

    // Validate auth for providers that need it
    for (const auto& p : config.providers) {
        if (p.id == config.provider && p.requires_auth && config.api_key.empty()) {
            std::cerr << "Error: Provider '" << config.provider << "' requires authentication.\n"
                      << "Set the appropriate API key environment variable.\n";
            return 1;
        }
    }

    // Setup logging
    ProjectState project(project_root);
    project.ensure_data_dir();
    log::set_file(project.data_dir() + "/cortex.log");
    log::set_level(log::Level::Debug);
    log::info("Cortex starting in " + project_root);
    log::info("Provider: " + config.provider + " | Model: " + config.model);
    log::info("Base URL: " + config.base_url);

    // Create tools
    auto file_read_tool  = std::make_shared<FileReadTool>();
    auto file_write_tool = std::make_shared<FileWriteTool>();
    auto bash_tool       = std::make_shared<BashTool>();
    auto glob_tool       = std::make_shared<GlobTool>();
    auto grep_tool       = std::make_shared<GrepTool>();

    // Create agents with tools
    AgentRegistry agents;
    agents.register_agent(std::make_unique<FileReadAgent>(file_read_tool));
    agents.register_agent(std::make_unique<FileWriteAgent>(file_write_tool));
    agents.register_agent(std::make_unique<BashAgent>(bash_tool));
    agents.register_agent(std::make_unique<SearchAgent>(glob_tool, grep_tool));
    agents.register_agent(std::make_unique<GlobAgent>(glob_tool));
    agents.register_agent(std::make_unique<GrepAgent>(grep_tool));

    // Create provider based on config
    // Providers with "anthropic" in their ID use the Anthropic API format
    // Everything else uses the OpenAI-compatible format
    std::unique_ptr<IProvider> provider;
    if (config.provider == "anthropic") {
        provider = std::make_unique<AnthropicProvider>();
    } else {
        // Default: OpenAI-compatible (works with ATM-AI, OpenAI, and any compatible server)
        provider = std::make_unique<OpenAiProvider>();
    }

    // Create orchestrator
    auto provider_config = to_provider_config(config);
    Orchestrator orchestrator(std::move(provider), agents, project_root, provider_config);

    // Run TUI
    App app(orchestrator);
    app.run();

    return 0;
}
