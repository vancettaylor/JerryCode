#include "cortex/core/session.hpp"
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
#include "cortex/providers/openai.hpp"
#include "cortex/util/config.hpp"
#include "cortex/util/log.hpp"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    using namespace cortex;

    std::string project_root = "/tmp/cortex-test-project";
    std::string user_input = "Hello";

    if (argc > 1) user_input = argv[1];
    if (argc > 2) project_root = argv[2];

    auto config = load_config(project_root);
    log::set_level(log::Level::Debug);

    std::cout << "=== Cortex Session ===\n";
    std::cout << "Model:   " << config.model << "\n";
    std::cout << "Project: " << project_root << "\n";
    std::cout << "Task:    " << user_input.substr(0, 80) << "\n";
    std::cout << "======================\n\n";

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

    auto provider = std::make_unique<OpenAiProvider>();
    auto provider_config = to_provider_config(config);

    // Create session
    Session session(std::move(provider), agents, project_root, provider_config);

    // Run
    SessionCallbacks callbacks{
        .on_phase = [](const std::string& phase) {
            std::cout << "\n--- " << phase << " ---\n";
        },
        .on_stream = [](const std::string& text) {
            // Only show first 500 chars of streamed output
            if (text.size() > 500) {
                std::cout << text.substr(0, 500) << "\n...\n";
            } else {
                std::cout << text;
            }
        },
        .on_status = [](const std::string& status) {
            std::cout << status << "\n";
        },
        .on_error = [](const std::string& err) {
            std::cerr << "[ERROR] " << err << "\n";
        }
    };

    session.run(user_input, callbacks);

    // Print final stats
    std::cout << "\n" << session.stats_summary();

    return session.stats().tasks_failed > 0 ? 1 : 0;
}
