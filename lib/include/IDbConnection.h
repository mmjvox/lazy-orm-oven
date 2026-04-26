#ifndef IDBCONNECTION_H
#define IDBCONNECTION_H

#include "SqlTask.h"

namespace LazyOrm {

class IDbConnection
{
protected:
    std::string mLastError;
    unsigned long long mAffectedRows;

public:

    enum QueryState{
        UndefinedQuery,
        QuerySuccess,
        QueryFailed,
        QueryExecuting,
        NoResult
    };

    IDbConnection();
    ~IDbConnection();

    virtual bool connect() = 0;
    virtual QueryState exec(const SqlTask& task) = 0;
    virtual void close() = 0;
    virtual bool healthy() = 0;
    virtual unsigned long long getLastInsertId() = 0;

    std::string getLastError() const;
};

}

#endif // IDBCONNECTION_H
