#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/utils.h>

#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/request_executor_factory.h>
#include <nx/utils/db/request_execution_thread.h>
#include <nx/utils/db/query.h>

#include "base_db_test.h"

namespace nx {
namespace utils {
namespace db {
namespace test {

//-------------------------------------------------------------------------------------------------
// DbRequestExecutionThreadTestWrapper

class DbRequestExecutionThreadTestWrapper:
    public DbRequestExecutionThread
{
public:
    DbRequestExecutionThreadTestWrapper(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue)
        :
        DbRequestExecutionThread(connectionOptions, queryExecutorQueue)
    {
    }

    ~DbRequestExecutionThreadTestWrapper()
    {
        if (m_onDestructionHandler)
            m_onDestructionHandler();
    }

    void setOnDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onDestructionHandler = std::move(handler);
    }

private:
    nx::utils::MoveOnlyFunc<void()> m_onDestructionHandler;
};

//-------------------------------------------------------------------------------------------------
// AsyncSqlQueryExecutor

class DbConnectionEventsReceiver
{
public:
    MOCK_METHOD0(onConnectionCreated, void());
    MOCK_METHOD0(onConnectionDestroyed, void());
};

//-------------------------------------------------------------------------------------------------
// AsyncSqlQueryExecutor

constexpr int kDefaultMaxConnectionCount = 29;

class DbAsyncSqlQueryExecutor:
    public test::BaseDbTest
{
public:
    DbAsyncSqlQueryExecutor():
        m_eventsReceiver(nullptr)
    {
        using namespace std::placeholders;
        RequestExecutorFactory::setFactoryFunc(
            std::bind(&DbAsyncSqlQueryExecutor::createConnection, this, _1, _2));

        connectionOptions().maxConnectionCount = kDefaultMaxConnectionCount;
    }

    ~DbAsyncSqlQueryExecutor()
    {
        RequestExecutorFactory::setFactoryFunc(nullptr);
    }

    void setConnectionEventsReceiver(DbConnectionEventsReceiver* eventsReceiver)
    {
        m_eventsReceiver = eventsReceiver;
    }

protected:
    void givenRandomData()
    {
        executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Microsoft', 1975)");
        executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Google', 1998)");
        executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('NetworkOptix', 2010)");
    }

    void whenIssueMultipleReads()
    {
        using namespace std::placeholders;

        m_issuedRequestCount = connectionOptions().maxConnectionCount * 20;
        for (int i = 0; i < m_issuedRequestCount; ++i)
            issueSelect();
    }

    void whenIssueMultipleUpdates()
    {
        using namespace std::placeholders;

        m_issuedRequestCount = connectionOptions().maxConnectionCount * 2;
        for (int i = 0; i < m_issuedRequestCount; ++i)
            issueUpdate();
    }

    void whenIssueMultipleReadWriteQueries()
    {
        using namespace std::placeholders;

        m_issuedRequestCount = connectionOptions().maxConnectionCount * 20;
        for (int i = 0; i < m_issuedRequestCount; ++i)
        {
            if (random::number<bool>())
                issueSelect();
            else
                issueUpdate();
        }
    }

    void whenIssueMultipleConcurrentReadQueriesInTheSameThread()
    {
        m_issuedRequestCount = 17;

        asyncSqlQueryExecutor().executeSelect(
            [this](QueryContext* queryContext)
            {
                std::vector<std::unique_ptr<SqlQuery>> queries(m_issuedRequestCount);
                std::generate(
                    queries.begin(), queries.end(),
                    [queryContext]()
                    {
                        return std::make_unique<SqlQuery>(*queryContext->connection());
                    });

                std::for_each(
                    queries.begin(), queries.end(),
                    [](std::unique_ptr<SqlQuery>& query)
                    {
                        query->setForwardOnly(true);
                        query->prepare("SELECT * FROM company");
                        query->exec();
                    });

                std::for_each(
                    queries.begin(), queries.end(),
                    [this, queryContext](std::unique_ptr<SqlQuery>& query)
                    {
                        saveQueryResult(
                            queryContext,
                            query->next() ? DBResult::ok : DBResult::ioError);
                    });

                return DBResult::ok;
            },
            nullptr);
    }

    void thenEveryQuerySucceeded()
    {
        for (int i = 0; i < m_issuedRequestCount; ++i)
        {
            ASSERT_EQ(DBResult::ok, m_queryResults.pop());
        }
    }

    void andMaxNumberOfConcurrentDataModificationRequestsIs(int count)
    {
        ASSERT_EQ(count, m_maxNumberOfConcurrentDataModificationRequests.load());
    }

    void initializeDatabase()
    {
        BaseDbTest::initializeDatabase();

        executeUpdate("CREATE TABLE company(name VARCHAR(256), yearFounded INTEGER)");
    }

    void emulateUnrecoverableQueryError()
    {
        emulateQueryError(DBResult::connectionError);
    }

    void emulateRecoverableQueryError()
    {
        emulateQueryError(DBResult::uniqueConstraintViolation);
    }

    void startHangingReadQuery()
    {
        using namespace std::placeholders;

        nx::utils::promise<void> queryExecuted;

        asyncSqlQueryExecutor().executeSelect(
            [this, &queryExecuted](QueryContext* queryContext)
            {
                SqlQuery query(*queryContext->connection());
                query.setForwardOnly(true);
                query.prepare("SELECT * FROM company");
                query.exec();
                queryExecuted.set_value();
                m_finishHangingQuery.get_future().wait();
                return DBResult::ok;
            },
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1, _2));

        queryExecuted.get_future().wait();
    }

