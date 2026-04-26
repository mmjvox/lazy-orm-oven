#ifndef POSTGRESCONNECTION_H
#define POSTGRESCONNECTION_H

#include "IDbConnection.h"
#include <string>
#include <libpq-fe.h>

namespace LazyOrm {

class PostgresConnection : public IDbConnection
{
public:
    PostgresConnection(const std::string& host = "127.0.0.1",
                       const std::string& user = "postgres",
                       const std::string& password = "",
                       const std::string& database = "",
                       unsigned int port = 5432);

    ~PostgresConnection();

    bool connect() override;
    QueryState exec(const SqlTask &task) override;
    void close() override;
    bool healthy() override;
    unsigned long long getLastInsertId() override;

    // Additional methods for PostgreSQL
    void setSslMode(const std::string& mode) { mSslMode = mode; }  // "disable", "allow", "prefer", "require"

private:
    std::string mHost;
    std::string mUser;
    std::string mPassword;
    std::string mDatabase;
    unsigned int mPort;
    std::string mSslMode;
    PGconn* mPgConn;  // PostgreSQL connection object
    bool mConnected;

    std::string buildConnectionString() const;
    unsigned long long safeToUll(const char* value);

};

}

#endif // POSTGRESCONNECTION_H
