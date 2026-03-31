/**
 * @file context_expander.hpp
 * @brief Context expansion engine that manages a working set of content items
 *        and drives an iterative read/hide loop with the LLM.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <chrono>

namespace cortex {

// ─── Working Set Item ─────────────────────────────────────────
/**
 * @brief A single item in the working set, either expanded (content visible to the model)
 *        or hidden (listed in the index only). The model controls expansion state.
 */
struct WorkingSetItem {
    std::string id;           ///< Unique key, e.g. "file:main.cpp".
    std::string kind;         ///< Category: "file", "search", "output", or "note".
    std::string label;        ///< Human-readable label, e.g. "main.cpp (42 lines, C++)".
    std::string content;      ///< The actual content (always stored, shown only when expanded).
    int tokens = 0;           ///< Estimated token count of content.
    bool expanded = false;    ///< Whether this item is currently in the model's context.

    // Stats
    int times_expanded = 0;   ///< How many times this item has been expanded.
    int times_hidden = 0;     ///< How many times this item has been hidden.
    std::chrono::steady_clock::time_point last_accessed; ///< Last time this item was accessed.
};

// ─── Context Statistics ───────────────────────────────────────
/**
 * @brief Snapshot of context window usage and efficiency metrics.
 */
struct ContextStats {
    int total_items = 0;         ///< Total items in the working set.
    int expanded_items = 0;      ///< Number of currently expanded items.
    int hidden_items = 0;        ///< Number of currently hidden items.
    int expanded_tokens = 0;     ///< Tokens currently visible in context.
    int hidden_tokens = 0;       ///< Tokens stored but not in context.
    int total_tokens = 0;        ///< Total tokens across all tracked content.
    int index_tokens = 0;        ///< Tokens consumed by the index listing.
    int prompt_overhead = 0;     ///< Tokens used by system prompt and task description.
    int context_window_used = 0; ///< Total tokens sent to the model this round.
    int context_window_max = 0;  ///< Maximum available context window size.

    // Efficiency metrics
    double utilization = 0.0;    ///< Ratio of expanded_tokens to context_window_used.
    double compression = 0.0;    ///< Ratio of expanded_tokens to total_tokens.

    // Per-round tracking
    int round = 0;               ///< Current expansion round number.
    int reads_this_session = 0;  ///< Cumulative read (expand) operations this session.
    int hides_this_session = 0;  ///< Cumulative hide operations this session.
    int peak_expanded_tokens = 0;///< High-water mark of expanded tokens.
};

// ─── Action Resolver ──────────────────────────────────────────
/** @brief Callback that resolves an action tag argument and returns optional content. */
using ActionResolver = std::function<std::optional<std::string>(const std::string& argument)>;

// ─── Action Tag (parsed from model output) ────────────────────
/**
 * @brief A parsed action tag extracted from model output text.
 */
struct ActionTag {
    std::string full_match; ///< The complete matched tag text.
    std::string action;     ///< The action name (e.g. "read", "hide", "done").
    std::string argument;   ///< The argument following the action.
    size_t position;        ///< Character offset of the tag in the output.
};

// ─── The Context Expander ─────────────────────────────────────
/**
 * @brief Engine that manages a working set of content items and drives an iterative
 *        expansion loop where the model reads, hides, and navigates context on demand.
 */
class ContextExpander {
public:
    /**
     * @brief Construct a ContextExpander with a given context window limit.
     * @param context_window_max Maximum tokens available in the context window.
     */
    explicit ContextExpander(int context_window_max = 128000);

    /**
     * @brief Register a resolver for a named action type (e.g. "read", "search").
     * @param action The action name.
     * @param resolver Callback to invoke when this action is triggered.
     */
    void register_action(const std::string& action, ActionResolver resolver);

    /**
     * @brief Add a new item to the working set (initially hidden).
     * @param id Unique identifier for the item.
     * @param kind Category of the item.
     * @param label Human-readable label.
     * @param content The full content of the item.
     */
    void add_item(const std::string& id, const std::string& kind,
                  const std::string& label, const std::string& content);

