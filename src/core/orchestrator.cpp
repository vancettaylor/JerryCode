#include "cortex/core/orchestrator.hpp"
#include "cortex/util/json_util.hpp"
#include "cortex/core/context_expander.hpp"
#include "cortex/util/uuid.hpp"
#include "cortex/util/log.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace cortex {

namespace fs = std::filesystem;

// ─── Utility: Strip markdown fences from LLM output ───────────

static std::string strip_fences(const std::string& input) {
    auto s = input;

    // Trim leading/trailing whitespace
    while (!s.empty() && (s.front() == '\n' || s.front() == '\r' || s.front() == ' ')) {
        s.erase(s.begin());
    }

    // Case 1: Entire output wrapped in ```lang ... ```
    if (s.find("```") == 0) {
        auto first_nl = s.find('\n');
        auto last_fence = s.rfind("```");
        if (first_nl != std::string::npos && last_fence > first_nl) {
            s = s.substr(first_nl + 1, last_fence - first_nl - 1);
        }
    }

    // Case 2: Code followed by trailing fence garbage (```\n```cpp\n... or just ```)
    // Remove any ``` and everything after the last valid code
    while (true) {
        // Find the last occurrence of ```
        auto last_fence = s.rfind("```");
        if (last_fence == std::string::npos) break;

        // Check if everything after this ``` is just whitespace or more fences
        auto after = s.substr(last_fence + 3);
        bool is_trailing_garbage = true;
        for (char c : after) {
            if (c != '\n' && c != '\r' && c != ' ' && c != '`') {
                // Check if this looks like code starting again (language tag like "cpp")
                auto trimmed = after;
                while (!trimmed.empty() && (trimmed.front() == '\n' || trimmed.front() == ' ')) {
                    trimmed.erase(trimmed.begin());
                }
                // If what's left starts with a short word + newline, it's a language tag
                auto nl = trimmed.find('\n');
                if (nl != std::string::npos && nl < 10) {
                    // language tag — this is a fence, strip it
                } else {
                    is_trailing_garbage = false;
                }
                break;
            }
        }

        if (is_trailing_garbage) {
            // Trim from the last ``` to the end
            s = s.substr(0, last_fence);
        } else {
            break;
        }
    }

    // Case 3: Multiple code blocks with explanatory text between them
    // If there are still fences, extract the largest code block
    if (s.find("```") != std::string::npos) {
        std::string best_block;
        size_t pos = 0;
        while (pos < s.size()) {
            auto fence_start = s.find("```", pos);
            if (fence_start == std::string::npos) break;

            auto code_start = s.find('\n', fence_start);
            if (code_start == std::string::npos) break;
            code_start++;

            auto fence_end = s.find("```", code_start);
            if (fence_end == std::string::npos) {
                // Unclosed fence — take everything after the opening
                auto block = s.substr(code_start);
                if (block.size() > best_block.size()) best_block = block;
                break;
            }

            auto block = s.substr(code_start, fence_end - code_start);
            if (block.size() > best_block.size()) best_block = block;
            pos = fence_end + 3;
        }

        if (!best_block.empty() && best_block.size() > s.size() / 4) {
            s = best_block;
        }
    }

    // Final cleanup: trim trailing whitespace, ensure single trailing newline
    while (s.size() > 1 && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r')) {
        s.pop_back();
    }
    s += '\n';

    return s;
}

// ─── Construction ──────────────────────────────────────────────

Orchestrator::Orchestrator(std::unique_ptr<IProvider> provider,
                           AgentRegistry& agents,
                           const std::string& project_root,
                           const ProviderConfig& provider_config)
    : provider_(std::move(provider))
    , provider_config_(provider_config)
    , agents_(agents)
    , compiler_(index_, budget_)
{
    index_.set_project_root(project_root);

    // Scan project files
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(project_root,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto rel = fs::relative(entry.path(), project_root).string();
            if (rel[0] == '.' || rel.find("/.") != std::string::npos) continue;
            if (rel.find("build/") == 0 || rel.find("node_modules/") == 0) continue;
            if (rel.find(".cortex/") == 0) continue;
            files.push_back(rel);
            if (files.size() > 100) break;
        }
    } catch (const fs::filesystem_error&) {}
    index_.update_file_tree(files);
    log::info("Indexed " + std::to_string(files.size()) + " files in " + project_root);

    // Setup context expansion actions
    setup_expander();
}

