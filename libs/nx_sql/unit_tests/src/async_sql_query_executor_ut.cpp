// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/utils.h>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/detail/query_execution_thread.h>
#include <nx/sql/filter.h>
#include <nx/sql/sql_cursor.h>
#include <nx/sql/query.h>
#include <nx/sql/test_support/db_connection_mock.h>

#include "base_db_test.h"

namespace nx::sql::test {

struct Company
{
    std::string name;
    int yearFounded;

    bool operator<(const Company& right) const
    {
        return std::tie(name, yearFounded) < std::tie(right.name, right.yearFounded);
    }

    bool operator==(const Company& right) const
    {
        return std::tie(name, yearFounded) == std::tie(right.name, right.yearFounded);
    }
};

void readSqlRecord(SqlQuery* query, Company* company)
{
    company->name = query->value("name").toString().toStdString();
    company->yearFounded = query->value("yearFounded").toInt();
}

//-------------------------------------------------------------------------------------------------
// AsyncSqlQueryExecutor

class DbConnectionEventsReceiver
{
public:
    MOCK_METHOD(void, onConnectionCreated, ());
    MOCK_METHOD(void, onConnectionDestroyed, ());
};

//-------------------------------------------------------------------------------------------------
// AsyncSqlQueryExecutor

constexpr int kDefaultMaxConnectionCount = 29;

class DbAsyncSqlQueryExecutor:
    public test::BaseDbTest
{
    using base_type = test::BaseDbTest;

public:
    DbAsyncSqlQueryExecutor():
        m_forcedDbConnectionError(std::make_shared<DBResult>(DBResult::ok))
    {
        setConnectionFactory([this](auto&&... args) {
            return createConnection(std::forward<decltype(args)>(args)...);
        });

        connectionOptions().maxConnectionCount = kDefaultMaxConnectionCount;
    }

    ~DbAsyncSqlQueryExecutor()
    {
        closeDatabase();

        setConnectionFactory(std::nullopt);
    }

    void setConnectionEventsReceiver(DbConnectionEventsReceiver* eventsReceiver)
    {
        m_eventsReceiver = eventsReceiver;
    }

protected:
    std::vector<Company> givenRandomData()
    {
        std::vector<Company> generatedData;

        EXPECT_EQ(DBResult::ok, executeUpdate(
            "INSERT INTO company (name, yearFounded) VALUES ('Example company 1', 1975)"));
        generatedData.push_back(Company{"Example company 1", 1975});

        EXPECT_EQ(DBResult::ok, executeUpdate(
            "INSERT INTO company (name, yearFounded) VALUES ('Example company 2', 1998)"));
        generatedData.push_back(Company{"Example company 2", 1998});

        EXPECT_EQ(DBResult::ok, executeUpdate(
            "INSERT INTO company (name, yearFounded) VALUES ('Example company 3', 2010)"));
        generatedData.push_back(Company{"Example company 3", 2010});

        return generatedData;
    }

    void whenInitializeDbWithInaccessibleFilePath()
    {
        connectionOptions().dbName += "/raz-raz-raz.sqlite";
        m_lastDbOpenResult = initializeQueryExecutor(connectionOptions());
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
            if (nx::utils::random::number<bool>())
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
                        return std::make_unique<SqlQuery>(queryContext->connection());
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
                    [this](std::unique_ptr<SqlQuery>& query)
                    {
                        saveQueryResult(query->next() ? DBResult::ok : DBResult::ioError);
                    });

                return DBResult::ok;
            },
            nullptr);
    }

    void thenDbInitializationFailed()
    {
        ASSERT_FALSE(m_lastDbOpenResult);
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

        ASSERT_EQ(DBResult::ok, executeUpdate(
            "CREATE TABLE company(name VARCHAR(256), yearFounded INTEGER)"));
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
                auto query = queryContext->connection()->createQuery();
                query->setForwardOnly(true);
                query->prepare("SELECT * FROM company");
                query->exec();
                queryExecuted.set_value();
                m_finishHangingQuery.get_future().wait();
                return DBResult::ok;
            },
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1));

        queryExecuted.get_future().wait();
    }

    void startHangingUpdateQuery()
    {
        using namespace std::placeholders;

        nx::utils::promise<void> queryExecuted;

        asyncSqlQueryExecutor().executeUpdate(
            [this, &queryExecuted](QueryContext* queryContext)
            {
                SqlQuery query(queryContext->connection());
                query.prepare(
                    "INSERT INTO company (name, yearFounded) VALUES ('Foo Inc.', 1975)");
                query.exec();
                // At this point we have active DB transaction with something in rollback journal.
                queryExecuted.set_value();
                m_finishHangingQuery.get_future().wait();
                return DBResult::ok;
            },
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1));

        queryExecuted.get_future().wait();
    }

    void finishHangingQuery()
    {
        m_finishHangingQuery.set_value();
        ASSERT_EQ(DBResult::ok, m_queryResults.pop());
    }

    void makeDbUnavailable()
    {
        *m_forcedDbConnectionError = DBResult::connectionError;
    }

    void makeDbAvailable()
    {
        *m_forcedDbConnectionError = DBResult::ok;
    }

