#include "MariadbConnection.h"
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

IDbConnection::QueryState MariadbConnection::exec(const SqlTask& task)
{
    if (!mConnected || !mMariadb) {
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

    // Execute the query
    if (mysql_query(mMariadb, sql.c_str())) {
        mLastError = "Query execution failed: " + std::string{mysql_error(mMariadb)};
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;
        return IDbConnection::QueryFailed;
    }

    // For SELECT queries, store the result
    MYSQL_RES* result = mysql_store_result(mMariadb);
    if (result) {
        // Get column count
        int numFields = mysql_num_fields(result);
        MYSQL_ROW row;

        // Print results (for debugging)
        std::cout << "Query executed successfully. Results:" << std::endl;
        while ((row = mysql_fetch_row(result))) {
            for (int i = 0; i < numFields; i++) {
                std::cout << (row[i] ? row[i] : "NULL") << "\t";
            }
            std::cout << std::endl;
        }

        mysql_free_result(result);
        return IDbConnection::QuerySuccess;
    }
    else {
        // For INSERT, UPDATE, DELETE queries
        if (mysql_field_count(mMariadb) == 0) {
            mAffectedRows = mysql_affected_rows(mMariadb);
            LOG_INFO << "Query executed successfully. Affected rows: " << mAffectedRows;
            return IDbConnection::QuerySuccess;
        }
        else {
            // Some error occurred
            mLastError = "Failed to store result: " + std::string{mysql_error(mMariadb)};
            LOG_ERROR << mLastError;
            return IDbConnection::QueryFailed;
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
