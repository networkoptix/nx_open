// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>

#include <gtest/gtest.h>

#include <nx/sql/query_scheduler.h>

#include "base_db_test.h"

namespace nx::sql::test {

static constexpr auto kPeriodicDelay = std::chrono::milliseconds(100);

class QueryScheduler:
    public test::BaseDbTest
{
    using base_type = test::BaseDbTest;

public:
    ~QueryScheduler()
    {
        m_scheduler.reset();
        closeDatabase();
    }

protected:
    virtual void SetUp() override
    {
        BaseDbTest::initializeDatabase();

        m_scheduler = std::make_unique<nx::sql::QueryScheduler>(
            &asyncSqlQueryExecutor(),
            [this](auto&&... args) { runPeriodicQuery(std::forward<decltype(args)>(args)...); });
    }

    void setPeriodicDelay(std::chrono::milliseconds delay)
    {
        m_periodicDelay = delay;
    }

    void start()
    {
        m_scheduler->start(m_periodicDelay, m_periodicDelay);
    }

    void requestExecuteImmediately()
    {
        m_scheduler->executeImmediately();
    }

    void waitForPeriodicQueryInvokedAtLeast(int expected)
    {
        while (m_queryInvocationCount.load() < expected)
            std::this_thread::sleep_for(kPeriodicDelay);
    }

private:
    void runPeriodicQuery(nx::sql::QueryContext* /*ctx*/)
    {
        ++m_queryInvocationCount;
    }

private:
    std::unique_ptr<nx::sql::QueryScheduler> m_scheduler;
    std::atomic<int> m_queryInvocationCount{0};
    std::chrono::milliseconds m_periodicDelay = kPeriodicDelay;
};

TEST_F(QueryScheduler, query_invoked_periodically)
{
    start();
    waitForPeriodicQueryInvokedAtLeast(2);
}

TEST_F(QueryScheduler, query_can_be_executed_immediately)
{
    setPeriodicDelay(std::chrono::hours(1));
    start();

    requestExecuteImmediately();

    waitForPeriodicQueryInvokedAtLeast(1);
}

} // namespace nx::sql::test