private:
    DbConnectionEventsReceiver* m_eventsReceiver = nullptr;
    nx::utils::SyncQueue<DBResult> m_queryResults;
    int m_issuedRequestCount = 0;
    nx::utils::promise<void> m_finishHangingQuery;
    std::atomic<int> m_maxNumberOfConcurrentDataModificationRequests = 0;
    std::atomic<int> m_concurrentDataModificationRequests = 0;
    bool m_lastDbOpenResult = false;
    std::shared_ptr<DBResult> m_forcedDbConnectionError;

    std::unique_ptr<AbstractDbConnection> createConnection(
        const ConnectionOptions& connectionOptions)
    {
        auto connection = std::make_unique<DbConnectionMock>(
            connectionOptions, m_forcedDbConnectionError);

        if (m_eventsReceiver)
        {
            m_eventsReceiver->onConnectionCreated();
            connection->setOnDestructionHandler(
                std::bind(&DbConnectionEventsReceiver::onConnectionDestroyed, m_eventsReceiver));
        }

        return connection;
    }

    void emulateQueryError(DBResult dbResultToEmulate)
    {
        const auto dbResult = executeQuery(
            [dbResultToEmulate](nx::sql::QueryContext* /*queryContext*/)
            {
                return dbResultToEmulate;
            });
        ASSERT_EQ(dbResultToEmulate, dbResult);
    }

    void issueSelect()
    {
        using namespace std::placeholders;
        asyncSqlQueryExecutor().executeSelect(
            std::bind(&DbAsyncSqlQueryExecutor::selectSomeData, this, _1),
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1));
    }

    void issueUpdate()
    {
        using namespace std::placeholders;
        asyncSqlQueryExecutor().executeUpdate(
            std::bind(&DbAsyncSqlQueryExecutor::insertSomeData, this, _1),
            std::bind(&DbAsyncSqlQueryExecutor::saveQueryResult, this, _1));
    }

    DBResult selectSomeData(QueryContext* queryContext)
    {
        SqlQuery query(queryContext->connection());
        query.setForwardOnly(true);
        query.prepare("SELECT * FROM company");
        query.exec();
        return DBResult::ok;
    }

    DBResult insertSomeData(QueryContext* queryContext)
    {
        const auto concurrentDataModificationRequests = ++m_concurrentDataModificationRequests;
        if (concurrentDataModificationRequests >
            m_maxNumberOfConcurrentDataModificationRequests)
        {
            m_maxNumberOfConcurrentDataModificationRequests =
                concurrentDataModificationRequests;
        }

        auto scopedGuard = nx::utils::makeScopeGuard([this]() { --m_concurrentDataModificationRequests; });

        SqlQuery query(queryContext->connection());
        query.prepare(
            nx::format("INSERT INTO company (name, yearFounded) VALUES ('%1', %2)")
                .args(nx::utils::generateRandomName(7), nx::utils::random::number<int>(1, 2017)).toStdString());
        query.exec();
        return DBResult::ok;
    }

    void saveQueryResult(DBResult dbResult)
    {
        m_queryResults.push(dbResult);
    }
};

TEST_F(DbAsyncSqlQueryExecutor, able_to_execute_query)
{
    initializeDatabase();
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 3', 2010)"));
    const auto companies = executeSelect<Company>("SELECT * FROM company");

    ASSERT_EQ(1U, companies.size());
    ASSERT_EQ("Example company 3", companies[0].name);
    ASSERT_EQ(2010, companies[0].yearFounded);
}

