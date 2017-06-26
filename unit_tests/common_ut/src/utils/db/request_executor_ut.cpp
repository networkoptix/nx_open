#include "base_db_test.h"

#include <memory>

#include <boost/optional.hpp>

#include <nx/utils/db/db_connection_holder.h>
#include <nx/utils/db/request_executor.h>

namespace nx {
namespace db {
namespace test {

class DbRequestExecutor:
    public BaseDbTest
{
public:
    DbRequestExecutor()
    {
        initializeDatabase();
        
        m_dbConnectionHolder = std::make_unique<DbConnectionHolder>(connectionOptions());
    }

protected:
    void whenExecutingFunctionThatThrows()
    {
        using namespace std::placeholders;

        SelectExecutor selectExecutor(
            std::bind(&DbRequestExecutor::queryFunctionThatAlwaysThrows, this, _1),
            std::bind(&DbRequestExecutor::queryCompletionHandler, this, _1, _2));
        m_executeResult = selectExecutor.execute(m_dbConnectionHolder->dbConnection());
    }

    void thenExceptionIsHandledAndErrorCodeIsReturned()
    {
        ASSERT_EQ(*m_expectedResult, m_executeResult);
        ASSERT_EQ(*m_expectedResult, m_resultProvidedToCompletionHandler);
    }

private:
    std::unique_ptr<DbConnectionHolder> m_dbConnectionHolder;
    boost::optional<DBResult> m_expectedResult;
    DBResult m_executeResult = DBResult::ok;
    DBResult m_resultProvidedToCompletionHandler = DBResult::ok;

    DBResult queryFunctionThatAlwaysThrows(QueryContext*)
    {
        m_expectedResult = DBResult::statementError;
        throw Exception(*m_expectedResult);
    }

    void queryCompletionHandler(QueryContext*, DBResult result)
    {
        m_resultProvidedToCompletionHandler = result;
    }
};

TEST_F(DbRequestExecutor, handles_exception)
{
    whenExecutingFunctionThatThrows();
    thenExceptionIsHandledAndErrorCodeIsReturned();
}

} // namespace test
} // namespace db
} // namespace nx
