#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/db/detail/query_queue.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace utils {
namespace db {
namespace detail {
namespace test {

namespace {

class QueryExecutorStub:
    public AbstractExecutor
{
public:
    QueryExecutorStub(QueryType queryType):
        m_queryType(queryType)
    {
    }

    ~QueryExecutorStub()
    {
        if (m_onBeforeDestructionHandler)
            m_onBeforeDestructionHandler();
    }

    virtual DBResult execute(QSqlDatabase* const /*connection*/) override
    {
        return DBResult::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult /*errorCode*/) override
    {
    }

    virtual QueryType queryType() const override
    {
        return m_queryType;
    }

    virtual void setOnBeforeDestruction(
        nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_onBeforeDestructionHandler.swap(handler);
    }

private:
    const QueryType m_queryType;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
};

} // namespace

constexpr std::chrono::milliseconds kQueryTimeout = std::chrono::milliseconds(10);

class QueryQueue:
    public ::testing::Test
{
protected:
    void enableQueueItemTimeout()
    {
        using namespace std::placeholders;

        m_queryQueue.enableItemStayTimeoutEvent(
            kQueryTimeout,
            std::bind(&QueryQueue::processTimedOutQuery, this, _1));
    }

    void addMultipleModificationQueries()
    {
        const int kQueryCount = 10;
        for (int i = 0; i < kQueryCount; ++i)
            m_queryQueue.push(std::make_unique<QueryExecutorStub>(QueryType::modification));
    }

    void whenQueryTimedOut()
    {
        std::this_thread::sleep_for(kQueryTimeout * 2);
        ASSERT_FALSE(m_queryQueue.pop(std::chrono::milliseconds(1)));
        m_timedOutQueries.pop();
    }

    void thenAnotherModificationQueryCanBeReadFromTheQueue()
    {
        using namespace std::placeholders;

        m_queryQueue.enableItemStayTimeoutEvent(
            std::chrono::hours(1),
            std::bind(&QueryQueue::processTimedOutQuery, this, _1));

        m_queryQueue.push(std::make_unique<QueryExecutorStub>(QueryType::modification));
        ASSERT_TRUE(m_queryQueue.pop());
    }

    detail::QueryQueue& queryQueue()
    {
        return m_queryQueue;
    }

private:
    detail::QueryQueue m_queryQueue;
    nx::utils::SyncQueue<int /*dummy*/> m_timedOutQueries;

    void processTimedOutQuery(std::unique_ptr<AbstractExecutor> /*query*/)
    {
        m_timedOutQueries.push(0 /*dummy*/);
    }
};

TEST_F(QueryQueue, modification_query_limit_correctly_updated_on_query_timeout)
{
    queryQueue().setConcurrentModificationQueryLimit(1);
    enableQueueItemTimeout();

    addMultipleModificationQueries();
    whenQueryTimedOut();
    thenAnotherModificationQueryCanBeReadFromTheQueue();
}

} // namespace test
} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
