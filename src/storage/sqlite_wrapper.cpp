#include "cortex/storage/sqlite_wrapper.hpp"
#include <sqlite3.h>
#include <stdexcept>

namespace cortex {

SqliteStmt::SqliteStmt(sqlite3* db, const std::string& sql) {
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLite prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
}

SqliteStmt::~SqliteStmt() { if (stmt_) sqlite3_finalize(stmt_); }

void SqliteStmt::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}
void SqliteStmt::bind(int index, int value) { sqlite3_bind_int(stmt_, index, value); }
void SqliteStmt::bind(int index, double value) { sqlite3_bind_double(stmt_, index, value); }

bool SqliteStmt::step() { return sqlite3_step(stmt_) == SQLITE_ROW; }
void SqliteStmt::reset() { sqlite3_reset(stmt_); }

std::string SqliteStmt::column_text(int index) const {
    auto p = sqlite3_column_text(stmt_, index);
    return p ? reinterpret_cast<const char*>(p) : "";
}
int SqliteStmt::column_int(int index) const { return sqlite3_column_int(stmt_, index); }
double SqliteStmt::column_double(int index) const { return sqlite3_column_double(stmt_, index); }

SqliteDb::SqliteDb(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + path);
    }
    execute("PRAGMA journal_mode=WAL;");
}

SqliteDb::~SqliteDb() { if (db_) sqlite3_close(db_); }

void SqliteDb::execute(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

SqliteStmt SqliteDb::prepare(const std::string& sql) {
    return SqliteStmt(db_, sql);
}

} // namespace cortex
