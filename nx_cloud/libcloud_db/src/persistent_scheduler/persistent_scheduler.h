#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
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

using OnTimerUserFunc = nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)>;

class AbstractPersistentScheduleEventReceiver
{
public:
    virtual OnTimerUserFunc persistentTimerFired(
        const QnUuid& taskId,
        const ScheduleParams& params) = 0;

    virtual ~AbstractPersistentScheduleEventReceiver() {}
};

class PersistentSheduler
{
    using FunctorToReceiverMap = std::unordered_map<QnUuid, AbstractPersistentScheduleEventReceiver*>;
public:
    PersistentSheduler(
        nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
        AbstractSchedulerDbHelper* dbHelper);
    ~PersistentSheduler();

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentScheduleEventReceiver* receiver);

    void start();
    void stop();

    /**
     * Functions below should be called inside dbHandler
     * of AbstractAsyncSqlExecutor::execute{Update,Select} or
     * from AbstractPersistentScheduleEventReceiver::persistentTimerFired()
     */
    nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        std::chrono::milliseconds timeout,
        const ScheduleParams& params);

    nx::db::DBResult unsubscribe(nx::db::QueryContext* queryContext, const QnUuid& taskId);

private:
    void addTimer(
        const QnUuid& functorId,
        const QnUuid& taskId,
        const ScheduleTaskInfo& taskInfo);

    void removeTimer(const QnUuid& taskId);

    nx::db::DBResult subscribe(
        nx::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo);

private:
    nx::db::AbstractAsyncSqlQueryExecutor* m_sqlExecutor;
    AbstractSchedulerDbHelper* m_dbHelper;
    FunctorToReceiverMap m_functorToReceiver;
    QnMutex m_mutex;
    ScheduleData m_scheduleData;
    std::unique_ptr<nx::utils::StandaloneTimerManager> m_timerManager;
    QnMutex m_timerManagerMutex;
    std::unordered_map<QnUuid, nx::utils::TimerId> m_taskToTimer;
};

}
}