    void startHangingUpdateQuery()
    {
        using namespace std::placeholders;

        nx::utils::promise<void> queryExecuted;

        asyncSqlQueryExecutor().executeUpdate(
            [this, &queryExecuted](QueryContext* queryContext)
            {
                SqlQuery query(*queryContext->connection());
                query.prepare(
                    "INSERT INTO company (name, yearFounded) VALUES ('DurakTekstil', 1975)");
                query.exec();
                // At this point we have active DB transaction with something in rollback journal.
                queryExecuted.set_value();
                m_finishHangingQuery.get_future().wait();
                return DBResult::ok;
            },
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1, _2));

        queryExecuted.get_future().wait();
    }

    void finishHangingQuery()
    {
        m_finishHangingQuery.set_value();
        ASSERT_EQ(DBResult::ok, m_queryResults.pop());
    }

private:
    DbConnectionEventsReceiver* m_eventsReceiver;
    nx::utils::SyncQueue<DBResult> m_queryResults;
    int m_issuedRequestCount = 0;
    nx::utils::promise<void> m_finishHangingQuery;
    std::atomic<int> m_maxNumberOfConcurrentDataModificationRequests = 0;
    std::atomic<int> m_concurrentDataModificationRequests = 0;

    void emulateQueryError(DBResult dbResultToEmulate)
    {
        const auto dbResult = executeQuery(
            [dbResultToEmulate](nx::utils::db::QueryContext* /*queryContext*/)
            {
                return dbResultToEmulate;
            });
        NX_GTEST_ASSERT_EQ(dbResultToEmulate, dbResult);
    }

    std::unique_ptr<BaseRequestExecutor> createConnection(
        const ConnectionOptions& connectionOptions,
        QueryExecutorQueue* const queryExecutorQueue)
    {
        auto connection = std::make_unique<DbRequestExecutionThreadTestWrapper>(
            connectionOptions,
            queryExecutorQueue);
        if (m_eventsReceiver)
        {
            m_eventsReceiver->onConnectionCreated();
            connection->setOnDestructionHandler(
                std::bind(&DbConnectionEventsReceiver::onConnectionDestroyed, m_eventsReceiver));
        }
        return std::move(connection);
    }

    void issueSelect()
    {
        using namespace std::placeholders;
        asyncSqlQueryExecutor().executeSelect(
            std::bind(&DbAsyncSqlQueryExecutor::selectSomeData, this, _1),
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1, _2));
    }

    void issueUpdate()
    {
        using namespace std::placeholders;
        asyncSqlQueryExecutor().executeUpdate(
            std::bind(&DbAsyncSqlQueryExecutor::insertSomeData, this, _1),
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1, _2));
    }

    DBResult selectSomeData(QueryContext* queryContext)
    {
        SqlQuery query(*queryContext->connection());
        query.setForwardOnly(true);
        query.prepare("SELECT * FROM company");
        query.exec();
        return DBResult::ok;
    }

    DBResult insertSomeData(QueryContext* queryContext)
    {
        const auto concurrentDataModificationRequests = ++m_concurrentDataModificationRequests;
        if (concurrentDataModificationRequests > 1)
            int x = 0;
        if (concurrentDataModificationRequests >
            m_maxNumberOfConcurrentDataModificationRequests)
        {
            m_maxNumberOfConcurrentDataModificationRequests =
                concurrentDataModificationRequests;
        }

        auto scopedGuard = makeScopeGuard([this]() { --m_concurrentDataModificationRequests; });

        SqlQuery query(*queryContext->connection());
        query.prepare(
            lm("INSERT INTO company (name, yearFounded) VALUES ('%1', %2)")
                .args(nx::utils::generateRandomName(7), random::number<int>(1, 2017)));
        query.exec();
        return DBResult::ok;
    }

    void saveQueryResult(QueryContext* /*queryContext*/, DBResult dbResult)
    {
        m_queryResults.push(dbResult);
    }
};

