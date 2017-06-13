#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/utils/std/future.h>
#include <chrono>
#include <persistent_scheduler/persistent_scheduler.h>
#include <persistent_scheduler/persistent_scheduler_db_helper.h>

namespace nx {
namespace cdb {
namespace test {

using namespace ::testing;

class DbHelperStub: public nx::cdb::AbstractSchedulerDbHelper
{
public:
    MOCK_CONST_METHOD2(getScheduleData, nx::db::DBResult(nx::db::QueryContext*, nx::cdb::ScheduleData*));
    MOCK_METHOD2(registerEventReceiver, nx::db::DBResult(nx::db::QueryContext*, const QnUuid&));
    MOCK_METHOD4(
        subscribe,
        nx::db::DBResult(
            nx::db::QueryContext*,
            const QnUuid&,
            QnUuid*,
            const ScheduleTaskInfo&));
    MOCK_METHOD2(unsubscribe, nx::db::DBResult(nx::db::QueryContext*, const QnUuid&));
};

class SchedulerUser: public nx::cdb::AbstractPersistentScheduleEventReceiver
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
    static const QnUuid functorTypeId;

    SchedulerUser(
        nx::db::AbstractAsyncSqlQueryExecutor* executor,
        nx::cdb::PersistentSheduler* scheduler,
        nx::utils::promise<void>* readyPromise)
        :
        m_executor(executor),
        m_scheduler(scheduler),
        m_readyPromise(readyPromise)
    {
        m_scheduler->registerEventReceiver(functorTypeId, this);
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
                    functorTypeId,
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
    nx::cdb::PersistentSheduler* m_scheduler;
    nx::utils::promise<void>* m_readyPromise;
    std::atomic<bool> m_shouldUnsubscribe{false};
    std::atomic<bool> m_shouldSubscribe{false};
    nx::utils::MoveOnlyFunc<void(const Task& task)> m_unsubscribeCallback;
    QnUuid m_unsubscribeId;
    TaskMap m_tasks;
    QnMutex m_mutex;
};

class SqlExecutorStub: public nx::db::AbstractAsyncSqlQueryExecutor
{
public:
    virtual const nx::db::ConnectionOptions& connectionOptions() const override
    {
        return nx::db::ConnectionOptions();
    }

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, nx::db::DBResult)> completionHandler) override
    {
        std::thread(
            [dbUpdateFunc = std::move(dbUpdateFunc), completionHandler= std::move(completionHandler)]()
            {
                completionHandler(nullptr, dbUpdateFunc(nullptr));
            }).detach();
    }

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, nx::db::DBResult)> completionHandler) override
    {
    }

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, nx::db::DBResult)> completionHandler) override
    {
        std::thread(
            [dbSelectFunc = std::move(dbSelectFunc), completionHandler= std::move(completionHandler)]()
            {
                completionHandler(nullptr, dbSelectFunc(nullptr));
            }).detach();
    }

    virtual nx::db::DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::db::QueryContext* const queryContext) override
    {
        return nx::db::DBResult::ok;
    }
};


class PersistentScheduler: public ::testing::Test
{
protected:
    PersistentScheduler()
    {
        readyFuture = readyPromise.get_future();
    }

    virtual void TearDown() override
    {
        if (scheduler)
            scheduler->stop();
    }

    void expectingDbHelperInitFunctionsWillBeCalled()
    {
        EXPECT_CALL(dbHelper, getScheduleData(_, NotNull()))
            .Times(AtLeast(1))
            .WillRepeatedly(
                ::testing::DoAll(
                    ::testing::SetArgPointee<1>(ScheduleData()),
                    Return(nx::db::DBResult::ok)));

        EXPECT_CALL(dbHelper, registerEventReceiver(nullptr, SchedulerUser::functorTypeId))
            .Times(AtLeast(1))
            .WillRepeatedly(::testing::Return(nx::db::DBResult::ok));
    }

    void expectingDbHelperSubscribeWillBeCalledOnce()
    {
        EXPECT_CALL(
            dbHelper,
            subscribe(
                nullptr,
                SchedulerUser::functorTypeId,
                _,
                _)).Times(AtLeast(1)).WillOnce(
                    ::testing::DoAll(
                        ::testing::SetArgPointee<2>(QnUuid::createUuid()),
                        Return(nx::db::DBResult::ok)));
    }

    void expectingDbHelperUnsubscribeWillBeCalled()
    {
        EXPECT_CALL(
            dbHelper,
            unsubscribe(
                nullptr,
                _)).Times(AtLeast(1)).WillOnce(Return(nx::db::DBResult::ok));
    }

