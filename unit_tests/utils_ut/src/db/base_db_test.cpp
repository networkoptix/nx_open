#include "base_db_test.h"

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/utils/db/query.h>
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

    m_dbInstanceController = std::make_unique<InstanceController>(m_connectionOptions);
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

void BaseDbTest::executeUpdate(const QString& queryText)
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
