#ifndef SQLTASK_H
#define SQLTASK_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <chrono>


namespace LazyOrm {

class DbResult;
class DbException;

using SuccessCallback = std::function<void(DbResult&&)>;

using ErrorCallback = std::function<void(const DbException&)>;

enum class TaskPriority : uint8_t
{
    Low=0,
    Normal=1,
    High=2
};

class SqlTask
{

private:
    uint64_t mID;
    std::string mSQL;
    uint32_t mTimeoutMs = 5000;
    bool mTransactional = false;
    uint64_t mTransactionId = 0;
    SuccessCallback *mSuccess;
    ErrorCallback *mError;
    std::chrono::steady_clock::time_point mSubmittedAt;
    TaskPriority mPriority = TaskPriority::Normal;

public:
    SqlTask();

    const std::string& getSqlQuery() const;
    uint64_t id() const;
    uint32_t timeoutMs() const;
    bool transactional() const;
    uint64_t transactionId() const;
    SuccessCallback* success() const;
    ErrorCallback* error() const;
    std::chrono::steady_clock::time_point submittedAt() const;
    TaskPriority priority() const;

    void setID(uint64_t newID);
    void setSQL(const std::string &newSQL);
    void setTimeoutMs(uint32_t newTimeoutMs);
    void setTransactional(bool newTransactional);
    void setTransactionId(uint64_t newTransactionId);
    void setSuccess(SuccessCallback* newSuccess);
    void setError(ErrorCallback* newError);
    void setSubmittedAt(std::chrono::steady_clock::time_point newSubmittedAt);
    void setPriority(TaskPriority newPriority);
};

}

#endif // SQLTASK_H
