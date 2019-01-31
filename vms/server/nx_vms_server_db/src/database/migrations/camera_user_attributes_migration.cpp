#include "camera_user_attributes_migration.h"

#include <QtSql/QtSql>

#include <nx/fusion/model_functions.h>

#include <nx/utils/uuid.h>
#include <nx/sql/sql_query_execution_helper.h>

namespace ec2 {
namespace db {

namespace detail {

struct ScheduleTaskThresholdsWithRefData
{
    qint16 before_threshold = 0;
    qint16 after_threshold = 0;
    int camera_attrs_id = 0;
};
#define ScheduleTaskThresholdsWithRefData_Fields (before_threshold)(after_threshold)(camera_attrs_id)

#define MIGRATION_PARAM_TYPES \
    (ScheduleTaskThresholdsWithRefData)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(MIGRATION_PARAM_TYPES, (sql_record))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    MIGRATION_PARAM_TYPES, (sql_record), _Fields)

using ScheduleTasks = std::vector<ScheduleTaskThresholdsWithRefData>;
using nx::sql::SqlQueryExecutionHelper;

bool fetchScheduleTasks(const QSqlDatabase& database, ScheduleTasks& scheduleTasks)
{
    // All schedule tasks must have the same thresholds.
    const auto queryText = R"sql(
        SELECT DISTINCT camera_attrs_id, before_threshold, after_threshold
        FROM vms_scheduletask
        ORDER BY camera_attrs_id
    )sql";

    QSqlQuery query(database);
    query.setForwardOnly(true);

    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, queryText, Q_FUNC_INFO))
        return false;

    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QnSql::fetch_many(query, &scheduleTasks);
    return true;
}

bool updateRecordingThresholds(const QSqlDatabase& database, const ScheduleTasks& scheduleTasks)
{
    const auto queryText = R"sql(
        UPDATE vms_camera_user_attributes
        SET record_before_motion_sec = :before_threshold,
            record_after_motion_sec = :after_threshold
        WHERE id = :camera_attrs_id
    )sql";

    QSqlQuery query(database);

    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, queryText, Q_FUNC_INFO))
        return false;

    for (const auto& task: scheduleTasks)
    {
        QnSql::bind(task, &query);
        if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace detail

bool migrateRecordingThresholds(const QSqlDatabase& database)
{
    detail::ScheduleTasks scheduleTasks;

    // Select recording thresholds for each camera, that has schedule tasks.
    if (!fetchScheduleTasks(database, scheduleTasks))
        return false;

    // Migrate these thresholds to the camera attributes table.
    return updateRecordingThresholds(database, scheduleTasks);
}

} // namespace db
} // namespace ec2
