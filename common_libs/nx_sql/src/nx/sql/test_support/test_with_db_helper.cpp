#include "test_with_db_helper.h"

#include <QtCore/QDir>

#include <nx/utils/log/log_message.h>
#include <nx/utils/std/optional.h>

#include "../db_connection_holder.h"
#include "../sql_query_execution_helper.h"

namespace nx::sql::test {

std::optional<nx::sql::ConnectionOptions> TestWithDbHelper::sDbConnectionOptions;

TestWithDbHelper::TestWithDbHelper(QString moduleName, QString tmpDir):
    utils::test::TestWithTemporaryDirectory(moduleName, tmpDir)
{
    m_dbConnectionOptions.driverType = RdbmsDriverType::sqlite;

    if (sDbConnectionOptions)
        m_dbConnectionOptions = *sDbConnectionOptions;
    m_dbConnectionOptions.maxConnectionCount = 7; // TODO: #ak Make tunable

    if (m_dbConnectionOptions.dbName.isEmpty() && m_dbConnectionOptions.driverType == RdbmsDriverType::sqlite)
    {
        m_dbConnectionOptions.dbName =
            QString("%1/%2").arg(testDataDir()).arg(QString("%1_ut.sqlite").arg(moduleName));
    }

    cleanDatabase();
}

TestWithDbHelper::~TestWithDbHelper()
{
}

const nx::sql::ConnectionOptions& TestWithDbHelper::dbConnectionOptions() const
{
    return m_dbConnectionOptions;
}

nx::sql::ConnectionOptions& TestWithDbHelper::dbConnectionOptions()
{
    return m_dbConnectionOptions;
}

void TestWithDbHelper::setDbConnectionOptions(
    const nx::sql::ConnectionOptions& connectionOptions)
{
    sDbConnectionOptions = connectionOptions;
}

void TestWithDbHelper::cleanDatabase()
{
    if (m_dbConnectionOptions.driverType == RdbmsDriverType::mysql)
    {
        DbConnectionHolder dbConnectionHolder(m_dbConnectionOptions);
        if (!dbConnectionHolder.open())
            throw Exception(DBResult::ioError);

        const bool result = SqlQueryExecutionHelper::execSQLScript(lm(R"sql(
                DROP DATABASE %1;
                CREATE DATABASE %1;
            )sql").args(m_dbConnectionOptions.dbName).toUtf8(),
            *dbConnectionHolder.dbConnection());
        if (!result)
            throw Exception(DBResult::ioError);
    }
}

} // namespace nx::sql::test
