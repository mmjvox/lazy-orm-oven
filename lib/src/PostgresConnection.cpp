#include "PostgresConnection.h"
#include <charconv>
#include <iostream>
#include <trantor/utils/Logger.h>

PostgresConnection::PostgresConnection(const std::string& host,
                                       const std::string& user,
                                       const std::string& password,
                                       const std::string& database,
                                       unsigned int port)
: mHost(host)
, mUser(user)
, mPassword(password)
, mDatabase(database)
, mPort(port)
, mSslMode("prefer")  // Default SSL mode
, mPgConn(nullptr)
, mConnected(false)
{
}

PostgresConnection::~PostgresConnection()
{
    close();
}

std::string PostgresConnection::buildConnectionString() const
{
    std::string connStr;
    connStr += "host=" + mHost + " ";
    connStr += "port=" + std::to_string(mPort) + " ";
    connStr += "user=" + mUser + " ";
    connStr += "password=" + mPassword + " ";
    connStr += "dbname=" + mDatabase + " ";
    connStr += "sslmode=" + mSslMode + " ";
    connStr += "connect_timeout=10 ";

    return connStr;
}

unsigned long long PostgresConnection::safeToUll(const char *value)
{
    if (!value || *value == '\0') {
        return 0;
    }

    unsigned long long result = 0;
    const char* begin = value;
    const char* end = value + std::strlen(value);

    auto [ptr, ec] = std::from_chars(begin, end, result);

    if (ec != std::errc{} || ptr != end) {
        return 0;
    }

    return result;
}

bool PostgresConnection::connect()
{
    if (mConnected) {
        return true;
    }

    // Build connection string
    std::string connStr = buildConnectionString();

    // Connect to PostgreSQL
    mPgConn = PQconnectdb(connStr.c_str());

    // Check connection status
    if (PQstatus(mPgConn) != CONNECTION_OK) {
        mLastError = "Connection failed: " + std::string(PQerrorMessage(mPgConn));
        LOG_ERROR << mLastError;
        PQfinish(mPgConn);
        mPgConn = nullptr;
        mConnected = false;
        return false;
    }

    mConnected = true;
    LOG_INFO << "Successfully connected to PostgreSQL database: " << mDatabase;
    return true;
}

IDbConnection::QueryState PostgresConnection::exec(const SqlTask& task)
{
    if (!mConnected || !mPgConn) {
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
    PGresult* res = PQexec(mPgConn, sql.c_str());

    if (!res) {
        mLastError = "Query execution failed: PQexec returned null: " + std::string(PQerrorMessage(mPgConn));
        LOG_ERROR << mLastError;
        return IDbConnection::QueryFailed;
    }

    // Check for errors
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        mLastError = "Query execution failed: " + std::string(PQerrorMessage(mPgConn));
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;
        PQclear(res);
        return IDbConnection::QueryFailed;
    }

        // Handle SELECT queries that return data
        if (status == PGRES_TUPLES_OK) {
            int rows = PQntuples(res);
            int cols = PQnfields(res);

            // Print results (for debugging)
            std::cout << "Query executed successfully. " << rows << " row(s) returned." << std::endl;

            // Print column headers
            for (int i = 0; i < cols; i++) {
                std::cout << PQfname(res, i) << "\t";
            }
            std::cout << std::endl;

            // Print data rows
            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    std::cout << (PQgetvalue(res, i, j) ? PQgetvalue(res, i, j) : "NULL") << "\t";
                }
                std::cout << std::endl;
            }

            PQclear(res);
            return IDbConnection::QuerySuccess;
        }

        // Handle INSERT, UPDATE, DELETE, etc.
        if (status == PGRES_COMMAND_OK) {
            // Get affected rows
            mAffectedRows = safeToUll(PQcmdTuples(res));
            LOG_INFO << "Query executed successfully. Affected rows: " << mAffectedRows;
            PQclear(res);
            return IDbConnection::QuerySuccess;
        }

        PQclear(res);
        return IDbConnection::QueryFailed;
}

void PostgresConnection::close()
{
    if (mPgConn) {
        PQfinish(mPgConn);
        mPgConn = nullptr;
    }
    mConnected = false;
}

bool PostgresConnection::healthy()
{
    if (!mConnected || !mPgConn) {
        return false;
    }

    // Send a simple query to check connection
    PGresult* res = PQexec(mPgConn, "SELECT 1");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        mConnected = false;
        return false;
    }

    PQclear(res);
    return true;
}

unsigned long long PostgresConnection::getLastInsertId()
{
    if (!mPgConn) {
        return 0;
    }

    PGresult* res = PQexec(mPgConn, "SELECT LASTVAL()");

    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) {
            PQclear(res);
        }
        return 0;
    }

    unsigned long long lastId = 0;

    if (PQntuples(res) > 0 && PQnfields(res) > 0 && !PQgetisnull(res, 0, 0)) {
        lastId = safeToUll(PQgetvalue(res, 0, 0));
    }

    PQclear(res);
    return lastId;
}
