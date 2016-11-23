#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>

#include <utils/db/async_sql_query_executor.h>

#include <dao/rdb/db_instance_controller.h>
#include <dao/rdb/account_data_object.h>
#include <dao/rdb/system_data_object.h>
#include <dao/rdb/system_sharing_data_object.h>
#include <ec2/data_conversion.h>
#include <ec2/outgoing_transaction_dispatcher.h>
#include <ec2/transaction_log.h>
#include <test_support/business_data_generator.h>
#include <test_support/test_with_db_helper.h>

#include "data/account_data.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

using DbInstanceController = cdb::dao::rdb::DbInstanceController;
using AccountDataObject = cdb::dao::rdb::AccountDataObject;
using SystemDataObject = cdb::dao::rdb::SystemDataObject;
using SystemSharingDataObject = cdb::dao::rdb::SystemSharingDataObject;

class BasePersistentDataTest:
    public TestWithDbHelper
{
public:
    enum class DbInitializationType
    {
        immediate,
        delayed
    };

    BasePersistentDataTest(DbInitializationType dbInitializationType)
    {
        if (dbInitializationType == DbInitializationType::immediate)
            initializeDatabase();
    }

protected:
    const std::unique_ptr<DbInstanceController>& persistentDbManager() const
    {
        return m_persistentDbManager;
    }

    void insertRandomAccount()
    {
        using namespace std::placeholders;

        auto account = cdb::test::BusinessDataGenerator::generateRandomAccount();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&AccountDataObject::insert, &m_accountDbController,
                _1, account));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_accounts.push_back(std::move(account));
    }

    void insertRandomSystem(const api::AccountData& account)
    {
        using namespace std::placeholders;

        auto system = cdb::test::BusinessDataGenerator::generateRandomSystem(account);
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&SystemDataObject::insert, &m_systemDbController,
                _1, system, account.id));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
        m_systems.push_back(std::move(system));
    }

    void insertSystemSharing(const api::SystemSharingEx& sharing)
    {
        using namespace std::placeholders;

        const auto dbResult = executeUpdateQuerySync(
            std::bind(&SystemSharingDataObject::insertOrReplaceSharing,
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

    SystemSharingDataObject& systemSharingController()
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

    void initializeDatabase()
    {
        m_persistentDbManager =
            std::make_unique<DbInstanceController>(dbConnectionOptions());
        ASSERT_TRUE(m_persistentDbManager->initialize());
    }

private:
    std::unique_ptr<DbInstanceController> m_persistentDbManager;
    AccountDataObject m_accountDbController;
    SystemDataObject m_systemDbController;
    SystemSharingDataObject m_systemSharingController;
    std::vector<api::AccountData> m_accounts;
    std::vector<data::SystemData> m_systems;
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

class TransactionDataObject:
    public ec2::dao::AbstractTransactionDataObject
{
public:
    virtual nx::db::DBResult insertOrReplaceTransaction(
        nx::db::QueryContext* /*queryContext*/,
        const dao::TransactionData& /*transactionData*/) override
    {
        return nx::db::DBResult::ok;
    }
};

class TransactionLog:
    public BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog():
        BasePersistentDataTest(DbInitializationType::delayed)
    {
        using namespace std::placeholders;

        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        ec2::dao::AbstractTransactionDataObject::setDataObjectType<TransactionDataObject>();

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

        constexpr int transactionToAddCount = 100;

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
                    if (dbResult != nx::db::DBResult::cancelled)
                    {
                        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
                    }

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
    QnMutex m_queryMutex;
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
        std::cout<<std::this_thread::get_id()<<std::endl;

        api::SystemSharingEx sharing;
        sharing.systemId = getSystem(0).id;
        const auto& accountToShareWith = 
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size()-1));
        sharing.accountId = accountToShareWith.id;
        sharing.accountEmail = accountToShareWith.email;
        sharing.accessRole = api::SystemAccessRole::advancedViewer;

        // It does not matter for transaction log whether we save application data or not.
        // TODO #ak But, it is still better to mockup data access object here.
        //NX_GTEST_ASSERT_EQ(
        //    nx::db::DBResult::ok,
        //    systemSharingController().insertOrReplaceSharing(queryContext, sharing));

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

        std::this_thread::sleep_for(
            std::chrono::milliseconds(nx::utils::random::number<int>(0, 1000)));

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
