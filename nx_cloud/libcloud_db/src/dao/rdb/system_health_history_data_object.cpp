#include "system_health_history_data_object.h"

#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

SystemHealthHistoryDataObject::SystemHealthHistoryDataObject()
{
}

nx::db::DBResult SystemHealthHistoryDataObject::insert(
    nx::db::QueryContext* queryContext,
    const std::string& systemId,
    const api::SystemHealthHistoryItem& historyItem)
{
    QSqlQuery insertHistoryItemQuery(*queryContext->connection());
    insertHistoryItemQuery.prepare(
        R"sql(
        INSERT INTO system_health_history(system_id, state, timestamp_utc)
        VALUES(:systemId, :state, :timestamp)
        )sql");

    insertHistoryItemQuery.bindValue(":systemId", QnSql::serialized_field(systemId));
    QnSql::bind(historyItem, &insertHistoryItemQuery);
    if (!insertHistoryItemQuery.exec())
    {
        NX_LOG(lm("Could not insert system %1 history into DB. %2")
            .arg(systemId).arg(insertHistoryItemQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult SystemHealthHistoryDataObject::selectHistoryBySystem(
    nx::db::QueryContext* queryContext,
    const std::string& systemId,
    api::SystemHealthHistory* history)
{
    QSqlQuery selectSystemHealthHistoryQuery(*queryContext->connection());
    selectSystemHealthHistoryQuery.setForwardOnly(true);
    selectSystemHealthHistoryQuery.prepare(
        R"sql(
        SELECT state, timestamp_utc as timestamp
        FROM system_health_history
        WHERE system_id=:systemId
        )sql");
    selectSystemHealthHistoryQuery.bindValue(0, QnSql::serialized_field(systemId));
    if (!selectSystemHealthHistoryQuery.exec())
    {
        NX_LOG(lm("Error selecting system %1 history. %2")
            .arg(systemId).arg(selectSystemHealthHistoryQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }
    
    QnSql::fetch_many(selectSystemHealthHistoryQuery, &history->items);

    return db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
