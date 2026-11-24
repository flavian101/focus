#include "storage/Database.h"

#include <sqlite3.h>

#include <utility>

namespace focus {

namespace {
[[noreturn]] void fail(sqlite3* db, const char* what) {
    throw StorageError(std::string(what) + ": " + (db ? sqlite3_errmsg(db) : "unknown error"));
}
}  // namespace

// ---------------------------------------------------------------- Statement

Statement::Statement(Statement&& other) noexcept
    : db_(std::exchange(other.db_, nullptr)), stmt_(std::exchange(other.stmt_, nullptr)) {}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(stmt_);
        db_   = std::exchange(other.db_, nullptr);
        stmt_ = std::exchange(other.stmt_, nullptr);
    }
    return *this;
}

Statement::~Statement() { sqlite3_finalize(stmt_); }  // finalize(nullptr) is a harmless no-op

Statement& Statement::bind(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) fail(db_, "bind int");
    return *this;
}

Statement& Statement::bind(int index, const std::string& value) {
    // SQLITE_TRANSIENT = "copy this text now", so `value` may die right after the call.
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) !=
        SQLITE_OK)
        fail(db_, "bind text");
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) fail(db_, "bind null");
    return *this;
}

void Statement::reset() {
    sqlite3_reset(stmt_);  // its return value repeats the error of the previous step, which was already reported
    sqlite3_clear_bindings(stmt_);
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    fail(db_, "step");
}

void Statement::run() {
    if (step()) throw StorageError("run: statement unexpectedly returned a row");
}

std::int64_t Statement::columnInt(int index) const { return sqlite3_column_int64(stmt_, index); }

std::string Statement::columnText(int index) const {
    const auto* text = sqlite3_column_text(stmt_, index);
    return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();  // NULL -> ""
}

bool Statement::columnIsNull(int index) const { return sqlite3_column_type(stmt_, index) == SQLITE_NULL; }

// ----------------------------------------------------------------- Database

Database Database::open(const std::string& utf8Path) {
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(utf8Path.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        const std::string msg = raw ? sqlite3_errmsg(raw) : "out of memory";
        sqlite3_close(raw);  // even a failed open may hand back a handle that must be closed
        throw StorageError("open '" + utf8Path + "': " + msg);
    }
    Database db(raw);
    db.exec("PRAGMA foreign_keys = ON;");   // SQLite ignores FOREIGN KEY unless asked, per connection
    db.exec("PRAGMA journal_mode = WAL;");  // readers (a future dashboard) don't block the writer
    return db;
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        sqlite3_close(db_);
        db_ = std::exchange(other.db_, nullptr);
    }
    return *this;
}

Database::~Database() { sqlite3_close(db_); }

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw StorageError("exec: " + msg);
    }
}

Statement Database::prepare(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK)
        fail(db_, "prepare");
    return Statement(db_, stmt);
}

std::int64_t Database::lastInsertRowId() const { return sqlite3_last_insert_rowid(db_); }

int Database::userVersion() {
    auto st = prepare("PRAGMA user_version;");
    st.step();
    return static_cast<int>(st.columnInt(0));
}

void Database::setUserVersion(int version) {
    // PRAGMA does not accept bound parameters, so the number is formatted into the SQL.
    // Safe: it is an int we control, never user text.
    exec("PRAGMA user_version = " + std::to_string(version) + ";");
}

// -------------------------------------------------------------- Transaction

Transaction::Transaction(Database& db) : db_(db) { db_.exec("BEGIN;"); }

Transaction::~Transaction() {
    if (!done_) {
        try {
            db_.exec("ROLLBACK;");
        } catch (...) {
            // A destructor must not throw. If rollback fails the connection is already unusable.
        }
    }
}

void Transaction::commit() {
    db_.exec("COMMIT;");
    done_ = true;
}

}  // namespace focus
