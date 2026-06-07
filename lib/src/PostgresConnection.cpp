#include "PostgresConnection.h"
#include "Result.h"
#include "ResultRow.h"
#include <charconv>
#include <iostream>
#include <trantor/utils/Logger.h>
#include <vector>

#ifndef POSTGRES_OIDS_DEFINED
/*
 * PostgreSQL Type OIDs
 * Based on pg_type.h from PostgreSQL source
 */

namespace PgOid {

/* Numeric Types */
constexpr Oid INT8          = 20;   // bigint / int8
constexpr Oid INT2          = 21;   // smallint / int2
constexpr Oid INT4          = 23;   // integer / int4
constexpr Oid FLOAT4        = 700;  // real / float4
constexpr Oid FLOAT8        = 701;  // double precision / float8
constexpr Oid NUMERIC       = 1700; // numeric / decimal

/* Money Type */
constexpr Oid CASH          = 790;

/* Character Types */
constexpr Oid CHAR          = 18;
constexpr Oid BPCHAR        = 1042;
constexpr Oid VARCHAR       = 1043;
constexpr Oid TEXT          = 25;

/* Binary Data */
constexpr Oid BYTEA         = 17;

/* Boolean */
constexpr Oid BOOL          = 16;

/* Date/Time */
constexpr Oid TIME          = 1083;
constexpr Oid TIMETZ        = 1266;
constexpr Oid DATE          = 1082;
constexpr Oid TIMESTAMP     = 1114;
constexpr Oid TIMESTAMPTZ   = 1184;
constexpr Oid INTERVAL      = 1186;

/* Geometric */
constexpr Oid POINT         = 600;
constexpr Oid LINE          = 601;
constexpr Oid LSEG          = 602;
constexpr Oid BOX           = 603;
constexpr Oid PATH          = 604;
constexpr Oid POLYGON       = 606;
constexpr Oid CIRCLE        = 718;

/* Network */
constexpr Oid INET          = 869;
constexpr Oid CIDR          = 650;
constexpr Oid MACADDR       = 774;
constexpr Oid MACADDR8      = 829;

/* Bit String */
constexpr Oid BIT           = 1560;
constexpr Oid VARBIT        = 1562;

/* Text Search */
constexpr Oid TSVECTOR      = 3614;
constexpr Oid TSQUERY       = 3615;

/* UUID */
constexpr Oid UUID          = 2950;

/* JSON */
constexpr Oid JSON          = 114;
constexpr Oid JSONB         = 3802;

/* Arrays */
constexpr Oid INT2_ARRAY    = 1005;
constexpr Oid INT4_ARRAY    = 1007;
constexpr Oid INT8_ARRAY    = 1016;
constexpr Oid TEXT_ARRAY    = 1009;
constexpr Oid FLOAT4_ARRAY  = 1021;
constexpr Oid FLOAT8_ARRAY  = 1022;
constexpr Oid BOOL_ARRAY    = 1000;
constexpr Oid BYTEA_ARRAY   = 1001;
constexpr Oid CHAR_ARRAY    = 1002;
constexpr Oid VARCHAR_ARRAY = 1015;
constexpr Oid NUMERIC_ARRAY = 1231;
constexpr Oid DATE_ARRAY    = 1182;
constexpr Oid TIMESTAMP_ARRAY   = 1115;
constexpr Oid TIMESTAMPTZ_ARRAY = 1185;
constexpr Oid UUID_ARRAY    = 2951;
constexpr Oid JSONB_ARRAY   = 3807;

/* Ranges */
constexpr Oid INT4_RANGE    = 3904;
constexpr Oid INT8_RANGE    = 3926;
constexpr Oid NUM_RANGE     = 3906;
constexpr Oid TS_RANGE      = 3908;
constexpr Oid TSTZ_RANGE    = 3910;
constexpr Oid DATE_RANGE    = 3912;

/* Composite */
constexpr Oid RECORD        = 2249;

}
#endif

