#include "Test.h"
#include <LazyOrm/DbVariant.h>
#include <LazyOrm/Result.h>
#include <LazyOrm/ResultRow.h>

void Test::asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback)
{
    std::string query = "SELECT * FROM repairs";

    auto dbClientPtr = drogon::app().getDbClient("monitoring");

    dbClientPtr->execSqlAsync(query,
        [req, callback](const drogon::orm::Result &drogonOrmResult){

            size_t columnsSize = drogonOrmResult.columns();

            LazyOrm::Result lazyOrmResult;
            for(const auto &row : drogonOrmResult){
                LazyOrm::ResultRow resultRow;
                for(int i=0; i<columnsSize; i++){
                    std::string columnName = drogonOrmResult.columnName(i);
                    // LazyOrm::DbVariant var = row[columnName].as<std::string>();
                    resultRow[columnName] = row[columnName].as<std::string>();
                    // std::cout << columnName << " (" << var.typeName() << "): " << var.toString() << std::endl;
                }
                lazyOrmResult.push_back(resultRow);
            }


            auto resp=HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->setBody(lazyOrmResult.toIndentedString());
            callback(resp);
        },
        [callback](const drogon::orm::DrogonDbException &e){

            auto resp=HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->setBody("stationsName.dump()");
            callback(resp);
        });


}
