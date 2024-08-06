#pragma once

#include <drogon/HttpSimpleController.h>

using namespace drogon;

class Test : public drogon::HttpSimpleController<Test>
{
  public:
    void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    // list path definitions here;
    PATH_ADD("/test", Get,Post);
    PATH_LIST_END
};
