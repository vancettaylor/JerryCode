#pragma once
/**
 * @file api.hpp
 * @brief JerryCode public API — single header for programmatic use.
 *
 * This is the entry point for using JerryCode as a library.
 * Include this header and link against cortex_lib to use
 * JerryCode from your own C++ programs.
 *
 * Example:
 *   #include <cortex/api.hpp>
 *
 *   int main() {
 *       auto result = cortex::run_task(
 *           "Add a factorial function to main.cpp",
 *           "/path/to/project",
 *           {.model = "qwen3-coder-next-80b",
 *            .base_url = "http://192.168.11.25:8080"});
 *
 *       if (result.success) {
 *           std::cout << "Done! " << result.tasks_completed
 *                     << " tasks completed\n";
 *       }
 *       return result.success ? 0 : 1;
 *   }
 */

#include "cortex/core/session.hpp"
#include "cortex/core/task_manager.hpp"
#include "cortex/core/context_expander.hpp"
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
#include "cortex/providers/anthropic.hpp"
#include "cortex/util/config.hpp"
#include "cortex/util/log.hpp"
#include <memory>
#include <functional>

namespace cortex {

/// Configuration for a single API call.
struct ApiConfig {
    std::string model = "qwen3-coder-next-80b";
    std::string base_url = "http://192.168.11.25:8080";
    std::string api_key;            ///< Only needed for Anthropic/OpenAI
    std::string provider = "openai"; ///< "openai" for any OpenAI-compatible, "anthropic" for Claude
    int max_tokens = 4096;
};

/// Result of running a task through the API.
struct ApiResult {
    bool success = false;
    int tasks_completed = 0;
    int tasks_failed = 0;
    int tasks_total = 0;
    int llm_calls = 0;
    int tokens_in = 0;
    int tokens_out = 0;
    int errors_fixed = 0;
    double time_ms = 0;
    std::string summary;
};

/// Callback for streaming output from the session.
using ApiCallback = std::function<void(const std::string& phase,
                                        const std::string& message)>;

/**
 * @brief Run a coding task on a project directory.
 *
 * This is the simplest way to use JerryCode programmatically.
 * It creates a session, runs the task, and returns the result.
 *
 * @param task        Natural language description of the task.
 * @param project_dir Path to the project directory.
 * @param config      API configuration (model, base URL, etc.).
 * @param callback    Optional callback for streaming output.
 * @return ApiResult  Result with statistics and success/failure.
 */
inline ApiResult run_task(const std::string& task,
                           const std::string& project_dir,
                           const ApiConfig& config = {},
                           ApiCallback callback = nullptr) {
    // Setup logging
    log::set_level(log::Level::Info);

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

    // Create provider
    std::unique_ptr<IProvider> provider;
    if (config.provider == "anthropic") {
        provider = std::make_unique<AnthropicProvider>();
    } else {
        provider = std::make_unique<OpenAiProvider>();
    }

    // Build provider config
    ProviderConfig pcfg;
    pcfg.api_key = config.api_key;
    pcfg.base_url = config.base_url;
    pcfg.model = config.model;
    pcfg.max_tokens = config.max_tokens;

    // Create and run session
    Session session(std::move(provider), agents, project_dir, pcfg);

    SessionCallbacks cb;
    if (callback) {
        cb.on_phase = [&](const std::string& p) { callback("phase", p); };
        cb.on_stream = [&](const std::string& t) { callback("stream", t); };
        cb.on_status = [&](const std::string& s) { callback("status", s); };
        cb.on_error = [&](const std::string& e) { callback("error", e); };
    }

    session.run(task, cb);

    // Collect results
    auto& stats = session.stats();
    ApiResult result;
    result.success = (stats.tasks_failed == 0 && stats.tasks_completed > 0);
    result.tasks_completed = stats.tasks_completed;
    result.tasks_failed = stats.tasks_failed;
    result.tasks_total = session.tasks().total_count();
    result.llm_calls = stats.total_llm_calls;
    result.tokens_in = stats.total_tokens_in;
    result.tokens_out = stats.total_tokens_out;
    result.errors_fixed = stats.errors_fixed;
    result.time_ms = stats.total_time_ms;
    result.summary = session.stats_summary();

    return result;
}

} // namespace cortex
