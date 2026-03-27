#pragma once
#include "cortex/core/types.hpp"
#include "cortex/storage/sqlite_wrapper.hpp"
#include <memory>
#include <optional>

namespace cortex {

class MetadataStore {
public:
    explicit MetadataStore(const std::string& db_path);

    void store(const MetadataRecord& record);
    std::optional<MetadataRecord> get(const std::string& id) const;
    std::vector<MetadataRecord> query_by_file(const std::string& path) const;
    std::vector<MetadataRecord> recent(int limit = 20) const;
    int total_tokens() const;

private:
    void create_tables();
    std::unique_ptr<SqliteDb> db_;
};

} // namespace cortex
