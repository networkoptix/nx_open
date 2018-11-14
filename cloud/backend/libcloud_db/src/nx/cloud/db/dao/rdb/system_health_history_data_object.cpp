#include "system_health_history_data_object.h"

#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

SystemHealthHistoryDataObject::SystemHealthHistoryDataObject()
{
}

nx::sql::DBResult SystemHealthHistoryDataObject::insert(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    const api::SystemHealthHistoryItem& historyItem)
{
    QSqlQuery insertHistoryItemQuery(*queryContext->connection()->qtSqlConnection());
    insertHistoryItemQuery.prepare(R"sql(
        INSERT INTO system_health_history(system_id, state, timestamp_utc)
        VALUES(:systemId, :state, :timestamp)
    )sql");

    insertHistoryItemQuery.bindValue(
        ":systemId", QnSql::serialized_field(systemId));
    insertHistoryItemQuery.bindValue(
        ":state", QnSql::serialized_field(static_cast<int>(historyItem.state)));
    insertHistoryItemQuery.bindValue(
        ":timestamp", QnSql::serialized_field(historyItem.timestamp));
    if (!insertHistoryItemQuery.exec())
    {
        const auto s = insertHistoryItemQuery.lastError().text();
        NX_DEBUG(this, lm("Could not insert system %1 history into DB. %2")
            .arg(systemId).arg(insertHistoryItemQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemHealthHistoryDataObject::selectHistoryBySystem(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    api::SystemHealthHistory* history)
{
    QSqlQuery selectSystemHealthHistoryQuery(
        *queryContext->connection()->qtSqlConnection());
    selectSystemHealthHistoryQuery.setForwardOnly(true);
    selectSystemHealthHistoryQuery.prepare(R"sql(
        SELECT state, timestamp_utc as timestamp
        FROM system_health_history
        WHERE system_id=:systemId
    )sql");
    selectSystemHealthHistoryQuery.bindValue(0, QnSql::serialized_field(systemId));
    if (!selectSystemHealthHistoryQuery.exec())
    {
        NX_DEBUG(this, lm("Error selecting system %1 history. %2")
            .arg(systemId).arg(selectSystemHealthHistoryQuery.lastError().text()));
        return nx::sql::DBResult::ioError;
    }

    QnSql::fetch_many(selectSystemHealthHistoryQuery, &history->events);

    return nx::sql::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
