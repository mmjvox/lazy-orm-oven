#include <trantor/net/EventLoop.h>
#include <iostream>


int main(int argc, char** argv)
{
    trantor::EventLoop loop;


    // oven.testAsync1(10);
    // oven.testAsync1(1);
    // oven.testAsync1(2);
    // oven.testAsync1(3);
    // oven.testAsync1(4);


    loop.loop();

    std::cout << "exit" << std::endl;

    return 0;
}
