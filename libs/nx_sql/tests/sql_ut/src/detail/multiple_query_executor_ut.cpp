#include <memory>

#include <gtest/gtest.h>

#include <nx/sql/detail/multiple_query_executor.h>
#include <nx/utils/random.h>

namespace nx::sql::detail::test {

namespace {

class DbConnectionStub:
    public AbstractDbConnection
{
public:
    virtual bool open() override
    {
        return true;
    }

    virtual void close() override
    {
    }

    virtual bool begin() override
    {
        ++m_totalTransactionCount;
        return true;
    }

    virtual bool commit() override
    {
        ++m_committedTransactionCount;
        return true;
    }

    virtual bool rollback() override
    {
        ++m_rolledBackTransactionCount;
        return true;
    }

    virtual DBResult lastError() override
    {
        return DBResult::ioError;
    }

    virtual std::string lastErrorText() override
    {
        return std::string();
    }

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override
    {
        return nullptr;
    }

    virtual QSqlDatabase* qtSqlConnection() override
    {
        return nullptr;
    }

    int totalTransactionCount() const
    {
        return m_totalTransactionCount;
    }

    int committedTransactionCount() const
    {
        return m_committedTransactionCount;
    }

    int rolledBackTransactionCount() const
    {
        return m_rolledBackTransactionCount;
    }

private:
    int m_totalTransactionCount = 0;
    int m_committedTransactionCount = 0;
    int m_rolledBackTransactionCount = 0;
};

} // namespace

class MultipleQueryExecutor:
    public ::testing::Test
{
protected:
    void givenMultipleGoodQueries()
    {
        using namespace std::placeholders;

        constexpr int queryCount = 3;

        for (int i = 0; i < queryCount; ++i)
        {
            m_queries.push_back(std::make_unique<UpdateWithoutAnyDataExecutor>(
                [](QueryContext*) { return DBResult::ok; },
                std::bind(&MultipleQueryExecutor::saveQueryResult, this, _1),
                std::string()));
        }
    }

    void insertBrokenQueryToRandomPositionInQueryList()
    {
        using namespace std::placeholders;

        auto pos = nx::utils::random::number<std::size_t>(0, m_queries.size()-1);
        m_queries.insert(
            m_queries.begin() + pos,
            std::make_unique<UpdateWithoutAnyDataExecutor>(
                [](QueryContext*) { return DBResult::ioError; },
                std::bind(&MultipleQueryExecutor::saveQueryResult, this, _1),
                std::string()));
    }

    void whenExecute()
    {
        m_queryCount = m_queries.size();
        detail::MultipleQueryExecutor multipleQueryExecutor(std::move(m_queries));
        m_execResult = multipleQueryExecutor.execute(&m_dbConnection);
    }

    void whenRequestReportErrorWithoutExecution()
    {
        m_queryCount = m_queries.size();
        detail::MultipleQueryExecutor multipleQueryExecutor(std::move(m_queries));
        multipleQueryExecutor.reportErrorWithoutExecution(DBResult::ioError);
    }

    void thenExecuteSucceeded()
    {
        ASSERT_EQ(DBResult::ok, m_execResult);
    }

    void thenExecuteFailed()
    {
        ASSERT_NE(DBResult::ok, m_execResult);
    }

    void andEveryQueryHasReportedFailure()
    {
        ASSERT_EQ(m_queryCount, m_queryResults.size());
        for (const auto& queryResult: m_queryResults)
        {
            ASSERT_NE(DBResult::ok, queryResult);
        }
    }

    void andEveryQueryHasReportedSuccess()
    {
        ASSERT_EQ(m_queryCount, m_queryResults.size());
        for (const auto& queryResult : m_queryResults)
        {
            ASSERT_EQ(DBResult::ok, queryResult);
        }
    }

    void andTransactionCommitted()
    {
        ASSERT_EQ(
            m_dbConnection.totalTransactionCount(),
            m_dbConnection.committedTransactionCount());
    }

    void andTransactionRolledBack()
    {
        ASSERT_EQ(
            m_dbConnection.totalTransactionCount(),
            m_dbConnection.rolledBackTransactionCount());
    }

private:
    std::vector<std::unique_ptr<AbstractExecutor>> m_queries;
    std::vector<DBResult> m_queryResults;
    DBResult m_execResult = DBResult::ok;
    int m_queryCount = 0;
    DbConnectionStub m_dbConnection;

    void saveQueryResult(DBResult dbResult)
    {
        m_queryResults.push_back(dbResult);
    }
};

TEST_F(MultipleQueryExecutor, every_query_is_executed)
{
    givenMultipleGoodQueries();

    whenExecute();

    thenExecuteSucceeded();
    andEveryQueryHasReportedSuccess();
    andTransactionCommitted();
}

TEST_F(MultipleQueryExecutor, every_query_is_failed_if_any_query_fails)
{
    givenMultipleGoodQueries();
    insertBrokenQueryToRandomPositionInQueryList();

    whenExecute();

    thenExecuteFailed();
    andEveryQueryHasReportedFailure();
    andTransactionRolledBack();
}

TEST_F(MultipleQueryExecutor, reports_error_without_execution)
{
    givenMultipleGoodQueries();
    whenRequestReportErrorWithoutExecution();
    andEveryQueryHasReportedFailure();
}

} // namespace nx::sql::detail::test
