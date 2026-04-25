#ifndef SQLITECONNECTION_H
#define SQLITECONNECTION_H

#include "IDbConnection.h"
#include <sqlite3.h>
#include <string>

class SqliteConnection : public IDbConnection
{
public:
    explicit SqliteConnection(const std::string& databasePath, bool wallMode = false);
    ~SqliteConnection();

    bool connect() override;
    QueryState exec(const SqlTask &task) override;
    void close() override;
    bool healthy() override;
    unsigned long long getLastInsertId() override;

    // SQLite specific methods
    void setBusyTimeout(int milliseconds);
    void setReadOnly(bool readOnly);

private:
    std::string mDatabasePath;
    bool mWallMode;
    sqlite3* mDb;
    bool mConnected;
    bool mReadOnly;
    int mBusyTimeoutMs;
    sqlite3_stmt* mCurrentStmt;
};

#endif // SQLITECONNECTION_H
