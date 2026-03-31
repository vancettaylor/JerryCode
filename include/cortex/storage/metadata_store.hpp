/**
 * @file metadata_store.hpp
 * @brief SQLite-backed persistent storage for metadata records.
 */

#pragma once
#include "cortex/core/types.hpp"
#include "cortex/storage/sqlite_wrapper.hpp"
#include <memory>
#include <optional>

namespace cortex {

/**
 * @brief Persistent metadata store backed by SQLite.
 *
 * Stores and retrieves MetadataRecord entries, supporting lookup by ID,
 * file path queries, and recent-record retrieval.
 */
class MetadataStore {
public:
    /**
     * @brief Open or create a metadata store at the given database path.
     * @param db_path Filesystem path to the SQLite database file.
     */
    explicit MetadataStore(const std::string& db_path);

    /**
     * @brief Insert or update a metadata record.
     * @param record The MetadataRecord to store.
     */
    void store(const MetadataRecord& record);

    /**
     * @brief Retrieve a metadata record by its unique ID.
     * @param id The record identifier.
     * @return The matching record, or std::nullopt if not found.
     */
    std::optional<MetadataRecord> get(const std::string& id) const;

    /**
     * @brief Query all metadata records associated with a file path.
     * @param path The file path to search for.
     * @return Vector of matching records.
     */
    std::vector<MetadataRecord> query_by_file(const std::string& path) const;

    /**
     * @brief Retrieve the most recent metadata records.
     * @param limit Maximum number of records to return (default 20).
     * @return Vector of recent records, newest first.
     */
    std::vector<MetadataRecord> recent(int limit = 20) const;

    /**
     * @brief Get the total token count across all stored records.
     * @return Aggregate token count.
     */
    int total_tokens() const;

private:
    void create_tables();
    std::unique_ptr<SqliteDb> db_;  ///< Owned SQLite database connection.
};

} // namespace cortex
