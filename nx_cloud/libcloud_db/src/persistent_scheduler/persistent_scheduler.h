#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <utils/db/async_sql_query_executor.h>
#include "persistent_scheduler_db_helper.h"
#include "persistent_scheduler_common.h"

namespace nx {
namespace db {

class QueryContext;

}
}

namespace nx {
namespace cdb {

class PersistentSheduler;

class AbstractPersistentScheduleEventReceiver
{
public:
    virtual void persistentTimerFired(
        const QnUuid& taskId,
        const ScheduleParams& params,
        nx::db::QueryContext* context) = 0;

    virtual ~AbstractPersistentScheduleEventReceiver() {}
};

class PersistentSheduler
{
    using FunctorToReceiverMap = std::unordered_map<QnUuid, AbstractPersistentScheduleEventReceiver*>;
public:
    PersistentSheduler(
        nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
        AbstractSchedulerDbHelper* dbHelper);

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentScheduleEventReceiver* receiver);

    void start();

    nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        std::chrono::milliseconds timeout,
        const ScheduleParams& params);

    void unsubscribe(nx::db::QueryContext* queryContext, const QnUuid& taskId);

private:
    nx::db::AbstractAsyncSqlQueryExecutor* m_sqlExecutor;
    AbstractSchedulerDbHelper* m_dbHelper;
    FunctorToReceiverMap m_functorToReceiver;
    QnMutex m_mutex;
    ScheduleData m_scheduleData;
};

}
}
