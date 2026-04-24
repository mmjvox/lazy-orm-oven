#ifndef MARIADBCONNECTION_H
#define MARIADBCONNECTION_H

#include "IDbConnection.h"
#include <string>

#ifdef LIBMARIADB
#include <mariadb/mysql.h>
#else
#include <mysql/mysql.h>
#endif

using MariaDB = MYSQL;

class MariadbConnection : public IDbConnection
{
public:
    MariadbConnection(const std::string& host = "127.0.0.1",  // Changed default to 127.0.0.1
                      const std::string& user = "root",
                      const std::string& password = "",
                      const std::string& database = "",
                      unsigned int port = 3306,
                      const std::string& socketPath = "");  // Optional socket path

    ~MariadbConnection();

    bool connect() override;
    QueryState exec(const SqlTask &task) override;
    void close() override;
    bool healthy() override;
    unsigned long long getLastInsertId() override;

    // New methods for connection configuration
    void setForceTcp(bool force) { mUseTcp = force; }
    void setSocketPath(const std::string& socketPath) { mSocketPath = socketPath; }

private:
    std::string mHost;
    std::string mUser;
    std::string mPassword;
    std::string mDatabase;
    unsigned int mPort;
    std::string mSocketPath;
    bool mUseTcp;
    MariaDB* mMariadb;
    bool mConnected;
    uint mConnectTimeOut = 10;
    uint mReadTimeOut = 60;
    uint mWriteTimeOut = 60;
};

#endif // MARIADBCONNECTION_H
