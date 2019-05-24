#include "base_db_test.h"

#include <nx/sql/query.h>
#include <nx/utils/test_support/utils.h>

namespace nx::sql::test {

BasicFixture::BasicFixture(const std::string& testModuleName):
    nx::utils::test::TestWithTemporaryDirectory(testModuleName.c_str(), "")
{
    m_tmpDir = testDataDir().toStdString() + "/db_test";
    std::filesystem::remove_all(m_tmpDir);
    [this]() { ASSERT_TRUE(std::filesystem::create_directories(m_tmpDir)); }();
    
    m_dbFilePath = m_tmpDir;
    m_dbFilePath += "/db.sqlite";

    m_connectionOptions.driverType = RdbmsDriverType::sqlite;
    m_connectionOptions.dbName = QString::fromStdString(m_dbFilePath.string());
}

ConnectionOptions& BasicFixture::connectionOptions()
{
    return m_connectionOptions;
}

const ConnectionOptions& BasicFixture::connectionOptions() const
{
    return m_connectionOptions;
}

void BasicFixture::initializeDatabase()
{
    ASSERT_TRUE(initializeQueryExecutor(m_connectionOptions));
}

void BasicFixture::executeUpdate(const QString& queryText)
{
    const auto dbResult = executeQuery(
        [queryText](nx::sql::QueryContext* queryContext)
        {
            SqlQuery query(queryContext->connection());
            query.prepare(queryText);
            query.exec();
            return DBResult::ok;
        });
    NX_GTEST_ASSERT_EQ(DBResult::ok, dbResult);
}

std::filesystem::path BasicFixture::dbFilePath() const
{
    return m_dbFilePath;
}

//-------------------------------------------------------------------------------------------------

bool BaseDbTest::initializeQueryExecutor(const ConnectionOptions& connectionOptions)
{
    m_dbInstanceController = std::make_unique<InstanceController>(connectionOptions);
    return m_dbInstanceController->initialize();
}

void BaseDbTest::closeDatabase()
{
    m_dbInstanceController.reset();
}

AsyncSqlQueryExecutor& BaseDbTest::asyncSqlQueryExecutor()
{
    return m_dbInstanceController->queryExecutor();
}

//-------------------------------------------------------------------------------------------------

bool FixtureWithQueryExecutorOnly::initializeQueryExecutor(
    const ConnectionOptions& connectionOptions)
{
    m_queryExecutor = std::make_unique<AsyncSqlQueryExecutor>(connectionOptions);
    return m_queryExecutor->init();
}

void FixtureWithQueryExecutorOnly::closeDatabase()
{
    m_queryExecutor.reset();
}

AsyncSqlQueryExecutor& FixtureWithQueryExecutorOnly::asyncSqlQueryExecutor()
{
    return *m_queryExecutor;
}

} // namespace nx::sql::test
