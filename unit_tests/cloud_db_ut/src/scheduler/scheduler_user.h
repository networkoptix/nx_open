#pragma once

#include <persistent_scheduler/persistent_scheduler.h>

namespace nx {
namespace cdb {
namespace test {

template<typename Predicate>
void waitForPredicateBecomeTrue(Predicate p, const char* description)
{
    while (!p())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::cout <<"Waiting for " << description << std::endl;
    }
}

class SchedulerUser: public nx::cdb::AbstractPersistentScheduleEventReceiver
{
public:
    struct Task
    {
        int fired;
        bool subscribed;

        Task(int fired, int subscribed): fired(fired), subscribed(subscribed) {}
        Task(): fired(0), subscribed(false) {}
    };

    using UnsubscribeCb = nx::utils::MoveOnlyFunc<void(const QnUuid&, const Task& task)>;
    using SubscribeCb = nx::utils::MoveOnlyFunc<void(const QnUuid&)>;
    using TaskMap = std::unordered_map<QnUuid, Task>;

    SchedulerUser(
        nx::utils::db::AbstractAsyncSqlQueryExecutor* executor,
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
            //qWarning() << taskId << "fired";
            ++m_tasks[taskId].fired;
        }

        return
            [this](nx::utils::db::QueryContext* /*queryContext*/)
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

                return nx::utils::db::DBResult::ok;
            };
    }

    void unsubscribe(
        const QnUuid& taskId,
        UnsubscribeCb completionHandler = [](const QnUuid&, const Task&) {})
    {
        m_executor->executeUpdate(
            [this, taskId, completionHandler = std::move(completionHandler)](
                nx::utils::db::QueryContext* queryContext)
            {
                auto dbResult = m_scheduler->unsubscribe(queryContext, taskId);
                if (dbResult != nx::utils::db::DBResult::ok)
                    return dbResult;
                Task task;
                {
                    QnMutexLocker lock(&m_mutex);
                    NX_ASSERT(m_tasks.find(taskId) != m_tasks.cend());
                    m_tasks[taskId].subscribed = false;
                    task = m_tasks[taskId];
                }
                completionHandler(taskId, task);
                return nx::utils::db::DBResult::ok;
            },
            [](nx::utils::db::QueryContext* /*queryContext*/, nx::utils::db::DBResult /*result*/) {});
    }

    void subscribe(
        std::chrono::milliseconds timeout,
        SubscribeCb completionHandler = [](const QnUuid&) {})
    {
        nx::cdb::ScheduleParams params;
        params["key1"] = "value1";
        params["key2"] = "value1";

        m_executor->executeUpdate(
            [this, params, timeout, completionHandler = std::move(completionHandler)](
                nx::utils::db::QueryContext* queryContext)
            {
                QnUuid taskId;
                auto dbResult = m_scheduler->subscribe(
                    queryContext,
                    m_functorId,
                    &taskId,
                    timeout,
                    params);
                if (dbResult != nx::utils::db::DBResult::ok)
                    return dbResult;
                NX_ASSERT(!taskId.isNull());
                {
                    QnMutexLocker lock(&m_mutex);
                    //qWarning() << taskId << "subscribed";
                    m_tasks.emplace(taskId, Task(0, true));
                }
                completionHandler(taskId);
                return nx::utils::db::DBResult::ok;
            },
            [](nx::utils::db::QueryContext* /*queryContext*/, nx::utils::db::DBResult /*result*/)
            {
            });
    }

    void setShouldUnsubscribe(const QnUuid& taskId, UnsubscribeCb unsubscribeCb)
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
    nx::utils::db::AbstractAsyncSqlQueryExecutor* m_executor;
    nx::cdb::PersistentScheduler* m_scheduler;
    std::atomic<bool> m_shouldUnsubscribe{false};
    std::atomic<bool> m_shouldSubscribe{false};
    UnsubscribeCb m_unsubscribeCallback;
    QnUuid m_unsubscribeId;
    TaskMap m_tasks;
    QnMutex m_mutex;
    QnUuid m_functorId;
};


} // namespace test
} // namespace cdb
} // namespace nx

