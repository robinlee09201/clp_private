#include "SQLiteDB.hpp"

// Project headers
#include "Defs.h"
#include "spdlog_with_specializations.hpp"

using std::string;

void SQLiteDB::open (const string& path) {
    auto return_value = sqlite3_open(path.c_str(), &m_db_handle);
    if (SQLITE_OK != return_value) {
        SPDLOG_ERROR("Failed to open sqlite database {} - {}", path.c_str(), sqlite3_errmsg(m_db_handle));
        close();
        throw OperationFailed(ErrorCode_Failure, __FILENAME__, __LINE__);
    }
}

void SQLiteDB::open(std::pair<void *, size_t> memory) {
    if (auto const retval{sqlite3_open(":memory:", &m_db_handle)}; SQLITE_OK != retval) {
        SPDLOG_ERROR("Failed to open in-memory sqlite database: {}", sqlite3_errmsg(m_db_handle));
        close();
        throw OperationFailed(ErrorCode_Failure, __FILENAME__, __LINE__);
    }
    if (auto const return_value = sqlite3_deserialize(
                m_db_handle,
                "main",
                size_checked_pointer_cast<unsigned char>(static_cast<char*>(memory.first)),
                memory.second,
                memory.second,
                SQLITE_DESERIALIZE_READONLY
        );
        SQLITE_OK != return_value)
    {
        // Note: if deserialization fails, sqlite3 API will automatically frees
        // the buffer.
        SPDLOG_ERROR("Failed to deserialize sqlite database - {}", sqlite3_errmsg(m_db_handle));
        close();
        throw OperationFailed(ErrorCode_Failure, __FILENAME__, __LINE__);
    }
}

bool SQLiteDB::close () {
    auto return_value = sqlite3_close(m_db_handle);
    if (SQLITE_BUSY == return_value) {
        // Database objects (e.g., statements) not deallocated
        return false;
    }
    m_db_handle = nullptr;
    return true;
}

SQLitePreparedStatement SQLiteDB::prepare_statement (const char* statement, size_t statement_length) {
    if (nullptr == m_db_handle) {
        throw OperationFailed(ErrorCode_NotInit, __FILENAME__, __LINE__);
    }

    return {statement, statement_length, m_db_handle};
}
