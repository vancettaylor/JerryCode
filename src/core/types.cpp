#include "cortex/core/types.hpp"

namespace cortex {

void to_json(Json& j, const ActionPlan& p) {
    j = Json{
        {"id", p.id},
        {"task_description", p.task_description},
        {"reasoning", p.reasoning},
        {"files_needed", p.files_needed},
        {"tool_name", p.tool_name},
        {"tool_arguments", p.tool_arguments},
        {"expected_outcome", p.expected_outcome},
        {"risk_level", p.risk_level},
        {"dependencies", p.dependencies}
    };
}

void from_json(const Json& j, ActionPlan& p) {
    j.at("task_description").get_to(p.task_description);
    if (j.contains("id")) j.at("id").get_to(p.id);
    if (j.contains("reasoning")) j.at("reasoning").get_to(p.reasoning);
    if (j.contains("files_needed")) j.at("files_needed").get_to(p.files_needed);
    if (j.contains("tool_name")) j.at("tool_name").get_to(p.tool_name);
    if (j.contains("tool_arguments")) p.tool_arguments = j.at("tool_arguments");
    if (j.contains("expected_outcome")) j.at("expected_outcome").get_to(p.expected_outcome);
    if (j.contains("risk_level")) j.at("risk_level").get_to(p.risk_level);
    if (j.contains("dependencies")) j.at("dependencies").get_to(p.dependencies);
}

std::string trigger_kind_to_string(TriggerKind kind) {
    switch (kind) {
        case TriggerKind::FileCreated:       return "file_created";
        case TriggerKind::FileModified:      return "file_modified";
        case TriggerKind::FileDeleted:       return "file_deleted";
        case TriggerKind::ErrorDetected:     return "error_detected";
        case TriggerKind::DependencyChanged: return "dependency_changed";
        case TriggerKind::ScopeExpanded:     return "scope_expanded";
        case TriggerKind::ScopeNarrowed:     return "scope_narrowed";
        case TriggerKind::TaskCompleted:     return "task_completed";
        case TriggerKind::TaskBlocked:       return "task_blocked";
    }
    return "unknown";
}

TriggerKind trigger_kind_from_string(const std::string& s) {
    if (s == "file_created")       return TriggerKind::FileCreated;
    if (s == "file_modified")      return TriggerKind::FileModified;
    if (s == "file_deleted")       return TriggerKind::FileDeleted;
    if (s == "error_detected")     return TriggerKind::ErrorDetected;
    if (s == "dependency_changed") return TriggerKind::DependencyChanged;
    if (s == "scope_expanded")     return TriggerKind::ScopeExpanded;
    if (s == "scope_narrowed")     return TriggerKind::ScopeNarrowed;
    if (s == "task_completed")     return TriggerKind::TaskCompleted;
    if (s == "task_blocked")       return TriggerKind::TaskBlocked;
    return TriggerKind::ErrorDetected;
}

void to_json(Json& j, const Trigger& t) {
    j = Json{
        {"kind", trigger_kind_to_string(t.kind)},
        {"description", t.description}
    };
    if (t.file_path) j["file_path"] = *t.file_path;
    if (!t.extra.empty()) j["extra"] = t.extra;
}

void from_json(const Json& j, Trigger& t) {
    t.kind = trigger_kind_from_string(j.at("kind").get<std::string>());
    j.at("description").get_to(t.description);
    if (j.contains("file_path")) t.file_path = j.at("file_path").get<std::string>();
    if (j.contains("extra")) t.extra = j.at("extra");
}

Message Message::system(const std::string& text) {
    return Message{MessageRole::System, {{.type = "text", .text = text}}};
}

Message Message::user(const std::string& text) {
    return Message{MessageRole::User, {{.type = "text", .text = text}}};
}

Message Message::assistant(const std::string& text) {
    return Message{MessageRole::Assistant, {{.type = "text", .text = text}}};
}

} // namespace cortex
