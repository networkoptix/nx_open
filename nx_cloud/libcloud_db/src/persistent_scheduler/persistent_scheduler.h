#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <nx/utils/uuid.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/db/async_sql_query_executor.h>
#include "persistent_scheduler_db_helper.h"
#include "persistent_scheduler_common.h"

namespace nx {
namespace cdb {

class PersistentScheduler;

using OnTimerUserFunc = nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)>;

class AbstractPersistentScheduleEventReceiver
{
public:
    virtual OnTimerUserFunc persistentTimerFired(
        const QnUuid& taskId,
        const ScheduleParams& params) = 0;

    virtual ~AbstractPersistentScheduleEventReceiver() {}
};

class PersistentScheduler
{
    using FunctorToReceiverMap = std::unordered_map<QnUuid, AbstractPersistentScheduleEventReceiver*>;
public:
    PersistentScheduler(
        nx::utils::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
        AbstractSchedulerDbHelper* dbHelper);
    ~PersistentScheduler();

    void registerEventReceiver(
        const QnUuid& functorId,
        AbstractPersistentScheduleEventReceiver* receiver);

    void start();
    void stop();

    nx::utils::db::DBResult subscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        std::chrono::milliseconds timeout,
        const ScheduleParams& params);

    nx::utils::db::DBResult unsubscribe(nx::utils::db::QueryContext* queryContext, const QnUuid& taskId);

private:
    void addTimer(
        const QnUuid& functorId,
        const QnUuid& taskId,
        const ScheduleTaskInfo& taskInfo);

    void removeTimer(const QnUuid& taskId);

    nx::utils::db::DBResult subscribe(
        nx::utils::db::QueryContext* queryContext,
        const QnUuid& functorId,
        QnUuid* outTaskId,
        const ScheduleTaskInfo& taskInfo);

    void timerFunction(
        const QnUuid& functorId,
        const QnUuid& taskId,
        const nx::cdb::ScheduleParams& params);

private:
    nx::utils::db::AbstractAsyncSqlQueryExecutor* m_sqlExecutor;
    AbstractSchedulerDbHelper* m_dbHelper;
    FunctorToReceiverMap m_functorToReceiver;
    QnMutex m_mutex;
    ScheduleData m_scheduleData;
    std::unique_ptr<nx::utils::StandaloneTimerManager> m_timerManager;
    QnMutex m_timerManagerMutex;
    std::unordered_map<QnUuid, nx::utils::TimerId> m_taskToTimer;
    std::unordered_map<QnUuid, std::unordered_map<QnUuid, ScheduleParams>> m_functorToTaskToParams;
    std::unordered_set<QnUuid> m_delayed;
};

}
}
