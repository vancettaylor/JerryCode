#include "cortex/core/context_expander.hpp"
#include "cortex/util/log.hpp"
#include <sstream>
#include <regex>
#include <algorithm>
#include <iomanip>

namespace cortex {

// ─── Construction ─────────────────────────────────────────────

ContextExpander::ContextExpander(int context_window_max)
    : context_window_max_(context_window_max) {}

int ContextExpander::estimate_tokens(const std::string& text) const {
    return static_cast<int>(text.size()) / 4;
}

void ContextExpander::register_action(const std::string& action, ActionResolver resolver) {
    resolvers_[action] = std::move(resolver);
}

// ─── Working Set Management ───────────────────────────────────

void ContextExpander::add_item(const std::string& id, const std::string& kind,
                                const std::string& label, const std::string& content) {
    WorkingSetItem item;
    item.id = id;
    item.kind = kind;
    item.label = label;
    item.content = content;
    item.tokens = estimate_tokens(content);
    item.expanded = false;
    item.last_accessed = std::chrono::steady_clock::now();
    items_[id] = std::move(item);
}

void ContextExpander::expand_item(const std::string& id) {
    auto it = items_.find(id);
    if (it == items_.end()) return;
    it->second.expanded = true;
    it->second.times_expanded++;
    it->second.last_accessed = std::chrono::steady_clock::now();
    reads_this_session_++;
}

void ContextExpander::hide_item(const std::string& id) {
    auto it = items_.find(id);
    if (it == items_.end()) return;
    it->second.expanded = false;
    it->second.times_hidden++;
    hides_this_session_++;
}

void ContextExpander::remove_item(const std::string& id) {
    items_.erase(id);
}

void ContextExpander::clear() {
    items_.clear();
    reads_this_session_ = 0;
    hides_this_session_ = 0;
    peak_expanded_tokens_ = 0;
}

bool ContextExpander::has_item(const std::string& id) const {
    return items_.count(id) > 0;
}

bool ContextExpander::is_expanded(const std::string& id) const {
    auto it = items_.find(id);
    return it != items_.end() && it->second.expanded;
}

// ─── Context Compilation ──────────────────────────────────────
// This is the core of the system: each round, we compile a FRESH
// prompt from the working set. No chat history. No accumulated junk.

ContextExpander::CompiledContext ContextExpander::compile() const {
    CompiledContext ctx;

    // Build the index
    std::ostringstream idx;
    idx << "## Context Index\n";
    idx << "Items in your working set. [+] = expanded (in context), [-] = hidden (available).\n\n";

    // Sort items by kind for readability
    std::vector<const WorkingSetItem*> sorted;
    for (const auto& [id, item] : items_) {
        sorted.push_back(&item);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) {
        if (a->kind != b->kind) return a->kind < b->kind;
        return a->id < b->id;
    });

    for (const auto* item : sorted) {
        idx << "  " << (item->expanded ? "[+]" : "[-]") << " "
            << item->id << "  (" << item->tokens << " tokens)";
        if (!item->label.empty() && item->label != item->id) {
            idx << "  — " << item->label;
        }
        idx << "\n";
    }

    ctx.index_block = idx.str();

    // Build expanded content
    std::ostringstream exp;
    for (const auto* item : sorted) {
        if (!item->expanded) continue;
        exp << "\n─── " << item->id << " ───\n";
        exp << item->content;
        if (!item->content.empty() && item->content.back() != '\n') exp << "\n";
        exp << "─── end " << item->id << " ───\n";
    }
    ctx.expanded_block = exp.str();

    // Compute budget info
    int expanded_tokens = 0;
    int hidden_tokens = 0;
    int expanded_count = 0;
    for (const auto& [id, item] : items_) {
        if (item.expanded) { expanded_tokens += item.tokens; expanded_count++; }
        else hidden_tokens += item.tokens;
    }

    // Build actions block with budget awareness
    std::ostringstream act;
    act << "## Context Budget\n";
    act << "Expanded: " << expanded_count << " items (" << expanded_tokens << " tokens)\n";
    act << "Hidden: " << (int)items_.size() - expanded_count << " items (" << hidden_tokens << " tokens available)\n";
    act << "Limit: MAX 4 expanded items. If you @read a 5th, the oldest gets auto-hidden.\n";
    act << "To control which items stay: @hide items you're done with BEFORE @reading new ones.\n\n";

    act << R"(## Actions
Emit ONE action tag per response, then STOP generating immediately.

  @read(id)    — load a file into context ([-] becomes [+])
  @hide(id)    — unload a file from context ([+] becomes [-])
  @search(pat) — search codebase, results added to working set
  @run(cmd)    — execute a command, output added to working set
  @done()      — signal you are finished

Rules:
- Use IDs from the Context Index above.
- ALWAYS @hide files you're done referencing before @reading new ones.
- Keep at most 3-4 items expanded at once.
- After @done(), everything before it is your final output.
- If you don't need any actions, just respond normally (no tags).
)";
    ctx.actions_block = act.str();

    ctx.total_tokens = estimate_tokens(ctx.index_block) +
                       estimate_tokens(ctx.expanded_block) +
                       estimate_tokens(ctx.actions_block);

    return ctx;
}