TEST_F(DbAsyncSqlQueryExecutor, db_connection_reopens_after_error)
{
    DbConnectionEventsReceiver connectionEventsReceiver;
    setConnectionEventsReceiver(&connectionEventsReceiver);
    EXPECT_CALL(connectionEventsReceiver, onConnectionCreated()).Times(2);
    EXPECT_CALL(connectionEventsReceiver, onConnectionDestroyed()).Times(2);

    initializeDatabase();
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 1', 1975)"));
    emulateUnrecoverableQueryError();
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 2', 1998)"));

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
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 1', 1975)"));
    emulateRecoverableQueryError();
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 2', 1998)"));

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
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 1', 1975)"));
    for (int i = 0; i < connectionOptions().maxErrorsInARowBeforeClosingConnection + 1; ++i)
        emulateRecoverableQueryError();
    ASSERT_EQ(DBResult::ok, executeUpdate(
        "INSERT INTO company (name, yearFounded) VALUES ('Example company 2', 1998)"));

    const auto companies = executeSelect<Company>("SELECT * FROM company");
    ASSERT_EQ(2U, companies.size());

    closeDatabase();
}

TEST_F(DbAsyncSqlQueryExecutor, initial_error_to_open_a_connection_is_reported)
{
    whenInitializeDbWithInaccessibleFilePath();
    thenDbInitializationFailed();
}

