#include "Oven.h"
#include <iostream>
#include <LazyOrm/DbVariant.h>
#include <LazyOrm/ResultRow.h>
#include "VersionInfo.h"


Oven::Oven() {
    printInfo();
    objectThread.run();
    drogon::app().loadConfigFile("./config.json");

    if(!drogon::app().isRunning()){
        std::thread([](){drogon::app().run();}).detach();
    }
}

void Oven::execAsync(LazyOrm::LazyOrm lazyOrm, const std::function<void(LazyOrm::Result)>& resultCallback)
{
    objectThread.getLoop()->runInLoop([lazyOrm, resultCallback]() {

        auto dbClientPtr = drogon::app().getDbClient("monitoring");

        auto dbms_type = static_cast<LazyOrm::LazyOrm::DBMS_TYPE>(dbClientPtr->type());


        dbClientPtr->execSqlAsync(lazyOrm.queryString(dbms_type),
            [resultCallback](const drogon::orm::Result &drogonOrmResult){

                size_t columnsSize = drogonOrmResult.columns();
                LazyOrm::Result lazyOrmResult;
                for(const auto &row : drogonOrmResult){
                    LazyOrm::ResultRow resultRow;
                    for(int i=0; i<columnsSize; i++){
                        std::string columnName = drogonOrmResult.columnName(i);
                        resultRow.insert(columnName, row[columnName].as<std::string>());
                    }
                    lazyOrmResult.push_back(resultRow);
                }

                resultCallback(lazyOrmResult);
            },
            [resultCallback](const drogon::orm::DrogonDbException &e){
                LazyOrm::Result lazyOrmResult;
                lazyOrmResult.setError(e.base().what());
                resultCallback(lazyOrmResult);
            });

    });
}

LazyOrm::Result Oven::execSync(LazyOrm::LazyOrm lazyOrm){
    objectThread.getLoop()->runInLoop([lazyOrm]() {

        auto dbClientPtr = drogon::app().getDbClient("monitoring");

        auto dbms_type = static_cast<LazyOrm::LazyOrm::DBMS_TYPE>(dbClientPtr->type());

        auto drogonOrmResult = dbClientPtr->execSqlSync(lazyOrm.queryString(dbms_type));

        size_t columnsSize = drogonOrmResult.columns();
        LazyOrm::Result lazyOrmResult;
        for(const auto &row : drogonOrmResult){
            LazyOrm::ResultRow resultRow;
            for(int i=0; i<columnsSize; i++){
                std::string columnName = drogonOrmResult.columnName(i);
                resultRow.insert(columnName, row[columnName].as<std::string>());
            }
            lazyOrmResult.push_back(resultRow);
        }

        return lazyOrmResult;
    });

    return LazyOrm::Result{};
}

void Oven::func1(){
    objectThread.getLoop()->runAfter(std::chrono::seconds(4), []() {

        LazyOrm::LazyOrm lazyOrm;
        lazyOrm[LazyOrm::SELECT]="repairs";
        lazyOrm<<"*";

        auto dbClientPtr = drogon::app().getDbClient("monitoring");


        dbClientPtr->execSqlAsync(lazyOrm.queryString(LazyOrm::LazyOrm::Sqlite3),
            [](const drogon::orm::Result &drogonOrmResult){

                size_t columnsSize = drogonOrmResult.columns();

                LazyOrm::Result lazyOrmResult;
                for(const auto &row : drogonOrmResult){
                    LazyOrm::ResultRow resultRow;
                    for(int i=0; i<columnsSize; i++){
                        std::string columnName = drogonOrmResult.columnName(i);
                        resultRow.insert(columnName, row[columnName].as<std::string>());
                    }
                    lazyOrmResult.push_back(resultRow);
                }

                std::cout << lazyOrmResult.toString() << std::endl;
            },
            [](const drogon::orm::DrogonDbException &e){});

    });
}

void Oven::func2(){
    objectThread.getLoop()->runAfter(std::chrono::seconds(1), []() {

        std::string query = "SELECT * FROM repairs";
        std::cout << query << std::endl;
        auto dbClientPtr = drogon::app().getDbClient("monitoring");


        auto drogonOrmResult = dbClientPtr->execSqlSync(query);

        size_t columnsSize = drogonOrmResult.columns();

        LazyOrm::Result lazyOrmResult;
        for(const auto &row : drogonOrmResult){
            LazyOrm::ResultRow resultRow;
            for(int i=0; i<columnsSize; i++){
                std::string columnName = drogonOrmResult.columnName(i);
                resultRow.insert(columnName, row[columnName].as<std::string>());
            }
            lazyOrmResult.push_back(resultRow);
        }

        std::cout << lazyOrmResult.toString() << std::endl;

    });
}
