#include <trantor/net/EventLoop.h>
#include <iostream>
#include "DbExecutor.h"


int main(int argc, char** argv)
{
    trantor::EventLoop loop;

    LazyOrm::DbExecutor exec(&loop);

    exec.start(8);

    std::cout << "exit" << std::endl;

    return 0;
}
