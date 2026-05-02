#include "SqliteConnection.h"
#include "LazyOrm/Result.h"
#include "LazyOrm/ResultRow.h"
#include <trantor/utils/Logger.h>

namespace LazyOrm {

SqliteConnection::SqliteConnection(const std::string& databasePath, bool wallMode)
: mDatabasePath(databasePath)
, mWallMode(wallMode)
, mDb(nullptr)
, mConnected(false)
, mReadOnly(false)
, mBusyTimeoutMs(5000)
, mCurrentStmt(nullptr)
{
}

SqliteConnection::~SqliteConnection()
{
    close();
}

bool SqliteConnection::connect()
{
    if (mConnected) {
        return true;
    }

    if(mDatabasePath.empty()){
        mLastError = "Database path is empty";
        LOG_ERROR << mLastError << ". If you need to open an in-memory database, set path to \":memory:\".";
        return false;
    }

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (mReadOnly) {
        flags = SQLITE_OPEN_READONLY;
    }

    int rc = sqlite3_open_v2(mDatabasePath.c_str(), &mDb, flags, nullptr);

    if (rc != SQLITE_OK) {
        mLastError = "Failed to open database: " + std::string(sqlite3_errmsg(mDb));
        LOG_ERROR << mLastError;
        LOG_ERROR << "Database path: " << mDatabasePath;
        sqlite3_close(mDb);
        mDb = nullptr;
        mConnected = false;
        return false;
    }

    // Set busy timeout
    sqlite3_busy_timeout(mDb, mBusyTimeoutMs);

    // Enable foreign keys
    char* errMsg = nullptr;
    rc = sqlite3_exec(mDb, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        LOG_WARN << "Failed to enable foreign keys: " << (errMsg ? errMsg : "unknown error");
        sqlite3_free(errMsg);
    }

    // Enable WAL mode for better concurrency
    if(mWallMode){
        rc = sqlite3_exec(mDb, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            LOG_WARN << "Failed to set WAL mode: " << (errMsg ? errMsg : "unknown error");
            sqlite3_free(errMsg);
        }
    }

    mConnected = true;
    LOG_INFO << "Successfully connected to SQLite database: " << mDatabasePath;
    return true;
}

IDbConnection::QueryState SqliteConnection::exec(const SqlTask& task)
{
    if (!mConnected || !mDb) {
        mLastError = "Cannot execute query: Not connected to database";
        LOG_ERROR << mLastError;
        return IDbConnection::UndefinedQuery;
    }

    const std::string& sql = task.getSqlQuery();
    if (sql.empty()) {
        mLastError = "Cannot execute empty query";
        LOG_ERROR << mLastError;
        return IDbConnection::UndefinedQuery;
    }

    if (mCurrentStmt) {
        sqlite3_finalize(mCurrentStmt);
        mCurrentStmt = nullptr;
    }

    const char* tail = nullptr;
    int rc = sqlite3_prepare_v2(mDb, sql.c_str(), -1, &mCurrentStmt, &tail);

    if (rc != SQLITE_OK) {
        mLastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(mDb));
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;
        return IDbConnection::QueryFailed;
    }

    const int columnCount = sqlite3_column_count(mCurrentStmt);
    const bool isSelectQuery = (columnCount > 0);

    std::shared_ptr<Result> lazyOrmResult = std::make_shared<Result>();

    if(isSelectQuery){

        std::vector<std::string> columnNames;
        std::vector<int> columnTypes;
        for (int i = 0; i < columnCount; ++i) {
            const char* colName = sqlite3_column_name(mCurrentStmt, i);
            const auto type = sqlite3_column_type(mCurrentStmt, i);
            columnNames.push_back(colName ? colName : "Unknown");
            columnTypes.push_back(type);
        }

        lazyOrmResult->setColumnNames(columnNames);

        while ((rc = sqlite3_step(mCurrentStmt)) == SQLITE_ROW) {
            ResultRow resultRow;
            for (int i = 0; i < columnCount; ++i) {
                const auto &colName = columnNames[i];
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(mCurrentStmt, i));
                DbVariant value = text;
                switch (columnTypes[i]) {
                    case SQLITE_INTEGER:
                        resultRow.insert(colName, value.toLongLong());
                    case SQLITE_FLOAT:
                        resultRow.insert(colName, value.toLongDouble());
                    case SQLITE_TEXT:
                        resultRow.insert(colName, value.toString());
                    case SQLITE_BLOB:
                        resultRow.insert(colName, value.toBlob());
                    case SQLITE_NULL:
                        resultRow.insert(colName, "NULL");
                    default:
                        resultRow.insert(colName, "UNKNOWN");
                }
            }
            lazyOrmResult->push_back(resultRow);
        }
    }

    std::cout << lazyOrmResult->toIndentedString() << std::endl;

    if (rc != SQLITE_DONE) {
        mLastError = "Query execution failed: " + std::string(sqlite3_errmsg(mDb));
        lazyOrmResult->setError(mLastError);
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;

        sqlite3_finalize(mCurrentStmt);
        mCurrentStmt = nullptr;
        return IDbConnection::QueryFailed;
    }

    mAffectedRows = sqlite3_changes(mDb);
    lazyOrmResult->setAffectedRows(mAffectedRows);
    lazyOrmResult->setInsertId(getLastInsertId());

    sqlite3_finalize(mCurrentStmt);
    mCurrentStmt = nullptr;

    return IDbConnection::QuerySuccess;
}

void SqliteConnection::close()
{
    if (mCurrentStmt) {
        sqlite3_finalize(mCurrentStmt);
        mCurrentStmt = nullptr;
    }

    if (mDb) {
        sqlite3_close(mDb);
        mDb = nullptr;
    }
    mConnected = false;
}

bool SqliteConnection::healthy()
{
    if (!mConnected || !mDb) {
        return false;
    }

    // Simple query to check connection
    int rc = sqlite3_exec(mDb, "SELECT 1;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        mConnected = false;
        return false;
    }

    return true;
}

unsigned long long SqliteConnection::getLastInsertId()
{
    if (!mDb) return 0;
    return sqlite3_last_insert_rowid(mDb);
}

void SqliteConnection::setBusyTimeout(int milliseconds)
{
    mBusyTimeoutMs = milliseconds;
    if (mConnected && mDb) {
        sqlite3_busy_timeout(mDb, mBusyTimeoutMs);
    }
}

void SqliteConnection::setReadOnly(bool readOnly)
{
    mReadOnly = readOnly;
}

}
