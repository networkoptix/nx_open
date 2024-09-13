// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_with_db_helper.h"

#include <QtCore/QDir>

#include <nx/utils/log/format.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/test_support/test_options.h>

#include "../db_connection_holder.h"
#include "../sql_query_execution_helper.h"

namespace nx::sql::test {

std::optional<nx::sql::ConnectionOptions> TestWithDbHelper::sDbConnectionOptions;

TestWithDbHelper::TestWithDbHelper(QString tmpDir):
    utils::test::TestWithTemporaryDirectory(tmpDir)
{
    m_dbConnectionOptions.driverType = RdbmsDriverType::sqlite;

    if (sDbConnectionOptions)
        m_dbConnectionOptions = *sDbConnectionOptions;
    // SqLite doesn't support multi-thread access.
    m_dbConnectionOptions.maxConnectionCount =
        m_dbConnectionOptions.driverType == RdbmsDriverType::sqlite ? 1 : 7;

    if (m_dbConnectionOptions.dbName.isEmpty() && m_dbConnectionOptions.driverType == RdbmsDriverType::sqlite)
    {
        static std::atomic<int> sequence{0};
        m_dbConnectionOptions.dbName =
            QString("%1/%2").arg(testDataDir()).arg(QString("%1.db").arg(++sequence));
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
        DbConnectionHolder dbConnectionHolder(m_dbConnectionOptions, nullptr);
        if (!dbConnectionHolder.open())
            throw Exception(dbConnectionHolder.lastError());

        const bool result = SqlQueryExecutionHelper::execSQLScript(nx::format(R"sql(
                DROP DATABASE %1;
                CREATE DATABASE %1;
            )sql").args(m_dbConnectionOptions.dbName).toUtf8(),
            *dbConnectionHolder.dbConnection());
        if (!result)
            throw Exception(DBResultCode::ioError);
    }
}

} // namespace nx::sql::test
