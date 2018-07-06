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

nx::utils::db::DBResult DataObject::save(
    nx::utils::db::QueryContext* queryContext,
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
        return nx::utils::db::DBResult::statementError;
    }

    QnSql::bind(stats, &saveRecordQuery);
    if (!saveRecordQuery.exec())
    {
        NX_LOGX(lm("Failed to save connect session statistics to DB. %1")
            .arg(saveRecordQuery.lastError().text()), cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult DataObject::readAllRecords(
    nx::utils::db::QueryContext* queryContext,
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
        return nx::utils::db::DBResult::statementError;
    }

    if (!fetchAllRecordsQuery.exec())
    {
        NX_LOGX(lm("Failed to fetch connect session statistics from DB. %1")
            .arg(fetchAllRecordsQuery.lastError().text()), cl_logDEBUG1);
        return nx::utils::db::DBResult::ioError;
    }

    QnSql::fetch_many(fetchAllRecordsQuery, connectionRecords);

    return nx::utils::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
