/**
 * @file types.hpp
 * @brief Core data types used throughout the cortex framework.
 */

#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <variant>

namespace cortex {

using Clock     = std::chrono::system_clock;
using Timestamp = std::chrono::time_point<Clock>;
using Json      = nlohmann::json;

// ─── ActionPlan (Phase 1 output) ───────────────────────────────
/**
 * @brief Describes a planned action produced during the breakdown phase.
 */
struct ActionPlan {
    std::string id;                         ///< Unique plan identifier.
    std::string task_description;           ///< Human-readable description of what will be done.
    std::string reasoning;                  ///< Model's reasoning for choosing this action.
    std::vector<std::string> files_needed;  ///< Files required to execute the plan.
    std::string tool_name;                  ///< Name of the tool to invoke.
    Json tool_arguments = Json::object();   ///< Arguments passed to the tool.
    std::string expected_outcome;           ///< What the plan expects to achieve.
    std::string risk_level = "low";         ///< Risk classification: "low", "medium", or "high".
    std::vector<std::string> dependencies;  ///< IDs of plans that must complete first.
    Timestamp created_at = Clock::now();    ///< When this plan was created.
};

/**
 * @brief Serialize an ActionPlan to JSON.
 * @param j Output JSON object.
 * @param p The ActionPlan to serialize.
 */
void to_json(Json& j, const ActionPlan& p);

/**
 * @brief Deserialize an ActionPlan from JSON.
 * @param j Input JSON object.
 * @param p The ActionPlan to populate.
 */
void from_json(const Json& j, ActionPlan& p);

// ─── Trigger (detected state changes) ──────────────────────────
/**
 * @brief Enumeration of state-change events detected during execution.
 */
enum class TriggerKind {
    FileCreated,
    FileModified,
    FileDeleted,
    ErrorDetected,
    DependencyChanged,
    ScopeExpanded,
    ScopeNarrowed,
    TaskCompleted,
    TaskBlocked
};

/**
 * @brief Convert a TriggerKind to its string representation.
 * @param kind The trigger kind.
 * @return String name of the trigger.
 */
std::string trigger_kind_to_string(TriggerKind kind);

/**
 * @brief Parse a TriggerKind from its string representation.
 * @param s The string to parse.
 * @return Corresponding TriggerKind value.
 */
TriggerKind trigger_kind_from_string(const std::string& s);

/**
 * @brief A detected state change event with optional file context.
 */
struct Trigger {
    TriggerKind kind;                       ///< The type of state change.
    std::string description;                ///< Human-readable description.
    std::optional<std::string> file_path;   ///< Associated file path, if any.
    Json extra = Json::object();            ///< Additional metadata.
};

/**
 * @brief Serialize a Trigger to JSON.
 * @param j Output JSON object.
 * @param t The Trigger to serialize.
 */
void to_json(Json& j, const Trigger& t);

/**
 * @brief Deserialize a Trigger from JSON.
 * @param j Input JSON object.
 * @param t The Trigger to populate.
 */
void from_json(const Json& j, Trigger& t);

// ─── MetadataRecord (one per action cycle) ─────────────────────
/**
 * @brief Full record of a single action cycle including plan, output, and metrics.
 */
struct MetadataRecord {
    std::string id;                         ///< Unique record identifier.
    std::string action_plan_id;             ///< ID of the ActionPlan that was executed.
    ActionPlan plan;                        ///< Copy of the executed plan.
    std::string raw_model_output;           ///< Raw text returned by the model.
    std::string result_summary;             ///< Condensed outcome description.
    bool success = false;                   ///< Whether the action succeeded.
    std::vector<Trigger> triggers;          ///< State changes detected during execution.
    int input_tokens = 0;                   ///< Tokens sent to the model.
    int output_tokens = 0;                  ///< Tokens received from the model.
    Timestamp started_at = Clock::now();    ///< When execution began.
    Timestamp completed_at = Clock::now();  ///< When execution finished.
    double latency_ms = 0.0;               ///< Wall-clock duration in milliseconds.
};

// ─── TimelineEntry (compressed for review) ─────────────────────
/**
 * @brief Compact summary of an action cycle for timeline display and review.
 */
struct TimelineEntry {
    std::string metadata_id;     ///< Reference to the full MetadataRecord.
    int sequence_number = 0;     ///< Ordinal position in the timeline.
    std::string one_line_summary;///< Short description of what happened.
    std::string tool_used;       ///< Name of the tool that was invoked.
    bool success = false;        ///< Whether the action succeeded.
    int token_cost = 0;          ///< Total tokens consumed (input + output).
    Timestamp timestamp = Clock::now(); ///< When this entry was recorded.
};

// ─── ContextPointer (expandable reference in the index) ────────
/**
 * @brief A reference to content that can be expanded into or hidden from the model's context.
 */
struct ContextPointer {
    std::string label;              ///< Human-readable label.
    std::string kind;               ///< Category: "file", "action", "search_result", or "error".
    std::string ref;                ///< Path or metadata ID for retrieval.
    int estimated_tokens = 0;       ///< Estimated token count of the referenced content.
    bool currently_expanded = false; ///< Whether this content is currently in context.
};

// ─── Provider-agnostic message types ───────────────────────────
/**
 * @brief Role of a message in the conversation.
 */
enum class MessageRole { System, User, Assistant, ToolResult };

/**
 * @brief A single content block within a message (text, tool use, or tool result).
 */
struct ContentBlock {
    std::string type;       ///< Block type: "text", "tool_use", or "tool_result".
    std::string text;       ///< Text content (for "text" and "tool_result" types).
    std::string tool_use_id;///< Unique ID of the tool invocation.
    std::string tool_name;  ///< Name of the tool being used.
    Json tool_input = Json::object(); ///< Input arguments for the tool.
    bool is_error = false;  ///< Whether this block represents an error.
};

/**
 * @brief A provider-agnostic conversation message.
 */
struct Message {
    MessageRole role;                   ///< The role of this message's sender.
    std::vector<ContentBlock> content;  ///< Content blocks within the message.

    /**
     * @brief Create a system message.
     * @param text The system prompt text.
     * @return A Message with System role.
     */
    static Message system(const std::string& text);

    /**
     * @brief Create a user message.
     * @param text The user's input text.
     * @return A Message with User role.
     */
    static Message user(const std::string& text);

    /**
     * @brief Create an assistant message.
     * @param text The assistant's response text.
     * @return A Message with Assistant role.
     */
    static Message assistant(const std::string& text);
};

// ─── Token usage ───────────────────────────────────────────────
/**
 * @brief Token consumption counters for a single LLM call.
 */
struct TokenUsage {
    int input_tokens = 0;       ///< Tokens sent to the model.
    int output_tokens = 0;      ///< Tokens generated by the model.
    int cache_read_tokens = 0;  ///< Tokens served from prompt cache.
};

// ─── TUI event types (for cross-thread communication) ──────────
/** @brief Incremental text chunk from a streaming response. */
struct StreamDelta { std::string text; };
/** @brief Notification that an action plan is ready. */
struct PlanReady { ActionPlan plan; };
/** @brief Notification that an action cycle has completed. */
struct ActionComplete { MetadataRecord record; };
/** @brief Notification that the session has entered a new phase. */
struct PhaseChange { std::string phase; };
/** @brief An error message forwarded to the UI. */
struct ErrorEvent { std::string message; };

/** @brief Variant type carrying any UI event for cross-thread dispatch. */
using UiEvent = std::variant<StreamDelta, PlanReady, ActionComplete,
                              PhaseChange, ErrorEvent>;

} // namespace cortex
