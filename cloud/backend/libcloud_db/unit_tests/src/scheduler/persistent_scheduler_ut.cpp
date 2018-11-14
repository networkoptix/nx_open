#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/utils/std/future.h>
#include <chrono>
#include <nx/cloud/db/persistent_scheduler/persistent_scheduler.h>
#include <nx/cloud/db/persistent_scheduler/persistent_scheduler_db_helper.h>
#include "scheduler_user.h"

namespace nx::cloud::db {
namespace test {

using namespace ::testing;

class DbHelperStub: public nx::cloud::db::AbstractSchedulerDbHelper
{
public:
    MOCK_CONST_METHOD2(
        getScheduleData,
        nx::sql::DBResult(nx::sql::QueryContext*, nx::cloud::db::ScheduleData*));
    MOCK_METHOD4(
        subscribe,
        nx::sql::DBResult(nx::sql::QueryContext*, const QnUuid&, QnUuid*, const ScheduleTaskInfo&));
    MOCK_METHOD2(
        unsubscribe,
        nx::sql::DBResult(nx::sql::QueryContext*, const QnUuid&));
};

class SqlExecutorStub: public nx::sql::AbstractAsyncSqlQueryExecutor
{
public:
    virtual ~SqlExecutorStub()
    {
        while (runningCount() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    virtual const nx::sql::ConnectionOptions& connectionOptions() const override
    {
        return m_connectionOptions;
    }

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> completionHandler,
        const std::string& /*queryAggregationKey*/) override
    {
        incRunningCount();
        std::thread(
            [this, dbUpdateFunc = std::move(dbUpdateFunc),
                completionHandler = std::move(completionHandler)]()
            {
                completionHandler(dbUpdateFunc(nullptr));
                decRunningCount();
            }).detach();
    }

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> /*dbUpdateFunc*/,
        nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> /*completionHandler*/) override
    {
    }

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<nx::sql::DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::sql::DBResult)> completionHandler) override
    {
        incRunningCount();
        std::thread(
            [this, dbSelectFunc = std::move(dbSelectFunc),
                completionHandler = std::move(completionHandler)]()
            {
                completionHandler(dbSelectFunc(nullptr));
                decRunningCount();
            }).detach();
    }

    virtual nx::sql::DBResult execSqlScriptSync(
        const QByteArray& /*script*/,
        nx::sql::QueryContext* const /*queryContext*/) override
    {
        return nx::sql::DBResult::ok;
    }

private:
    nx::sql::ConnectionOptions m_connectionOptions;
    QnMutex m_mutex;
    int m_runningThreads = 0;

    void incRunningCount()
    {
        QnMutexLocker lock(&m_mutex);
        ++m_runningThreads;
    }

    void decRunningCount()
    {
        QnMutexLocker lock(&m_mutex);
        --m_runningThreads;
    }

    int runningCount()
    {
        QnMutexLocker lock(&m_mutex);
        return m_runningThreads;
    }
};

static const QnUuid functorId = QnUuid::createUuid();

class PersistentScheduler: public ::testing::Test
{
protected:
    PersistentScheduler() {}

    virtual void TearDown() override
    {
        if (scheduler)
            scheduler->stop();
    }