// ─── Context Expander Setup ───────────────────────────────────

void Orchestrator::setup_expander() {
    // @read(path) — read a file from the project
    expander_.register_action("read", [this](const std::string& arg) -> std::optional<std::string> {
        auto content = read_file(arg);
        if (content.empty()) {
            // Try absolute path
            std::ifstream f(arg);
            if (f.is_open()) {
                std::ostringstream ss;
                ss << f.rdbuf();
                content = ss.str();
            }
        }
        if (content.empty()) return std::nullopt;
        return content;
    });

    // @search(pattern) — grep the project for a pattern
    expander_.register_action("search", [this](const std::string& arg) -> std::optional<std::string> {
        auto* agent = agents_.get("grep");
        if (!agent) return std::nullopt;
        Json args;
        args["pattern"] = arg;
        args["path"] = index_.project_root();
        AgentContext ctx;
        ctx.plan.tool_arguments = args;
        auto result = agent->execute(ctx, *provider_, nullptr);
        if (result.tool_result.output.empty()) return "No matches found for: " + arg;
        return result.tool_result.output;
    });

    // @run(command) — execute a shell command
    expander_.register_action("run", [this](const std::string& arg) -> std::optional<std::string> {
        auto* agent = agents_.get("bash");
        if (!agent) return std::nullopt;
        Json args;
        args["command"] = "cd " + index_.project_root() + " && " + arg;
        AgentContext ctx;
        ctx.plan.tool_arguments = args;
        auto result = agent->execute(ctx, *provider_, nullptr);
        return result.tool_result.output;
    });
}

// ─── Helpers ───────────────────────────────────────────────────

std::string Orchestrator::abs_path(const std::string& rel_path) {
    if (!rel_path.empty() && rel_path[0] == '/') return rel_path;
    return (fs::path(index_.project_root()) / rel_path).string();
}

std::string Orchestrator::read_file(const std::string& rel_path) {
    if (file_cache_.count(rel_path)) return file_cache_[rel_path];
    std::ifstream f(abs_path(rel_path));
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    auto content = ss.str();
    file_cache_[rel_path] = content;
    return content;
}

std::string Orchestrator::build_file_context() {
    std::string ctx;
    for (const auto& [path, content] : file_cache_) {
        if (content.empty()) continue;
        ctx += "\n### " + path + "\n```\n";
        if (content.size() > 6000) {
            ctx += content.substr(0, 6000) + "\n...(truncated)\n";
        } else {
            ctx += content;
        }
        ctx += "\n```\n";
    }
    return ctx;
}

std::string Orchestrator::llm_complete(const std::string& system_prompt,
                                        const std::string& user_prompt,
                                        int max_tokens) {
    ProviderConfig config = provider_config_;
    config.temperature = 0.0;
    config.max_tokens = max_tokens;

    std::vector<Message> messages;
    if (user_prompt.empty()) {
        // Single compiled prompt — used by the context expander
        messages.push_back(Message::user(system_prompt));
    } else {
        // Traditional system + user split
        messages.push_back(Message::system(system_prompt));
        messages.push_back(Message::user(user_prompt));
    }

    auto completion = provider_->complete(messages, config);

    std::string text;
    for (const auto& block : completion.content) {
        if (block.type == "text") text += block.text;
    }
    return text;
}

std::string Orchestrator::llm_complete_multi(
    const std::string& system_prompt,
    const std::vector<std::pair<std::string, std::string>>& messages,
    int max_tokens) {

    ProviderConfig config = provider_config_;
    config.temperature = 0.0;
    config.max_tokens = max_tokens;

    std::vector<Message> msgs;
    msgs.push_back(Message::system(system_prompt));
    for (const auto& [role, content] : messages) {
        if (role == "user") {
            msgs.push_back(Message::user(content));
        } else if (role == "assistant") {
            msgs.push_back(Message::assistant(content));
        }
    }

    auto completion = provider_->complete(msgs, config);

    std::string text;
    for (const auto& block : completion.content) {
        if (block.type == "text") text += block.text;
    }
    return text;
}

