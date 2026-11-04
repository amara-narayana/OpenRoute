#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace openroute::storage {

class Statement {
public:
    Statement(sqlite3* db, std::string_view sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void BindInt(int index, int val);
    void BindInt64(int index, int64_t val);
    void BindDouble(int index, double val);
    void BindText(int index, std::string_view text);
    void BindBlob(int index, const void* data, size_t size);
    void BindNull(int index);

    bool Step(); // returns true if row is available, false if done
    void Reset();

    int GetInt(int col);
    int64_t GetInt64(int col);
    double GetDouble(int col);
    std::string GetText(int col);
    std::vector<uint8_t> GetBlob(int col);
    bool IsNull(int col);

private:
    sqlite3_stmt* stmt_ = nullptr;
};

class Transaction {
public:
    explicit Transaction(sqlite3* db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void Commit();
    void Rollback();

private:
    sqlite3* db_ = nullptr;
    bool committed_ = false;
};

class Database {
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool Open(const std::string& filepath);
    void Close();
    [[nodiscard]] bool IsOpen() const { return db_ != nullptr; }

    bool Execute(std::string_view sql);
    Statement Prepare(std::string_view sql);
    Transaction BeginTransaction();

    bool ApplyMigrations();
    int GetSchemaVersion();

    [[nodiscard]] sqlite3* raw_handle() const { return db_; }

private:
    sqlite3* db_ = nullptr;
};

} // namespace openroute::storage