    void expectingGetScheduledDataFromDbWithSomeDataWillBeCalled()
    {
        nx::cloud::db::TaskToParams taskParams;
        nx::cloud::db::ScheduleTaskInfo taskInfo;
        const auto kDbTaskPeriod = std::chrono::milliseconds(10);

        taskInfo.params["key1"] = "dbValue1";
        taskInfo.params["key2"] = "dbValue2";
        taskInfo.schedulePoint = std::chrono::steady_clock::now();
        taskInfo.period = kDbTaskPeriod;

        auto taskId = QnUuid::createUuid();
        taskParams.emplace(taskId, taskInfo);

        FunctorToTasks functorToTasks;
        functorToTasks.emplace(functorId, std::set<QnUuid>{taskId});

        ScheduleData dbData = {functorToTasks, taskParams};

        EXPECT_CALL(dbHelper, getScheduleData(_, NotNull()))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<1>(dbData), Return(nx::sql::DBResult::ok)));
    }

    void expectingGetScheduledDataFromDbWithNoDataWillBeCalled()
    {
        EXPECT_CALL(dbHelper, getScheduleData(_, NotNull()))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<1>(ScheduleData()), Return(nx::sql::DBResult::ok)));
    }

    void expectingDbHelperSubscribeWillBeCalledOnce(nx::sql::DBResult result = nx::sql::DBResult::ok)
    {
        EXPECT_CALL(dbHelper, subscribe(nullptr, functorId, _, _))
            .Times(AtLeast(1))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(result)));
    }

    void expectingDbHelperUnsubscribeWillBeCalled(nx::sql::DBResult result = nx::sql::DBResult::ok)
    {
        EXPECT_CALL(dbHelper, unsubscribe(nullptr, _))
            .Times(AtLeast(1))
            .WillOnce(Return(result));
    }

    void expectingDbHelperSubscribeWillBeCalledTwice()
    {
        EXPECT_CALL(dbHelper, subscribe(nullptr, functorId, _, _))
            .Times(AtLeast(1))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(nx::sql::DBResult::ok)))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(nx::sql::DBResult::ok)));
    }

    void whenSchedulerAndUserInitialized()
    {
        scheduler = std::unique_ptr<nx::cloud::db::PersistentScheduler>(
            new nx::cloud::db::PersistentScheduler(&executor, &dbHelper));

        user = std::unique_ptr<SchedulerUser>(new SchedulerUser(&executor, scheduler.get(), functorId));
        user->registerAsAnEventReceiver();
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
        SchedulerUser::UnsubscribeCb unsubscribeCb)
    {
        user->setShouldUnsubscribe(taskId, std::move(unsubscribeCb));
    }

    void thenTaskCountShouldBe(size_t desiredTaskCount)
    {
        while (user->tasks().size() != desiredTaskCount)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    void thenTimersShouldHaveFiredSeveralTimes()
    {
        bool allFired = false;
        while (!allFired)
        {
            allFired = true;
            for (const auto& task: user->tasks())
                allFired &= task.second.fired > 0;

            if (!allFired)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::unique_ptr<nx::cloud::db::PersistentScheduler> scheduler;
    std::unique_ptr<SchedulerUser> user;
    const std::chrono::milliseconds kSleepTimeout{ 100 };
    DbHelperStub dbHelper;
    SqlExecutorStub executor;
};

#if 0

TEST_F(PersistentScheduler, initialization)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    whenSchedulerAndUserInitialized();
}

TEST_F(PersistentScheduler, subscribe)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    thenTaskCountShouldBe(1);
}

TEST_F(PersistentScheduler, subscribe_dbError)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce(nx::sql::DBResult::ioError);
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskCountShouldBe(0);
}

TEST_F(PersistentScheduler, unsubscribe)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled();

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    std::atomic<int> firedAlready;
    user->unsubscribe(
        taskId,
        [&firedAlready](const QnUuid&, const nx::cloud::db::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    waitForPredicateBecomeTrue(
        [&firedAlready, this, taskId]()
        {
            return firedAlready == user->tasks()[taskId].fired
                && !user->tasks()[taskId].subscribed;
        },
        "PersistentScheduler.unsubscribe");

    ASSERT_EQ(firedAlready, user->tasks()[taskId].fired);
    ASSERT_FALSE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, unsubscribe_dbError)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled(nx::sql::DBResult::ioError);

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    int firedAlready = -1;
    user->unsubscribe(
        taskId,
        [&firedAlready](const QnUuid&, const nx::cloud::db::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(firedAlready, -1);
    ASSERT_TRUE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, running2Tasks)
{
    const std::chrono::milliseconds kFirstTaskTimeout{10};
    const std::chrono::milliseconds kSecondTaskTimeout{20};

    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledTwice();
    whenSchedulerAndUserInitialized();

    user->subscribe(kFirstTaskTimeout);
    user->subscribe(kSecondTaskTimeout);

    thenTaskCountShouldBe(2);
    thenTimersShouldHaveFiredSeveralTimes();
}

TEST_F(PersistentScheduler, subscribeFromHandler)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledTwice();
    whenSchedulerAndUserInitialized();
    whenShouldSubscribeFromHandler();

    user->subscribe(std::chrono::milliseconds(10));

    thenTaskCountShouldBe(2);
    thenTimersShouldHaveFiredSeveralTimes();
}

TEST_F(PersistentScheduler, unsubscribeFromHandler)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled();

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    ASSERT_FALSE(taskId.isNull());
    int firedAlready;
    whenShouldUnsubscribeFromHandler(
        taskId,
        [&firedAlready](const QnUuid&, const nx::cloud::db::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(firedAlready, user->tasks()[taskId].fired);
    ASSERT_FALSE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, tasksLoadedFromDb)
{
    expectingGetScheduledDataFromDbWithSomeDataWillBeCalled();
    whenSchedulerAndUserInitialized();
    thenTimersShouldHaveFiredSeveralTimes();
}

#endif

} // namespace test
} // namespace nx::cloud::db
