#include "WorkerContext.h"

namespace LazyOrm {


WorkerContext::WorkerContext() {
    mLoopThread = std::make_unique<trantor::EventLoopThread>();

}

uint32_t WorkerContext::workerId() const
{
    return mWorkerId;
}

void WorkerContext::setWorkerId(uint32_t newWorkerId)
{
    mWorkerId = newWorkerId;
}

trantor::EventLoopThread*WorkerContext::loopThread() const
{
    return mLoopThread.get();
}

trantor::EventLoop *WorkerContext::loop() const
{
    return mLoop;
}

void WorkerContext::setLoop(trantor::EventLoop *newLoop)
{
    mLoop = newLoop;
}

IDbConnection*WorkerContext::connection() const
{
    return mConnection.get();
}

void WorkerContext::setConnection(std::unique_ptr<IDbConnection> newConnection)
{
    mConnection = std::move(newConnection);
}

uint64_t WorkerContext::executedQueries() const
{
    return mExecutedQueries.load(std::memory_order_acquire);
}

void WorkerContext::setExecutedQueries(uint64_t newExecutedQueries)
{
    mExecutedQueries.exchange(newExecutedQueries, std::memory_order_acq_rel);
}

bool WorkerContext::busy() const
{
    return mBusy.load(std::memory_order_acquire);
}

void WorkerContext::setBusy(bool newBusy)
{
    mBusy.exchange(newBusy, std::memory_order_acq_rel);
}

}
