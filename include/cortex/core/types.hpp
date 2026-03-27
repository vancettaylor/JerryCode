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
struct ActionPlan {
    std::string id;
    std::string task_description;
    std::string reasoning;
    std::vector<std::string> files_needed;
    std::string tool_name;
    Json tool_arguments = Json::object();
    std::string expected_outcome;
    std::string risk_level = "low";
    std::vector<std::string> dependencies;
    Timestamp created_at = Clock::now();
};

void to_json(Json& j, const ActionPlan& p);
void from_json(const Json& j, ActionPlan& p);

// ─── Trigger (detected state changes) ──────────────────────────
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

std::string trigger_kind_to_string(TriggerKind kind);
TriggerKind trigger_kind_from_string(const std::string& s);

struct Trigger {
    TriggerKind kind;
    std::string description;
    std::optional<std::string> file_path;
    Json extra = Json::object();
};

void to_json(Json& j, const Trigger& t);
void from_json(const Json& j, Trigger& t);

// ─── MetadataRecord (one per action cycle) ─────────────────────
struct MetadataRecord {
    std::string id;
    std::string action_plan_id;
    ActionPlan plan;
    std::string raw_model_output;
    std::string result_summary;
    bool success = false;
    std::vector<Trigger> triggers;
    int input_tokens = 0;
    int output_tokens = 0;
    Timestamp started_at = Clock::now();
    Timestamp completed_at = Clock::now();
    double latency_ms = 0.0;
};

// ─── TimelineEntry (compressed for review) ─────────────────────
struct TimelineEntry {
    std::string metadata_id;
    int sequence_number = 0;
    std::string one_line_summary;
    std::string tool_used;
    bool success = false;
    int token_cost = 0;
    Timestamp timestamp = Clock::now();
};

// ─── ContextPointer (expandable reference in the index) ────────
struct ContextPointer {
    std::string label;
    std::string kind;  // "file" | "action" | "search_result" | "error"
    std::string ref;   // Path or metadata ID
    int estimated_tokens = 0;
    bool currently_expanded = false;
};

// ─── Provider-agnostic message types ───────────────────────────
enum class MessageRole { System, User, Assistant, ToolResult };

struct ContentBlock {
    std::string type;  // "text" | "tool_use" | "tool_result"
    std::string text;
    std::string tool_use_id;
    std::string tool_name;
    Json tool_input = Json::object();
    bool is_error = false;
};

struct Message {
    MessageRole role;
    std::vector<ContentBlock> content;

    static Message system(const std::string& text);
    static Message user(const std::string& text);
    static Message assistant(const std::string& text);
};

// ─── Token usage ───────────────────────────────────────────────
struct TokenUsage {
    int input_tokens = 0;
    int output_tokens = 0;
    int cache_read_tokens = 0;
};

// ─── TUI event types (for cross-thread communication) ──────────
struct StreamDelta { std::string text; };
struct PlanReady { ActionPlan plan; };
struct ActionComplete { MetadataRecord record; };
struct PhaseChange { std::string phase; };
struct ErrorEvent { std::string message; };

using UiEvent = std::variant<StreamDelta, PlanReady, ActionComplete,
                              PhaseChange, ErrorEvent>;

} // namespace cortex
