/**
 * @file session.cpp
 * @brief Session implementation — the core orchestration engine.
 *
 * A Session handles a single user request from start to finish:
 *   1. Decomposes the request into discrete tasks (using LLM)
 *   2. Executes each task: read files, generate code, run commands
 *   3. Handles errors: analyzes failures, generates fixes, retries
 *   4. Tracks statistics: LLM calls, tokens, context usage
 *
 * The Session uses the ContextExpander for working set management
 * (large projects) and direct LLM calls for small projects.
 *
 * Key design decisions:
 * - Small projects (≤8 files): all files pre-loaded, single LLM call per step
 * - Large projects (>8 files): expansion loop with @read/@hide
 * - Task classification: keyword-based with file extension detection
 * - Error recovery: meta-agent analyzes errors, targets correct file
 */
#include "cortex/core/session.hpp"
#include "cortex/util/uuid.hpp"
#include "cortex/util/log.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <thread>

namespace cortex {

namespace fs = std::filesystem;

// ─── Construction ─────────────────────────────────────────────

Session::Session(std::unique_ptr<IProvider> provider,
                 AgentRegistry& agents,
                 const std::string& project_root,
                 const ProviderConfig& provider_config)
    : provider_(std::move(provider))
    , provider_config_(provider_config)
    , agents_(agents)
    , project_root_(project_root)
    , expander_(provider_config.max_tokens > 0 ? 128000 : 128000)
{
    // Register expansion action resolvers
    expander_.register_action("read", [this](const std::string& arg) -> std::optional<std::string> {
        return read_file(arg);
    });

    expander_.register_action("search", [this](const std::string& arg) -> std::optional<std::string> {
        auto* agent = agents_.get("grep");
        if (!agent) return std::nullopt;
        Json args;
        args["pattern"] = arg;
        args["path"] = project_root_;
        AgentContext ctx;
        ctx.plan.tool_arguments = args;
        auto result = agent->execute(ctx, *provider_, nullptr);
        return result.tool_result.output.empty() ? "No matches." : result.tool_result.output;
    });

    expander_.register_action("run", [this](const std::string& arg) -> std::optional<std::string> {
        return do_bash(arg);
    });

    // @note(text) — record a finding to the session notebook
    expander_.register_action("note", [this](const std::string& arg) -> std::optional<std::string> {
        auto id = notebook_.add(arg, "round " + std::to_string(round_counter_));
        log::info("Note #" + std::to_string(id) + ": " + arg.substr(0, 80));
        return "Note #" + std::to_string(id) + " recorded.";
    });

    // @markoff(number) — mark a task as complete
    expander_.register_action("markoff", [this](const std::string& arg) -> std::optional<std::string> {
        auto* task = tasks_.find(arg);
        if (!task) return "Task " + arg + " not found.";
        tasks_.update_status(arg, "done");
        log::info("Marked off task " + arg + ": " + task->title);
        return "Task " + arg + " marked as done.";
    });

    // Load prompt templates from JSON file
    std::string template_path = project_root_ + "/prompts/templates.json";
    if (!templates_.load(template_path)) {
        // Try the build-time prompts directory
        #ifdef CORTEX_PROMPTS_DIR
        templates_.load(std::string(CORTEX_PROMPTS_DIR) + "/templates.json");
        #endif
    }

    // Register project files in the working set (hidden, lazy-loaded)
    try {
        for (const auto& entry : fs::recursive_directory_iterator(project_root_,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            auto rel = fs::relative(entry.path(), project_root_).string();
            if (rel[0] == '.' || rel.find("/.") != std::string::npos) continue;
            if (rel.find("build/") == 0 || rel.find("node_modules/") == 0) continue;
            if (rel.find(".cortex/") == 0) continue;
            auto size = fs::file_size(entry.path());
            expander_.add_item(rel, "file",
                rel + " (" + std::to_string(size) + "B)", "");
        }
    } catch (const fs::filesystem_error&) {}

    log::info("Session created: " + std::to_string(expander_.stats().total_items) +
              " files indexed in " + project_root_);
}

// ─── File Helpers ─────────────────────────────────────────────

std::string Session::abs_path(const std::string& rel_path) const {
    if (!rel_path.empty() && rel_path[0] == '/') return rel_path;
    return (fs::path(project_root_) / rel_path).string();
}

std::string Session::read_file(const std::string& rel_path) {
    if (file_cache_.count(rel_path) && !file_cache_[rel_path].empty()) {
        return file_cache_[rel_path];
    }
    auto p = abs_path(rel_path);
    std::ifstream f(p);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    auto content = ss.str();
    file_cache_[rel_path] = content;
    return content;
}

// ─── LLM Helpers ──────────────────────────────────────────────

std::string Session::llm_call(const std::string& system_prompt,
                               const std::string& user_message,
                               int max_tokens) {
    ProviderConfig config = provider_config_;
    config.temperature = 0.0;
    config.max_tokens = max_tokens;

    std::vector<Message> messages;
    if (user_message.empty()) {
        // Single prompt mode (from expansion loop)
        messages.push_back(Message::user(system_prompt));
    } else {
        messages.push_back(Message::system(system_prompt));
        messages.push_back(Message::user(user_message));
    }

    // Retry on transient errors (server busy, proxy errors)
    for (int attempt = 0; attempt < 3; attempt++) {
        stats_.total_llm_calls++;
        auto completion = provider_->complete(messages, config);
        stats_.total_tokens_in += completion.usage.input_tokens;
        stats_.total_tokens_out += completion.usage.output_tokens;

        std::string text;
        for (const auto& block : completion.content) {
            if (block.type == "text") text += block.text;
        }

        // Check for server errors
        if (text.find("API Error") == 0 || text.find("<!DOCTYPE") != std::string::npos ||
            text.find("Connection failed") == 0 || text.find("Error:") == 0) {
            log::warn("LLM call failed (attempt " + std::to_string(attempt+1) + "): " + text.substr(0, 80));
            if (attempt < 2) {
                log::info("Retrying in 3 seconds...");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }
        }

        return text;
    }
    return "";
}

std::string Session::compile_prompt(const std::string& system_prompt) const {
    auto ctx = expander_.compile();
    std::ostringstream ss;
    ss << system_prompt << "\n\n";
    ss << ctx.index_block << "\n";
    if (!ctx.expanded_block.empty()) {
        ss << ctx.expanded_block << "\n";
    }
    ss << ctx.actions_block << "\n";
    return ss.str();
}

std::string Session::llm_with_expansion(const std::string& system_prompt,
                                         const std::string& task_description,
                                         int max_tokens,
                                         int max_rounds) {
    auto llm_fn = [this](const std::string& prompt, int tokens) -> std::string {
        return llm_call(prompt, "", tokens);
    };

    auto result = expander_.run(system_prompt, task_description, llm_fn, max_tokens, max_rounds);

    // Update stats
    stats_.context_reads = result.final_stats.reads_this_session;
    stats_.context_hides = result.final_stats.hides_this_session;
    if (result.final_stats.peak_expanded_tokens > stats_.peak_expanded_tokens) {
        stats_.peak_expanded_tokens = result.final_stats.peak_expanded_tokens;
    }

    // Log per-round stats
    for (const auto& rs : result.round_stats) {
        log::info("  Round " + std::to_string(rs.round) + ": " +
                  std::to_string(rs.expanded_items) + " expanded (" +
                  std::to_string(rs.expanded_tokens) + " tok), " +
                  std::to_string(rs.hidden_items) + " hidden (" +
                  std::to_string(rs.hidden_tokens) + " tok)");
    }

    return result.output;
}

// ─── Strip markdown fences ────────────────────────────────────

static std::string strip_fences(const std::string& input) {
    auto s = input;
    while (!s.empty() && (s.front() == '\n' || s.front() == '\r' || s.front() == ' '))
        s.erase(s.begin());

    // Wrapped in ```
    if (s.find("```") == 0) {
        auto nl = s.find('\n');
        auto last = s.rfind("```");
        if (nl != std::string::npos && last > nl)
            s = s.substr(nl + 1, last - nl - 1);
    }

    // Trailing fences
    while (true) {
        auto last = s.rfind("```");
        if (last == std::string::npos) break;
        auto after = s.substr(last + 3);
        bool garbage = true;
        for (char c : after) {
            if (c != '\n' && c != '\r' && c != ' ' && c != '`') {
                auto trimmed = after;
                while (!trimmed.empty() && (trimmed.front() == '\n' || trimmed.front() == ' '))
                    trimmed.erase(trimmed.begin());
                auto nl = trimmed.find('\n');
                if (nl == std::string::npos || nl >= 10) garbage = false;
                break;
            }
        }
        if (garbage) s = s.substr(0, last);
        else break;
    }

    // Multiple blocks — extract largest
    if (s.find("```") != std::string::npos) {
        std::string best;
        size_t pos = 0;
        while (pos < s.size()) {
            auto fs = s.find("```", pos);
            if (fs == std::string::npos) break;
            auto cs = s.find('\n', fs);
            if (cs == std::string::npos) break;
            cs++;
            auto fe = s.find("```", cs);
            if (fe == std::string::npos) {
                auto block = s.substr(cs);
                if (block.size() > best.size()) best = block;
                break;
            }
            auto block = s.substr(cs, fe - cs);
            if (block.size() > best.size()) best = block;
            pos = fe + 3;
        }
        if (!best.empty() && best.size() > s.size() / 4) s = best;
    }

    while (s.size() > 1 && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r'))
        s.pop_back();
    s += '\n';
    return s;
}

// ─── Main Entry Point ─────────────────────────────────────────

void Session::run(const std::string& user_request, SessionCallbacks cb) {
    auto start = std::chrono::steady_clock::now();
    original_request_ = user_request;
    round_counter_ = 0;

    log::info("=== Session start: " + user_request.substr(0, 80) + " ===");

    try {
        // Phase 1: Break down the request
        if (cb.on_phase) cb.on_phase("breaking down task");
        phase_breakdown(user_request);

        if (tasks_.total_count() == 0) {
            if (cb.on_error) cb.on_error("Failed to break down task");
            return;
        }

        if (cb.on_status) cb.on_status(tasks_.render());

        // Phase 2: Execute tasks
        phase_execute(cb);
    } catch (const std::exception& e) {
        log::error("Session error: " + std::string(e.what()));
        if (cb.on_error) cb.on_error(std::string("Error: ") + e.what());
    } catch (...) {
        log::error("Session unknown error");
        if (cb.on_error) cb.on_error("Unknown error occurred");
    }

    // Final stats
    auto end = std::chrono::steady_clock::now();
    stats_.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    stats_.tasks_completed = tasks_.completed_count();
    stats_.tasks_failed = tasks_.failed_count();

    log::info("=== Session complete ===");
    log::info(stats_summary());

    if (cb.on_status) cb.on_status("\n" + stats_summary());
}

// ─── Phase 1: Task Breakdown ─────────────────────────────────

void Session::phase_breakdown(const std::string& request) {
    // Build project context
    std::ostringstream project_ctx;
    project_ctx << "## Project: " << project_root_ << "\n";
    project_ctx << "## Files:\n";
    auto st = expander_.stats();
    for (int i = 0; i < st.total_items && i < 50; i++) {
        // Just use the index render
    }
    project_ctx << expander_.compile().index_block;

    auto system = PromptEngine::task_breakdown(project_ctx.str());

    log::info("Phase 1: Breaking down task...");

    std::string response;
    auto file_count = expander_.stats().total_items;

    if (file_count <= 8) {
        // Small project: pre-read all files, single LLM call (fast path)
        std::string all_files;
        int files_loaded = 0;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(project_root_,
                    fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto rel = fs::relative(entry.path(), project_root_).string();
                if (rel[0] == '.' || rel.find("build/") == 0 || rel.find(".cortex/") == 0) continue;
                auto content = read_file(rel);
                if (!content.empty()) {
                    all_files += "\n## " + rel + ":\n" + content + "\n";
                    files_loaded++;
                }
            }
        } catch (...) {}
        if (!all_files.empty()) system += "\n## File contents:" + all_files;
        response = llm_call(system, request, 2048);
    } else {
        // Large project: use expansion loop — model explores with @read/@hide
        log::info("Large project (" + std::to_string(file_count) +
                  " files) — using expansion loop for selective exploration");
        response = llm_with_expansion(system, request, 2048, 8);
    }

    log::debug("Breakdown response: " + response.substr(0, 500));

    // Extract JSON array — handle multiple response formats
    std::string json_str;

    auto arr_start = response.find('[');
    auto arr_end = response.rfind(']');
    if (arr_start != std::string::npos && arr_end != std::string::npos && arr_end > arr_start) {
        json_str = response.substr(arr_start, arr_end - arr_start + 1);
    } else {
        // Model returned objects without array wrapper — try to salvage
        auto obj_start = response.find('{');
        if (obj_start != std::string::npos) {
            // Try wrapping and parsing progressively shorter substrings
            auto text = response.substr(obj_start);
            bool parsed = false;
            // Find each } and try to parse [text up to }]
            for (size_t pos = text.size(); pos > 0; pos--) {
                if (text[pos-1] == '}') {
                    auto candidate = "[" + text.substr(0, pos) + "]";
                    try {
                        auto test = Json::parse(candidate);
                        if (test.is_array() && !test.empty()) {
                            json_str = candidate;
                            parsed = true;
                            log::info("Salvaged " + std::to_string(test.size()) + " tasks from non-array response");
                            break;
                        }
                    } catch (...) {}
                }
            }
            if (!parsed) {
                log::error("Could not parse task breakdown from: " + text.substr(0, 200));
                return;
            }
        } else {
            log::error("No JSON in breakdown: " + response.substr(0, 200));
            return;
        }
    }

    try {
        auto json = Json::parse(json_str);
        tasks_.load_from_json(json);
        log::info("Task breakdown: " + std::to_string(tasks_.total_count()) + " tasks");
        for (const auto& t : tasks_.tasks()) {
            log::info("  " + t.number + " " + t.title);
            for (const auto& st : t.subtasks) {
                log::info("    " + st.number + " " + st.title);
            }
        }
    } catch (const std::exception& e) {
        log::error("Failed to parse task breakdown: " + std::string(e.what()));
    }
}

// ─── Phase 2: Task Execution ─────────────────────────────────

void Session::phase_execute(SessionCallbacks& cb) {
    int review_interval = 4;  // Review progress every N tasks
    int tasks_since_review = 0;

    int max_total_attempts = 100;  // Safety: never loop more than this
    int total_attempts = 0;

    while (!tasks_.all_done() && total_attempts < max_total_attempts) {
        auto next = tasks_.next_pending();
        if (!next) break;

        auto task_number = next->number;
        tasks_.update_status(task_number, "active");
        auto task = *next;  // Copy for working

        if (cb.on_phase) cb.on_phase("task " + task.number + "/" +
                                      std::to_string(tasks_.total_count()) + ": " + task.title);
        // Send full task list so TUI sidebar shows checkmarks
        if (cb.on_status) cb.on_status(tasks_.render());

        log::info("Executing task #" + task.number + ": " + task.title +
                  " (attempt " + std::to_string(task.attempts + 1) + ")");
        total_attempts++;

        bool success = execute_task(task, cb);

        if (success) {
            tasks_.update_status(task_number, "done", task.result);
            action_log_.push_back("OK #" + task.number + " " + task.title);
            // Update task list in TUI after each completion
            if (cb.on_status) cb.on_status(tasks_.render());
        } else {
            int attempts = task.attempts + 1;
            if (attempts >= task.max_attempts) {
                tasks_.update_status(task_number, "failed", task.result);
                action_log_.push_back("FAIL #" + task.number + " " + task.title);
                if (cb.on_status) cb.on_status(tasks_.render());
                log::warn("Task #" + task.number + " failed after " +
                          std::to_string(attempts) + " attempts");
            } else {
                tasks_.update_status(task_number, "failed", task.result);
                action_log_.push_back("FAIL " + task.number + " " + task.title);
                if (cb.on_status) cb.on_status(tasks_.render());
            }
        }

        tasks_since_review++;

        // Periodic progress review
        if (tasks_since_review >= review_interval && !tasks_.all_done()) {
            review_progress(cb);
            tasks_since_review = 0;
        }
    }
}

// ─── Execute Single Task ──────────────────────────────────────

bool Session::execute_task(Task& task, SessionCallbacks& cb) {
    // Determine action type from the task title/description
    // Use the task's type field for classification (set by decomposition LLM)
    // Fallback to keyword-based if type is empty
    std::string task_type = task.type;

    // Extract target file from task
    std::string target_file;
    if (!task.files.empty()) target_file = task.files[0];
    if (target_file.empty()) {
        std::regex file_re(R"((\S+\.(?:cpp|hpp|h|c|py|js|ts|sh|txt|json|md|rs|go|java|rb|css|html))\b)");
        std::smatch match;
        if (std::regex_search(task.title, match, file_re)) target_file = match[1].str();
        else if (std::regex_search(task.description, match, file_re)) target_file = match[1].str();
    }

    // If no type from decomposition, classify by keywords
    if (task_type.empty()) {
        auto combined = task.title + " " + task.description;
        std::transform(combined.begin(), combined.end(), combined.begin(), ::tolower);
        if (combined.find("compile") != std::string::npos || combined.find("run ") != std::string::npos)
            task_type = "bash";
        else if (combined.find("create ") != std::string::npos || combined.find("write ") != std::string::npos ||
                 combined.find("add ") != std::string::npos || combined.find("modify ") != std::string::npos ||
                 combined.find("fix ") != std::string::npos || combined.find("update ") != std::string::npos)
            task_type = "write";
        else if (combined.find("read ") != std::string::npos || combined.find("examine") != std::string::npos)
            task_type = "read";
        else
            task_type = "write";  // Default to write if we have a file
    }

    // For bash tasks, the command should come from the task's JSON
    // (set by decomposition LLM in the "command" field)
    // Fallback: extract from title/description/original request
    std::string bash_cmd;
    // The task_manager stores command in the description for bash tasks
    // since our Task struct doesn't have a separate command field yet.
    // For now, check if description looks like a command.
    if (task_type == "bash") {
        // Check task title for a direct command
        auto& title = task.title;
        auto& desc = task.description;
        // If title IS the command (starts with g++, make, python, ./, bash, etc.)
        std::regex cmd_re(R"(^(g\+\+\s.*|make\b.*|python3?\s+.*|\.\/\S.*|bash\s+.*|cmake\s.*|npm\s+.*|cargo\s+.*)$)");
        std::smatch match;
        if (std::regex_match(title, match, cmd_re)) {
            bash_cmd = title;
        } else if (std::regex_match(desc, match, cmd_re)) {
            bash_cmd = desc;
        } else {
            // Search within title/desc for a command
            std::regex search_re(R"(\b(g\+\+\s+[^\n]+|make\b|python3?\s+\S+|\.\/\S+|bash\s+\S+|cmake\s+[^\n]+))");
            if (std::regex_search(title, match, search_re)) bash_cmd = match[1].str();
            else if (std::regex_search(desc, match, search_re)) bash_cmd = match[1].str();
            else if (std::regex_search(original_request_, match, search_re)) bash_cmd = match[1].str();
        }
        // Fallback: .sh file → bash it, no extension → run it
        if (bash_cmd.empty() && !target_file.empty()) {
            if (target_file.size() > 3 && target_file.substr(target_file.size()-3) == ".sh")
                bash_cmd = "bash " + target_file;
            else if (target_file.find('.') == std::string::npos)
                bash_cmd = "./" + target_file;
        }
    }

    log::debug("Task dispatch: type=" + task_type + " file=" + target_file + " cmd=" + bash_cmd);

    try {
        if (task_type == "bash" && !bash_cmd.empty()) {
            auto output = do_bash(bash_cmd);
            task.result = output;
            // Check if the command failed
            bool failed = output.find("error:") != std::string::npos ||
                         output.find("Error") != std::string::npos ||
                         output.find("FAILED") != std::string::npos ||
                         output.find("not found") != std::string::npos;
            if (failed) {
                // Try to fix
                log::info("Command failed, attempting fix...");
                if (try_fix_error(output, cb)) {
                    // Retry the command
                    auto retry_output = do_bash(bash_cmd);
                    task.result = retry_output;
                    bool retry_failed = retry_output.find("error:") != std::string::npos;
                    if (cb.on_stream) cb.on_stream(retry_output);
                    return !retry_failed;
                }
                if (cb.on_stream) cb.on_stream(output);
                return false;
            }
            if (cb.on_stream) cb.on_stream(output);
            return true;
        }

        if (task_type == "write" && !target_file.empty()) {
            auto code = do_write(target_file, task.description.empty() ? task.title : task.description);
            if (code.empty()) {
                task.result = "Code generation returned empty";
                return false;
            }
            task.result = "Wrote " + std::to_string(code.size()) + " bytes to " + target_file;
            if (cb.on_stream) cb.on_stream(code);
            return true;
        }

        if (task_type == "read" && !target_file.empty()) {
            auto content = do_read(target_file);
            task.result = content.empty() ? "File not found" : "Read " + target_file;
            if (cb.on_stream) cb.on_stream(content);
            return !content.empty();
        }

        // Fallback: if we have a bash command candidate, try it
        if (!bash_cmd.empty()) {
            auto output = do_bash(bash_cmd);
            task.result = output;
            if (cb.on_stream) cb.on_stream(output);
            return true;
        }

        // Truly unclassifiable — skip it
        log::warn("Could not classify task: " + task.title + " — skipping");
        task.result = "Skipped (unclassifiable)";
        return true;  // Don't retry

    } catch (const std::exception& e) {
        task.result = std::string("Exception: ") + e.what();
        log::error("Task exception: " + task.result);
        return false;
    }
}

// ─── Task Actions ─────────────────────────────────────────────

std::string Session::do_read(const std::string& path) {
    auto content = read_file(path);
    if (content.empty()) return "";

    // Add to working set and expand
    if (expander_.has_item(path)) expander_.remove_item(path);
    expander_.add_item(path, "file", path, content);
    expander_.expand_item(path);

    stats_.context_reads++;
    log::info("Read " + path + " (" + std::to_string(content.size()) + " bytes)");
    return content;
}

std::string Session::do_write(const std::string& path, const std::string& description) {
    // Read existing file if it exists
    auto existing = read_file(path);

    // Build code gen prompt
    auto system = PromptEngine::code_gen(path, description, original_request_);

    if (!existing.empty()) {
        // Detect if this is a bug fix task
        auto desc_lower = description;
        std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
        bool is_fix = desc_lower.find("fix") != std::string::npos ||
                      desc_lower.find("bug") != std::string::npos ||
                      desc_lower.find("error") != std::string::npos;

        system += "\n## Current content:\n" + existing + "\n";
        if (is_fix) {
            system += "\nIMPORTANT: This is a BUG FIX task. The code above has a bug. ";
            system += "You MUST find and fix the bug. Do NOT copy the code unchanged. ";
            system += "If there are comments indicating the bug (like '// BUG:'), apply the fix they describe. ";
            system += "Output the COMPLETE corrected file.\n";
        } else {
            system += "\nYou MUST modify the above file according to the task. ";
            system += "Apply ALL changes described. Do NOT return the file unchanged. ";
            system += "Output the COMPLETE updated file.\n";
        }
    } else {
        system += "\nNew file. Output the complete content.\n";
    }

    log::info("Generating code for " + path);
    round_counter_++;

    std::string code;
    auto file_count = expander_.stats().total_items;

    if (file_count <= 8) {
        // Small project: dump all references, single call
        for (const auto& [p, c] : file_cache_) {
            if (p == path || c.empty()) continue;
            system += "\n## Reference: " + p + "\n" + c + "\n";
        }
        code = llm_call(system, "Generate the complete file content.", 8192);
    } else {
        // Large project: use expansion loop — model @reads what it needs
        // The working set has all project files available to @read
        code = llm_with_expansion(system,
            "Generate the complete file content for " + path + ". "
            "Use @read(file) to load any reference files you need. "
            "Use @hide(file) when you're done with a reference file to free context. "
            "When ready, output ONLY the raw code with no action tags.",
            8192, 6);
    }
    code = strip_fences(code);

    if (code.empty()) return "";

    // Sanity check: reject if the LLM returned an error instead of code
    if (code.find("API Error") == 0 || code.find("<!DOCTYPE") != std::string::npos ||
        code.find("<html") != std::string::npos || code.find("Error:") == 0 ||
        code.find("Connection failed") == 0) {
        log::error("LLM returned error instead of code: " + code.substr(0, 100));
        return "";
    }

    // Write to disk
    auto full_path = abs_path(path);
    auto parent = fs::path(full_path).parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    std::ofstream f(full_path);
    if (!f.is_open()) return "";
    f << code;
    f.close();

    // Update cache and working set
    file_cache_[path] = code;
    if (expander_.has_item(path)) expander_.remove_item(path);
    expander_.add_item(path, "file", path + " [written]", code);
    expander_.expand_item(path);

    log::info("Wrote " + std::to_string(code.size()) + " bytes to " + path);
    return code;
}

std::string Session::do_bash(const std::string& command) {
    auto* agent = agents_.get("bash");
    if (!agent) return "bash agent not found";

    Json args;
    args["command"] = "cd " + project_root_ + " && " + command;
    AgentContext ctx;
    ctx.plan.tool_arguments = args;

    auto result = agent->execute(ctx, *provider_, nullptr);
    return result.tool_result.output;
}

// ─── Error Recovery ───────────────────────────────────────────

bool Session::try_fix_error(const std::string& error_output, SessionCallbacks& cb) {
    // Use meta-agent to analyze the error
    auto analysis_prompt = PromptEngine::error_analysis(error_output);

    // Add all cached files for reference
    for (const auto& [p, c] : file_cache_) {
        if (!c.empty()) {
            analysis_prompt += "\n## " + p + ":\n" + c + "\n";
        }
    }

    auto response = llm_call(analysis_prompt, "Analyze this error.", 512);

    // Parse the analysis
    auto json_start = response.find('{');
    auto json_end = response.rfind('}');
    if (json_start == std::string::npos) return false;

    try {
        auto analysis = Json::parse(response.substr(json_start, json_end - json_start + 1));
        auto target_file = analysis.value("file", "");
        auto fix_desc = analysis.value("fix_description", "");

        if (target_file.empty() || fix_desc.empty()) return false;

        log::info("Error analysis: fix " + target_file + " — " + fix_desc);

        // Read the current file
        auto current = read_file(target_file);
        if (current.empty()) return false;

        // Generate fix
        auto fix_prompt = PromptEngine::error_fix(target_file, error_output);
        fix_prompt += "\n## Current code:\n" + current + "\n";
        fix_prompt += "\n## Fix: " + fix_desc + "\n";

        // Add reference files
        for (const auto& [p, c] : file_cache_) {
            if (p == target_file || c.empty()) continue;
            fix_prompt += "\n## Reference: " + p + "\n" + c + "\n";
        }

        auto fixed = llm_call(fix_prompt, "Output the corrected file.", 8192);
        fixed = strip_fences(fixed);

        if (fixed.empty()) return false;

        // Write the fix
        auto full_path = abs_path(target_file);
        std::ofstream f(full_path);
        if (!f.is_open()) return false;
        f << fixed;
        f.close();

        file_cache_[target_file] = fixed;
        stats_.errors_fixed++;

        log::info("Applied fix to " + target_file + " (" + std::to_string(fixed.size()) + " bytes)");
        if (cb.on_stream) cb.on_stream("\n[FIX] Corrected " + target_file + "\n");

        return true;
    } catch (const std::exception& e) {
        log::warn("Error analysis failed: " + std::string(e.what()));
        return false;
    }
}

// ─── Meta-Agent: Progress Review ──────────────────────────────

void Session::review_progress(SessionCallbacks& cb) {
    log::info("Progress review...");

    auto system = PromptEngine::progress_review();
    system += "\n\n" + tasks_.render();
    system += "\n## Action Log:\n";
    for (const auto& a : action_log_) system += "  " + a + "\n";

    auto response = llm_call(system, "Review progress and suggest next steps.", 512);

    auto json_start = response.find('{');
    auto json_end = response.rfind('}');
    if (json_start == std::string::npos) return;

    try {
        auto review = Json::parse(response.substr(json_start, json_end - json_start + 1));

        if (review.contains("should_retry_failed") && review["should_retry_failed"].is_array()) {
            for (const auto& id : review["should_retry_failed"]) {
                auto retry_num = id.get<std::string>();
                tasks_.update_status(retry_num, "pending");
                log::info("  Retrying task " + retry_num);
            }
        }

        auto suggestion = review.value("suggestion", "");
        if (!suggestion.empty()) {
            log::info("  Review suggestion: " + suggestion);
        }
    } catch (...) {}
}

// ─── Stats ────────────────────────────────────────────────────

std::string Session::stats_summary() const {
    std::ostringstream ss;
    ss << "╔══════════════════════════════════════╗\n";
    ss << "║         Session Statistics           ║\n";
    ss << "╠══════════════════════════════════════╣\n";
    ss << "║  Tasks: " << stats_.tasks_completed << " done, "
       << stats_.tasks_failed << " failed / "
       << tasks_.total_count() << " total\n";
    ss << "║  LLM Calls: " << stats_.total_llm_calls << "\n";
    ss << "║  Tokens: " << stats_.total_tokens_in << " in, "
       << stats_.total_tokens_out << " out\n";
    ss << "║  Errors Fixed: " << stats_.errors_fixed << "\n";
    ss << "║  Context: " << stats_.context_reads << " reads, "
       << stats_.context_hides << " hides\n";
    ss << "║  Peak Expanded: " << stats_.peak_expanded_tokens << " tokens\n";
    ss << "║  Time: " << static_cast<int>(stats_.total_time_ms / 1000) << "s\n";
    ss << "╚══════════════════════════════════════╝\n";
    return ss.str();
}

} // namespace cortex
