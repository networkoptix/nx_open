#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <utils/db/async_sql_query_executor.h>

#include <db/db_instance_controller.h>
#include <ec2/outgoing_transaction_dispatcher.h>
#include <ec2/transaction_log.h>
#include <managers/persistent_layer/account_controller.h>
#include <managers/persistent_layer/system_controller.h>
#include <managers/persistent_layer/system_sharing_controller.h>
#include <test_support/test_with_db_helper.h>

#include "data/account_data.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

class BusinessDataGenerator
{
public:
    static api::AccountData generateRandomAccount()
    {
        return api::AccountData();
    }

    static data::SystemData generateRandomSystem(const api::AccountData& account)
    {
        //NX_ASSERT(!result->systemData.id.empty());
        //result->systemData.name = newSystem.name;
        //result->systemData.customization = newSystem.customization;
        //result->systemData.opaque = newSystem.opaque;
        //result->systemData.authKey = QnUuid::createUuid().toSimpleString().toStdString();
        //result->systemData.ownerAccountEmail = account.email;
        //result->systemData.status = api::SystemStatus::ssNotActivated;
        //result->systemData.expirationTimeUtc =
        //    nx::utils::timeSinceEpoch().count() +
        //    std::chrono::duration_cast<std::chrono::seconds>(
        //        m_settings.systemManager().notActivatedSystemLivePeriod).count();

        return data::SystemData();
    }
};

class BasePersistentDataTest:
    public TestWithDbHelper
{
public:
    BasePersistentDataTest()
    {
        createDatabase();
    }

protected:
    const std::unique_ptr<persistent_layer::DbInstanceController>& persistentDbManager() const
    {
        return m_persistentDbManager;
    }

    void insertRandomAccount()
    {
        using namespace std::placeholders;

        auto account = BusinessDataGenerator::generateRandomAccount();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&persistent_layer::AccountController::insert, &m_accountDbController,
                _1, account));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_accounts.push_back(std::move(account));
    }

    void insertRandomSystem(const api::AccountData& account)
    {
        using namespace std::placeholders;

        auto system = BusinessDataGenerator::generateRandomSystem(account);
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&persistent_layer::SystemController::insert, &m_systemDbController,
                _1, system, account.id));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_systems.push_back(std::move(system));
    }

    void shareSystem(const api::SystemSharingEx& sharing)
    {
        using namespace std::placeholders;

        const auto dbResult = executeUpdateQuerySync(
            std::bind(&persistent_layer::SystemSharingController::insertOrReplaceSharing,
                &m_systemSharingController, _1, sharing));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    }

    const api::AccountData& getAccount(std::size_t index) const
    {
        return m_accounts[index];
    }

    const data::SystemData& getSystem(std::size_t index) const
    {
        return m_systems[index];
    }

protected:
    template<typename QueryFunc, typename... OutputData>
    nx::db::DBResult executeUpdateQuerySync(QueryFunc queryFunc)
    {
        std::promise<nx::db::DBResult> queryDonePromise;
        m_persistentDbManager->queryExecutor()->executeUpdate(
            queryFunc,
            [&queryDonePromise](
                nx::db::QueryContext*,
                nx::db::DBResult dbResult,
                OutputData... outputData)
            {
                queryDonePromise.set_value(dbResult);
            });
        return queryDonePromise.get_future().get();
    }

private:
    std::unique_ptr<persistent_layer::DbInstanceController> m_persistentDbManager;
    persistent_layer::AccountController m_accountDbController;
    persistent_layer::SystemController m_systemDbController;
    persistent_layer::SystemSharingController m_systemSharingController;
    std::vector<api::AccountData> m_accounts;
    std::vector<data::SystemData> m_systems;

    void createDatabase()
    {
        dbConnectionOptions().maxConnectionCount = 10;

        m_persistentDbManager =
            std::make_unique<persistent_layer::DbInstanceController>(dbConnectionOptions());
        ASSERT_TRUE(m_persistentDbManager->initialize());
    }
};

class TransactionLog:
    public BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog()
    {
        initializeTransactionLog();
    }

    void givenRandomSystem()
    {
        using namespace std::placeholders;

        constexpr int accountCount = 10;

        // Creating accounts.
        for (int i = 0; i < accountCount; ++i)
            insertRandomAccount();
        insertRandomSystem(getAccount(0));

        // Adding users to the system.
        for (int i = 1; i < accountCount; ++i)
        {
            api::SystemSharingEx sharing;
            sharing.systemId = getSystem(0).id;
            sharing.accountEmail = getAccount(i).email;
            sharing.accessRole = api::SystemAccessRole::advancedViewer;
            shareSystem(std::move(sharing));
        }

        // Initializing system's transaction log.
        const nx::String systemId = getSystem(0).id.c_str();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&ec2::TransactionLog::updateTimestampHiForSystem, m_transactionLog.get(),
                _1, systemId, getSystem(0).systemSequence));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    }

    void havingAddedBunchOfTransactionsConcurrently()
    {
        // TODO
    }

    void expectTransactionsAreSentInAscendingSequenceOrder()
    {
        // TODO
    }

private:
    OutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    std::unique_ptr<ec2::TransactionLog> m_transactionLog;

    void initializeTransactionLog()
    {
        m_transactionLog = std::make_unique<ec2::TransactionLog>(
            QnUuid::createUuid(),
            persistentDbManager()->queryExecutor().get(),
            &m_outgoingTransactionDispatcher);
    }
};

TEST_F(TransactionLog, multiple_simultaneous_transactions)
{
    givenRandomSystem();
    havingAddedBunchOfTransactionsConcurrently();
    expectTransactionsAreSentInAscendingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
