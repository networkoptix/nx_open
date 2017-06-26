#include "base_db_test.h"

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/utils/test_support/utils.h>

namespace nx {
namespace utils {
namespace db {
namespace test {

BaseDbTest::BaseDbTest():
    nx::utils::test::TestWithTemporaryDirectory("utils_ut", "")
{
    init();
}

BaseDbTest::~BaseDbTest()
{
}

ConnectionOptions& BaseDbTest::connectionOptions()
{
    return m_connectionOptions;
}

const ConnectionOptions& BaseDbTest::connectionOptions() const
{
    return m_connectionOptions;
}

void BaseDbTest::initializeDatabase()
{
    m_connectionOptions.driverType = RdbmsDriverType::sqlite;
    m_connectionOptions.dbName = m_tmpDir + "/db.sqlite";

    m_asyncSqlQueryExecutor = std::make_unique<AsyncSqlQueryExecutor>(m_connectionOptions);
    ASSERT_TRUE(m_asyncSqlQueryExecutor->init());
}

void BaseDbTest::closeDatabase()
{
    m_asyncSqlQueryExecutor.reset();
}

const std::unique_ptr<AsyncSqlQueryExecutor>& BaseDbTest::asyncSqlQueryExecutor()
{
    return m_asyncSqlQueryExecutor;
}

void BaseDbTest::executeUpdate(const QString& queryText)
{
    const auto dbResult = executeQuery(
        [queryText](nx::utils::db::QueryContext* queryContext)
        {
            QSqlQuery query(*queryContext->connection());
            query.prepare(queryText);
            if (!query.exec())
                return DBResult::ioError;
            return DBResult::ok;
        });
    NX_GTEST_ASSERT_EQ(DBResult::ok, dbResult);
}

void BaseDbTest::init()
{
    m_tmpDir = testDataDir() + "/db_test/";
    QDir(m_tmpDir).removeRecursively();
    ASSERT_TRUE(QDir().mkpath(m_tmpDir));
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
