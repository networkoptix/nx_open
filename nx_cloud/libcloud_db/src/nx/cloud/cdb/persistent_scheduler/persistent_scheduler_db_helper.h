#pragma once

#include <nx/utils/db/query_context.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/db_structure_updater.h>
#include <nx/utils/uuid.h>

#include "persistent_scheduler_common.h"

namespace nx {
namespace cdb {

class AbstractSchedulerDbHelper
{
public:
    virtual ~AbstractSchedulerDbHelper() = default;

    virtual nx::utils::db::DBResult getScheduleData(
        nx::utils::db::QueryContext* queryContext,
        ScheduleData* scheduleData) const = 0;

    virtual nx::utils::db::DBResult subscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) = 0;

    virtual nx::utils::db::DBResult unsubscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& taskId) = 0;
};

class SchedulerDbHelper:
    public AbstractSchedulerDbHelper
{
public:
    SchedulerDbHelper(nx::utils::db::AbstractAsyncSqlQueryExecutor* sqlExecutor);

    bool initDb();

    virtual nx::utils::db::DBResult getScheduleData(
        nx::utils::db::QueryContext* queryContext,
        ScheduleData* scheduleData) const override;

    virtual nx::utils::db::DBResult subscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) override;

    virtual nx::utils::db::DBResult unsubscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& taskId) override;

private:
    nx::utils::db::DbStructureUpdater m_dbStructureUpdater;
};

} // namespace cdb
} // namespace nx
