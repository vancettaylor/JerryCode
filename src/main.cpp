/**
 * @file main.cpp
 * @brief JerryCode entry point — launches TUI or test harness.
 *
 * Usage:
 *   jerrycode                    # Launch TUI in current directory
 *   jerrycode --project /path    # Launch TUI for specific project
 *   jerrycode --init             # Generate default cortex.json
 *   jerrycode --help             # Show help
 */

#include "cortex/tui/app.hpp"
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
#include "cortex/util/config.hpp"
#include "cortex/util/log.hpp"
#include <iostream>
#include <filesystem>
#include <memory>

int main(int argc, char* argv[]) {
    using namespace cortex;

    std::string project_root = std::filesystem::current_path().string();
    bool init_config = false;

    // Pre-parse for --project and --init
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

    // Load config (layered: defaults -> global -> project -> env -> CLI)
    auto config = load_config(project_root);

    // CLI overrides
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) { i++; }
        else if (arg == "--model" && i + 1 < argc) config.model = argv[++i];
        else if (arg == "--provider" && i + 1 < argc) config.provider = argv[++i];
        else if (arg == "--base-url" && i + 1 < argc) config.base_url = argv[++i];
        else if (arg == "--help") {
            std::cout << "JerryCode — context-managed coding agent\n\n"
                      << "Usage: jerrycode [options]\n\n"
                      << "Options:\n"
                      << "  --project <path>      Project root (default: cwd)\n"
                      << "  --model <model>       Model name\n"
                      << "  --provider <name>     Provider ID\n"
                      << "  --base-url <url>      API base URL\n"
                      << "  --init                Generate cortex.json\n"
                      << "  --help                Show this help\n\n"
                      << "TUI Commands:\n"
                      << "  /sidebar              Toggle sidebar\n"
                      << "  /clear                Clear chat\n"
                      << "  /quit                 Exit\n\n";

            if (!config.providers.empty()) {
                std::cout << "Configured providers:\n";
                for (const auto& p : config.providers) {
                    std::cout << "  " << p.id << " (" << p.name << ") " << p.base_url;
                    if (p.requires_auth) std::cout << " [auth required]";
                    std::cout << "\n";
                    for (const auto& m : p.models) {
                        std::cout << "    - " << m.id << " (" << m.name << ")\n";
                    }
                }
            }
            return 0;
        }
    }

    // Auth check
    for (const auto& p : config.providers) {
        if (p.id == config.provider && p.requires_auth && config.api_key.empty()) {
            std::cerr << "Error: Provider '" << config.provider << "' requires authentication.\n";
            return 1;
        }
    }

    // Setup logging
    ProjectState project(project_root);
    project.ensure_data_dir();
    log::set_file(project.data_dir() + "/jerrycode.log");
    log::set_level(log::Level::Debug);
    log::info("JerryCode starting | provider=" + config.provider +
              " model=" + config.model + " project=" + project_root);

    // Create tools and agents
    auto file_read_tool  = std::make_shared<FileReadTool>();
    auto file_write_tool = std::make_shared<FileWriteTool>();
    auto bash_tool       = std::make_shared<BashTool>();
    auto glob_tool       = std::make_shared<GlobTool>();
    auto grep_tool       = std::make_shared<GrepTool>();

    AgentRegistry agents;
    agents.register_agent(std::make_unique<FileReadAgent>(file_read_tool));
    agents.register_agent(std::make_unique<FileWriteAgent>(file_write_tool));
    agents.register_agent(std::make_unique<BashAgent>(bash_tool));
    agents.register_agent(std::make_unique<SearchAgent>(glob_tool, grep_tool));
    agents.register_agent(std::make_unique<GlobAgent>(glob_tool));
    agents.register_agent(std::make_unique<GrepAgent>(grep_tool));

    auto provider_config = to_provider_config(config);

    // Launch TUI
    TuiApp app(project_root, provider_config, agents, config);
    app.run();

    return 0;
}
