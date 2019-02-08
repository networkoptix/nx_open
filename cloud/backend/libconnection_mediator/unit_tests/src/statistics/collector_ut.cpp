#include <gtest/gtest.h>

#include <memory>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/utils.h>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx/cloud/mediator/settings.h>
#include <nx/cloud/mediator/statistics/collector.h>
#include <nx/cloud/mediator/statistics/dao/abstract_data_object.h>
#include <nx/cloud/mediator/statistics/dao/data_object_factory.h>
#include <nx/cloud/mediator/statistics/dao/memory/in_memory_data_object.h>

namespace nx {
namespace hpm {
namespace stats {
namespace test {

class TestDataObject:
    public dao::AbstractDataObject
{
public:
    TestDataObject():
        m_saveMethodBlocker(nullptr)
    {
    }

    virtual nx::sql::DBResult save(
        nx::sql::QueryContext* queryContext,
        stats::ConnectSession connectionRecord) override
    {
        if (m_saveMethodBlocker)
            m_saveMethodBlocker->get_future().wait();

        NX_GTEST_ASSERT_EQ(
            nx::sql::DBResult::ok,
            m_delegate.save(queryContext, std::move(connectionRecord)));

        if (m_onRecordSavedHandler)
            m_onRecordSavedHandler();

        return nx::sql::DBResult::ok;
    }

    virtual nx::sql::DBResult readAllRecords(
        nx::sql::QueryContext* queryContext,
        std::deque<stats::ConnectSession>* connectionRecords) override
    {
        return m_delegate.readAllRecords(queryContext, connectionRecords);
    }

    const std::deque<stats::ConnectSession>& records() const
    {
        return m_delegate.records();
    }

    void setOnRecordSavedHandler(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onRecordSavedHandler = std::move(handler);
    }

    void setSaveMethodBlocker(nx::utils::promise<void>* promise)
    {
        m_saveMethodBlocker = promise;
    }

private:
    dao::memory::InMemoryDataObject m_delegate;
    nx::utils::MoveOnlyFunc<void()> m_onRecordSavedHandler;
    nx::utils::promise<void>* m_saveMethodBlocker;
};

class Statistics:
    public nx::sql::test::TestWithDbHelper,
    public ::testing::Test
{
public:
    Statistics():
        nx::sql::test::TestWithDbHelper("hpm", QString()),
        m_testDataObject(nullptr)
    {
        init();
    }

    ~Statistics()
    {
        m_queryExecutor.reset();
        stats::dao::DataObjectFactory::setFactoryFunc(nullptr);
    }

protected:
    void whenAddedStatisticsRecord()
    {
        m_collector->saveConnectSessionStatistics(stats::ConnectSession());
    }

    void verifyThatRecordHasBeenSaved()
    {
        while (m_testDataObject->records().empty())
        {
            QnMutexLocker lock(&m_mutex);
            m_cond.wait(lock.mutex());
        }
    }

    void havingDeadlockedStatsDataObject()
    {
        m_testDataObject->setSaveMethodBlocker(&m_testDonePromise);
    }

    void verifyThereWasNoDelayWhileAddingRecord()
    {
        m_testDonePromise.set_value();
        verifyThatRecordHasBeenSaved();
    }

private:
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_queryExecutor;
    std::unique_ptr<stats::PersistentCollector> m_collector;
    TestDataObject* m_testDataObject;
    conf::Statistics m_statisticsSettings;
    nx::utils::promise<void> m_testDonePromise;
    QnMutex m_mutex;
    QnWaitCondition m_cond;

    void init()
    {
        m_statisticsSettings.enabled = true;

        dbConnectionOptions().maxConnectionCount = 1;
        m_queryExecutor = std::make_unique<nx::sql::AsyncSqlQueryExecutor>(
            dbConnectionOptions());

        ASSERT_TRUE(m_queryExecutor->init());

        stats::dao::DataObjectFactory::setFactoryFunc(
            [this]()
            {
                auto result = std::make_unique<TestDataObject>();
                m_testDataObject = result.get();
                return result;
            });
        m_collector = std::make_unique<stats::PersistentCollector>(
            m_statisticsSettings,
            m_queryExecutor.get());
        ASSERT_NE(nullptr, m_testDataObject);
        m_testDataObject->setOnRecordSavedHandler(
            std::bind(&Statistics::onRecordSaved, this));
    }

    void onRecordSaved()
    {
        m_cond.wakeAll();
    }
};

TEST_F(Statistics, stats_is_actually_saved)
{
    whenAddedStatisticsRecord();
    verifyThatRecordHasBeenSaved();
}

TEST_F(Statistics, saving_stats_is_done_asynchronously)
{
    havingDeadlockedStatsDataObject();
    whenAddedStatisticsRecord();
    verifyThereWasNoDelayWhileAddingRecord();
}

} // namespace test
} // namespace stats
} // namespace hpm
} // namespace nx
