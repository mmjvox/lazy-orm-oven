#ifndef DBEXECUTOR_H
#define DBEXECUTOR_H

#include <vector>
#include <memory>
#include <atomic>
#include <trantor/net/EventLoop.h>

#include "SqlTask.h"
#include "WorkerContext.h"

namespace LazyOrm {

class DbExecutor
{
private:
    trantor::EventLoop* mMainLoop;
    std::vector<std::unique_ptr<WorkerContext>> mWorkers;
    std::atomic_uint64_t mNextTaskId{1};

public:
    explicit DbExecutor(trantor::EventLoop* mainLoop);

    void start(size_t workers);

    void stop();

    uint64_t submit(SqlTask&& task);

private:
    WorkerContext* pickWorker(const SqlTask&);
};

}

#endif // DBEXECUTOR_H
