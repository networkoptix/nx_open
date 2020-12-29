#include <memory>

#include <nx/sql/db_connection_holder.h>
#include <nx/sql/detail/query_executor.h>
#include <nx/utils/std/optional.h>

#include "../base_db_test.h"

namespace nx::sql::detail::test {

class DbRequestExecutor:
    public nx::sql::test::BaseDbTest
{
    using base_type = nx::sql::test::BaseDbTest;

public:
    DbRequestExecutor():
        base_type(kModuleName)
    {
        initializeDatabase();

        m_dbConnectionHolder = std::make_unique<DbConnectionHolder>(connectionOptions());
    }

protected:
    void whenExecutingFunctionThatThrows()
    {
        using namespace std::placeholders;

        detail::SelectExecutor selectExecutor(
            std::bind(&DbRequestExecutor::queryFunctionThatAlwaysThrows, this, _1),
            std::bind(&DbRequestExecutor::queryCompletionHandler, this, _1),
            std::string());
        m_executeResult = selectExecutor.execute(m_dbConnectionHolder->dbConnection());
    }

    void thenExceptionIsHandledAndErrorCodeIsReturned()
    {
        ASSERT_EQ(*m_expectedResult, m_executeResult);
        ASSERT_EQ(*m_expectedResult, m_resultProvidedToCompletionHandler);
    }

private:
    std::unique_ptr<DbConnectionHolder> m_dbConnectionHolder;
    std::optional<DBResult> m_expectedResult;
    DBResult m_executeResult = DBResult::ok;
    DBResult m_resultProvidedToCompletionHandler = DBResult::ok;

    DBResult queryFunctionThatAlwaysThrows(QueryContext*)
    {
        m_expectedResult = DBResult::statementError;
        throw Exception(*m_expectedResult);
    }

    void queryCompletionHandler(DBResult result)
    {
        m_resultProvidedToCompletionHandler = result;
    }
};

TEST_F(DbRequestExecutor, handles_exception)
{
    whenExecutingFunctionThatThrows();
    thenExceptionIsHandledAndErrorCodeIsReturned();
}

} // namespace nx::sql::detail::test
