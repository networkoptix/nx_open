#include "base_db_test.h"

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/utils/db/query.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace utils {
namespace db {
namespace test {

BasicFixture::BasicFixture():
    nx::utils::test::TestWithTemporaryDirectory("utils_ut", "")
{
    init();
}

BasicFixture::~BasicFixture()
{
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
    m_connectionOptions.driverType = RdbmsDriverType::sqlite;
    m_connectionOptions.dbName = m_tmpDir + "/db.sqlite";

    initializeQueryExecutor(m_connectionOptions);
}

void BasicFixture::executeUpdate(const QString& queryText)
{
    const auto dbResult = executeQuery(
        [queryText](nx::utils::db::QueryContext* queryContext)
        {
            SqlQuery query(*queryContext->connection());
            query.prepare(queryText);
            query.exec();
            return DBResult::ok;
        });
    NX_GTEST_ASSERT_EQ(DBResult::ok, dbResult);
}

void BasicFixture::init()
{
    m_tmpDir = testDataDir() + "/db_test/";
    QDir(m_tmpDir).removeRecursively();
    ASSERT_TRUE(QDir().mkpath(m_tmpDir));
}

//-------------------------------------------------------------------------------------------------

void BaseDbTest::initializeQueryExecutor(const ConnectionOptions& connectionOptions)
{
    m_dbInstanceController = std::make_unique<InstanceController>(connectionOptions);
    ASSERT_TRUE(m_dbInstanceController->initialize());
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

void FixtureWithQueryExecutorOnly::initializeQueryExecutor(
    const ConnectionOptions& connectionOptions)
{
    m_queryExecutor = std::make_unique<AsyncSqlQueryExecutor>(connectionOptions);
    ASSERT_TRUE(m_queryExecutor->init());
}

void FixtureWithQueryExecutorOnly::closeDatabase()
{
    m_queryExecutor.reset();
}

AsyncSqlQueryExecutor& FixtureWithQueryExecutorOnly::asyncSqlQueryExecutor()
{
    return *m_queryExecutor;
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