    void expectingDbHelperSubscribeWillBeCalledTwice()
    {
        EXPECT_CALL(
            dbHelper,
            subscribe(
                nullptr,
                SchedulerUser::functorTypeId,
                _,
                _)).Times(AtLeast(1)).WillOnce(
                    ::testing::DoAll(
                        ::testing::SetArgPointee<2>(QnUuid::createUuid()),
                        Return(nx::db::DBResult::ok))).WillOnce(
                            ::testing::DoAll(
                                ::testing::SetArgPointee<2>(QnUuid::createUuid()),
                                Return(nx::db::DBResult::ok)));
    }

    void whenSchedulerAndUserInitialized()
    {
        scheduler = std::unique_ptr<nx::cdb::PersistentSheduler>(
            new nx::cdb::PersistentSheduler(&executor, &dbHelper));

        user = std::unique_ptr<SchedulerUser>(new SchedulerUser(&executor, scheduler.get(), &readyPromise));
        scheduler->start();
    }

    QnUuid whenSubscribedOnce()
    {
        QnUuid taskId;
        nx::utils::promise<void> subPromise;
        auto subFuture = subPromise.get_future();

        user->subscribe(
            std::chrono::milliseconds(10),
            [&taskId, subPromise = std::move(subPromise)](const QnUuid& subscribedId) mutable
        {
            taskId = subscribedId;
            subPromise.set_value();
        });
        subFuture.wait();

        return taskId;
    }

    void whenShouldSubscribeFromHandler()
    {
        user->setShouldSubscribe();
    }

    void whenShouldUnsubscribeFromHandler(
        const QnUuid& taskId,
        nx::utils::MoveOnlyFunc<void(const nx::cdb::test::SchedulerUser::Task&)> unsubscribeCb)
    {
        user->setShouldUnsubscribe(taskId, std::move(unsubscribeCb));
    }

    void thenTaskIdsAreFilled()
    {
        for (const auto& task : user->tasks())
            ASSERT_FALSE(task.first.isNull());
    }

    void thenTimersFiredSeveralTimes(int taskCount = -1)
    {
        if (taskCount != -1)
            ASSERT_EQ(user->tasks().size(), taskCount);

        for (const auto& task : user->tasks())
            ASSERT_GT(task.second.fired, 0);
    }

    SqlExecutorStub executor;
    DbHelperStub dbHelper;
    std::unique_ptr<nx::cdb::PersistentSheduler> scheduler;
    std::unique_ptr<SchedulerUser> user;
    nx::utils::future<void> readyFuture;
    nx::utils::promise<void> readyPromise;
    const std::chrono::milliseconds kSleepTimeout{ 100 };
};


const QnUuid SchedulerUser::functorTypeId = QnUuid::fromStringSafe("{EC05F182-9380-48E3-9C76-AD6C10295136}");

TEST_F(PersistentScheduler, initialization)
{
    expectingDbHelperInitFunctionsWillBeCalled();
    whenSchedulerAndUserInitialized();
}

TEST_F(PersistentScheduler, subscribe)
{
    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskIdsAreFilled();
}

TEST_F(PersistentScheduler, unsubscribe)
{
    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled();

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    int firedAlready;
    user->unsubscribe(
        taskId,
        [&firedAlready](const nx::cdb::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(firedAlready, user->tasks()[taskId].fired);
    ASSERT_FALSE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, running2Tasks)
{
    const std::chrono::milliseconds kFirstTaskTimeout{ 10 };
    const std::chrono::milliseconds kSecondTaskTimeout{ 20 };

    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledTwice();
    whenSchedulerAndUserInitialized();

    user->subscribe(kFirstTaskTimeout);
    user->subscribe(kSecondTaskTimeout);

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(user->tasks().size(), 2);
    thenTaskIdsAreFilled();
    thenTimersFiredSeveralTimes();
}

TEST_F(PersistentScheduler, subscribeFromHandler)
{
    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledTwice();
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    whenShouldSubscribeFromHandler();

    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskIdsAreFilled();
    thenTimersFiredSeveralTimes(2);
}

TEST_F(PersistentScheduler, unsubscribeFromHandler)
{
    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled();

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    ASSERT_FALSE(taskId.isNull());
    int firedAlready;
    whenShouldUnsubscribeFromHandler(
        taskId,
        [&firedAlready](const nx::cdb::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(firedAlready, user->tasks()[taskId].fired);
    ASSERT_FALSE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, tasksLoadedFromDb)
{

}

TEST_F(PersistentScheduler, unsuccessful_DB_operations)
{

}

TEST_F(PersistentScheduler, real_DB_operations)
{

}

} // namespace test
} // namespace cdb
} // namespace nx
