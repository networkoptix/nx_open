#pragma once

#include <persistent_scheduler/persistent_scheduler.h>

namespace nx {
namespace cdb {
namespace test {

class SchedulerUser : public nx::cdb::AbstractPersistentScheduleEventReceiver
{
public:
    struct Task
    {
        int fired;
        bool subscribed;

        Task(int fired, int subscribed) : fired(fired), subscribed(subscribed) {}
        Task() : fired(0), subscribed(false) {}
    };

    using TaskMap = std::unordered_map<QnUuid, Task>;

    SchedulerUser(
        nx::db::AbstractAsyncSqlQueryExecutor* executor,
        nx::cdb::PersistentScheduler* scheduler,
        const QnUuid& functorId)
        :
        m_executor(executor),
        m_scheduler(scheduler),
        m_functorId(functorId)
    {
        m_scheduler->registerEventReceiver(m_functorId, this);
    }

    virtual OnTimerUserFunc persistentTimerFired(
        const QnUuid& taskId,
        const ScheduleParams& params) override
    {
        NX_ASSERT(params.find("key1") != params.cend());
        NX_ASSERT(params.find("key2") != params.cend());

        {
            QnMutexLocker lock(&m_mutex);
            ++m_tasks[taskId].fired;
        }

        return
            [this](nx::db::QueryContext*)
        {
            if (m_shouldSubscribe)
            {
                subscribe(std::chrono::milliseconds(10));
                m_shouldSubscribe = false;
            }

            if (m_shouldUnsubscribe)
            {
                unsubscribe(m_unsubscribeId, std::move(m_unsubscribeCallback));
                m_shouldUnsubscribe = false;
            }

            return nx::db::DBResult::ok;
        };
    }

    void unsubscribe(
        const QnUuid& taskId,
        nx::utils::MoveOnlyFunc<void(const Task& task)> completionHandler = [](const Task&) {})
    {
        m_executor->executeUpdate(
            [this, taskId, completionHandler = std::move(completionHandler)](nx::db::QueryContext* queryContext)
        {
            auto dbResult = m_scheduler->unsubscribe(queryContext, taskId);
            if (dbResult != nx::db::DBResult::ok)
                return dbResult;
            Task task;
            {
                QnMutexLocker lock(&m_mutex);
                NX_ASSERT(m_tasks.find(taskId) != m_tasks.cend());
                m_tasks[taskId].subscribed = false;
                task = m_tasks[taskId];
            }
            completionHandler(task);
            return nx::db::DBResult::ok;
        },
            [](nx::db::QueryContext*, nx::db::DBResult)
        {
        });
    }

    void subscribe(
        std::chrono::milliseconds timeout,
        nx::utils::MoveOnlyFunc<void(const QnUuid&)> completionHandler = [](const QnUuid&) {})
    {
        nx::cdb::ScheduleParams params;
        params["key1"] = "value1";
        params["key2"] = "value1";

        m_executor->executeUpdate(
            [this, params, timeout, completionHandler = std::move(completionHandler)](nx::db::QueryContext* queryContext)
        {
            QnUuid taskId;
            auto dbResult = m_scheduler->subscribe(
                queryContext,
                m_functorId,
                &taskId,
                timeout,
                params);
            if (dbResult != nx::db::DBResult::ok)
                return dbResult;
            NX_ASSERT(!taskId.isNull());
            {
                QnMutexLocker lock(&m_mutex);
                m_tasks.emplace(taskId, Task(0, true));
            }
            completionHandler(taskId);
            return nx::db::DBResult::ok;
        },
            [](nx::db::QueryContext*, nx::db::DBResult result)
        {
        });
    }

    void setShouldUnsubscribe(
        const QnUuid& taskId,
        nx::utils::MoveOnlyFunc<void(const nx::cdb::test::SchedulerUser::Task&)> unsubscribeCb)
    {
        m_unsubscribeId = taskId;
        m_unsubscribeCallback = std::move(unsubscribeCb);
        m_shouldUnsubscribe = true;
    }

    void setShouldSubscribe() { m_shouldSubscribe = true; }
    TaskMap tasks()
    {
        QnMutexLocker lock(&m_mutex);
        return m_tasks;
    }

private:
    nx::db::AbstractAsyncSqlQueryExecutor* m_executor;
    nx::cdb::PersistentScheduler* m_scheduler;
    std::atomic<bool> m_shouldUnsubscribe{ false };
    std::atomic<bool> m_shouldSubscribe{ false };
    nx::utils::MoveOnlyFunc<void(const Task& task)> m_unsubscribeCallback;
    QnUuid m_unsubscribeId;
    TaskMap m_tasks;
    QnMutex m_mutex;
    QnUuid m_functorId;
};


}
}
}

