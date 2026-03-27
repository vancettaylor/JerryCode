#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <chrono>

namespace cortex {

// ─── Working Set Item ─────────────────────────────────────────
// Each item in the working set is either expanded (content in context)
// or hidden (just an index entry). The model controls this.
struct WorkingSetItem {
    std::string id;           // unique key, e.g. "file:main.cpp"
    std::string kind;         // "file", "search", "output", "note"
    std::string label;        // human-readable, e.g. "main.cpp (42 lines, C++)"
    std::string content;      // the actual content (always stored, shown only when expanded)
    int tokens = 0;           // estimated token count
    bool expanded = false;    // is this currently in the model's context?

    // Stats
    int times_expanded = 0;
    int times_hidden = 0;
    std::chrono::steady_clock::time_point last_accessed;
};

// ─── Context Statistics ───────────────────────────────────────
struct ContextStats {
    int total_items = 0;
    int expanded_items = 0;
    int hidden_items = 0;
    int expanded_tokens = 0;    // tokens currently in context
    int hidden_tokens = 0;      // tokens available but not in context
    int total_tokens = 0;       // all tracked content
    int index_tokens = 0;       // tokens used by the index itself
    int prompt_overhead = 0;    // system prompt + task description tokens
    int context_window_used = 0;// total tokens sent to model this round
    int context_window_max = 0; // max available

    // Efficiency metrics
    double utilization = 0.0;   // expanded_tokens / context_window_used
    double compression = 0.0;   // expanded_tokens / total_tokens (how much we're hiding)

    // Per-round tracking
    int round = 0;
    int reads_this_session = 0;
    int hides_this_session = 0;
    int peak_expanded_tokens = 0;
};

// ─── Action Resolver ──────────────────────────────────────────
using ActionResolver = std::function<std::optional<std::string>(const std::string& argument)>;

// ─── Action Tag (parsed from model output) ────────────────────
struct ActionTag {
    std::string full_match;
    std::string action;
    std::string argument;
    size_t position;
};

// ─── The Context Expander ─────────────────────────────────────
class ContextExpander {
public:
    explicit ContextExpander(int context_window_max = 128000);

    // Register resolvers for action types
    void register_action(const std::string& action, ActionResolver resolver);

    // Working set management
    void add_item(const std::string& id, const std::string& kind,
                  const std::string& label, const std::string& content);
    void expand_item(const std::string& id);
    void hide_item(const std::string& id);
    void remove_item(const std::string& id);
    void clear();

    // Check if an item exists and/or is expanded
    bool has_item(const std::string& id) const;
    bool is_expanded(const std::string& id) const;

    // Compile the current context: index + expanded items
    // This is what gets sent to the model each round
    struct CompiledContext {
        std::string index_block;       // the index listing all items
        std::string expanded_block;    // all currently expanded content
        std::string actions_block;     // instructions about available actions
        int total_tokens;
    };
    CompiledContext compile() const;

    // Parse model output for action tags
    std::optional<ActionTag> find_action(const std::string& text) const;

    // Resolve an action (may add/expand/hide items in the working set)
    // Returns the content if it's a @read, or status message for @hide/@done
    std::optional<std::string> resolve_action(const ActionTag& tag);

    // Get current statistics
    ContextStats stats() const;

    // The main expansion loop
    // Each round: compile fresh context -> call LLM -> check for actions -> repeat
    using LlmFn = std::function<std::string(
        const std::string& compiled_prompt,
        int max_tokens)>;

    struct LoopResult {
        std::string output;
        int rounds = 0;
        ContextStats final_stats;
        std::vector<ContextStats> round_stats;  // stats per round
    };

    LoopResult run(
        const std::string& system_prompt,
        const std::string& task_description,
        LlmFn llm_fn,
        int max_tokens = 4096,
        int max_rounds = 6) ;

private:
    int estimate_tokens(const std::string& text) const;

    std::unordered_map<std::string, WorkingSetItem> items_;
    std::unordered_map<std::string, ActionResolver> resolvers_;
    int context_window_max_;

    // Session-level stats
    int reads_this_session_ = 0;
    int hides_this_session_ = 0;
    int peak_expanded_tokens_ = 0;
};

} // namespace cortex
