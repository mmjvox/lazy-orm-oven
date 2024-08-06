#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <Oven.h>
#include <LazyOrm/LazyOrm.h>
#include <LazyOrm/Result.h>
#include <LazyOrm/ResultRow.h>
#include <LazyOrm/DbVariant.h>



void test_func_1(Oven *oven){

    LazyOrm::LazyOrm lazyOrm;
    lazyOrm[LazyOrm::SELECT]="repairs";
    lazyOrm<<"*";

    // oven.func1();
    // oven.func2();
    oven->execAsync(lazyOrm, [](LazyOrm::Result result){
        if(result.hasError()){
            std::cerr << result.errorMessage() << std::endl;
            return;
        }
        for(const auto rowItem : result){
            // std::cout << rowItem.toString() << std::endl;
            std::cout << rowItem["pyramid"].toULongLong() << " " << rowItem["carname"].toString() << std::endl;
        }

        if(result.size()>0){
            const auto firstRow  = result[0];
            std::cout <<"1: " << firstRow["pyramid"].toULongLong() << " " << firstRow["carname"].toString() << std::endl;
            std::cout <<"1: " << firstRow["pyramidx"].toULongLong() << " " << firstRow["car_name"].toString() << std::endl;
            std::cout <<"1: " << firstRow.at(1).toULongLong() << " " << firstRow.at(11).toString() << " " << firstRow.at(110).toString() << std::endl;
        }


        const auto row800  = result.value(800);
        std::cout <<"1: " << row800["pyramid"].toULongLong() << " " << row800["carname"].toString() << std::endl;
        std::cout <<"1: " << row800["pyramidx"].toULongLong() << " " << row800["car_name"].toString() << std::endl;
    });

}

void test_func_2(Oven *oven){

    LazyOrm::LazyOrm lazyOrm;
    lazyOrm[LazyOrm::SELECT]="repairs";
    lazyOrm<<"*";

    LazyOrm::Result result = oven->execSync(lazyOrm);
    if(result.hasError()){
        std::cerr << result.errorMessage() << std::endl;
        return;
    }
    for(const auto rowItem : result){
        // std::cout << rowItem.toString() << std::endl;
        std::cout << rowItem["pyramid"].toULongLong() << " " << rowItem["carname"].toString() << std::endl;
    }

    if(result.size()>0){
        const auto firstRow  = result[0];
        std::cout <<"1: " << firstRow["pyramid"].toULongLong() << " " << firstRow["carname"].toString() << std::endl;
        std::cout <<"1: " << firstRow["pyramidx"].toULongLong() << " " << firstRow["car_name"].toString() << std::endl;
        std::cout <<"1: " << firstRow.at(1).toULongLong() << " " << firstRow.at(11).toString() << " " << firstRow.at(110).toString() << std::endl;
    }


    const auto row800  = result.value(800);
    std::cout <<"1: " << row800["pyramid"].toULongLong() << " " << row800["carname"].toString() << std::endl;
    std::cout <<"1: " << row800["pyramidx"].toULongLong() << " " << row800["car_name"].toString() << std::endl;

}

int main(int argc, char** argv)
{
    QCoreApplication a(argc, argv);


    Oven oven;
    oven.func2();

    // QTimer::singleShot(1000, [&oven](){
    //     test_func_1(&oven);
    // });
    QTimer::singleShot(1000, [&oven](){
        // test_func_2(&oven);
    });

    qDebug()<<"a.exec()";

    return a.exec();
}
