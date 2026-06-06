#include "MariadbConnection.h"
#include "LazyVariant/Result.h"
#include "LazyVariant/ResultRow.h"
#include <iostream>
#include <trantor/utils/Logger.h>

namespace LazyOrm {

MariadbConnection::MariadbConnection(const std::string& host,
                                     const std::string& user,
                                     const std::string& password,
                                     const std::string& database,
                                     unsigned int port,
                                     const std::string& socketPath)
    : mHost(host)
    , mUser(user)
    , mPassword(password)
    , mDatabase(database)
    , mPort(port)
    , mSocketPath(socketPath)
    , mUseTcp(socketPath.empty())
    , mMariadb(nullptr)
    , mConnected(false)
{
}

MariadbConnection::~MariadbConnection()
{
    close();
}

bool MariadbConnection::connect()
{
    if (mConnected) {
        return true;
    }

    // Initialize MariaDB object
    mMariadb = mysql_init(nullptr);
    if (!mMariadb) {
        mLastError = "mysql_init() failed";
        LOG_ERROR << mLastError;
        return false;
    }

    // Force TCP/IP connection if needed (especially for Docker)
    if (mUseTcp) {
        unsigned int protocol = MYSQL_PROTOCOL_TCP;
        mysql_options(mMariadb, MYSQL_OPT_PROTOCOL, &protocol);
    }

    // Set connection timeout (seconds)
    mysql_options(mMariadb, MYSQL_OPT_CONNECT_TIMEOUT, &mConnectTimeOut);

    // Set read timeout (optional - available in most versions)
    mysql_options(mMariadb, MYSQL_OPT_READ_TIMEOUT, &mReadTimeOut);

    // Set write timeout (optional)
    mysql_options(mMariadb, MYSQL_OPT_WRITE_TIMEOUT, &mWriteTimeOut);

    // Enable automatic reconnection (optional)
    my_bool reconnect = 1;
    mysql_options(mMariadb, MYSQL_OPT_RECONNECT, &reconnect);

    // Get socket path (nullptr if empty)
    const char* socketPtr = mSocketPath.empty() ? nullptr : mSocketPath.c_str();

    // Handle localhost with custom socket
    if (mHost == "localhost" && socketPtr == nullptr) {
        LOG_WARN << "Warning: Connecting to 'localhost' without socket path. "
                  << "Consider using '127.0.0.1' for TCP/IP or provide socket path.";
    }

    // Connect to the database
    MYSQL* connection = mysql_real_connect(mMariadb,
                                           mHost.c_str(),
                                           mUser.c_str(),
                                           mPassword.c_str(),
                                           mDatabase.empty() ? nullptr : mDatabase.c_str(),
                                           mPort,
                                           socketPtr,
                                           0);

    if (!connection) {
        mLastError = "Connection failed: " + std::string{mysql_error(mMariadb)};
        LOG_ERROR << mLastError;
        LOG_ERROR << "  Host: " << mHost;
        LOG_ERROR << "  Port: " << mPort;
        LOG_ERROR << "  Socket: " << (socketPtr ? socketPtr : "nullptr (using default)");
        mysql_close(mMariadb);
        mMariadb = nullptr;
        mConnected = false;
        return false;
    }

    mConnected = true;
    LOG_INFO << "Successfully connected to MariaDB";
    return true;
}

std::shared_ptr<Result> MariadbConnection::exec(const SqlTask& task)
{
    std::shared_ptr<Result> lazyOrmResult = std::make_shared<Result>();

    if (!mConnected || !mMariadb) {
        mLastError = "Cannot execute query: Not connected to database";
        LOG_ERROR << mLastError;
        lazyOrmResult->setQueryState(LazyOrm::UndefinedQuery);
        return lazyOrmResult;
    }

    const std::string& sql = task.getSqlQuery();
    if (sql.empty()) {
        mLastError = "Cannot execute empty query";
        LOG_ERROR << mLastError;
        lazyOrmResult->setQueryState(LazyOrm::UndefinedQuery);
        return lazyOrmResult;
    }

    // Execute the query
    if (mysql_query(mMariadb, sql.c_str())) {
        mLastError = "Query execution failed: " + std::string{mysql_error(mMariadb)};
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;
        lazyOrmResult->setQueryState(LazyOrm::QueryFailed);
        return lazyOrmResult;
    }


    // For SELECT queries, store the result
    MYSQL_RES* result = mysql_store_result(mMariadb);
    if (result) {
        // Get column count
        const int columnsCount = mysql_num_fields(result);
        MYSQL_FIELD* fields = mysql_fetch_fields(result);

        std::vector<std::string> columnNames;
        for (int i = 0; i < columnsCount; ++i) {
            columnNames.push_back(fields[i].name);
        }

        lazyOrmResult->setColumnNames(columnNames);

        MYSQL_ROW row;
        unsigned long* lengths = nullptr;

        while ((row = mysql_fetch_row(result))) {
            lengths = mysql_fetch_lengths(result);

            ResultRow resultRow;

            for (int i = 0; i < columnsCount; ++i) {
                const std::string& columnName = columnNames[i];

                if (!row[i]) {
                    resultRow.insert(columnName, DbVariant{});
                    continue;
                }

                DbVariant value = std::string(row[i], lengths[i]);
                const bool isUnsigned = fields[i].flags & UNSIGNED_FLAG;

                switch (fields[i].type) {
                case MYSQL_TYPE_TINY:
                case MYSQL_TYPE_SHORT:
                case MYSQL_TYPE_LONG:
                case MYSQL_TYPE_INT24:
                case MYSQL_TYPE_LONGLONG:
                case MYSQL_TYPE_YEAR:
                    if (isUnsigned) {
                        resultRow.insert(columnName, value.toULongLong());
                    } else {
                        resultRow.insert(columnName, value.toLongLong());
                    }
                    break;

                case MYSQL_TYPE_FLOAT:
                case MYSQL_TYPE_DOUBLE:
                    resultRow.insert(columnName, value.toLongDouble());
                    break;

                case MYSQL_TYPE_DECIMAL:
                case MYSQL_TYPE_NEWDECIMAL:
                    resultRow.insert(columnName, value);
                    break;

                case MYSQL_TYPE_NULL:
                    resultRow.insert(columnName, DbVariant{});
                    break;

                case MYSQL_TYPE_BIT:
                    resultRow.insert(columnName, value);
                    break;

                case MYSQL_TYPE_DATE:
                case MYSQL_TYPE_TIME:
                case MYSQL_TYPE_DATETIME:
                case MYSQL_TYPE_TIMESTAMP:
                case MYSQL_TYPE_NEWDATE:
                case MYSQL_TYPE_TIMESTAMP2:
                case MYSQL_TYPE_DATETIME2:
                case MYSQL_TYPE_TIME2:
                    resultRow.insert(columnName, value);
                    break;

                case MYSQL_TYPE_JSON:
                case MYSQL_TYPE_ENUM:
                case MYSQL_TYPE_SET:
                case MYSQL_TYPE_VARCHAR:
                case MYSQL_TYPE_VAR_STRING:
                case MYSQL_TYPE_STRING:
                    resultRow.insert(columnName, value);
                    break;

                case MYSQL_TYPE_TINY_BLOB:
                case MYSQL_TYPE_MEDIUM_BLOB:
                case MYSQL_TYPE_LONG_BLOB:
                case MYSQL_TYPE_BLOB:
                case MYSQL_TYPE_GEOMETRY: {
                    BlobType blob;
                    blob.reserve(lengths[i]);

                    const unsigned char* raw =
                        reinterpret_cast<const unsigned char*>(row[i]);

                    for (unsigned long j = 0; j < lengths[i]; ++j) {
                        blob.push_back(static_cast<ByteType>(raw[j]));
                    }

                    resultRow.insert(columnName, blob);
                    break;
                }

                default:
                    resultRow.insert(columnName, value);
                    break;
                }
            }

            lazyOrmResult->push_back(resultRow);
        }

        std::cout << lazyOrmResult->toIndentedString() << std::endl;

        mysql_free_result(result);
        lazyOrmResult->setQueryState(LazyOrm::QuerySuccess);
        return lazyOrmResult;
    }
    else {
        // For INSERT, UPDATE, DELETE queries
        if (mysql_field_count(mMariadb) == 0) {
            mAffectedRows = mysql_affected_rows(mMariadb);
            lazyOrmResult->setAffectedRows(mAffectedRows);
            lazyOrmResult->setInsertId(getLastInsertId());
            LOG_INFO << "Query executed successfully. Affected rows: " << mAffectedRows;
            lazyOrmResult->setQueryState(LazyOrm::QuerySuccess);
            return lazyOrmResult;
        }
        else {
            // Some error occurred
            mLastError = "Failed to store result: " + std::string{mysql_error(mMariadb)};
            lazyOrmResult->setError(mLastError);
            LOG_ERROR << mLastError;
            lazyOrmResult->setQueryState(LazyOrm::QueryFailed);
            return lazyOrmResult;
        }
    }
}

void MariadbConnection::close()
{
    if (mMariadb) {
        mysql_close(mMariadb);
        mMariadb = nullptr;
    }
    mConnected = false;
}

bool MariadbConnection::healthy()
{
    if (!mConnected || !mMariadb) {
        return false;
    }

    if (mysql_ping(mMariadb) != 0) {
        mConnected = false;
        return false;
    }

    return true;
}

unsigned long long MariadbConnection::getLastInsertId()
{
    if (!mMariadb) return 0;
    return mysql_insert_id(mMariadb);
}

}
