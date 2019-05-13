#pragma once

#include <memory>
#include <string>
#include <tuple>

#include <QtSql/QSqlDatabase>

#include <nx/utils/type_utils.h>

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

    /**
     * Arguments are bound using AbstractSqlQuery::addBindValue.
     * NOTE: Throws on failure.
     */
    template<typename... Args>
    void executeQuery(
        const std::string& sqlText,
        Args&&... args)
    {
        auto query = createQuery();
        query->prepare(sqlText.c_str());

        nx::utils::apply_each(
            [&query](const auto& arg) { query->addBindValue(arg); },
            std::forward<Args>(args)...);

        query->exec();
    }
};

} // namespace nx::sql
