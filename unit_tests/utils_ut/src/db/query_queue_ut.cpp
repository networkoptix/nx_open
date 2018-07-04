#include <deque>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/db/detail/query_queue.h>
#include <nx/utils/random.h>
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

    void raiseSelectQueryPriority()
    {
        m_queryQueue.setQueryPriority(
            QueryType::lookup,
            detail::QueryQueue::kDefaultPriority + 1);
    }

    void addSeveralQueriesOfDifferentType()
    {
        const int queryCount = 17;
        for (int i = 0; i < queryCount; ++i)
        {
            const auto queryType = nx::utils::random::choice(
                {QueryType::modification, QueryType::lookup});
            pushQuery(queryType);
        }
    }

    void addSelectQuery()
    {
        pushQuery(QueryType::lookup);
    }

    void addMultipleModificationQueries()
    {
        const int queryCount = 10;
        for (int i = 0; i < queryCount; ++i)
            pushQuery(QueryType::modification);
    }

    void whenQueryTimeoutPasses()
    {
        std::this_thread::sleep_for(kQueryTimeout * 2);
    }

    void whenQueryTimedOut()
    {
        whenQueryTimeoutPasses();

        ASSERT_FALSE(static_cast<bool>(m_queryQueue.pop(std::chrono::milliseconds(1))));
        m_timedOutQueries.pop();
    }

    void whenPopWithTimeout()
    {
        m_prevPosResult = m_queryQueue.pop(std::chrono::milliseconds(1));
    }

    void thenTimedOutQueriesAreReported()
    {
        ASSERT_FALSE(m_queryQueue.pop(std::chrono::milliseconds(1)));

        while (!m_queryTypes.empty())
        {
            ASSERT_EQ(m_queryTypes.front(), m_timedOutQueries.pop());
            m_queryTypes.pop_front();
        }
    }

    void thenAnotherModificationQueryCanBeReadFromTheQueue()
    {
        using namespace std::placeholders;

        m_queryQueue.enableItemStayTimeoutEvent(
            std::chrono::hours(1),
            std::bind(&QueryQueue::processTimedOutQuery, this, _1));

        m_queryQueue.push(std::make_unique<QueryExecutorStub>(QueryType::modification));
        ASSERT_TRUE(static_cast<bool>(m_queryQueue.pop()));
    }

    void thenEmptyValueIsReturned()
    {
        ASSERT_FALSE(static_cast<bool>(m_prevPosResult));
    }

    void assertQueriesArePoppedInTheSameOrderAsPushed()
    {
        while (!m_queryTypes.empty())
        {
            ASSERT_EQ(m_queryTypes.front(), (*m_queryQueue.pop())->queryType());
            m_queryTypes.pop_front();
        }
    }

    void assertSelectQueryIsReadFromQueue()
    {
        const auto query = m_queryQueue.pop();
        ASSERT_TRUE(static_cast<bool>(query));
        ASSERT_EQ(QueryType::lookup, (*query)->queryType());
    }

    detail::QueryQueue& queryQueue()
    {
        return m_queryQueue;
    }

private:
    std::optional<std::unique_ptr<AbstractExecutor>> m_prevPosResult;
    detail::QueryQueue m_queryQueue;
    nx::utils::SyncQueue<QueryType> m_timedOutQueries;
    std::deque<QueryType> m_queryTypes;

    void processTimedOutQuery(std::unique_ptr<AbstractExecutor> query)
    {
        m_timedOutQueries.push(query->queryType());
    }

    void pushQuery(QueryType queryType)
    {
        m_queryQueue.push(std::make_unique<QueryExecutorStub>(queryType));
        m_queryTypes.push_back(queryType);
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(QueryQueue, timeout_is_supported)
{
    whenPopWithTimeout();
    thenEmptyValueIsReturned();
}

TEST_F(QueryQueue, expired_queries_are_reported)
{
    enableQueueItemTimeout();

    addSeveralQueriesOfDifferentType();
    whenQueryTimeoutPasses();

    thenTimedOutQueriesAreReported();
}

TEST_F(QueryQueue, modification_query_limit_correctly_updated_on_query_timeout)
{
    queryQueue().setConcurrentModificationQueryLimit(1);
    enableQueueItemTimeout();

    addMultipleModificationQueries();
    whenQueryTimedOut();
    thenAnotherModificationQueryCanBeReadFromTheQueue();
}

TEST_F(QueryQueue, event_query_has_same_priority_by_default)
{
    addSeveralQueriesOfDifferentType();
    assertQueriesArePoppedInTheSameOrderAsPushed();
}

TEST_F(QueryQueue, select_query_priority_can_be_raised)
{
    raiseSelectQueryPriority();

    addMultipleModificationQueries();
    addSelectQuery();

    assertSelectQueryIsReadFromQueue();
}

} // namespace test
} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
