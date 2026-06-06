#include "SqlTask.h"

namespace LazyOrm {

SqlTask::SqlTask()
{

}

void SqlTask::setID(uint64_t newID)
{
    mID = newID;
}

void SqlTask::setSQL(const std::string &newSQL)
{
    mSQL = newSQL;
}

void SqlTask::setTimeoutMs(uint32_t newTimeoutMs)
{
    mTimeoutMs = newTimeoutMs;
}

void SqlTask::setTransactional(bool newTransactional)
{
    mTransactional = newTransactional;
}

void SqlTask::setTransactionId(uint64_t newTransactionId)
{
    mTransactionId = newTransactionId;
}

void SqlTask::setSuccess(SuccessCallback* newSuccess)
{
    if(newSuccess!=nullptr){
        mSuccess = newSuccess;
    }
}

void SqlTask::setError(ErrorCallback* newError)
{
    if(newError!=nullptr){
        mError = newError;
    }
}

void SqlTask::setSubmittedAt(std::chrono::steady_clock::time_point newSubmittedAt)
{
    mSubmittedAt = newSubmittedAt;
}

void SqlTask::setPriority(TaskPriority newPriority)
{
    mPriority = newPriority;
}

const std::string &SqlTask::getSqlQuery() const {
    return mSQL;
}

uint64_t SqlTask::id() const
{
    return mID;
}

uint32_t SqlTask::timeoutMs() const
{
    return mTimeoutMs;
}

bool SqlTask::transactional() const
{
    return mTransactional;
}

uint64_t SqlTask::transactionId() const
{
    return mTransactionId;
}

SuccessCallback* SqlTask::success() const
{
    return mSuccess;
}

ErrorCallback* SqlTask::error() const
{
    return mError;
}

std::chrono::steady_clock::time_point SqlTask::submittedAt() const
{
    return mSubmittedAt;
}

TaskPriority SqlTask::priority() const
{
    return mPriority;
}

}
