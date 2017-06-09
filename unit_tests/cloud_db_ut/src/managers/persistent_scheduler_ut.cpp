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
            nx::db::QueryContext* queryContext,
            const QnUuid& functorId,
            QnUuid* outTaskId,
            const ScheduleTaskInfo& taskInfo));
};

class SchedulerUser: public nx::cdb::AbstractPersistentScheduleEventReceiver
{
public:
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
            ++m_tasks[taskId];
        }

        if (m_shouldUnsubscribe)
        {
//            m_scheduler->unsubscribe(functorTypeId);
//            m_readyPromise->set_value();
        }

        return [](nx::db::QueryContext*) { return nx::db::DBResult::ok; };
    }

    void subscribe(std::chrono::milliseconds timeout)
    {
        nx::cdb::ScheduleParams params;
        params["key1"] = "value1";
        params["key2"] = "value1";

        m_executor->executeUpdate(
            [this, params, timeout](nx::db::QueryContext* queryContext)
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

                QnMutexLocker lock(&m_mutex);
                m_tasks.emplace(taskId, 0);
                return nx::db::DBResult::ok;
            },
            [](nx::db::QueryContext*, nx::db::DBResult result)
            {
            });
    }

    void setShouldUnsubscribe() { m_shouldUnsubscribe = true; }
    std::unordered_map<QnUuid, int> tasks()
    {
        QnMutexLocker lock(&m_mutex);
        return m_tasks;
    }

private:
    nx::db::AbstractAsyncSqlQueryExecutor* m_executor;
    nx::cdb::PersistentSheduler* m_scheduler;
    nx::utils::promise<void>* m_readyPromise;
    std::atomic<bool> m_shouldUnsubscribe{false};
    std::unordered_map<QnUuid, int> m_tasks;
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
        std::thread([&]() {completionHandler(nullptr, dbUpdateFunc(nullptr)); }).detach();
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
        std::thread([&]() {completionHandler(nullptr, dbSelectFunc(nullptr)); }).detach();
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

    void expectingDbHelperSubscribeWillBeCalled()
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

    void expectingDbHelperSubscribeWillBeCalled2Times()
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
    }

    void thenTaskIdsAreFilled()
    {
        for (const auto& task : user->tasks())
            ASSERT_FALSE(task.first.isNull());
    }

    void thenTimersFiredSeveralTimes()
    {
        for (const auto& task : user->tasks())
            ASSERT_GT(task.second, 0);
    }

    SqlExecutorStub executor;
    DbHelperStub dbHelper;
    std::unique_ptr<nx::cdb::PersistentSheduler> scheduler;
    std::unique_ptr<SchedulerUser> user;
    nx::utils::future<void> readyFuture;
    nx::utils::promise<void> readyPromise;
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
    expectingDbHelperSubscribeWillBeCalled();
    whenSchedulerAndUserInitialized();

    user->subscribe(std::chrono::milliseconds(10));
    thenTaskIdsAreFilled();
}

TEST_F(PersistentScheduler, running2Tasks)
{
    const std::chrono::milliseconds kFirstTaskTimeout{ 10 };
    const std::chrono::milliseconds kSecondTaskTimeout{ 20 };
    const std::chrono::milliseconds kSleepTimeout{ 100 };

    expectingDbHelperInitFunctionsWillBeCalled();
    expectingDbHelperSubscribeWillBeCalled2Times();
    whenSchedulerAndUserInitialized();

    user->subscribe(kFirstTaskTimeout);
    user->subscribe(kSecondTaskTimeout);

    std::this_thread::sleep_for(kSleepTimeout);

    ASSERT_EQ(user->tasks().size(), 2);
    thenTaskIdsAreFilled();
    thenTimersFiredSeveralTimes();
}

TEST_F(PersistentScheduler, running2Tasks_subscribeFromHandler)
{

}

TEST_F(PersistentScheduler, running2Tasks_unsubscribeFromHandler)
{

}

TEST_F(PersistentScheduler, tasksLoadedFromDb)
{

}


} // namespace test
} // namespace cdb
} // namespace nx
