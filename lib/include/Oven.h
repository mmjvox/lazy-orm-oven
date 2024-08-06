#ifndef OVEN_H
#define OVEN_H

#include <trantor/net/EventLoopThread.h>
#include <drogon/drogon.h>
#include <LazyOrm/LazyOrm.h>
#include <LazyOrm/Result.h>


class Oven
{
private:
    trantor::EventLoopThread objectThread;

public:
    Oven();

    void execAsync(LazyOrm::LazyOrm lazyOrm, const std::function<void(LazyOrm::Result)>& resultCallback);
    LazyOrm::Result execSync(LazyOrm::LazyOrm lazyOrm);

    void func1();
    void func2();
};

#endif // OVEN_H
