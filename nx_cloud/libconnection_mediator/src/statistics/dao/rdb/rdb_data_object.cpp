#include "rdb_data_object.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/serialization/sql.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

nx::db::DBResult DataObject::save(
    nx::db::QueryContext* queryContext,
    ConnectSession stats)
{
    const char queryText[] = R"sql(
        INSERT INTO connect_session_statistics(
            start_time_utc, end_time_utc, result_code, session_id, originating_host_endpoint, 
            originating_host_name, destination_host_endpoint, destination_host_name)
        VALUES(
            :startTime, :endTime, :resultCode, :sessionId, :originatingHostEndpoint,
            :originatingHostName, :destinationHostEndpoint, :destinationHostName)
    )sql";
    
    QSqlQuery saveRecordQuery(*queryContext->connection());
    if (!saveRecordQuery.prepare(queryText))
    {
        NX_ASSERT(false);
        NX_LOGX(lm("Failed to prepare connect session statistics query. %1")
            .arg(saveRecordQuery.lastError().text()), cl_logDEBUG1);
        return nx::db::DBResult::statementError;
    }

    QnSql::bind(stats, &saveRecordQuery);

    //saveRecordQuery.bindValue(":start_time_utc", stats.startTime.time_since_epoch().count());
    //saveRecordQuery.bindValue(":end_time", stats.endTime.time_since_epoch().count());
    //saveRecordQuery.bindValue(":result_code", static_cast<int>(stats.resultCode));
    //saveRecordQuery.bindValue(":session_id", QLatin1String(stats.sessionId));
    //saveRecordQuery.bindValue(":originating_host_endpoint", stats.originatingHostEndpoint.toString());
    //saveRecordQuery.bindValue(":originating_host_name", QLatin1String(stats.originatingHostName));
    //saveRecordQuery.bindValue(":destination_host_endpoint", stats.destinationHostEndpoint.toString());
    //saveRecordQuery.bindValue(":destination_host_name", QLatin1String(stats.destinationHostName));

    if (!saveRecordQuery.exec())
    {
        NX_LOGX(lm("Failed to save connect session statistics to DB. %1")
            .arg(saveRecordQuery.lastError().text()), cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    return nx::db::DBResult::ok;
}

nx::db::DBResult DataObject::readAllRecords(
    nx::db::QueryContext* queryContext,
    std::deque<ConnectSession>* connectionRecords)
{
    const char queryText[] = R"sql(
        SELECT
            start_time_utc AS startTime,
            end_time_utc AS endTime,
            result_code AS resultCode,
            session_id AS sessionId,
            originating_host_endpoint AS originatingHostEndpoint,
            originating_host_name AS originatingHostName,
            destination_host_endpoint AS destinationHostEndpoint,
            destination_host_name AS destinationHostName
        FROM connect_session_statistics
    )sql";

    QSqlQuery fetchAllRecordsQuery(*queryContext->connection());
    fetchAllRecordsQuery.setForwardOnly(true);
    if (!fetchAllRecordsQuery.prepare(queryText))
    {
        NX_ASSERT(false);
        NX_LOGX(lm("Failed to prepare fetch connect session statistics query. %1")
            .arg(fetchAllRecordsQuery.lastError().text()), cl_logDEBUG1);
        return nx::db::DBResult::statementError;
    }

    if (!fetchAllRecordsQuery.exec())
    {
        NX_LOGX(lm("Failed to fetch connect session statistics from DB. %1")
            .arg(fetchAllRecordsQuery.lastError().text()), cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    QnSql::fetch_many(fetchAllRecordsQuery, connectionRecords);

    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
