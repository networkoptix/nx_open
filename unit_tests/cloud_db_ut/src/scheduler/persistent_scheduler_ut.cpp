#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nx/utils/std/future.h>
#include <chrono>
#include <nx/cloud/cdb/persistent_scheduler/persistent_scheduler.h>
#include <nx/cloud/cdb/persistent_scheduler/persistent_scheduler_db_helper.h>
#include "scheduler_user.h"

namespace nx {
namespace cdb {
namespace test {

using namespace ::testing;

class DbHelperStub: public nx::cdb::AbstractSchedulerDbHelper
{
public:
    MOCK_CONST_METHOD2(
        getScheduleData,
        nx::utils::db::DBResult(nx::utils::db::QueryContext*, nx::cdb::ScheduleData*));
    MOCK_METHOD4(
        subscribe,
        nx::utils::db::DBResult(nx::utils::db::QueryContext*, const QnUuid&, QnUuid*, const ScheduleTaskInfo&));
    MOCK_METHOD2(
        unsubscribe,
        nx::utils::db::DBResult(nx::utils::db::QueryContext*, const QnUuid&));
};

class SqlExecutorStub: public nx::utils::db::AbstractAsyncSqlQueryExecutor
{
public:
    virtual const nx::utils::db::ConnectionOptions& connectionOptions() const override
    {
        return m_connectionOptions;
    }

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, nx::utils::db::DBResult)> completionHandler) override
    {
        std::thread(
            [dbUpdateFunc = std::move(dbUpdateFunc), completionHandler= std::move(completionHandler)]()
            {
                completionHandler(nullptr, dbUpdateFunc(nullptr));
            }).detach();
    }

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)> /*dbUpdateFunc*/,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, nx::utils::db::DBResult)> /*completionHandler*/) override
    {
    }

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, nx::utils::db::DBResult)> completionHandler) override
    {
        std::thread(
            [dbSelectFunc = std::move(dbSelectFunc), completionHandler= std::move(completionHandler)]()
            {
                completionHandler(nullptr, dbSelectFunc(nullptr));
            }).detach();
    }

    virtual nx::utils::db::DBResult execSqlScriptSync(
        const QByteArray& /*script*/,
        nx::utils::db::QueryContext* const /*queryContext*/) override
    {
        return nx::utils::db::DBResult::ok;
    }

    nx::utils::db::ConnectionOptions m_connectionOptions;
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
        nx::cdb::TaskToParams taskParams;
        nx::cdb::ScheduleTaskInfo taskInfo;
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
            .WillRepeatedly(DoAll(SetArgPointee<1>(dbData), Return(nx::utils::db::DBResult::ok)));
    }

    void expectingGetScheduledDataFromDbWithNoDataWillBeCalled()
    {
        EXPECT_CALL(dbHelper, getScheduleData(_, NotNull()))
            .Times(AtLeast(1))
            .WillRepeatedly(DoAll(SetArgPointee<1>(ScheduleData()), Return(nx::utils::db::DBResult::ok)));
    }

    void expectingDbHelperSubscribeWillBeCalledOnce(nx::utils::db::DBResult result = nx::utils::db::DBResult::ok)
    {
        EXPECT_CALL(dbHelper, subscribe(nullptr, functorId, _, _))
            .Times(AtLeast(1))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(result)));
    }

    void expectingDbHelperUnsubscribeWillBeCalled(nx::utils::db::DBResult result = nx::utils::db::DBResult::ok)
    {
        EXPECT_CALL(dbHelper, unsubscribe(nullptr, _))
            .Times(AtLeast(1))
            .WillOnce(Return(result));
    }

    void expectingDbHelperSubscribeWillBeCalledTwice()
    {
        EXPECT_CALL(dbHelper, subscribe(nullptr, functorId, _, _))
            .Times(AtLeast(1))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(nx::utils::db::DBResult::ok)))
            .WillOnce(DoAll(SetArgPointee<2>(QnUuid::createUuid()), Return(nx::utils::db::DBResult::ok)));
    }

    void whenSchedulerAndUserInitialized()
    {
        scheduler = std::unique_ptr<nx::cdb::PersistentScheduler>(
            new nx::cdb::PersistentScheduler(&executor, &dbHelper));

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

    enum TasksIdState
    {
        filled,
        notFilled
    };

    void thenTaskIdsAre(TasksIdState idState)
    {
        if (idState == notFilled)
        {
            ASSERT_TRUE(user->tasks().empty());
            return;
        }

        for (const auto& task : user->tasks())
            ASSERT_FALSE(task.first.isNull());
    }

    void thenTimersFiredSeveralTimes(int taskCount = -1)
    {
        if (taskCount >= 0)
        {
            ASSERT_EQ(user->tasks().size(), static_cast<std::size_t>(taskCount));
        }

        for (const auto& task : user->tasks())
        {
            ASSERT_GT(task.second.fired, 0);
        }
    }

    SqlExecutorStub executor;
    DbHelperStub dbHelper;
    std::unique_ptr<nx::cdb::PersistentScheduler> scheduler;
    std::unique_ptr<SchedulerUser> user;
    const std::chrono::milliseconds kSleepTimeout{ 100 };
};


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
    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskIdsAre(filled);
}

TEST_F(PersistentScheduler, subscribe_dbError)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce(nx::utils::db::DBResult::ioError);
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskIdsAre(notFilled);
}

TEST_F(PersistentScheduler, unsubscribe)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled();

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    int firedAlready;
    user->unsubscribe(
        taskId,
        [&firedAlready](const QnUuid&, const nx::cdb::test::SchedulerUser::Task& task)
        {
            firedAlready = task.fired;
        });

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(firedAlready, user->tasks()[taskId].fired);
    ASSERT_FALSE(user->tasks()[taskId].subscribed);
}

TEST_F(PersistentScheduler, unsubscribe_dbError)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledOnce();
    expectingDbHelperUnsubscribeWillBeCalled(nx::utils::db::DBResult::ioError);

    whenSchedulerAndUserInitialized();

    QnUuid taskId = whenSubscribedOnce();

    int firedAlready = -1;
    user->unsubscribe(
        taskId,
        [&firedAlready](const QnUuid&, const nx::cdb::test::SchedulerUser::Task& task)
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

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(2U, user->tasks().size());
    thenTaskIdsAre(filled);
    thenTimersFiredSeveralTimes();
}

TEST_F(PersistentScheduler, subscribeFromHandler)
{
    expectingGetScheduledDataFromDbWithNoDataWillBeCalled();
    expectingDbHelperSubscribeWillBeCalledTwice();
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    whenShouldSubscribeFromHandler();

    std::this_thread::sleep_for(kSleepTimeout);
    thenTaskIdsAre(filled);
    thenTimersFiredSeveralTimes(2);
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
        [&firedAlready](const QnUuid&, const nx::cdb::test::SchedulerUser::Task& task)
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

    std::this_thread::sleep_for(kSleepTimeout);
    thenTimersFiredSeveralTimes(1);
}

} // namespace test
} // namespace cdb
} // namespace nx
