#pragma once

#include <gtest/gtest.h>

#include <QtCore/QString>
#include <QtSql/QSqlQuery>

#include <nx/fusion/serialization/sql.h>
#include <nx/utils/test_support/utils.h>

#include <nx/utils/db/async_sql_query_executor.h>
#include <nx/utils/db/db_instance_controller.h>
#include <nx/utils/db/types.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx {
namespace utils {
namespace db {
namespace test {

class BaseDbTest:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    BaseDbTest();
    ~BaseDbTest();

protected:
    ConnectionOptions& connectionOptions();
    const ConnectionOptions& connectionOptions() const;
    void initializeDatabase();
    void closeDatabase();
    AsyncSqlQueryExecutor& asyncSqlQueryExecutor();
    void executeUpdate(const QString& queryText);

    template<typename RecordStructure>
    std::vector<RecordStructure> executeSelect(const QString& queryText)
    {
        nx::utils::promise<nx::utils::db::DBResult> queryCompletedPromise;
        auto future = queryCompletedPromise.get_future();

        std::vector<RecordStructure> records;

        asyncSqlQueryExecutor().executeSelect(
            [queryText, &records](
                nx::utils::db::QueryContext* queryContext)
            {
                QSqlQuery query(*queryContext->connection());
                query.prepare(queryText);
                if (!query.exec())
                    return DBResult::ioError;
                QnSql::fetch_many(query, &records);
                return DBResult::ok;
            },
            [&queryCompletedPromise](
                nx::utils::db::QueryContext* /*queryContext*/,
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
        nx::utils::promise<nx::utils::db::DBResult> queryCompletedPromise;

        //starting async operation
        asyncSqlQueryExecutor().executeUpdate(
            dbQueryFunc,
            [&queryCompletedPromise](
                nx::utils::db::QueryContext* /*queryContext*/, DBResult dbResult)
            {
                queryCompletedPromise.set_value(dbResult);
            });

        // Waiting for completion.
        return queryCompletedPromise.get_future().get();
    }

private:
    QString m_tmpDir;
    ConnectionOptions m_connectionOptions;
    std::unique_ptr<InstanceController> m_dbInstanceController;

    void init();
};

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
