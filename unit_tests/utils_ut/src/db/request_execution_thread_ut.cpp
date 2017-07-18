#include <gtest/gtest.h>

#include <memory>

#include <nx/utils/std/cpp14.h>

#include <nx/utils/db/request_execution_thread.h>

#include "base_db_test.h"

namespace nx {
namespace utils {
namespace db {
namespace test {

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
        m_thread = std::make_unique<nx::utils::db::DbRequestExecutionThread>(
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
    std::unique_ptr<nx::utils::db::DbRequestExecutionThread> m_thread;
};

TEST_F(DbRequestExecutionThread, stops_after_termination_request)
{
    givenRunningThread();
    whenAskedThreadToStop();
    verifyThatThreadHasStopped();
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
