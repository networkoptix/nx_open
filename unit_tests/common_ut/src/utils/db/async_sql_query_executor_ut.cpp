#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/utils.h>

#include <utils/db/async_sql_query_executor.h>
#include <utils/db/request_executor_factory.h>
#include <utils/db/request_execution_thread.h>

#include "base_db_test.h"

namespace nx {
namespace db {

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
// AsyncSqlQueryExecutorTest

class DbConnectionEventsReceiver
{
public:
    MOCK_METHOD0(onConnectionCreated, void());
    MOCK_METHOD0(onConnectionDestroyed, void());
};

//-------------------------------------------------------------------------------------------------
// AsyncSqlQueryExecutorTest

constexpr int kDefaultMaxConnectionCount = 10;

class AsyncSqlQueryExecutorTest:
    public test::BaseDbTest
{
public:
    AsyncSqlQueryExecutorTest():
        m_eventsReceiver(nullptr)
    {
        using namespace std::placeholders;
        RequestExecutorFactory::setFactoryFunc(
            std::bind(&AsyncSqlQueryExecutorTest::createConnection, this, _1, _2));

        connectionOptions().maxConnectionCount = kDefaultMaxConnectionCount;
    }

    ~AsyncSqlQueryExecutorTest()
    {
    }

    void setConnectionEventsReceiver(DbConnectionEventsReceiver* eventsReceiver)
    {
        m_eventsReceiver = eventsReceiver;
    }

protected:
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

private:
    DbConnectionEventsReceiver* m_eventsReceiver;

    void emulateQueryError(DBResult dbResultToEmulate)
    {
        const auto dbResult = executeQuery(
            [dbResultToEmulate](nx::db::QueryContext* /*queryContext*/)
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

TEST_F(AsyncSqlQueryExecutorTest, able_to_execute_query)
{
    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('NetworkOptix', 2010)");
    const auto companies = executeSelect<Company>("SELECT * FROM company");

    ASSERT_EQ(1, companies.size());
    ASSERT_EQ("NetworkOptix", companies[0].name);
    ASSERT_EQ(2010, companies[0].yearFounded);
}

TEST_F(AsyncSqlQueryExecutorTest, db_connection_reopens_after_error)
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
    ASSERT_EQ(2, companies.size());

    closeDatabase();
}

TEST_F(AsyncSqlQueryExecutorTest, db_connection_does_not_reopen_after_recoverable_error)
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
    ASSERT_EQ(2, companies.size());

    closeDatabase();
}

TEST_F(AsyncSqlQueryExecutorTest, many_recoverable_errors_in_a_row_cause_reconnect)
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
    ASSERT_EQ(2, companies.size());

    closeDatabase();
}

} // namespace db
} // namespace nx
