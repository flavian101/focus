#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

// Forward declarations: callers of this header never need <sqlite3.h>.
struct sqlite3;
struct sqlite3_stmt;

namespace focus {

class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Database;

// One prepared SQL statement. Move-only; finalized automatically (RAII).
// A Statement must not outlive the Database it came from.
// SQL parameters are numbered from 1 (SQLite's convention).
class Statement {
public:
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    ~Statement();

    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, const std::string& value);
    Statement& bindNull(int index);

    void reset();  // forget the last run and its bound values, so the statement can be used again
    bool step();  // true = a row is ready to read, false = finished
    void run();   // for INSERT/UPDATE/DDL: step once and expect "finished"

    std::int64_t columnInt(int index) const;  // columns are numbered from 0
    std::string columnText(int index) const;
    bool columnIsNull(int index) const;

private:
    friend class Database;
    Statement(sqlite3* db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt) {}

    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

// An open database file. Move-only; closed automatically (RAII).
class Database {
public:
    // utf8Path: UTF-8 encoded (SQLite expects UTF-8 even on Windows). ":memory:" = throwaway DB for tests.
    static Database open(const std::string& utf8Path);

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    ~Database();

    void exec(const std::string& sql);  // for SQL without parameters or results
    Statement prepare(const std::string& sql);
    std::int64_t lastInsertRowId() const;

    int userVersion();  // schema version stamp stored inside the file
    void setUserVersion(int version);

private:
    explicit Database(sqlite3* db) : db_(db) {}
    sqlite3* db_ = nullptr;
};

// "All or nothing" for a group of writes. Rolls back automatically unless commit() is called,
// including when an exception unwinds the stack.
class Transaction {
public:
    explicit Transaction(Database& db);
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    void commit();

private:
    Database& db_;
    bool done_ = false;
};

}  // namespace focus
