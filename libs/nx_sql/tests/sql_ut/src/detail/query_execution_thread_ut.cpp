#include <gtest/gtest.h>

#include <memory>

#include <nx/utils/std/cpp14.h>

#include <nx/sql/detail/query_execution_thread.h>

#include "base_db_test.h"

namespace nx::sql::detail::test {

class QueryExecutionThread:
    public nx::sql::test::BaseDbTest
{
    using base_type = nx::sql::test::BaseDbTest;

public:
    QueryExecutionThread():
        base_type(kModuleName)
    {
        initializeDatabase();
    }

protected:
    void givenRunningThread()
    {
        m_thread = std::make_unique<detail::QueryExecutionThread>(
            connectionOptions(),
            &m_queryExecutorQueue);
        m_thread->start();
    }

    void whenAskedThreadToStop()
    {
        m_thread->pleaseStop();
    }

    void verifyThatThreadHasStopped()
    {
        m_thread->join();
    }

private:
    QueryExecutorQueue m_queryExecutorQueue;
    std::unique_ptr<detail::QueryExecutionThread> m_thread;
};

TEST_F(QueryExecutionThread, stops_after_termination_request)
{
    givenRunningThread();
    whenAskedThreadToStop();
    verifyThatThreadHasStopped();
}

} // namespace nx::sql::detail::test
