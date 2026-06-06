#ifndef WORKERCONTEXT_H
#define WORKERCONTEXT_H

#include <memory>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/EventLoopThread.h>

namespace LazyOrm {

class IDbConnection;

class WorkerContext
    : private trantor::NonCopyable
{
private:
    uint32_t mWorkerId;
    std::unique_ptr<trantor::EventLoopThread> mLoopThread;
    trantor::EventLoop* mLoop=nullptr;
    std::unique_ptr<IDbConnection> mConnection;
    std::atomic_uint64_t mExecutedQueries{0};
    std::atomic_bool mBusy{false};

public:
    WorkerContext();

    uint32_t workerId() const;
    trantor::EventLoopThread* loopThread() const;
    trantor::EventLoop* loop() const;
    IDbConnection* connection() const;
    uint64_t executedQueries() const;
    bool busy() const;

    void setWorkerId(uint32_t newWorkerId);
    void setLoop(trantor::EventLoop *newLoop);
    void setConnection(std::unique_ptr<IDbConnection> newConnection);
    void setExecutedQueries(uint64_t newExecutedQueries);
    void setBusy(bool newBusy);
};

}

#endif // WORKERCONTEXT_H
