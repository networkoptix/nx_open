#pragma once

#include <utils/db/query_context.h>
#include <utils/db/types.h>
#include <utils/db/db_structure_updater.h>
#include <nx/utils/uuid.h>
#include "persistent_scheduler_common.h"

namespace nx {
namespace cdb {

class AbstractSchedulerDbHelper
{
public:
    virtual nx::db::DBResult getScheduleData(
        nx::db::QueryContext* queryContext,
        ScheduleData* scheduleData) const = 0;

    virtual nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) = 0;

    virtual nx::db::DBResult unsubscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& taskId) = 0;

};

class SchedulerDbHelper : public AbstractSchedulerDbHelper
{
public:
    SchedulerDbHelper(nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor);

    bool initDb();

    virtual nx::db::DBResult getScheduleData(
        nx::db::QueryContext* queryContext,
        ScheduleData* scheduleData) const override;

    virtual nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo) override;

    virtual nx::db::DBResult unsubscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& taskId) override;

private:
    nx::db::DbStructureUpdater m_dbStructureUpdater;
};

}
}
