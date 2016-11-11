#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/utils.h>

#include <utils/db/async_sql_query_executor.h>

#include "test_setup.h"

namespace nx {
namespace db {

namespace {

constexpr auto kQueryCompletionTimeout = std::chrono::seconds(10);

} // namespace

class AsyncSqlQueryExecutorTest:
    public ::testing::Test
{
public:
    AsyncSqlQueryExecutorTest()
    {
        init();
    }

    ~AsyncSqlQueryExecutorTest()
    {
        QDir(m_tmpDir).removeRecursively();
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

private:
    ConnectionOptions m_connectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_asyncSqlQueryExecutor;
    QString m_tmpDir;

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
    // mock: two connections created, one closed

    initializeDatabase();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Microsoft', 1975)");
    emulateUnrecoverableQueryError();
    executeUpdate("INSERT INTO company (name, yearFounded) VALUES ('Google', 1998)");

    const auto companies = executeSelect<Company>("SELECT * FROM company");
    ASSERT_EQ(2, companies.size());
}

} // namespace db
} // namespace nx