    /**
     * @brief Expand an item so its content is visible to the model.
     * @param id Identifier of the item to expand.
     */
    void expand_item(const std::string& id);

    /**
     * @brief Hide an item so only its index entry is visible.
     * @param id Identifier of the item to hide.
     */
    void hide_item(const std::string& id);

    /**
     * @brief Remove an item from the working set entirely.
     * @param id Identifier of the item to remove.
     */
    void remove_item(const std::string& id);

    /** @brief Remove all items from the working set. */
    void clear();

    /**
     * @brief Check whether an item exists in the working set.
     * @param id Identifier to look up.
     * @return True if the item exists.
     */
    bool has_item(const std::string& id) const;

    /**
     * @brief Check whether an item is currently expanded.
     * @param id Identifier to look up.
     * @return True if the item exists and is expanded.
     */
    bool is_expanded(const std::string& id) const;

    /**
     * @brief Compiled context ready to be sent to the model.
     */
    struct CompiledContext {
        std::string index_block;    ///< Index listing all items (expanded and hidden).
        std::string expanded_block; ///< Concatenated content of all expanded items.
        std::string actions_block;  ///< Instructions about available actions.
        int total_tokens;           ///< Estimated total tokens of the compiled context.
    };

    /**
     * @brief Compile the current working set into a context payload for the model.
     * @return The compiled context blocks and token count.
     */
    CompiledContext compile() const;

    /**
     * @brief Parse model output text for the first action tag.
     * @param text The model's output to scan.
     * @return The parsed ActionTag, or nullopt if none found.
     */
    std::optional<ActionTag> find_action(const std::string& text) const;

    /**
     * @brief Resolve an action tag, modifying the working set as needed.
     * @param tag The action tag to resolve.
     * @return Content for @read actions, or a status message for @hide/@done.
     */
    std::optional<std::string> resolve_action(const ActionTag& tag);

    /**
     * @brief Get a snapshot of current context statistics.
     * @return ContextStats with current usage and efficiency metrics.
     */
    ContextStats stats() const;

    /** @brief Signature for the LLM call function used in the expansion loop. */
    using LlmFn = std::function<std::string(
        const std::string& compiled_prompt,
        int max_tokens)>;

    /**
     * @brief Result of running the expansion loop.
     */
    struct LoopResult {
        std::string output;                     ///< The model's final output text.
        int rounds = 0;                         ///< Number of rounds executed.
        ContextStats final_stats;               ///< Context stats after the last round.
        std::vector<ContextStats> round_stats;  ///< Stats captured after each round.
    };

    /**
     * @brief Run the main expansion loop: compile context, call LLM, resolve actions, repeat.
     * @param system_prompt System prompt prepended each round.
     * @param task_description Description of the task being performed.
     * @param llm_fn Function to call the LLM with a compiled prompt.
     * @param max_tokens Maximum output tokens per round.
     * @param max_rounds Maximum number of expansion rounds.
     * @return LoopResult with final output and per-round statistics.
     */
    LoopResult run(
        const std::string& system_prompt,
        const std::string& task_description,
        LlmFn llm_fn,
        int max_tokens = 4096,
        int max_rounds = 6) ;

private:
    /**
     * @brief Estimate the token count for a string of text.
     * @param text The text to estimate.
     * @return Estimated token count.
     */
    int estimate_tokens(const std::string& text) const;

    std::unordered_map<std::string, WorkingSetItem> items_;    ///< Working set items by ID.
    std::unordered_map<std::string, ActionResolver> resolvers_;///< Registered action resolvers.
    int context_window_max_;                                    ///< Maximum context window size.

    // Session-level stats
    int reads_this_session_ = 0;       ///< Cumulative read operations.
    int hides_this_session_ = 0;       ///< Cumulative hide operations.
    int peak_expanded_tokens_ = 0;     ///< High-water mark of expanded tokens.
};

} // namespace cortex
