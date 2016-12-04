#pragma once

#include <gtest/gtest.h>

#include <QtCore/QString>

#include <utils/db/async_sql_query_executor.h>
#include <utils/db/types.h>

#include "test_setup.h"

namespace nx {
namespace db {
namespace test {

class BaseDbTest:
    public ::testing::Test
{
public:
    BaseDbTest();
    ~BaseDbTest();

protected:
    ConnectionOptions& connectionOptions();
    const ConnectionOptions& connectionOptions() const;
    void initializeDatabase();
    void closeDatabase();
    const std::unique_ptr<AsyncSqlQueryExecutor>& asyncSqlQueryExecutor();
    void executeUpdate(const QString& queryText);

    template<typename RecordStructure>
    std::vector<RecordStructure> executeSelect(const QString& queryText)
    {
        nx::utils::promise<nx::db::DBResult> queryCompletedPromise;
        auto future = queryCompletedPromise.get_future();

        std::vector<RecordStructure> records;

        asyncSqlQueryExecutor()->executeSelect(
            [queryText, &records](
                nx::db::QueryContext* queryContext)
            {
                QSqlQuery query(*queryContext->connection());
                query.prepare(queryText);
                if (!query.exec())
                    return DBResult::ioError;
                QnSql::fetch_many(query, &records);
                return DBResult::ok;
            },
            [&queryCompletedPromise](
                nx::db::QueryContext* /*queryContext*/,
                DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        NX_GTEST_ASSERT_EQ(DBResult::ok, future.get());

        return records;
    }

    template<typename DbQueryFunc>
    DBResult executeQuery(DbQueryFunc dbQueryFunc)
    {
        nx::utils::promise<nx::db::DBResult> queryCompletedPromise;

        //starting async operation
        asyncSqlQueryExecutor()->executeUpdate(
            dbQueryFunc,
            [&queryCompletedPromise](
                nx::db::QueryContext* /*queryContext*/, DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        // Waiting for completion.
        return queryCompletedPromise.get_future().get();
    }

private:
    QString m_tmpDir;
    ConnectionOptions m_connectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_asyncSqlQueryExecutor;

    void init();
};

} // namespace test
} // namespace db
} // namespace nx
