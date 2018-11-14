#pragma once

#include <nx/sql/query_context.h>
#include <nx/sql/types.h>
#include <nx/sql/db_structure_updater.h>
#include <nx/utils/uuid.h>

#include "persistent_scheduler_common.h"

namespace nx::cloud::db {

class AbstractSchedulerDbHelper
{
public:
    virtual ~AbstractSchedulerDbHelper() = default;

    virtual nx::sql::DBResult getScheduleData(
        nx::sql::QueryContext* queryContext,
        ScheduleData* scheduleData) const = 0;

    virtual nx::sql::DBResult subscribe(
        nx::sql::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) = 0;

    virtual nx::sql::DBResult unsubscribe(
        nx::sql::QueryContext* queryContext,
        const QnUuid& taskId) = 0;
};

class SchedulerDbHelper:
    public AbstractSchedulerDbHelper
{
public:
    SchedulerDbHelper(nx::sql::AbstractAsyncSqlQueryExecutor* sqlExecutor);

    bool initDb();

    virtual nx::sql::DBResult getScheduleData(
        nx::sql::QueryContext* queryContext,
        ScheduleData* scheduleData) const override;

    virtual nx::sql::DBResult subscribe(
        nx::sql::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) override;

    virtual nx::sql::DBResult unsubscribe(
        nx::sql::QueryContext* queryContext,
        const QnUuid& taskId) override;

private:
    nx::sql::DbStructureUpdater m_dbStructureUpdater;
};

} // namespace nx::cloud::db
