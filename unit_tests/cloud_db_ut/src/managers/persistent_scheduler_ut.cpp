#include <gtest/gtest.h>
#include <nx/utils/std/future.h>
#include <chrono>
#include <managers/persistent_scheduler.h>

namespace nx {
namespace cdb {
namespace test {

class DbHelperStub: public nx::cdb::AbstractSchedulerDbHelper
{
public:
private:
};

class ShedulerUser: public nx::cdb::AbstractPersistentScheduleEventReceiver
{
public:
    static const QnUuid functorTypeId;

    ShedulerUser(nx::cdb::PersistentSheduler* scheduler, nx::utils::promise<void>* readyPromise):
        m_scheduler(scheduler),
        m_readyPromise(readyPromise)
    {
        m_scheduler->registerEventReceiver(functorTypeId, this);
    }

    virtual void persistentTimerFired(
        const QnUuid& taskId,
        const PersistentParamsMap& params,
        nx::db::QueryContext* context) override
    {
        ASSERT_NE(params.find("key1"), params.cend());
        ASSERT_NE(params.find("key2"), params.cend());

        ++m_fired;
        if (m_shouldUnsubscribe)
        {
//            m_scheduler->unsubscribe(functorTypeId);
//            m_readyPromise->set_value();
        }
    }

    void subscribe(std::chrono::milliseconds timeout)
    {
        nx::cdb::PersistentParamsMap params;
        params["key1"] = "value1";
        params["key2"] = "value2";

//        m_manager->subscribe(functorTypeId, timeout, params);
    }

    void setShouldUnsubscribe() { m_shouldUnsubscribe = true; }
    int fired() const { return m_fired; }

private:
    nx::cdb::PersistentSheduler* m_scheduler;
    nx::utils::promise<void>* m_readyPromise;
    std::atomic<bool> m_shouldUnsubscribe{false};
    int m_fired = 0;
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
    PersistentScheduler():
        scheduler(&executor),
        user(&scheduler, &readyPromise)
    {}

    virtual void SetUp() override
    {
        readyFuture = readyPromise.get_future();
    }

    SqlExecutorStub executor;
    DbHelperStub dbHelper;
    nx::cdb::PersistentSheduler scheduler;
    ShedulerUser user;
    nx::utils::future<void> readyFuture;
    nx::utils::promise<void> readyPromise;
};


const QnUuid ShedulerUser::functorTypeId = QnUuid::fromStringSafe("{EC05F182-9380-48E3-9C76-AD6C10295136}");

TEST_F(PersistentScheduler, initialization)
{
}

TEST_F(PersistentScheduler, registerEventReceiver)
{

}

//TEST_F(PersistentSheduler, subscribeUnsubscribe_simple)
//{
//    // initialization
//    AbstractAsyncSqlQueryExecutor* executor = ;
//    PersistentSheduler manager(executor);

//    ShedulerUser user(executor, &manager, &readyPromise);

//    //
//    timerManager.start();

//    //
//    user.addTimer();
//    // executor.executeUpdate(
//    //     [](QueryContext* queryContext){queryContext, manager.subscribe(std::chrono::milliseconds(10));},
//    //     [](){}
//    // );

//    std::this_thread::sleep_for(std::chrono::milliseconds(100));

//    readyFuture.wait();
//    ASSERT_GE(manager.fired(), 0);
//}

}
}
}
