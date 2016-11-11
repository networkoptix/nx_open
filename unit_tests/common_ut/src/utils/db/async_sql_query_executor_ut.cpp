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

#include "test_setup.h"

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

constexpr static auto kQueryCompletionTimeout = std::chrono::seconds(10);

class AsyncSqlQueryExecutorTest:
    public ::testing::Test
{
public:
    AsyncSqlQueryExecutorTest():
        m_eventsReceiver(nullptr)
    {
        using namespace std::placeholders;
        RequestExecutorFactory::setFactoryFunc(
            std::bind(&AsyncSqlQueryExecutorTest::createConnection, this, _1, _2));

        init();
    }

    ~AsyncSqlQueryExecutorTest()
    {
        QDir(m_tmpDir).removeRecursively();
    }

    void setConnectionEventsReceiver(DbConnectionEventsReceiver* const eventsReceiver)
    {
        m_eventsReceiver = eventsReceiver;
    }

protected:
    const ConnectionOptions& connectionOptions() const
    {
        return m_connectionOptions;
    }

    const std::unique_ptr<AsyncSqlQueryExecutor>& asyncSqlQueryExecutor()
    {
        return m_asyncSqlQueryExecutor;
    }

    void initializeDatabase()
    {
        m_connectionOptions.maxConnectionCount = 10;
        m_connectionOptions.driverType = RdbmsDriverType::sqlite;
        m_connectionOptions.dbName = m_tmpDir + "/db.sqlite";

        m_asyncSqlQueryExecutor =
            std::make_unique<AsyncSqlQueryExecutor>(m_connectionOptions);
        ASSERT_TRUE(m_asyncSqlQueryExecutor->init());
        
        executeUpdate("CREATE TABLE company(name VARCHAR(256), yearFounded INTEGER)");
    }

    void closeDatabase()
    {
        m_asyncSqlQueryExecutor.reset();
    }

    void executeUpdate(const QString& queryText)
    {
        const auto dbResult = executeQuery(
            [queryText](nx::db::QueryContext* queryContext)
            {
                QSqlQuery query(*queryContext->connection());
                query.prepare(queryText);
                if (!query.exec())
                    return DBResult::ioError;
                return DBResult::ok;
            });
        NX_GTEST_ASSERT_EQ(DBResult::ok, dbResult);
    }

    template<typename RecordStructure>
    std::vector<RecordStructure> executeSelect(const QString& queryText)
    {
        nx::utils::promise<nx::db::DBResult> queryCompletedPromise;
        auto future = queryCompletedPromise.get_future();

        std::vector<RecordStructure> outputRecords;

        m_asyncSqlQueryExecutor->executeSelect<std::vector<RecordStructure>>(
            [queryText](
                nx::db::QueryContext* queryContext,
                std::vector<RecordStructure>* const records)
            {
                QSqlQuery query(*queryContext->connection());
                query.prepare(queryText);
                if (!query.exec())
                    return DBResult::ioError;
                QnSql::fetch_many(query, records);
                return DBResult::ok;
            },
            [&queryCompletedPromise, &outputRecords](
                nx::db::QueryContext* /*queryContext*/,
                DBResult dbResult,
                std::vector<RecordStructure> records)
            {
                outputRecords = std::move(records);
                queryCompletedPromise.set_value(dbResult);
            });

        NX_GTEST_ASSERT_EQ(std::future_status::ready, future.wait_for(kQueryCompletionTimeout));
        NX_GTEST_ASSERT_EQ(DBResult::ok, future.get());

        return outputRecords;
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
    ConnectionOptions m_connectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_asyncSqlQueryExecutor;
    QString m_tmpDir;
    DbConnectionEventsReceiver* m_eventsReceiver;

    void init()
    {
        m_tmpDir = TestSetup::getTemporaryDirectoryPath() + "/db_test/";
        QDir(m_tmpDir).removeRecursively();
        ASSERT_TRUE(QDir().mkpath(m_tmpDir));
    }

    template<typename DbQueryFunc>
    DBResult executeQuery(DbQueryFunc dbQueryFunc)
    {
        nx::utils::promise<nx::db::DBResult> queryCompletedPromise;
        auto future = queryCompletedPromise.get_future();

        //starting async operation
        m_asyncSqlQueryExecutor->executeUpdate(
            dbQueryFunc,
            [&queryCompletedPromise](
                nx::db::QueryContext* /*queryContext*/, DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        //waiting for completion
        NX_GTEST_ASSERT_EQ(std::future_status::ready, future.wait_for(kQueryCompletionTimeout));
        return future.get();
    }

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

} // namespace db
} // namespace nx
