// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/db_instance_controller.h>
#include <nx/sql/types.h>
#include <nx/utils/std/filesystem.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/test_support/utils.h>

#include <module_name.h>

namespace nx::sql::test {

// TODO: #akolesnikov Fix weird class names in this file.

class BasicFixture:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    BasicFixture();

protected:
    virtual bool initializeQueryExecutor(const ConnectionOptions& connectionOptions) = 0;
    virtual void closeDatabase() = 0;
    virtual AbstractAsyncSqlQueryExecutor& asyncSqlQueryExecutor() = 0;

    ConnectionOptions& connectionOptions();
    const ConnectionOptions& connectionOptions() const;
    void initializeDatabase();
    DBResult executeUpdate(const std::string_view& queryText);

    template<typename RecordStructure>
    std::vector<RecordStructure> executeSelect(const std::string_view& queryText)
    {
        nx::utils::promise<nx::sql::DBResult> queryCompletedPromise;
        auto future = queryCompletedPromise.get_future();

        std::vector<RecordStructure> records;

        asyncSqlQueryExecutor().executeSelect(
            [queryText, &records](
                nx::sql::QueryContext* queryContext)
            {
                sql::SqlQuery query(queryContext->connection());
                query.prepare(queryText);
                query.exec();

                while (query.next())
                {
                    RecordStructure data;
                    readSqlRecord(&query, &data);
                    records.push_back(std::move(data));
                }

                return DBResult::ok;
            },
            [&queryCompletedPromise](DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        NX_GTEST_ASSERT_EQ(DBResult::ok, future.get());

        return records;
    }

    template<typename DbQueryFunc>
    DBResult executeQuery(DbQueryFunc dbQueryFunc)
    {
        nx::utils::promise<nx::sql::DBResult> queryCompletedPromise;

        //starting async operation
        asyncSqlQueryExecutor().executeUpdate(
            dbQueryFunc,
            [&queryCompletedPromise](DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        // Waiting for completion.
        return queryCompletedPromise.get_future().get();
    }

    nx::utils::filesystem::path dbFilePath() const;

private:
    std::string m_tmpDir;
    ConnectionOptions m_connectionOptions;
    std::string m_dbFilePath;
};

//-------------------------------------------------------------------------------------------------

/**
 * This fixture prepares DB struct for updates, tunes DB.
 */
class BaseDbTest:
    public BasicFixture
{
    using base_type = BasicFixture;

public:
    using base_type::base_type;

    void setConnectionFactory(
        std::optional<AsyncSqlQueryExecutor::ConnectionFactoryFunc> connectionFactory);

protected:
    virtual bool initializeQueryExecutor(const ConnectionOptions& connectionOptions) override;
    virtual void closeDatabase() override;
    virtual AbstractAsyncSqlQueryExecutor& asyncSqlQueryExecutor() override;

private:
    std::optional<AsyncSqlQueryExecutor::ConnectionFactoryFunc> m_connectionFactory;
    std::unique_ptr<InstanceController> m_dbInstanceController;
};

//-------------------------------------------------------------------------------------------------

class FixtureWithQueryExecutorOnly:
    public BasicFixture
{
    using base_type = BasicFixture;

public:
    using base_type::base_type;

protected:
    virtual bool initializeQueryExecutor(const ConnectionOptions& connectionOptions) override;
    virtual void closeDatabase() override;
    virtual AbstractAsyncSqlQueryExecutor& asyncSqlQueryExecutor() override;

private:
    std::unique_ptr<AsyncSqlQueryExecutor> m_queryExecutor;
};

} // namespace nx::sql::test
