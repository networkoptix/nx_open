// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_db_test.h"

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/sql/query.h>
#include <nx/utils/test_support/utils.h>

namespace nx::sql::test {

BasicFixture::BasicFixture()
{
    m_tmpDir = testDataDir().toStdString() + "/db_test";
    QDir(QString::fromStdString(m_tmpDir)).removeRecursively();
    [this]() { ASSERT_TRUE(QDir().mkpath(m_tmpDir.c_str())); }();

    m_dbFilePath = m_tmpDir;
    m_dbFilePath += "/db.sqlite";

    m_connectionOptions.driverType = RdbmsDriverType::sqlite;
    m_connectionOptions.dbName = QString::fromStdString(m_dbFilePath);
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

DBResult BasicFixture::executeUpdate(const std::string_view& queryText)
{
    return executeQuery(
        [queryText](nx::sql::QueryContext* queryContext)
        {
            auto query = queryContext->connection()->createQuery();
            query->prepare(queryText);
            query->exec();
            return DBResult::ok;
        });
}

nx::utils::filesystem::path BasicFixture::dbFilePath() const
{
    return m_dbFilePath;
}

//-------------------------------------------------------------------------------------------------

void BaseDbTest::setConnectionFactory(
    std::optional<AsyncSqlQueryExecutor::ConnectionFactoryFunc> connectionFactory)
{
    m_connectionFactory = std::move(connectionFactory);
}

bool BaseDbTest::initializeQueryExecutor(const ConnectionOptions& connectionOptions)
{
    m_dbInstanceController = std::make_unique<InstanceController>(connectionOptions);

    if (m_connectionFactory)
    {
        static_cast<AsyncSqlQueryExecutor&>(asyncSqlQueryExecutor())
            .setCustomConnectionFactory([this](auto&&... args)
            {
                return (*m_connectionFactory)(std::forward<decltype(args)>(args)...);
            });
    }

    return m_dbInstanceController->initialize();
}

void BaseDbTest::closeDatabase()
{
    m_dbInstanceController.reset();
}

AbstractAsyncSqlQueryExecutor& BaseDbTest::asyncSqlQueryExecutor()
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

AbstractAsyncSqlQueryExecutor& FixtureWithQueryExecutorOnly::asyncSqlQueryExecutor()
{
    return *m_queryExecutor;
}

} // namespace nx::sql::test