std::string Orchestrator::llm_generate_code(const std::string& file_path,
                                             const std::string& description,
                                             const std::string& original_request) {
    auto existing = read_file(file_path);

    std::string system = R"(You are a precise code generator. Output ONLY the complete file content.
No markdown fences. No explanations. No commentary. Just the raw code.

STRICT RULES:
- Output raw code ONLY. No markdown. No ``` fences. No text before or after the code.
- Use #include "file.hpp" (quotes) for local headers, <header> for system headers.
- Include ALL necessary headers.
- Write complete, compilable, production-quality code.
- No placeholders, no TODOs, no "// rest of code here".
- Do EXACTLY what the task says. Do not add extra features, UI, HTML, or things not asked for.
- Keep it minimal and correct. Simple is better than complex.
- For C++ strings containing special characters, use proper escaping or raw string literals.
- NEVER put main() in a .hpp/.h header file. Headers are for declarations and definitions only.
- Header-only libraries (.hpp) should NOT be compiled as separate translation units.
- For Makefiles: only compile .cpp files to .o files, NEVER .hpp files.)";

    system += "\n\n## File: " + file_path + "\n## Task: " + description + "\n";

    if (!original_request.empty()) {
        system += "\n## Original user request (follow ALL details from this):\n" + original_request + "\n";
    }

    if (!existing.empty()) {
        system += "\n## Current content:\n```\n" + existing + "\n```\n";
        system += "\nModify the above file. Output the COMPLETE updated file.\n";
    } else {
        system += "\nThis is a new file. Output the complete content.\n";
    }

    // Ensure all cached files are in the working set and expanded
    for (const auto& [p, c] : file_cache_) {
        if (p == file_path || c.empty()) continue;
        if (!expander_.has_item(p)) {
            expander_.add_item(p, "file", p, c);
        }
        expander_.expand_item(p);  // Reference files should be visible during code gen
    }

    system += "\nNote: If you need to see another file, use @read(id) and STOP. "
              "Otherwise just output the code directly with NO action tags.\n";

    log::info("Generating code for " + file_path);

    auto llm_fn = [this](const std::string& prompt, int tokens) -> std::string {
        return llm_complete(prompt, "", tokens);
    };

    auto loop_result = expander_.run(system, "Generate the complete file content for " +
                                      file_path + ".", llm_fn, 8192, 4);
    auto result = loop_result.output;

    auto& st = loop_result.final_stats;
    log::info("Code gen: " + std::to_string(loop_result.rounds) + " rounds, " +
              std::to_string(st.expanded_tokens) + " tok expanded, " +
              "compression=" + std::to_string(int(st.compression * 100)) + "%");

    // Robust markdown fence stripping
    // The model might: wrap in ```lang ... ```, output explanations with multiple code blocks,
    // or output clean code. We need to handle all cases.
    result = strip_fences(result);

    // Trim excess trailing newlines
    while (result.size() > 1 && result.back() == '\n' && result[result.size()-2] == '\n') {
        result.pop_back();
    }

    return result;
}

// ─── Task Decomposition ───────────────────────────────────────

std::vector<TaskStep> Orchestrator::decompose_task(const std::string& user_input) {
    // Build context: file tree
    std::string file_list;
    for (const auto& f : index_.pointers()) {
        file_list += "  " + f.label + "\n";
    }
    // Use the raw file tree from index
    std::string context = index_.render();

    std::string system = R"(You are a task planner for a coding agent. Break the user's request into a sequence of concrete steps.

## Project
)" + context + R"(

## Output Format
Respond with ONLY a JSON array of steps. Each step is:
{"action": "read|write|bash", "target": "file_path_or_command", "description": "what this step does"}

## Actions
- "read": Read a file to understand it. target = relative file path.
- "write": Create or modify a file. target = relative file path. description = what to write/change.
- "bash": Run a shell command. target = the command. description = why.

## Rules
- Read files BEFORE writing them (if they exist).
- One step per file write. Don't combine multiple file writes.
- After writing code files, add a bash step to compile/run if applicable.
- Keep it minimal. Don't add unnecessary verification reads after writes.
- New files don't need a read step first.
- Paths are relative to project root.

## Examples
User: "Add a greet function to main.cpp"
[
  {"action": "read", "target": "main.cpp", "description": "Read current main.cpp"},
  {"action": "write", "target": "main.cpp", "description": "Add a greet(name) function that prints a greeting, call it from main"},
  {"action": "bash", "target": "g++ -std=c++17 -o main main.cpp", "description": "Compile to verify"}
]

User: "Create a config parser in config.hpp and use it in main.cpp"
[
  {"action": "read", "target": "main.cpp", "description": "Read current main.cpp"},
  {"action": "write", "target": "config.hpp", "description": "Create header-only config parser class"},
  {"action": "write", "target": "main.cpp", "description": "Update to include config.hpp and use the parser"},
  {"action": "bash", "target": "g++ -std=c++17 -o main main.cpp", "description": "Compile to verify"}
]
)";

    // Remove the old action docs — the expander compiles its own now
    log::info("Decomposing task...");

    // Use working-set expansion loop
    auto llm_fn = [this](const std::string& prompt, int tokens) -> std::string {
        return llm_complete(prompt, "", tokens);
    };

    auto loop_result = expander_.run(system, user_input, llm_fn, 1024, 6);
    auto response = loop_result.output;

    // Log stats
    auto& st = loop_result.final_stats;
    log::info("Decomposition: " + std::to_string(loop_result.rounds) + " rounds, " +
              std::to_string(st.expanded_items) + " items expanded (" +
              std::to_string(st.expanded_tokens) + " tok), " +
              std::to_string(st.reads_this_session) + " reads, " +
              std::to_string(st.hides_this_session) + " hides, " +
              "compression=" + std::to_string(int(st.compression * 100)) + "%");
    log::debug("Decomposition output: " + response);

    // Extract JSON array
    auto arr_start = response.find('[');
    auto arr_end = response.rfind(']');
    if (arr_start == std::string::npos || arr_end == std::string::npos) {
        log::error("Failed to parse decomposition: " + response.substr(0, 300));
        return {};
    }

    std::vector<TaskStep> steps;
    try {
        auto json_opt = safe_json_parse(response.substr(arr_start, arr_end - arr_start + 1)); if (!json_opt) { log::error("Invalid JSON in orchestrator"); return {}; } auto json = *json_opt;
        for (const auto& j : json) {
            TaskStep step;
            step.id = generate_uuid();
            step.action = j.value("action", "");
            step.target = j.value("target", "");
            step.description = j.value("description", "");
            step.status = "pending";
            steps.push_back(step);
        }
    } catch (const std::exception& e) {
        log::error("Step parse error: " + std::string(e.what()));
    }

    return steps;
}

// ─── Main Entry Point ─────────────────────────────────────────

void Orchestrator::handle_user_input(const std::string& input,
                                      OrchestratorCallbacks callbacks) {
    file_cache_.clear();
    steps_.clear();
    original_request_ = input;
    index_.set_active_task(input);

    // Initialize working set with project files (all hidden by default)
    expander_.clear();
    for (const auto& f : index_.pointers()) {
        // Don't pre-load content, just register IDs
    }
    // Pre-register known files so the model sees them in the index
    try {
        for (const auto& entry : fs::recursive_directory_iterator(index_.project_root(),
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto rel = fs::relative(entry.path(), index_.project_root()).string();
            if (rel[0] == '.' || rel.find("/.") != std::string::npos) continue;
            if (rel.find("build/") == 0 || rel.find("node_modules/") == 0) continue;
            if (rel.find(".cortex/") == 0) continue;
            auto size = fs::file_size(entry.path());
            int est_tokens = static_cast<int>(size) / 4;
            expander_.add_item(rel, "file",
                rel + " (" + std::to_string(size) + " bytes)", "");
            // Content loaded lazily on @read()
        }
    } catch (const fs::filesystem_error&) {}

    log::info("Working set initialized: " + std::to_string(expander_.stats().total_items) + " items");

    // Phase 0: Decompose into steps
    if (callbacks.on_phase_change) callbacks.on_phase_change("decomposing");
    steps_ = decompose_task(input);

    if (steps_.empty()) {
        if (callbacks.on_error) callbacks.on_error("Failed to decompose task into steps");
        return;
    }

    log::info("Task decomposed into " + std::to_string(steps_.size()) + " steps");
    for (size_t i = 0; i < steps_.size(); i++) {
        log::info("  Step " + std::to_string(i+1) + ": [" + steps_[i].action + "] " +
                  steps_[i].target + " — " + steps_[i].description);
    }

    // Execute each step in order
    for (size_t i = 0; i < steps_.size(); i++) {
        auto& step = steps_[i];
        log::info("Executing step " + std::to_string(i+1) + "/" +
                  std::to_string(steps_.size()) + ": [" + step.action + "] " + step.target);

        // Report plan
        ActionPlan plan;
        plan.id = step.id;
        plan.task_description = "Step " + std::to_string(i+1) + ": " + step.description;
        plan.tool_name = step.action;
        plan.reasoning = step.description;
        plan.expected_outcome = step.description;
        if (callbacks.on_plan_ready) callbacks.on_plan_ready(plan);

        if (callbacks.on_phase_change) {
            callbacks.on_phase_change(step.action + " " + std::to_string(i+1) +
                                      "/" + std::to_string(steps_.size()));
        }

        try {
            execute_step(step, callbacks);
        } catch (const std::exception& e) {
            step.status = "failed";
            step.result = e.what();
            log::error("Step failed: " + std::string(e.what()));
            if (callbacks.on_error) callbacks.on_error(
                "Step " + std::to_string(i+1) + " failed: " + e.what());
        }

        // Report completion
        MetadataRecord record;
        record.id = generate_uuid();
        record.plan = plan;
        record.success = (step.status == "done");
        record.result_summary = step.result.substr(0, 200);
        if (!record.success) {
            record.triggers.push_back({TriggerKind::ErrorDetected, step.result, step.target, {}});
        }
        if (callbacks.on_action_complete) callbacks.on_action_complete(record);

        // Timeline entry
        TimelineEntry entry;
        entry.metadata_id = record.id;
        entry.one_line_summary = "[" + step.action + "] " + step.target;
        entry.tool_used = step.action;
        entry.success = record.success;
        timeline_.append(entry);

        // If a bash step fails, try to fix by asking the model
        if (step.action == "bash" && step.status == "failed" && i > 0) {
            log::info("Bash step failed, attempting auto-fix...");
            auto fix_result = attempt_fix(step, callbacks);
            if (fix_result) {
                // Re-run the bash step
                step.status = "pending";
                step.result = "";
                try {
                    execute_step(step, callbacks);
                } catch (...) {}
            }
        }
    }

    // Final summary
    if (callbacks.on_phase_change) callbacks.on_phase_change("done");
}

// ─── Step Execution ───────────────────────────────────────────

void Orchestrator::execute_step(TaskStep& step, OrchestratorCallbacks& callbacks) {
    if (step.action == "read") {
        execute_read(step, callbacks);
    } else if (step.action == "write") {
        execute_write(step, callbacks);
    } else if (step.action == "bash") {
        execute_bash(step, callbacks);
    } else {
        step.status = "failed";
        step.result = "Unknown action: " + step.action;
    }
}

void Orchestrator::execute_read(TaskStep& step, OrchestratorCallbacks& callbacks) {
    auto content = read_file(step.target);
    if (content.empty()) {
        step.status = "done";
        step.result = "(file not found or empty: " + step.target + ")";
        return;
    }

    // Update the working set with actual content and expand it
    if (expander_.has_item(step.target)) {
        expander_.remove_item(step.target);
    }
    expander_.add_item(step.target, "file", step.target, content);
    expander_.expand_item(step.target);

    step.status = "done";
    step.result = content;

    if (callbacks.on_stream_text) {
        callbacks.on_stream_text(content);
    }
}

void Orchestrator::execute_write(TaskStep& step, OrchestratorCallbacks& callbacks) {
    auto code = llm_generate_code(step.target, step.description, original_request_);

    if (code.empty()) {
        step.status = "failed";
        step.result = "Code generation returned empty";
        return;
    }

    // Write to disk
    auto full_path = abs_path(step.target);
    auto parent = fs::path(full_path).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    std::ofstream f(full_path);
    if (!f.is_open()) {
        step.status = "failed";
        step.result = "Cannot write to " + full_path;
        return;
    }
    f << code;
    f.close();

    // Update cache and working set
    file_cache_[step.target] = code;
    if (expander_.has_item(step.target)) {
        expander_.remove_item(step.target);
    }
    expander_.add_item(step.target, "file", step.target + " [written]", code);
    expander_.expand_item(step.target);

    step.status = "done";
    step.result = "Wrote " + std::to_string(code.size()) + " bytes to " + step.target;

    if (callbacks.on_stream_text) {
        callbacks.on_stream_text(code);
    }

    log::info("Wrote " + std::to_string(code.size()) + " bytes to " + step.target);
}

void Orchestrator::execute_bash(TaskStep& step, OrchestratorCallbacks& callbacks) {
    // Run in project root
    std::string cmd = "cd " + index_.project_root() + " && " + step.target;

    auto* agent = agents_.get("bash");
    if (!agent) {
        step.status = "failed";
        step.result = "Bash agent not found";
        return;
    }

    AgentContext ctx;
    ctx.plan.tool_arguments["command"] = cmd;

    auto result = agent->execute(ctx, *provider_, callbacks.on_stream_text);

    step.result = result.tool_result.output;
    step.status = result.tool_result.success ? "done" : "failed";

    if (!result.tool_result.success) {
        log::warn("Bash failed: " + step.result.substr(0, 200));
    }
}

// ─── Error Recovery ───────────────────────────────────────────

bool Orchestrator::attempt_fix(TaskStep& failed_step, OrchestratorCallbacks& callbacks) {
    // Parse the error to identify the file that needs fixing
    // Compiler errors look like: "file.hpp:22:24: error: ..."
    TaskStep* target_step = nullptr;
    // Parse error output line by line to find the actual error (not "In file included from")
    std::regex err_file_re(R"((\S+\.\w+):(\d+):(\d+): error:)");
    std::string err_file;
    std::istringstream err_stream(failed_step.result);
    std::string err_line;
    while (std::getline(err_stream, err_line)) {
        std::smatch match;
        if (std::regex_search(err_line, match, err_file_re)) {
            err_file = match[1].str();
            break;
        }
    }

    if (!err_file.empty()) {
        // Strip path prefix to get relative name
        auto slash = err_file.rfind('/');
        if (slash != std::string::npos) err_file = err_file.substr(slash + 1);

        // Find the write step for this file
        for (auto& s : steps_) {
            if (&s == &failed_step) break;
            if (s.action == "write" && s.status == "done") {
                auto step_file = s.target;
                auto sl = step_file.rfind('/');
                if (sl != std::string::npos) step_file = step_file.substr(sl + 1);
                if (step_file == err_file) {
                    target_step = &s;
                    break;
                }
            }
        }
    }

    // Fallback: use the last write step
    if (!target_step) {
        for (auto& s : steps_) {
            if (&s == &failed_step) break;
            if (s.action == "write" && s.status == "done") target_step = &s;
        }
    }

    if (!target_step) return false;

    log::info("Attempting to fix " + target_step->target + " based on error");
    auto* last_write = target_step;

    // Ask the model to fix the code
    auto current_code = read_file(last_write->target);
    std::string system = R"(You are fixing a compilation/runtime error. Output ONLY the complete corrected file content.
No markdown fences. No explanations. Just the corrected code.

Rules:
- Fix the error described below.
- Output the COMPLETE file, not just the fix.
- Use #include "file.hpp" (quotes) for local headers.)";

    system += "\n\n## File: " + last_write->target;
    system += "\n## Error:\n```\n" + failed_step.result + "\n```\n";
    system += "\n## Current code:\n```\n" + current_code + "\n```\n";

    // Add reference files
    for (const auto& [p, c] : file_cache_) {
        if (p == last_write->target || c.empty()) continue;
        system += "\n## Reference: " + p + "\n```\n" + c + "\n```\n";
    }

    auto fixed = llm_complete(system, "Fix the error and output the complete corrected file.", 8192);

    fixed = strip_fences(fixed);

    if (fixed.empty()) return false;

    // Write the fix
    auto full_path = abs_path(last_write->target);
    std::ofstream f(full_path);
    f << fixed;
    f.close();

    file_cache_[last_write->target] = fixed;
    log::info("Applied fix to " + last_write->target + " (" + std::to_string(fixed.size()) + " bytes)");

    if (callbacks.on_stream_text) {
        callbacks.on_stream_text("\n[FIX] Corrected " + last_write->target + "\n");
    }

    return true;
}

// ─── Unused but kept for interface compat ─────────────────────

MetadataRecord Orchestrator::run_cycle(const std::string&, OrchestratorCallbacks&) { return {}; }
ActionPlan Orchestrator::run_phase1(const std::string&) { return {}; }
AgentResult Orchestrator::run_phase2(const ActionPlan&, StreamCallback) { return {}; }
MetadataRecord Orchestrator::finalize_cycle(const ActionPlan&, const AgentResult&) { return {}; }
bool Orchestrator::should_review_timeline() const { return false; }
void Orchestrator::trigger_timeline_review(OrchestratorCallbacks) {}

} // namespace cortex
