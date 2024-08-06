#include <drogon/drogon.h>
#include <Oven.h>

void initMainThread(){
    drogon::app().run();
}

int main(int argc, char** argv) 
{
    initMainThread();

    Oven oven;
    // oven.func1();
    oven.func2();

    // drogon::app().run();
    return 0;
}
