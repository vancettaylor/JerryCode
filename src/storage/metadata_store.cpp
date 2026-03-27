#include "cortex/storage/metadata_store.hpp"

namespace cortex {

MetadataStore::MetadataStore(const std::string& db_path)
    : db_(std::make_unique<SqliteDb>(db_path)) {
    create_tables();
}

void MetadataStore::create_tables() {
    db_->execute(R"(
        CREATE TABLE IF NOT EXISTS metadata_records (
            id TEXT PRIMARY KEY,
            action_plan_id TEXT,
            plan_json TEXT,
            raw_model_output TEXT,
            result_summary TEXT,
            success INTEGER,
            triggers_json TEXT,
            input_tokens INTEGER,
            output_tokens INTEGER,
            started_at TEXT,
            completed_at TEXT,
            latency_ms REAL
        );
        CREATE INDEX IF NOT EXISTS idx_metadata_success ON metadata_records(success);
    )");
}

void MetadataStore::store(const MetadataRecord& record) {
    auto stmt = db_->prepare(R"(
        INSERT OR REPLACE INTO metadata_records
        (id, action_plan_id, plan_json, raw_model_output, result_summary,
         success, triggers_json, input_tokens, output_tokens,
         started_at, completed_at, latency_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    Json plan_json = record.plan;
    Json triggers_json = record.triggers;

    stmt.bind(1, record.id);
    stmt.bind(2, record.action_plan_id);
    stmt.bind(3, plan_json.dump());
    stmt.bind(4, record.raw_model_output);
    stmt.bind(5, record.result_summary);
    stmt.bind(6, record.success ? 1 : 0);
    stmt.bind(7, triggers_json.dump());
    stmt.bind(8, record.input_tokens);
    stmt.bind(9, record.output_tokens);
    stmt.bind(10, std::to_string(record.started_at.time_since_epoch().count()));
    stmt.bind(11, std::to_string(record.completed_at.time_since_epoch().count()));
    stmt.bind(12, record.latency_ms);
    stmt.step();
}

std::optional<MetadataRecord> MetadataStore::get(const std::string& id) const {
    // TODO: Implement query
    return std::nullopt;
}

std::vector<MetadataRecord> MetadataStore::query_by_file(const std::string& path) const {
    // TODO: Implement query
    return {};
}

std::vector<MetadataRecord> MetadataStore::recent(int limit) const {
    // TODO: Implement query
    return {};
}

int MetadataStore::total_tokens() const {
    // TODO: Implement query
    return 0;
}

} // namespace cortex
