#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>

#include <utils/db/async_sql_query_executor.h>

#include <ec2/data_conversion.h>
#include <ec2/outgoing_transaction_dispatcher.h>
#include <ec2/transaction_log.h>
#include <test_support/test_with_db_helper.h>

#include "base_persistent_data_test.h"
#include "data/account_data.h"
#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

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

    virtual nx::db::DBResult updateTimestampHiForSystem(
        nx::db::QueryContext* /*queryContext*/,
        const nx::String& /*systemId*/,
        quint64 /*newValue*/) override
    {
        return nx::db::DBResult::ok;
    }

    virtual nx::db::DBResult fetchTransactionsOfAPeerQuery(
        nx::db::QueryContext* /*queryContext*/,
        const nx::String& /*systemId*/,
        const QString& /*peerId*/,
        const QString& /*dbInstanceId*/,
        std::int64_t /*minSequence*/,
        std::int64_t /*maxSequence*/,
        std::vector<dao::TransactionLogRecord>* const /*transactions*/) override
    {
        return nx::db::DBResult::ok;
    }
};

class TransactionLog:
    public cdb::test::BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog():
        BasePersistentDataTest(DbInitializationType::delayed)
    {
        using namespace std::placeholders;

        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        ec2::dao::TransactionDataObjectFactory::setDataObjectType<TransactionDataObject>();

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

TEST_F(TransactionLog, DISABLED_multiple_simultaneous_transactions)
{
    givenRandomSystem();
    havingAddedBunchOfTransactionsConcurrently();
    expectingTransactionsWereSentInAscendingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