namespace LazyOrm {

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

std::shared_ptr<Result> PostgresConnection::exec(const SqlTask& task)
{
    std::shared_ptr<Result> lazyOrmResult = std::make_shared<Result>();

    if (!mConnected || !mPgConn) {
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
    PGresult* res = PQexec(mPgConn, sql.c_str());

    if (!res) {
        mLastError = "Query execution failed: PQexec returned null: " + std::string(PQerrorMessage(mPgConn));
        LOG_ERROR << mLastError;
        lazyOrmResult->setQueryState(LazyOrm::QueryFailed);
        return lazyOrmResult;
    }

    // Check for errors
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        mLastError = "Query execution failed: " + std::string(PQerrorMessage(mPgConn));
        LOG_ERROR << mLastError;
        LOG_ERROR << "SQL: " << sql;
        PQclear(res);
        lazyOrmResult->setQueryState(LazyOrm::QueryFailed);
        return lazyOrmResult;
    }

        // Handle SELECT queries that return data
        if (status == PGRES_TUPLES_OK) {
            const int rowsCount = PQntuples(res);
            const int columnsCount = PQnfields(res);

            // Print results (for debugging)
            std::cout << "Query executed successfully. " << rowsCount << " row(s) returned." << std::endl;

            std::vector<std::string> columnNames;
            std::vector<Oid> columnTypes;
            for (int i = 0; i < columnsCount; ++i) {
                const char* colName = PQfname(res, i);
                const auto type = PQftype(res, i);
                columnNames.push_back(colName ? colName : "Unknown");
                columnTypes.push_back(type);
            }

            lazyOrmResult->setColumnNames(columnNames);


            // Print data rows
            for (int row = 0; row < rowsCount; ++row) {
                ResultRow resultRow;
                for (int col = 0; col < columnsCount; ++col) {
                    const auto& colName = columnNames[col];

                    // NULL check
                    if (PQgetisnull(res, row, col)) {
                        resultRow.insert(colName, "NULL");
                        continue;
                    }

                    const char* text = PQgetvalue(res, row, col);
                    DbVariant value = text ? text : "";

                    switch (columnTypes[col]) {
                    // ========== INTEGER TYPES ==========
                    case PgOid::INT2:
                    case PgOid::INT4:
                    case PgOid::INT8:
                        resultRow.insert(colName, value.toLongLong());
                        break;

                    // ========== FLOATING POINT TYPES ==========
                    case PgOid::FLOAT4:
                    case PgOid::FLOAT8:
                        resultRow.insert(colName, value.toLongDouble());
                        break;

                    case PgOid::NUMERIC:
                        // PostgreSQL numeric is arbitrary precision.
                        // Do NOT convert to long double unless you accept precision loss.
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== MONEY TYPE ==========
                    case PgOid::CASH:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== BOOLEAN TYPE ==========
                    case PgOid::BOOL:
                        resultRow.insert(colName, value.toBool());
                        break;

                    // ========== CHARACTER TYPES ==========
                    case PgOid::CHAR:
                    case PgOid::BPCHAR:
                    case PgOid::VARCHAR:
                    case PgOid::TEXT:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== BINARY TYPE ==========
                    case PgOid::BYTEA: {
                        size_t byteaLen = 0;

                        unsigned char* raw = PQunescapeBytea(
                            reinterpret_cast<const unsigned char*>(text),
                            &byteaLen
                            );

                        if (!raw) {
                            resultRow.insert(colName, BlobType{});
                            break;
                        }

                        BlobType blob;
                        blob.reserve(byteaLen);

                        for (size_t i = 0; i < byteaLen; ++i) {
                            blob.push_back(static_cast<ByteType>(raw[i]));
                        }

                        PQfreemem(raw);

                        resultRow.insert(colName, blob);
                        break;
                    }

                    // ========== DATE/TIME TYPES ==========
                    case PgOid::DATE:
                    case PgOid::TIME:
                    case PgOid::TIMETZ:
                    case PgOid::TIMESTAMP:
                    case PgOid::TIMESTAMPTZ:
                    case PgOid::INTERVAL:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== JSON TYPES ==========
                    case PgOid::JSON:
                    case PgOid::JSONB:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== UUID TYPE ==========
                    case PgOid::UUID:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== NETWORK ADDRESS TYPES ==========
                    case PgOid::INET:
                    case PgOid::CIDR:
                    case PgOid::MACADDR:
                    case PgOid::MACADDR8:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== BIT STRING TYPES ==========
                    case PgOid::BIT:
                    case PgOid::VARBIT:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== TEXT SEARCH TYPES ==========
                    case PgOid::TSVECTOR:
                    case PgOid::TSQUERY:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== GEOMETRIC TYPES ==========
                    case PgOid::POINT:
                    case PgOid::LINE:
                    case PgOid::LSEG:
                    case PgOid::BOX:
                    case PgOid::PATH:
                    case PgOid::POLYGON:
                    case PgOid::CIRCLE:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== RANGE TYPES ==========
                    case PgOid::INT4_RANGE:
                    case PgOid::INT8_RANGE:
                    case PgOid::NUM_RANGE:
                    case PgOid::TS_RANGE:
                    case PgOid::TSTZ_RANGE:
                    case PgOid::DATE_RANGE:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== ARRAY TYPES ==========
                    case PgOid::INT2_ARRAY:
                    case PgOid::INT4_ARRAY:
                    case PgOid::INT8_ARRAY:
                    case PgOid::FLOAT4_ARRAY:
                    case PgOid::FLOAT8_ARRAY:
                    case PgOid::TEXT_ARRAY:
                    case PgOid::BOOL_ARRAY:
                    case PgOid::BYTEA_ARRAY:
                    case PgOid::CHAR_ARRAY:
                    case PgOid::VARCHAR_ARRAY:
                    case PgOid::NUMERIC_ARRAY:
                    case PgOid::DATE_ARRAY:
                    case PgOid::TIMESTAMP_ARRAY:
                    case PgOid::TIMESTAMPTZ_ARRAY:
                    case PgOid::UUID_ARRAY:
                    case PgOid::JSONB_ARRAY:
                        // Still PostgreSQL array text format, e.g. "{1,2,3}".
                        // This is not real parsed array support.
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== COMPOSITE/RECORD TYPES ==========
                    case PgOid::RECORD:
                        resultRow.insert(colName, value.toString());
                        break;

                    // ========== UNKNOWN TYPE ==========
                    default:
                        resultRow.insert(colName, value.toString());
                        break;
                    }
                }

                lazyOrmResult->push_back(resultRow);
            }

            std::cout << lazyOrmResult->toIndentedString() << std::endl;

            PQclear(res);
            lazyOrmResult->setQueryState(LazyOrm::QuerySuccess);
            return lazyOrmResult;
        }

        // Handle INSERT, UPDATE, DELETE, etc.
        if (status == PGRES_COMMAND_OK) {
            // Get affected rows
            mAffectedRows = safeToUll(PQcmdTuples(res));
            LOG_INFO << "Query executed successfully. Affected rows: " << mAffectedRows;
            PQclear(res);
            lazyOrmResult->setQueryState(LazyOrm::QuerySuccess);
            return lazyOrmResult;
        }

        PQclear(res);
        lazyOrmResult->setQueryState(LazyOrm::QueryFailed);
        return lazyOrmResult;
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

}
