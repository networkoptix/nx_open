#include <future>

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/sql/detail/query_executor_factory.h>
#include <nx/sql/detail/query_execution_thread.h>

#include "functional_tests/test_setup.h"
#include "functional_tests/test_email_manager.h"

namespace nx::cloud::db {
namespace test {

class DbRegress:
    public CdbFunctionalTest
{
public:
    DbRegress()
    {
        QDir().mkpath(testDataDir());
    }
};

TEST_F(DbRegress, general)
{
    if (isStartedWithExternalDb())
        return; //< Ignoring.
    ASSERT_TRUE(placePreparedDB(":/cdb.sqlite"));

    ASSERT_TRUE(startAndWaitUntilStarted());

    //checking that cdb has started
    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    api::AccountData account1Tmp;
    result = getAccount(account1.email, account1Password, &account1Tmp);
    ASSERT_EQ(api::ResultCode::ok, result);

    //checking that data is there
    api::AccountData testAccount;
    ASSERT_EQ(
        api::ResultCode::ok,
        getAccount("akolesnikov@networkoptix.com", "123", &testAccount));
    ASSERT_EQ("Andrey Kolesnikov", testAccount.fullName);

    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems("akolesnikov@networkoptix.com", "123", &systems));
    ASSERT_EQ(6U, systems.size());

    const auto laOfficeTestSystemIter = std::find_if(
        systems.begin(), systems.end(),
        [](api::SystemDataEx& systemData)
        {
            return systemData.name == "la_office_test";
        });
    ASSERT_NE(systems.end(), laOfficeTestSystemIter);
    ASSERT_EQ(api::SystemStatus::activated, laOfficeTestSystemIter->status);
}

//-------------------------------------------------------------------------------------------------

namespace {

class QueryExecutorStub:
    public nx::sql::detail::QueryExecutionThread
{
    using base_type = nx::sql::detail::QueryExecutionThread;

public:
    QueryExecutorStub(
        const nx::sql::ConnectionOptions& connectionOptions,
        nx::sql::detail::QueryExecutorQueue* const queryExecutorQueue)
        :
        base_type(connectionOptions, queryExecutorQueue)
    {
    }

    void setForcedDbResult(boost::optional<nx::sql::DBResult> forcedDbResult)
    {
        m_forcedDbResult = forcedDbResult;
    }

protected:
    virtual void processTask(std::unique_ptr<nx::sql::detail::AbstractExecutor> task) override
    {
        if (m_forcedDbResult)
            task->reportErrorWithoutExecution(*m_forcedDbResult);
        else
            base_type::processTask(std::move(task));
    }

private:
    boost::optional<nx::sql::DBResult> m_forcedDbResult;
};

} // namespace

class DbFailure:
    public CdbFunctionalTest
{
public:
    DbFailure()
    {
        using namespace std::placeholders;

        QDir().mkpath(testDataDir());

        addArg("--db/maxPeriodQueryWaitsForAvailableConnection=1s");
        addArg("--db/maxConnectionCount=1");

        // Installing sql query executor that always reports timedout.
        m_requestExecutorFactoryBak =
            nx::sql::detail::RequestExecutorFactory::instance().setCustomFunc(
                std::bind(&DbFailure::createInfiniteLatencyDbQueryExecutorFactory, this, _1, _2));
    }

    ~DbFailure()
    {
        nx::sql::detail::RequestExecutorFactory::instance().setCustomFunc(
            std::move(m_requestExecutorFactoryBak));
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_cdbConnection = connection(std::string(), std::string());
    }

    void givenInfiniteLatencyDb()
    {
        m_prevDbQueryExecutor->setForcedDbResult(nx::sql::DBResult::retryLater);
    }

    void whenIssueDataModificationRequest()
    {
        using namespace std::placeholders;

        api::AccountRegistrationData accountData;
        accountData.email = generateRandomEmailAddress();
        accountData.passwordHa1 = "sdfdsfsdf";
        m_cdbConnection->accountManager()->registerNewAccount(
            std::move(accountData),
            std::bind(&DbFailure::saveRequestResult, this, _1));
    }

    void thenResultCodeIs(api::ResultCode expected)
    {
        ASSERT_EQ(expected, m_requestResultCodeQueue.pop());
    }

private:
    nx::sql::detail::RequestExecutorFactory::Function m_requestExecutorFactoryBak;
    nx::utils::SyncQueue<api::ResultCode> m_requestResultCodeQueue;
    std::unique_ptr<api::Connection> m_cdbConnection;
    QueryExecutorStub* m_prevDbQueryExecutor = nullptr;

    void saveRequestResult(api::ResultCode resultCode)
    {
        m_requestResultCodeQueue.push(resultCode);
    }

    std::unique_ptr<nx::sql::detail::BaseQueryExecutor>
        createInfiniteLatencyDbQueryExecutorFactory(
            const nx::sql::ConnectionOptions& connectionOptions,
            nx::sql::detail::QueryExecutorQueue* const queryExecutorQueue)
    {
        auto result = std::make_unique<QueryExecutorStub>(
            connectionOptions,
            queryExecutorQueue);
        m_prevDbQueryExecutor = result.get();
        return std::move(result);
    }
};

TEST_F(DbFailure, timed_out_db_query_results_in_retryLater_result)
{
    givenInfiniteLatencyDb();
    whenIssueDataModificationRequest();
    thenResultCodeIs(api::ResultCode::retryLater);
}

} // namespace test
} // namespace nx::cloud::db
