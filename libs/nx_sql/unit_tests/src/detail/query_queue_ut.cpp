// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>
#include <thread>

#include <gtest/gtest.h>

#include <nx/sql/detail/query_queue.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::sql::detail::test {

namespace {

class QueryExecutorStub:
    public AbstractExecutor
{
public:
    QueryExecutorStub(
        QueryType queryType,
        const std::string& queryAggregationKey = std::string(),
        std::function<void()> onQueryExecuted = nullptr)
        :
        m_queryType(queryType),
        m_queryAggregationKey(queryAggregationKey),
        m_onQueryExecuted(std::move(onQueryExecuted))
    {
    }

    ~QueryExecutorStub()
    {
        if (m_onBeforeDestructionHandler)
            m_onBeforeDestructionHandler();
    }

    virtual DBResult execute(AbstractDbConnection* const /*connection*/) override
    {
        if (m_onQueryExecuted)
            m_onQueryExecuted();
        return DBResultCode::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult /*errorCode*/) override
    {
    }

    virtual QueryType queryType() const override
    {
        return m_queryType;
    }

    virtual std::string aggregationKey() const override
    {
        return m_queryAggregationKey;
    }

    virtual void setOnBeforeDestruction(
        nx::MoveOnlyFunc<void()> handler) override
    {
        m_onBeforeDestructionHandler.swap(handler);
    }

    virtual void setExternalTransaction(Transaction* /*transaction*/) override
    {
    }

private:
    const QueryType m_queryType;
    const std::string m_queryAggregationKey;
    std::function<void()> m_onQueryExecuted;
    nx::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
};

//-------------------------------------------------------------------------------------------------

class DbConnectionStub:
    public AbstractDbConnection
{
public:
    virtual bool open() override
    {
        return true;
    }

    virtual void close() override
    {
    }

    virtual bool begin() override
    {
        return true;
    }

    virtual bool commit() override
    {
        return true;
    }

    virtual bool rollback() override
    {
        return true;
    }

    virtual DBResult lastError() override
    {
        return DBResultCode::ok;
    }

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override
    {
        return nullptr;
    }

    virtual RdbmsDriverType driverType() const override
    {
        return RdbmsDriverType::sqlite;
    }

    virtual QSqlDatabase* qtSqlConnection() override
    {
        return nullptr;
    }

    virtual bool tableExist(const std::string_view&)
    {
        return true;
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

constexpr std::chrono::milliseconds kQueryTimeout = std::chrono::milliseconds(10);

class QueryQueue:
    public ::testing::Test
{
protected:
    void setAggregationLimit()
    {
        m_queryQueue.setAggregationLimit(11);
    }

    void setConcurrentModificationQueryLimit(int limit)
    {
        m_queryQueue.setConcurrentModificationQueryLimit(limit);
    }

    void enableQueueItemTimeout()
    {
        m_queryQueue.setItemStayTimeout(kQueryTimeout);
        m_queryQueue.setOnItemStayTimeout(
            [this](auto&&... args) { processTimedOutQuery(std::forward<decltype(args)>(args)...); });
    }

    void raiseSelectQueryPriority()
    {
        m_queryQueue.setQueryPriority(
            QueryType::lookup,
            detail::QueryQueue::kDefaultPriority + 1);
    }

    // Returns number of posted queries.
    int addSeveralQueriesOfDifferentType()
    {
        const int queryCount = 17;
        for (int i = 0; i < queryCount; ++i)
        {
            const auto queryType = nx::utils::random::choice(
                {QueryType::modification, QueryType::lookup});
            pushQuery(queryType);
        }

        return queryCount;
    }

    void addSelectQuery()
    {
        pushQuery(QueryType::lookup);
    }

    void addMultipleModificationQueries(
        const std::string& queryAggregationKey = std::string())
    {
        const int queryCount = 10;
        for (int i = 0; i < queryCount; ++i)
            pushQuery(QueryType::modification, queryAggregationKey);
    }

    void addMultipleSelectQueries(
        const std::string& queryAggregationKey = std::string())
    {
        const int queryCount = 10;
        for (int i = 0; i < queryCount; ++i)
            pushQuery(QueryType::lookup, queryAggregationKey);
    }

    void addMoreModificationQueriesThanAggregationLimit()
    {
        for (int i = 0; i < m_queryQueue.aggregationLimit() * 2; ++i)
            pushQuery(QueryType::modification, "aggregation_key");
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
        m_prevPopResult = m_queryQueue.pop(std::chrono::milliseconds(1));
    }

    void whenPopQueries()
    {
        m_prevPopResult = m_queryQueue.pop();
    }

    void whenReleasePoppedQueries()
    {
        m_prevPopResult = std::nullopt;
    }

    void whenExecuteQueries()
    {
        m_executedQueries.clear();

        DbConnectionStub dbConnectionStub;
        (*m_prevPopResult)->execute(&dbConnectionStub);
    }

    void thenCurrentModificationCountIs(int expected)
    {
        ASSERT_EQ(expected, m_queryQueue.concurrentModificationCount());
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
        m_queryQueue.setItemStayTimeout(std::chrono::hours(1));
        m_queryQueue.setOnItemStayTimeout(
            [this](auto&&... args) { processTimedOutQuery(std::forward<decltype(args)>(args)...); });

        m_queryQueue.push(std::make_unique<QueryExecutorStub>(QueryType::modification));
        ASSERT_TRUE(static_cast<bool>(m_queryQueue.pop()));
    }

    void thenEmptyValueIsReturned()
    {
        ASSERT_FALSE(static_cast<bool>(m_prevPopResult));
    }

    void thenQueryIsProvided()
    {
        ASSERT_TRUE(static_cast<bool>(m_prevPopResult));
    }

    void thenQueriesAggregatedBeforeReturning()
    {
        thenQueryIsProvided();
        andQueryAggregatesMultipleQueries();
    }

    void thenAggregatedQueriesOfSpecificTypeAreProvided(
        QueryType expectedQueryType)
    {
        whenExecuteQueries();

        const auto expectedQueryCount =
            std::count_if(
                m_queryTypes.begin(), m_queryTypes.end(),
                [expectedQueryType](QueryType type) { return type == expectedQueryType; });

        ASSERT_EQ(expectedQueryCount, m_executedQueries.size());
        ASSERT_TRUE(std::all_of(
            m_executedQueries.begin(), m_executedQueries.end(),
            [expectedQueryType](QueryType type) { return type == expectedQueryType; }));
    }

    void thenProvidedQueryCountIsNotGreaterThanAggregationLimit()
    {
        whenExecuteQueries();

        ASSERT_LE(m_executedQueries.size(), m_queryQueue.aggregationLimit());
    }

    void andQueryAggregatesMultipleQueries()
    {
        whenExecuteQueries();

        ASSERT_EQ(m_queryTypes.size(), m_executedQueries.size());
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
    std::optional<std::unique_ptr<AbstractExecutor>> m_prevPopResult;
    detail::QueryQueue m_queryQueue;
    nx::utils::SyncQueue<QueryType> m_timedOutQueries;
    std::deque<QueryType> m_queryTypes;
    std::deque<QueryType> m_executedQueries;

    void processTimedOutQuery(std::unique_ptr<AbstractExecutor> query)
    {
        m_timedOutQueries.push(query->queryType());
    }

    void pushQuery(
        QueryType queryType,
        const std::string& queryAggregationKey = std::string())
    {
        m_queryQueue.push(std::make_unique<QueryExecutorStub>(
            queryType,
            queryAggregationKey,
            std::bind(&QueryQueue::recordQueryExecution, this, queryType)));
        m_queryTypes.push_back(queryType);
    }

    void recordQueryExecution(QueryType queryType)
    {
        m_executedQueries.push_back(queryType);
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

TEST_F(QueryQueue, queries_aggregated_when_possible)
{
    addMultipleModificationQueries("aggregation_key");
    whenPopQueries();
    thenQueriesAggregatedBeforeReturning();
}

TEST_F(QueryQueue, aggregated_queries_consume_concurrent_updates_limit_once)
{
    setConcurrentModificationQueryLimit(1);

    addMultipleModificationQueries("aggregation_key");
    whenPopQueries();
    thenCurrentModificationCountIs(1);

    whenReleasePoppedQueries();
    thenCurrentModificationCountIs(0);
}

TEST_F(QueryQueue, only_queries_of_same_type_are_aggregated)
{
    addMultipleModificationQueries("aggregation_key");
    addMultipleSelectQueries("aggregation_key");

    whenPopQueries();
    thenAggregatedQueriesOfSpecificTypeAreProvided(QueryType::modification);

    whenPopQueries();
    thenAggregatedQueriesOfSpecificTypeAreProvided(QueryType::lookup);
}

TEST_F(QueryQueue, aggregation_limit)
{
    setAggregationLimit();
    addMoreModificationQueriesThanAggregationLimit();

    whenPopQueries();

    thenProvidedQueryCountIsNotGreaterThanAggregationLimit();
}

TEST_F(QueryQueue, pending_query_count_is_reported_correctly)
{
    const int expected = addSeveralQueriesOfDifferentType();
    ASSERT_EQ(expected, queryQueue().pendingQueryCount());
}

//-------------------------------------------------------------------------------------------------

namespace {

class DummyExecutor:
    public BaseExecutor
{
protected:
    using BaseExecutor::BaseExecutor;

    virtual DBResult executeQuery(AbstractDbConnection* const /*connection*/) override
    {
        return DBResultCode::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult /*errorCode*/) override {}
    virtual void setExternalTransaction(Transaction* /*transaction*/) override {}
};

} // namespace

class QueryQueueManualLoadTest:
    public QueryQueue
{
public:
    QueryQueueManualLoadTest()
    {
        queryQueue().setConcurrentModificationQueryLimit(1000);

        queryQueue().setItemStayTimeout(std::chrono::seconds(35));
        queryQueue().setOnItemStayTimeout([](auto&&...) {});
    }

protected:
    void createPopThreads(int count)
    {
        for (int i = 0; i < count; ++i)
            m_popThreads.push_back(std::thread([this]() { popThreadMain(); }));
    }

    void createPushThreads(int count)
    {
        for (int i = 0; i < count; ++i)
            m_pushThreads.push_back(std::thread([this]() { pushThreadMain(); }));
    }

private:
    void popThreadMain()
    {
        while (!m_terminated)
        {
            if (queryQueue().pop(std::chrono::milliseconds(101)))
                ++m_tasksPopped;
        }
    }

    void pushThreadMain()
    {
        while (!m_terminated)
        {
            queryQueue().push(std::make_unique<DummyExecutor>(
                (rand() & 1) ? QueryType::lookup : QueryType::modification,
                (rand() & 1) ? "key" : ""));
            ++m_tasksPushed;
        }
    }

protected:
    std::atomic<long long> m_tasksPushed{0};
    std::atomic<long long> m_tasksPopped{0};

private:
    std::vector<std::thread> m_pushThreads;
    std::vector<std::thread> m_popThreads;
    bool m_terminated = false;
};

TEST_F(
    QueryQueueManualLoadTest,
    DISABLED_multiple_threads_can_push_into_queue_without_blocking_each_other)
{
    const int kPushThreadCount = 40;
    const int kPopThreadCount = 10;

    createPopThreads(kPopThreadCount);
    createPushThreads(kPushThreadCount);

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "tasks pushed/popped: " << std::setw(7)
            << m_tasksPushed <<"/"<< m_tasksPopped << std::endl;
    }
}

} // namespace nx::sql::detail::test
