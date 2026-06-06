#include "DbExecutor.h"
#include "IDbConnection.h"
#include "SqliteConnection.h"

namespace LazyOrm
{

DbExecutor::DbExecutor(
    trantor::EventLoop* loop)
    : mMainLoop(loop)
{
}

void DbExecutor::start(size_t workerCount){
    mWorkers.reserve(workerCount);

    for(size_t i=0;i<workerCount;i++)
    {
        auto worker = std::make_unique<WorkerContext>();

        worker->setWorkerId(i);

        worker->loopThread()->run();
        worker->setLoop(worker->loopThread()->getLoop());

        // worker->connection = createConnection();
        worker->setConnection(std::make_unique<SqliteConnection>("./aaa.db"));

        mWorkers.push_back(std::move(worker));
    }
}

uint64_t DbExecutor::submit(SqlTask&& task){
    task.setID(mNextTaskId++);

    auto* callback = new SuccessCallback([](DbResult&& result) {
        // Handle result
    });

    task.setSuccess(callback);

    auto* worker = pickWorker(task);

    worker->loop()->queueInLoop(
        [this,
         worker,
         task=std::move(task)]() mutable
        {

            worker->setBusy(true);

            auto result = worker->connection()->exec(task);

            worker->setExecutedQueries(worker->executedQueries()+1);

            worker->setBusy(false);

            mMainLoop->queueInLoop(
                [task=std::move(task),
                 result=std::move(result)]()
                mutable
                {
                    auto successCallback = task.success();
                    if (successCallback) {
                        (*successCallback)(std::move(result));
                    }
                });
        });

    return task.id();
}

WorkerContext* DbExecutor::pickWorker(const SqlTask&){
    static std::atomic_uint64_t rr{0};
    auto idx = rr++ % mWorkers.size();
    return mWorkers[idx].get();
}

}