// ─── Statistics ───────────────────────────────────────────────

ContextStats ContextExpander::stats() const {
    ContextStats s;
    s.context_window_max = context_window_max_;

    for (const auto& [id, item] : items_) {
        s.total_items++;
        s.total_tokens += item.tokens;
        if (item.expanded) {
            s.expanded_items++;
            s.expanded_tokens += item.tokens;
        } else {
            s.hidden_items++;
            s.hidden_tokens += item.tokens;
        }
    }

    auto ctx = compile();
    s.index_tokens = estimate_tokens(ctx.index_block);
    s.context_window_used = ctx.total_tokens;

    s.utilization = s.context_window_used > 0
        ? static_cast<double>(s.expanded_tokens) / s.context_window_used : 0.0;
    s.compression = s.total_tokens > 0
        ? 1.0 - static_cast<double>(s.expanded_tokens) / s.total_tokens : 0.0;

    s.reads_this_session = reads_this_session_;
    s.hides_this_session = hides_this_session_;
    s.peak_expanded_tokens = peak_expanded_tokens_;

    return s;
}

// ─── Action Parsing ───────────────────────────────────────────

std::optional<ActionTag> ContextExpander::find_action(const std::string& text) const {
    std::regex action_re(R"(@(\w+)\(([^)]*)\))");
    std::smatch match;
    if (std::regex_search(text, match, action_re)) {
        ActionTag tag;
        tag.full_match = match[0].str();
        tag.action = match[1].str();
        tag.argument = match[2].str();
        tag.position = match.position();

        // Clean up argument: strip quotes, key= prefixes
        auto& arg = tag.argument;
        if (arg.size() >= 2) {
            if ((arg.front() == '"' && arg.back() == '"') ||
                (arg.front() == '\'' && arg.back() == '\'')) {
                arg = arg.substr(1, arg.size() - 2);
            }
        }
        auto eq = arg.find('=');
        if (eq != std::string::npos) {
            auto val = arg.substr(eq + 1);
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                val = val.substr(1, val.size() - 2);
            }
            arg = val;
        }

        return tag;
    }
    return std::nullopt;
}

// ─── Action Resolution ───────────────────────────────────────

std::optional<std::string> ContextExpander::resolve_action(const ActionTag& tag) {
    if (tag.action == "read") {
        // If already expanded, just confirm
        if (has_item(tag.argument) && is_expanded(tag.argument)) {
            return "Already expanded: " + tag.argument + " (check [+] items in index)";
        }

        auto resolver_it = resolvers_.find("read");
        std::string content;

        if (resolver_it != resolvers_.end()) {
            auto resolved = resolver_it->second(tag.argument);
            if (resolved) content = *resolved;
        }

        if (content.empty()) {
            return std::nullopt;
        }

        // Update or create the item with actual content
        auto id = tag.argument;
        if (has_item(id)) {
            // Item exists (lazy-loaded) — replace with actual content
            remove_item(id);
        }
        add_item(id, "file", tag.argument, content);
        expand_item(id);

        // Enforce max expanded items — auto-hide oldest if over limit
        std::string auto_hide_msg;
        int max_expanded = 4;  // Tight budget: only 4 items in context at once
        int expanded_count = 0;
        std::string oldest_id;
        std::chrono::steady_clock::time_point oldest_time = std::chrono::steady_clock::now();

        for (const auto& [eid, eitem] : items_) {
            if (eitem.expanded && eid != id) {
                expanded_count++;
                if (eitem.last_accessed < oldest_time) {
                    oldest_time = eitem.last_accessed;
                    oldest_id = eid;
                }
            }
        }

        if (expanded_count >= max_expanded && !oldest_id.empty()) {
            auto hidden_tokens = items_[oldest_id].tokens;
            hide_item(oldest_id);
            auto_hide_msg = " (Auto-hid " + oldest_id + " [" +
                           std::to_string(hidden_tokens) + " tok] to stay within budget)";
            log::info("Auto-hide: " + oldest_id + " (" + std::to_string(hidden_tokens) + " tok freed)");
        }

        return "Expanded " + id + " (" +
               std::to_string(estimate_tokens(content)) + " tokens)." + auto_hide_msg;
    }

    if (tag.action == "hide") {
        auto id = tag.argument;
        if (!has_item(id)) {
            // Try "file:" prefix
            id = "file:" + tag.argument;
        }
        if (has_item(id)) {
            hide_item(id);
            return "Hidden " + id + " from context. (" +
                   std::to_string(items_[id].tokens) + " tokens freed)";
        }
        return "Item not found: " + tag.argument;
    }

    if (tag.action == "search") {
        auto resolver_it = resolvers_.find("search");
        if (resolver_it == resolvers_.end()) return std::nullopt;
        auto content = resolver_it->second(tag.argument);
        if (content) {
            auto id = "search:" + tag.argument;
            add_item(id, "search", "search results for '" + tag.argument + "'", *content);
            expand_item(id);
            return "Search results added and expanded (" +
                   std::to_string(estimate_tokens(*content)) + " tokens).";
        }
        return "No results for: " + tag.argument;
    }

    if (tag.action == "run") {
        auto resolver_it = resolvers_.find("run");
        if (resolver_it == resolvers_.end()) return std::nullopt;
        auto content = resolver_it->second(tag.argument);
        if (content) {
            auto id = "cmd:" + tag.argument.substr(0, 40);
            add_item(id, "output", "output of: " + tag.argument, *content);
            expand_item(id);
            return "Command output added (" +
                   std::to_string(estimate_tokens(*content)) + " tokens).";
        }
        return "Command failed: " + tag.argument;
    }

    return std::nullopt;
}

