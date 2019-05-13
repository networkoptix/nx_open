#include <string>

#include <QtSql>

#include <nx/utils/log/log.h>

#include "persistent_scheduler_db_helper.h"

namespace nx::cloud::db {

static const int kSchedulerDbStartingVersion = 1;
static const std::string kPersistentShedulerStructureName =
    "scheduler_7AF3398A-CFE0-4AEF-8D2F-F8FBD7B5F0BA";
static const char* const kSchedulerDbScheme = R"sql(
    CREATE TABLE schedule_data(
        functor_type_id     VARCHAR(64) NOT NULL,
        task_id             VARCHAR(64) NOT NULL,
        schedule_point      BIGINT,
        period              BIGINT,
        param_key           VARCHAR(4096) NOT NULL,
        param_value         VARCHAR(4096) NOT NULL
    );
)sql";

SchedulerDbHelper::SchedulerDbHelper(nx::sql::AbstractAsyncSqlQueryExecutor* sqlExecutor):
    m_dbStructureUpdater(kPersistentShedulerStructureName, sqlExecutor)
{
    m_dbStructureUpdater.addFullSchemaScript(kSchedulerDbStartingVersion, kSchedulerDbScheme);
}

bool SchedulerDbHelper::initDb()
{
    return m_dbStructureUpdater.updateStructSync();
}

nx::sql::DBResult SchedulerDbHelper::getScheduleData(
    nx::sql::QueryContext* queryContext,
    ScheduleData* scheduleData) const
{
    QSqlQuery scheduleDataQuery(*queryContext->connection()->qtSqlConnection());
    scheduleDataQuery.setForwardOnly(true);
    scheduleDataQuery.prepare(R"sql( SELECT * FROM schedule_data )sql");

    if (!scheduleDataQuery.exec())
    {
        NX_ERROR(this, "[Scheduler, db] Failed to fetch schedule data");
        return nx::sql::DBResult::ioError;
    }

    while (scheduleDataQuery.next())
    {
        QnUuid functorId = QnUuid::fromRfc4122(scheduleDataQuery.value(0).toByteArray());
        QnUuid taskId = QnUuid::fromRfc4122(scheduleDataQuery.value(1).toByteArray());
        qint64 schedulePointMs = scheduleDataQuery.value(2).toLongLong();
        qint64 periodMs = scheduleDataQuery.value(3).toLongLong();
        QString paramKey = scheduleDataQuery.value(4).toString();
        QString paramValue = scheduleDataQuery.value(5).toString();

        scheduleData->functorToTasks[functorId].emplace(taskId);
        scheduleData->taskToParams[taskId].params.emplace(
            paramKey.toStdString(),
            paramValue.toStdString());
        scheduleData->taskToParams[taskId].schedulePoint =
                std::chrono::steady_clock::time_point() + std::chrono::milliseconds(schedulePointMs);
        scheduleData->taskToParams[taskId].period = std::chrono::milliseconds(periodMs);
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SchedulerDbHelper::subscribe(
    nx::sql::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    const ScheduleTaskInfo& taskInfo)
{
    QSqlQuery subscribeQuery(*queryContext->connection()->qtSqlConnection());
    subscribeQuery.prepare(R"sql(
        INSERT INTO schedule_data(functor_type_id, task_id, schedule_point, period, param_key, param_value)
        VALUES(:functorId, :taskId, :schedulePoint, :period, :paramKey, :paramValue)
    )sql");

    *outTaskId = QnUuid::createUuid();

    for (const auto& param: taskInfo.params)
    {
        subscribeQuery.bindValue(":functorId", functorId.toRfc4122());
        subscribeQuery.bindValue(":taskId", outTaskId->toRfc4122());
        subscribeQuery.bindValue(
            ":schedulePoint",
            (qint64)std::chrono::duration_cast<std::chrono::milliseconds>(
                taskInfo.schedulePoint.time_since_epoch()).count());

        subscribeQuery.bindValue(":period", (qint64)taskInfo.period.count());
        subscribeQuery.bindValue(":paramKey", QString::fromStdString(param.first));
        subscribeQuery.bindValue(":paramValue", QString::fromStdString(param.second));

        if (!subscribeQuery.exec())
        {
            NX_ERROR(this, "[Scheduler, db] Failed to insert subscribe info, error: %1",
                subscribeQuery.lastError().text());
            return nx::sql::DBResult::ioError;
        }
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SchedulerDbHelper::unsubscribe(
    nx::sql::QueryContext* queryContext,
    const QnUuid& taskId)
{
    QSqlQuery unsubscribeQuery(*queryContext->connection()->qtSqlConnection());
    unsubscribeQuery.prepare(R"sql( DELETE FROM schedule_data WHERE task_id = :taskId )sql");
    unsubscribeQuery.bindValue(":taskId", taskId.toRfc4122());

    if (!unsubscribeQuery.exec())
    {
        NX_ERROR(this, "[Scheduler, db] Failed to delete task info, error: %1",
            unsubscribeQuery.lastError().text());
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

} // namespace nx::cloud::db
