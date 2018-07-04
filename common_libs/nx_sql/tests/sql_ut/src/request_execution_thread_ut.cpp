#include <gtest/gtest.h>

#include <memory>

#include <nx/utils/std/cpp14.h>

#include <nx/sql/request_execution_thread.h>

#include "base_db_test.h"

namespace nx::sql::test {

class DbRequestExecutionThread:
    public BaseDbTest
{
public:
    DbRequestExecutionThread()
    {
        initializeDatabase();
    }

protected:
    void givenRunningThread()
    {
        m_thread = std::make_unique<nx::sql::DbRequestExecutionThread>(
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
    std::unique_ptr<nx::sql::DbRequestExecutionThread> m_thread;
};

TEST_F(DbRequestExecutionThread, stops_after_termination_request)
{
    givenRunningThread();
    whenAskedThreadToStop();
    verifyThatThreadHasStopped();
}

} // namespace nx::sql::test
