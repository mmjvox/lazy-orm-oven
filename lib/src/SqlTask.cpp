#include "SqlTask.h"

SqlTask::SqlTask(const std::string &sql)
    : mSql(sql)
{
    //
}

void SqlTask::setSqlQuery(const std::string &sql) {
    mSql = sql;
}

const std::string &SqlTask::getSqlQuery() const {
    return mSql;
}