struct Company
{
    std::string name;
    int yearFounded;
};

#define Company_Fields (name)(yearFounded)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Company),
    (sql_record),
    _Fields)

TEST_F(DbAsyncSqlQueryExecutor, able_to_execute_query)
{
    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('NetworkOptix', 2010)");
    const auto companies = executeSelect<Company>("SELECT * FROM company");

    ASSERT_EQ(1U, companies.size());
    ASSERT_EQ("NetworkOptix", companies[0].name);
    ASSERT_EQ(2010, companies[0].yearFounded);
}

TEST_F(DbAsyncSqlQueryExecutor, db_connection_reopens_after_error)
{
    DbConnectionEventsReceiver connectionEventsReceiver;
    setConnectionEventsReceiver(&connectionEventsReceiver);
    EXPECT_CALL(connectionEventsReceiver, onConnectionCreated()).Times(2);
    EXPECT_CALL(connectionEventsReceiver, onConnectionDestroyed()).Times(2);

    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Microsoft', 1975)");
    emulateUnrecoverableQueryError();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Google', 1998)");

    const auto companies = executeSelect<Company>("SELECT * FROM company");
    ASSERT_EQ(2U, companies.size());

    closeDatabase();
}

TEST_F(DbAsyncSqlQueryExecutor, db_connection_does_not_reopen_after_recoverable_error)
{
    DbConnectionEventsReceiver connectionEventsReceiver;
    setConnectionEventsReceiver(&connectionEventsReceiver);
    EXPECT_CALL(connectionEventsReceiver, onConnectionCreated()).Times(1);
    EXPECT_CALL(connectionEventsReceiver, onConnectionDestroyed()).Times(1);

    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Microsoft', 1975)");
    emulateRecoverableQueryError();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Google', 1998)");

    const auto companies = executeSelect<Company>("SELECT * FROM company");
    ASSERT_EQ(2U, companies.size());

    closeDatabase();
}

TEST_F(DbAsyncSqlQueryExecutor, many_recoverable_errors_in_a_row_cause_reconnect)
{
    connectionOptions().maxErrorsInARowBeforeClosingConnection = 10;

    DbConnectionEventsReceiver connectionEventsReceiver;
    setConnectionEventsReceiver(&connectionEventsReceiver);
    EXPECT_CALL(connectionEventsReceiver, onConnectionCreated()).Times(2);
    EXPECT_CALL(connectionEventsReceiver, onConnectionDestroyed()).Times(2);

    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Microsoft', 1975)");
    for (int i = 0; i < connectionOptions().maxErrorsInARowBeforeClosingConnection + 1; ++i)
        emulateRecoverableQueryError();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Google', 1998)");

    const auto companies = executeSelect<Company>("SELECT * FROM company");
    ASSERT_EQ(2U, companies.size());

    closeDatabase();
}

//-------------------------------------------------------------------------------------------------

class DbAsyncSqlQueryExecutorNew:
    public DbAsyncSqlQueryExecutor
{
    using base_type = DbAsyncSqlQueryExecutor;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        initializeDatabase();
    }
};

TEST_F(DbAsyncSqlQueryExecutorNew, concurrent_reads)
{
    givenRandomData();
    whenIssueMultipleReads();
    thenEveryQuerySucceeded();
}

TEST_F(DbAsyncSqlQueryExecutorNew, concurrent_inserts)
{
    whenIssueMultipleUpdates();
    thenEveryQuerySucceeded();
}

TEST_F(DbAsyncSqlQueryExecutorNew, concurrent_read_writes)
{
    whenIssueMultipleReadWriteQueries();
    thenEveryQuerySucceeded();
}

TEST_F(DbAsyncSqlQueryExecutorNew, sqlite_only_one_update_query_at_a_time)
{
    whenIssueMultipleUpdates();

    thenEveryQuerySucceeded();
    andMaxNumberOfConcurrentDataModificationRequestsIs(1);
}

TEST_F(DbAsyncSqlQueryExecutorNew, interleaved_reads)
{
    givenRandomData();
    whenIssueMultipleConcurrentReadQueriesInTheSameThread();
    thenEveryQuerySucceeded();
}

TEST_F(DbAsyncSqlQueryExecutorNew, DISABLED_concurrent_reads_with_hanging_read)
{
    givenRandomData();

    startHangingReadQuery();

    whenIssueMultipleReads();
    thenEveryQuerySucceeded();

    finishHangingQuery();
}

TEST_F(DbAsyncSqlQueryExecutorNew, DISABLED_interleaved_reads_with_hanging_update)
{
    givenRandomData();

    startHangingUpdateQuery();

    whenIssueMultipleConcurrentReadQueriesInTheSameThread();
    thenEveryQuerySucceeded();

    finishHangingQuery();
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
