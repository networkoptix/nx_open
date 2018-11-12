#pragma once

#include <memory>
#include <string>

#include <QtSql/QSqlDatabase>

#include "query.h"
#include "types.h"

namespace nx::sql {

class AbstractDbConnection
{
public:
    virtual ~AbstractDbConnection() = default;

    virtual bool open() = 0;
    virtual void close() = 0;

    virtual bool begin() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    virtual DBResult lastError() = 0;
    virtual std::string lastErrorText() = 0;

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() = 0;

    // TODO: #ak Remove this method. This requires switching every SqlQuery usage to createQuery().
    virtual QSqlDatabase* qtSqlConnection() = 0;
};

} // namespace nx::sql