// ─── The Main Loop ────────────────────────────────────────────
// Each round: compile fresh context -> single LLM call -> process actions
// NO chat history accumulation. Each call is standalone.

ContextExpander::LoopResult ContextExpander::run(
    const std::string& system_prompt,
    const std::string& task_description,
    LlmFn llm_fn,
    int max_tokens,
    int max_rounds) {

    LoopResult result;

    for (int round = 0; round < max_rounds; round++) {
        result.rounds = round + 1;

        // Compile fresh context from working set
        auto ctx = compile();

        // Track peak
        auto st = stats();
        st.round = round + 1;
        if (st.expanded_tokens > peak_expanded_tokens_) {
            peak_expanded_tokens_ = st.expanded_tokens;
        }
        result.round_stats.push_back(st);

        // Build the full prompt — fresh each time, no history
        std::ostringstream prompt;
        prompt << system_prompt << "\n\n";
        prompt << ctx.index_block << "\n";
        if (!ctx.expanded_block.empty()) {
            prompt << ctx.expanded_block << "\n";
        }
        prompt << ctx.actions_block << "\n";
        prompt << "## Task\n" << task_description << "\n";

        if (round > 0) {
            prompt << "\n(Round " << (round + 1) << "/" << max_rounds << ". ";
            if (st.expanded_items > 3) {
                prompt << "WARNING: You have " << st.expanded_items << " items expanded ("
                       << st.expanded_tokens << " tokens). "
                       << "Consider using @hide(id) to unload items you no longer need. ";
            }
            prompt << "If you have enough context, produce your output now with NO action tags.)\n";
        }
        if (round >= max_rounds - 2) {
            prompt << "\nURGENT: Only " << (max_rounds - round) << " rounds left. "
                   << "Produce your final output NOW or you will run out of rounds.\n";
        }

        auto full_prompt = prompt.str();
        int prompt_tokens = estimate_tokens(full_prompt);

        log::info("Round " + std::to_string(round + 1) + ": " +
                  std::to_string(st.expanded_items) + " expanded (" +
                  std::to_string(st.expanded_tokens) + " tok), " +
                  std::to_string(st.hidden_items) + " hidden (" +
                  std::to_string(st.hidden_tokens) + " tok), " +
                  "prompt=" + std::to_string(prompt_tokens) + " tok");

        // Call LLM with the compiled prompt (single message, not multi-turn)
        auto response = llm_fn(full_prompt, max_tokens);

        // Check for action tag
        auto tag = find_action(response);

        if (!tag) {
            // No action — this is the final output
            result.output = response;
            log::info("Final output (round " + std::to_string(round + 1) + ", no action tag)");
            break;
        }

        if (tag->action == "done") {
            result.output = response.substr(0, tag->position);
            log::info("@done() received (round " + std::to_string(round + 1) + ")");
            break;
        }

        // Process the action
        log::info("Action: " + tag->full_match);
        auto action_result = resolve_action(*tag);

        if (action_result) {
            log::info("  → " + *action_result);
        } else {
            log::warn("  → action failed: " + tag->full_match);
        }

        // If this is the last round, force output on next iteration
        if (round == max_rounds - 1) {
            log::warn("Max rounds reached, forcing final output");
            auto forced_ctx = compile();
            std::ostringstream forced_prompt;
            forced_prompt << system_prompt << "\n\n"
                         << forced_ctx.index_block << "\n"
                         << forced_ctx.expanded_block << "\n"
                         << "## Task\n" << task_description << "\n"
                         << "\nDo NOT use any action tags. Produce your final response NOW.\n";
            result.output = llm_fn(forced_prompt.str(), max_tokens);
        }
    }

    // Final stats
    result.final_stats = stats();
    result.final_stats.round = result.rounds;
    result.final_stats.peak_expanded_tokens = peak_expanded_tokens_;

    return result;
}

} // namespace cortex
