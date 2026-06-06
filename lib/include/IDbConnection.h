#ifndef IDBCONNECTION_H
#define IDBCONNECTION_H

#include "SqlTask.h"
#include "Result.h"

namespace LazyOrm {

struct ExecMeta
{
    u_int64_t elapsedUs;
};

class IDbConnection
{
protected:
    std::string mLastError;
    unsigned long long mAffectedRows;

public:

    IDbConnection();
    ~IDbConnection();

    virtual bool connect() = 0;
    virtual std::shared_ptr<Result> exec(const SqlTask& task) = 0;
    virtual void close() = 0;
    virtual bool healthy() = 0;
    virtual unsigned long long getLastInsertId() = 0;

    std::string getLastError() const;
};

}

#endif // IDBCONNECTION_H
