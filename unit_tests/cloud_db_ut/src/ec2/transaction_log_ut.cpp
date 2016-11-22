#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>

#include <utils/db/async_sql_query_executor.h>

#include <db/db_instance_controller.h>
#include <ec2/data_conversion.h>
#include <ec2/outgoing_transaction_dispatcher.h>
#include <ec2/transaction_log.h>
#include <managers/persistent_layer/account_controller.h>
#include <managers/persistent_layer/system_controller.h>
#include <managers/persistent_layer/system_sharing_controller.h>
#include <test_support/business_data_generator.h>
#include <test_support/test_with_db_helper.h>

#include "data/account_data.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

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

        auto account = cdb::test::BusinessDataGenerator::generateRandomAccount();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&persistent_layer::AccountController::insert, &m_accountDbController,
                _1, account));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_accounts.push_back(std::move(account));
    }

    void insertRandomSystem(const api::AccountData& account)
    {
        using namespace std::placeholders;

        auto system = cdb::test::BusinessDataGenerator::generateRandomSystem(account);
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&persistent_layer::SystemController::insert, &m_systemDbController,
                _1, system, account.id));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_systems.push_back(std::move(system));
    }

    void insertSystemSharing(const api::SystemSharingEx& sharing)
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
    const std::vector<api::AccountData>& accounts() const
    {
        return m_accounts;
    }

    const std::vector<data::SystemData>& systems() const
    {
        return m_systems;
    }

    persistent_layer::SystemSharingController& systemSharingController()
    {
        return m_systemSharingController;
    }

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

class TestOutgoingTransactionDispatcher:
    public AbstractOutgoingTransactionDispatcher
{
public:
    typedef nx::utils::MoveOnlyFunc<
        void(const nx::String&, std::shared_ptr<const SerializableAbstractTransaction>)
    > OnNewTransactionHandler;

    virtual void dispatchTransaction(
        const nx::String& systemId,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer) override
    {
        if (m_onNewTransactionHandler)
            m_onNewTransactionHandler(systemId, std::move(transactionSerializer));
    }

    void setOnNewTransaction(OnNewTransactionHandler onNewTransactionHandler)
    {
        m_onNewTransactionHandler = std::move(onNewTransactionHandler);
    }

private:
    OnNewTransactionHandler m_onNewTransactionHandler;
};

class TransactionLog:
    public BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog()
    {
        using namespace std::placeholders;

        m_outgoingTransactionDispatcher.setOnNewTransaction(
            std::bind(&TransactionLog::storeNewTransaction, this, _1, _2));
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

        // Initializing system's transaction log.
        const nx::String systemId = getSystem(0).id.c_str();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&ec2::TransactionLog::updateTimestampHiForSystem,
                m_transactionLog.get(), _1, systemId, getSystem(0).systemSequence));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    }

    void havingAddedBunchOfTransactionsConcurrently()
    {
        using namespace std::placeholders;

        constexpr int transactionToAddCount = 1000;

        QnWaitCondition cond;
        int transactionsToWait = 0;

        QnMutexLocker lk(&m_mutex);

        for (int i = 0; i < transactionToAddCount; ++i)
        {
            ++transactionsToWait;
            m_transactionLog->startDbTransaction(
                getSystem(0).id.c_str(),
                std::bind(&TransactionLog::shareSystemToRandomUser, this, _1),
                [this, &transactionsToWait, &cond](
                    nx::db::QueryContext*, nx::db::DBResult dbResult)
                {
                    ASSERT_EQ(nx::db::DBResult::ok, dbResult);

                    QnMutexLocker lk(&m_mutex);
                    --transactionsToWait;
                    cond.wakeAll();
                });
        }

        while (transactionsToWait > 0)
            cond.wait(lk.mutex());
    }

    void expectingTransactionsWereSentInAscendingSequenceOrder()
    {
        int prevSequence = -1;
        for (const auto& transaction: m_transactions)
        {
            ASSERT_GT(transaction->transactionHeader().persistentInfo.sequence, prevSequence);
            prevSequence = transaction->transactionHeader().persistentInfo.sequence;
        }
    }

private:
    TestOutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    std::unique_ptr<ec2::TransactionLog> m_transactionLog;
    QnMutex m_mutex;
    std::list<std::shared_ptr<const SerializableAbstractTransaction>> m_transactions;

    void initializeTransactionLog()
    {
        m_transactionLog = std::make_unique<ec2::TransactionLog>(
            QnUuid::createUuid(),
            persistentDbManager()->queryExecutor().get(),
            &m_outgoingTransactionDispatcher);
    }

    nx::db::DBResult shareSystemToRandomUser(nx::db::QueryContext* queryContext)
    {
        api::SystemSharingEx sharing;
        sharing.systemId = getSystem(0).id;
        const auto& accountToShareWith = 
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size()-1));
        sharing.accountId = accountToShareWith.id;
        sharing.accountEmail = accountToShareWith.email;
        sharing.accessRole = api::SystemAccessRole::advancedViewer;
        NX_GTEST_ASSERT_EQ(
            nx::db::DBResult::ok,
            systemSharingController().insertOrReplaceSharing(queryContext, sharing));

        ::ec2::ApiUserData userData;
        convert(sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(accountToShareWith.fullName);
        auto dbResult = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::saveUser>(
                queryContext,
                sharing.systemId.c_str(),
                std::move(userData));
        NX_GTEST_ASSERT_EQ(nx::db::DBResult::ok, dbResult);

        return nx::db::DBResult::ok;
    }

    void storeNewTransaction(
        const nx::String& /*systemId*/,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer)
    {
        QnMutexLocker lk(&m_mutex);
        m_transactions.push_back(std::move(transactionSerializer));
    }
};

TEST_F(TransactionLog, multiple_simultaneous_transactions)
{
    givenRandomSystem();
    havingAddedBunchOfTransactionsConcurrently();
    expectingTransactionsWereSentInAscendingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
