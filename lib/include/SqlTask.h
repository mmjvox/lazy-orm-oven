#ifndef SQLTASK_H
#define SQLTASK_H

#include <string>

class SqlTask
{
public:
    SqlTask() = default;
    explicit SqlTask(const std::string& sql);

    void setSqlQuery(const std::string& sql);
    const std::string& getSqlQuery() const;

private:
    std::string mSql;
};

#endif // SQLTASK_H
