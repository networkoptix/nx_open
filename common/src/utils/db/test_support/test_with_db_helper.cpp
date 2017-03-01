#include "test_with_db_helper.h"

#include <QtCore/QDir>

namespace nx {
namespace db {
namespace test {

boost::optional<nx::db::ConnectionOptions> TestWithDbHelper::sDbConnectionOptions;

TestWithDbHelper::TestWithDbHelper(QString moduleName, QString tmpDir):
    utils::test::TestWithTemporaryDirectory(moduleName, tmpDir)
{
    m_dbConnectionOptions.driverType = RdbmsDriverType::sqlite;

    if (sDbConnectionOptions)
        m_dbConnectionOptions = *sDbConnectionOptions;
    m_dbConnectionOptions.maxConnectionCount = 7; // TODO: #ak Make tunable

    if (m_dbConnectionOptions.dbName.isEmpty())
    {
        m_dbConnectionOptions.dbName =
            lit("%1/%2").arg(testDataDir()).arg(lit("%1_ut.sqlite").arg(moduleName));
    }
}

TestWithDbHelper::~TestWithDbHelper()
{
}

const nx::db::ConnectionOptions& TestWithDbHelper::dbConnectionOptions() const
{
    return m_dbConnectionOptions;
}

nx::db::ConnectionOptions& TestWithDbHelper::dbConnectionOptions()
{
    return m_dbConnectionOptions;
}

void TestWithDbHelper::setDbConnectionOptions(
    const nx::db::ConnectionOptions& connectionOptions)
{
    sDbConnectionOptions = connectionOptions;
}

} // namespace test
} // namespace db
} // namespace nx