TEST_F(DbAsyncSqlQueryExecutor, reconnect_after_db_failure)
{
    // Given working DB.
    initializeDatabase();
    ASSERT_EQ(DBResult::ok, executeUpdate("INSERT INTO company VALUES ('Foo', 1975)"));

    makeDbUnavailable();

    // Then query fails.
    ASSERT_NE(DBResult::ok, executeUpdate("INSERT INTO company VALUES ('Bar', 1976)"));

    makeDbAvailable();

    // Then DB becomes available again.
    ASSERT_EQ(DBResult::ok, executeUpdate("INSERT INTO company VALUES ('Bar', 1976)"));
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

//-------------------------------------------------------------------------------------------------

class DbAsyncSqlQueryExecutorCursor:
    public DbAsyncSqlQueryExecutorNew
{
    using base_type = DbAsyncSqlQueryExecutorNew;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_initialData = givenRandomData();
        std::sort(m_initialData.begin(), m_initialData.end(), std::less<Company>());
    }

    void givenCursor()
    {
        whenRequestCursor();
        thenCursorIsProvided();
    }

    void whenRequestCursor()
    {
        using namespace std::placeholders;

        ++m_cursorsRequested;
        asyncSqlQueryExecutor().createCursor<Company>(
            std::bind(&DbAsyncSqlQueryExecutorCursor::prepareCursorQuery, this, _1),
            std::bind(&readSqlRecord, _1, _2),
            std::bind(&DbAsyncSqlQueryExecutorCursor::saveCursor, this, _1, _2));
    }

    void whenRequestMultipleCursors()
    {
        const auto cursorsCount = nx::utils::random::number<int>(5,11);
        for (int i = 0; i < cursorsCount; ++i)
            whenRequestCursor();
    }

    void whenDeleteCursor()
    {
        m_cursors.clear();
    }

    void thenCursorIsProvided()
    {
        thenAllCursorsAreProvided();
    }

    void thenAllCursorsAreProvided()
    {
        for (int i = 0; i < m_cursorsRequested; ++i)
        {
            CursorContext cursorContext;
            cursorContext.cursor = m_cursorsCreated.pop();
            m_cursors.push_back(std::move(cursorContext));
            ASSERT_NE(nullptr, m_cursors.back().cursor);
        }
    }

    void andDataCanBeReadUsingCursor()
    {
        readAllData();

        for (auto& cursorContext: m_cursors)
        {
            std::sort(
                cursorContext.recordsRead.begin(),
                cursorContext.recordsRead.end(),
                std::less<Company>());
            ASSERT_EQ(m_initialData, cursorContext.recordsRead);
        }
    }

    void thenCursorQueryIsDeleted()
    {
        while (asyncSqlQueryExecutor().openCursorCount() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    using CompanyCursor = Cursor<Company>;

    struct CursorContext
    {
        std::unique_ptr<CompanyCursor> cursor;
        std::vector<Company> recordsRead;
    };

    nx::utils::SyncQueue<std::unique_ptr<CompanyCursor>> m_cursorsCreated;
    std::vector<Company> m_initialData;
    std::vector<CursorContext> m_cursors;
    int m_cursorsRequested = 0;

    void prepareCursorQuery(SqlQuery* query)
    {
        query->prepare("SELECT * FROM company");
    }

    void saveCursor(DBResult /*resultCode*/, QnUuid cursorId)
    {
        m_cursorsCreated.push(std::make_unique<CompanyCursor>(&asyncSqlQueryExecutor(), cursorId));
    }

    void readAllData()
    {
        bool everyCursorDepleted = false;
        while (!everyCursorDepleted)
        {
            everyCursorDepleted = true;
            for (auto& cursorContext: m_cursors)
            {
                std::optional<Company> record = cursorContext.cursor->next();
                if (!record)
                    continue;
                cursorContext.recordsRead.push_back(std::move(*record));
                everyCursorDepleted = false;
            }
        }
    }
};

TEST_F(DbAsyncSqlQueryExecutorCursor, reading_all_data)
{
    whenRequestCursor();

    thenCursorIsProvided();
    andDataCanBeReadUsingCursor();
}

TEST_F(DbAsyncSqlQueryExecutorCursor, multiple_cursors)
{
    whenRequestMultipleCursors();

    thenAllCursorsAreProvided();
    andDataCanBeReadUsingCursor();
}

TEST_F(DbAsyncSqlQueryExecutorCursor, cursor_query_cleaned_up_when_after_early_cursor_deletion)
{
    givenCursor();
    whenDeleteCursor();
    thenCursorQueryIsDeleted();
}

// TEST_F(DbAsyncSqlQueryExecutorCursor, many_cursors_do_not_block_queries)

//-------------------------------------------------------------------------------------------------

class QueryWithFilter:
    public DbAsyncSqlQueryExecutor
{
    using base_type = DbAsyncSqlQueryExecutor;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        initializeDatabase();

        m_allData = givenRandomData();
    }

    void whenSelectWithFilter(const Filter& filter)
    {
        const auto filterStr = filter.toString();
        std::string queryStr = "SELECT * from company";
        if (!filterStr.empty())
            queryStr += " WHERE " + filterStr;

        m_prevSelectResult = asyncSqlQueryExecutor().executeSelectSync(
            [queryStr, &filter](nx::sql::QueryContext* queryContext)
            {
                SqlQuery query(queryContext->connection());
                query.prepare(queryStr);
                filter.bindFields(&query);
                query.exec();

                std::vector<Company> companies;
                while (query.next())
                {
                    Company data;
                    readSqlRecord(&query, &data);
                    companies.push_back(std::move(data));
                }

                return companies;
            });
    }

    void assertSelectedRecordCount(int count)
    {
        ASSERT_EQ(count, (int) m_prevSelectResult.size());
    }

    void assertRecordIsSelected(const std::string& companyName)
    {
        ASSERT_TRUE(nx::utils::contains_if(
            m_prevSelectResult,
            [&companyName](const Company& company) { return company.name == companyName; }));
    }

private:
    std::vector<Company> m_allData;
    std::vector<Company> m_prevSelectResult;
};

TEST_F(QueryWithFilter, field_is_any_of_value_list)
{
    Filter filter;
    filter.addCondition(std::make_unique<SqlFilterFieldAnyOf>(
        "name", ":name", "Example company 1", "Example company 2"));

    whenSelectWithFilter(filter);

    assertSelectedRecordCount(2);
    assertRecordIsSelected("Example company 1");
    assertRecordIsSelected("Example company 2");
}

TEST_F(QueryWithFilter, multiple_conditions)
{
    Filter filter;
    filter.addCondition(std::make_unique<SqlFilterFieldEqual>(
        "name", ":name", "Example company 1"));
    filter.addCondition(std::make_unique<SqlFilterFieldEqual>(
        "yearFounded", ":yearFounded", 1975));

    whenSelectWithFilter(filter);

    assertSelectedRecordCount(1);
    assertRecordIsSelected("Example company 1");
}

} // namespace nx::sql::test
