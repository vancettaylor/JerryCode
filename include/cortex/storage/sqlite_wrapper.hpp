#pragma once
#include <string>
#include <functional>
#include <vector>
#include <memory>

struct sqlite3;
struct sqlite3_stmt;

namespace cortex {

class SqliteStmt {
public:
    SqliteStmt(sqlite3* db, const std::string& sql);
    ~SqliteStmt();

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;

    void bind(int index, const std::string& value);
    void bind(int index, int value);
    void bind(int index, double value);

    bool step();
    void reset();

    std::string column_text(int index) const;
    int column_int(int index) const;
    double column_double(int index) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

class SqliteDb {
public:
    explicit SqliteDb(const std::string& path);
    ~SqliteDb();

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;

    void execute(const std::string& sql);
    SqliteStmt prepare(const std::string& sql);
    sqlite3* raw() { return db_; }

private:
    sqlite3* db_ = nullptr;
};

} // namespace cortex
