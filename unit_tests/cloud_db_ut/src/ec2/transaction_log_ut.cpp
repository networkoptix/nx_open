#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>

#include <utils/common/counter.h>
#include <utils/db/async_sql_query_executor.h>
#include <utils/db/request_execution_thread.h>

#include <ec2/dao/memory/transaction_data_object_in_memory.h>
#include <ec2/data_conversion.h>
#include <ec2/outgoing_transaction_dispatcher.h>
#include <ec2/transaction_log.h>
#include <nx_ec/ec_proto_version.h>
#include <test_support/business_data_generator.h>
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

class TransactionLog:
    public cdb::test::BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog():
        BasePersistentDataTest(DbInitializationType::delayed),
        m_peerId(QnUuid::createUuid())
    {
        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        // TODO: #ak uncomment after fixing all tests of memory::TransactionDataObject
        //ec2::dao::TransactionDataObjectFactory::setDataObjectType<
        //    ec2::dao::memory::TransactionDataObject>();

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

protected:
    const std::unique_ptr<ec2::TransactionLog>& transactionLog()
    {
        return m_transactionLog;
    }

    const QnUuid& peerId() const
    {
        return m_peerId;
    }

    TestOutgoingTransactionDispatcher& outgoingTransactionDispatcher()
    {
        return m_outgoingTransactionDispatcher;
    }

private:
    TestOutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    std::unique_ptr<ec2::TransactionLog> m_transactionLog;
    const QnUuid m_peerId;

    void initializeTransactionLog()
    {
        m_transactionLog = std::make_unique<ec2::TransactionLog>(
            m_peerId,
            persistentDbManager()->queryExecutor().get(),
            &m_outgoingTransactionDispatcher);
    }
};

class TransactionLogSameTransaction:
    public TransactionLog
{
public:
    TransactionLogSameTransaction():
        m_systemId(cdb::test::BusinessDataGenerator::generateRandomSystemId()),
        m_otherPeerId(QnUuid::createUuid()),
        m_otherPeerDbId(QnUuid::createUuid()),
        m_dbConnectionHolder(dbConnectionOptions()),
        m_otherPeerSequence(1)
    {
        init();
    }

    ~TransactionLogSameTransaction()
    {
    }

protected:
    void beginTran()
    {
        m_activeQuery = createNewTran();
    }

    void commitTran()
    {
        ASSERT_EQ(nx::db::DBResult::ok, m_activeQuery->transaction()->commit());
        m_activeQuery.reset();
    }

    void rollbackTran()
    {
        ASSERT_EQ(nx::db::DBResult::ok, m_activeQuery->transaction()->rollback());
        m_activeQuery.reset();
    }

    void havingGeneratedTransactionLocally()
    {
        auto queryContext = getQueryContext();
        auto transaction = transactionLog()->prepareLocalTransaction<::ec2::ApiCommand::saveUser>(
            queryContext.get(),
            m_systemId.c_str(),
            m_transactionData);
        if (!m_initialTransaction)
            m_initialTransaction = transaction;
        transactionLog()->saveLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            std::move(transaction));
    }

    void assertIfTransactionIsNotPresent()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        ASSERT_EQ(1U, allTransactions.size());

        boost::optional<::ec2::QnTransaction<::ec2::ApiUserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        ASSERT_TRUE(static_cast<bool>(transaction));
    }

    void havingReceivedTransactionFromOtherPeerWithGreaterTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(1);
    }

    void assertIfTransactionHasNotBeenReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_NE(*m_initialTransaction, finalTransaction);
    }

    void assertThatTransactionAuthorIsLocalPeer()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(peerId(), finalTransaction.peerID);
    }

    void assertThatTransactionAuthorIsOtherPeer()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(m_otherPeerId, finalTransaction.peerID);
    }

    void havingReceivedTransactionFromOtherPeerWithLesserTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(-1);
    }

    void assertIfTransactionHasBeenReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(*m_initialTransaction, finalTransaction);
    }

    void havingAddedTransactionLocallyWithGreaterSequence()
    {
        havingGeneratedTransactionLocally();
    }

    void havingReceivedOwnOldTransactionWithLesserSequence()
    {
        auto queryContext = getQueryContext();
        auto transaction = transactionLog()->prepareLocalTransaction<::ec2::ApiCommand::saveUser>(
            queryContext.get(),
            m_systemId.c_str(),
            m_transactionData);
        transaction.persistentInfo.sequence -= 2;
        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        const auto resultCode = transactionLog()->checkIfNeededAndSaveToLog(
            queryContext.get(),
            m_systemId.c_str(),
            cdb::ec2::SerializableTransaction<::ec2::ApiUserData>(std::move(transaction)));
        ASSERT_EQ(db::DBResult::cancelled, resultCode);
    }

private:
    const std::string m_systemId;
    const QnUuid m_otherPeerId;
    const QnUuid m_otherPeerDbId;
    int m_otherPeerSequence;
    ::ec2::ApiUserData m_transactionData;
    std::shared_ptr<nx::db::QueryContext> m_activeQuery;
    nx::db::DbConnectionHolder m_dbConnectionHolder;
    boost::optional<::ec2::QnTransaction<::ec2::ApiUserData>> m_initialTransaction;

    void init()
    {
        const auto sharing =
            cdb::test::BusinessDataGenerator::generateRandomSharing(
                cdb::test::BusinessDataGenerator::generateRandomAccount(),
                m_systemId);
        ec2::convert(sharing, &m_transactionData);
        ASSERT_TRUE(m_dbConnectionHolder.open());

        // Moving local peer sequence.
        transactionLog()->shiftLocalTransactionSequence(m_systemId.c_str(), 100);
    }

    std::shared_ptr<nx::db::QueryContext> getQueryContext()
    {
        // TODO: method is not clear
        if (m_activeQuery)
            return m_activeQuery;
        return createNewTran();
    }

    std::shared_ptr<nx::db::QueryContext> createNewTran()
    {
        auto deleter =
            [](nx::db::QueryContext* queryContext)
            {
                if (queryContext->transaction()->isActive())
                    ASSERT_EQ(nx::db::DBResult::ok, queryContext->transaction()->commit());
                delete queryContext->transaction();
                delete queryContext;
            };

        QSqlDatabase* dbConnection = m_dbConnectionHolder.dbConnection();
        nx::db::Transaction* transaction = new nx::db::Transaction(dbConnection);
        NX_GTEST_ASSERT_EQ(nx::db::DBResult::ok, transaction->begin());
        return std::shared_ptr<nx::db::QueryContext>(
            new nx::db::QueryContext(dbConnection, transaction),
            deleter);
    }

    template<typename TransactionDataType>
    boost::optional<::ec2::QnTransaction<TransactionDataType>> findTransaction(
        const std::vector<dao::TransactionLogRecord>& allTransactions,
        const TransactionDataType& transactionData)
    {
        for (const auto& logRecord : allTransactions)
        {
            const auto serializedTransactionFromLog =
                logRecord.serializer->serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION);

            ::ec2::QnTransaction<::ec2::ApiUserData> transaction(peerId());
            QnUbjsonReader<QByteArray> ubjsonStream(&serializedTransactionFromLog);
            const bool isDeserialized = QnUbjson::deserialize(&ubjsonStream, &transaction);
            NX_GTEST_ASSERT_TRUE(isDeserialized);

            if (transaction.params == transactionData)
                return transaction;
        }

        return boost::none;
    }

    std::vector<dao::TransactionLogRecord> readAllTransactions()
    {
        nx::utils::promise<std::vector<dao::TransactionLogRecord>> transactionsReadPromise;
        auto completionHandler =
            [&transactionsReadPromise](
                api::ResultCode resultCode,
                std::vector<dao::TransactionLogRecord> serializedTransactions,
                ::ec2::QnTranState /*readedUpTo*/)
            {
                ASSERT_EQ(api::ResultCode::ok, resultCode);
                transactionsReadPromise.set_value(std::move(serializedTransactions));
            };

        transactionLog()->readTransactions(
            m_systemId.c_str(),
            boost::none,
            boost::none,
            std::numeric_limits<int>::max(),
            completionHandler);

        return transactionsReadPromise.get_future().get();
    }

    void addTransactionFromOtherPeerWithTimestampDiff(int timestampDiff)
    {
        ::ec2::QnTransaction<::ec2::ApiUserData> transaction(m_otherPeerId);
        transaction.command = ::ec2::ApiCommand::saveUser;
        transaction.persistentInfo.dbID = m_otherPeerDbId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.sequence = m_otherPeerSequence++;
        transaction.persistentInfo.timestamp = 
            m_initialTransaction
            ? m_initialTransaction->persistentInfo.timestamp + timestampDiff
            : transactionLog()->generateTransactionTimestamp(m_systemId.c_str()) + timestampDiff;
        transaction.params = m_transactionData;

        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        auto queryContext = getQueryContext();
        const auto dbResult = transactionLog()->checkIfNeededAndSaveToLog(
            queryContext.get(),
            m_systemId.c_str(),
            cdb::ec2::UbjsonSerializedTransaction<::ec2::ApiUserData>(std::move(transaction)));
        ASSERT_TRUE(dbResult == nx::db::DBResult::ok || dbResult == nx::db::DBResult::cancelled);
    }

    ::ec2::QnTransaction<::ec2::ApiUserData> getTransactionFromLog()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        NX_GTEST_ASSERT_EQ(1U, allTransactions.size());

        boost::optional<::ec2::QnTransaction<::ec2::ApiUserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        NX_GTEST_ASSERT_TRUE(static_cast<bool>(transaction));

        return *transaction;
    }

    //void assertThatTransactionPresentAndBelongsTo(const QnUuid& peerId)
    //{
    //    const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
    //    ASSERT_EQ(1U, allTransactions.size());

    //    boost::optional<::ec2::QnTransaction<::ec2::ApiUserData>> transaction =
    //        findTransaction(allTransactions, m_transactionData);
    //    ASSERT_TRUE(static_cast<bool>(transaction));

    //    ASSERT_EQ(peerId, transaction->peerID);
    //}
};

TEST_F(TransactionLogSameTransaction, newly_generated_transaction_is_there)
{
    havingGeneratedTransactionLocally();
    assertIfTransactionIsNotPresent();
}

TEST_F(TransactionLogSameTransaction, transaction_from_remote_peer_has_been_added)
{
    havingReceivedTransactionFromOtherPeerWithGreaterTimestamp();
    assertIfTransactionIsNotPresent();
}

TEST_F(
    TransactionLogSameTransaction,
    transaction_from_other_peer_with_greater_timestamp_replaces_existing_one)
{
    havingGeneratedTransactionLocally();
    havingReceivedTransactionFromOtherPeerWithGreaterTimestamp();
    assertIfTransactionHasNotBeenReplaced();
    assertThatTransactionAuthorIsOtherPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_lesser_timestamp_is_ignored)
{
    havingGeneratedTransactionLocally();
    havingReceivedTransactionFromOtherPeerWithLesserTimestamp();
    assertIfTransactionHasBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_greater_sequence_replaces_existing)
{
    havingGeneratedTransactionLocally();
    havingAddedTransactionLocallyWithGreaterSequence();
    assertIfTransactionHasNotBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_lesser_sequence_is_ignored)
{
    havingGeneratedTransactionLocally();
    havingReceivedOwnOldTransactionWithLesserSequence();
    assertIfTransactionHasBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, /*DISABLED_*/tran_rollback_clears_raw_data)
{
    havingGeneratedTransactionLocally();
    beginTran();
    havingAddedTransactionLocallyWithGreaterSequence();
    rollbackTran();
    assertIfTransactionHasBeenReplaced();
}

//-------------------------------------------------------------------------------------------------

class TestTransactionController
{
public:
    enum class State
    {
        init,
        startedTransaction,
        modifiedBusinessData,
        generatedTransaction,
        done,
    };

    TestTransactionController(
        const std::unique_ptr<ec2::TransactionLog>& transactionLog,
        const api::SystemData& system,
        const api::AccountData& accountToShareWith)
        :
        m_transactionLog(transactionLog),
        m_system(system),
        m_accountToShareWith(accountToShareWith),
        m_completedState(State::init),
        m_desiredState(State::init)
    {
    }

    ~TestTransactionController()
    {
        m_startedAsyncCallsCounter.wait();
    }

    void startDbTransaction()
    {
        using namespace std::placeholders;

        m_completedState = State::startedTransaction;
        m_desiredState = State::startedTransaction;
        m_transactionLog->startDbTransaction(
            m_system.id.c_str(),
            std::bind(&TestTransactionController::doSomeDataModifications, this, _1),
            [this, locker = m_startedAsyncCallsCounter.getScopedIncrement()](
                nx::db::QueryContext* queryContext,
                nx::db::DBResult dbResult)
            {
                onDbTranCompleted(queryContext, dbResult);
            });
    }

    void generateTransaction()
    {
        proceedToState(State::generatedTransaction);
    }

    void commit()
    {
        proceedToState(State::done);
    }

private:
    const std::unique_ptr<ec2::TransactionLog>& m_transactionLog;
    const api::SystemData m_system;
    const api::AccountData m_accountToShareWith;
    State m_completedState;
    State m_desiredState;
    api::SystemSharingEx m_sharing;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    QnCounter m_startedAsyncCallsCounter;

    nx::db::DBResult doSomeDataModifications(nx::db::QueryContext* queryContext)
    {
        prepareData();

        for (;;)
        {
            {
                QnMutexLocker lock(&m_mutex);
                while (m_completedState == m_desiredState)
                    m_cond.wait(lock.mutex());

                NX_ASSERT(m_completedState < m_desiredState);
            }

            State thingToDo = static_cast<State>(static_cast<int>(m_completedState) + 1);

            switch (thingToDo)
            {
                case State::modifiedBusinessData:
                    modifyBusinessData();
                    setCompletedState(State::modifiedBusinessData);
                    break;

                case State::generatedTransaction:
                    addTransactionToLog(queryContext);
                    setCompletedState(State::generatedTransaction);
                    break;

                case State::done:
                    setCompletedState(State::done);
                    return nx::db::DBResult::ok;
            }
        }

        return nx::db::DBResult::ok;
    }

    void setCompletedState(State completedState)
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_completedState = completedState;
        }
        m_cond.wakeAll();
    }

    void prepareData()
    {
        m_sharing.systemId = m_system.id;
        m_sharing.accountId = m_accountToShareWith.id;
        m_sharing.accountEmail = m_accountToShareWith.email;
        m_sharing.accessRole = api::SystemAccessRole::advancedViewer;
    }

    void modifyBusinessData()
    {
        // It does not matter for transaction log whether we save application data or not.
        // TODO #ak But, it is still better to mockup data access object here.
        //NX_GTEST_ASSERT_EQ(
        //    nx::db::DBResult::ok,
        //    systemSharingController().insertOrReplaceSharing(queryContext, sharing));
    }

    void addTransactionToLog(nx::db::QueryContext* queryContext)
    {
        ::ec2::ApiUserData userData;
        convert(m_sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(m_accountToShareWith.fullName);
        auto dbResult = m_transactionLog->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::saveUser>(
                queryContext,
                m_sharing.systemId.c_str(),
                std::move(userData));
        ASSERT_EQ(nx::db::DBResult::ok, dbResult);
    }

    void proceedToState(State targetState)
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_desiredState = targetState;
        }
        m_cond.wakeAll();

        waitForReachingDesiredState();
    }

    void waitForReachingDesiredState()
    {
        QnMutexLocker lock(&m_mutex);
        while (m_completedState < m_desiredState)
            m_cond.wait(lock.mutex());
    }

    void onDbTranCompleted(
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult /*dbResult*/)
    {
    }

    TestTransactionController(const TestTransactionController&) = delete;
    TestTransactionController& operator=(const TestTransactionController&) = delete;
};

class TransactionLogOverlappingTransactions:
    public TransactionLog
{
public:
    TransactionLogOverlappingTransactions()
    {
        using namespace std::placeholders;

        outgoingTransactionDispatcher().setOnNewTransaction(
            std::bind(&TransactionLogOverlappingTransactions::storeNewTransaction, this, _1, _2));
    }

protected:
    void havingAddedOverlappingTransactions()
    {
        constexpr std::size_t transactionCount = 5;

        persistentDbManager()->queryExecutor()->reserveConnections(transactionCount);

        auto dbTransactions = startDbTransactions(transactionCount);
        for (std::size_t i = 0; i < dbTransactions.size(); ++i)
            dbTransactions[i]->generateTransaction();

        for (std::size_t i = dbTransactions.size(); i > 0; --i)
            dbTransactions[i-1]->commit();
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
            transactionLog()->startDbTransaction(
                getSystem(0).id.c_str(),
                std::bind(&TransactionLogOverlappingTransactions::shareSystemToRandomUser, this, _1),
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

    std::vector<std::unique_ptr<TestTransactionController>>
        startDbTransactions(std::size_t tranCount)
    {
        std::vector<std::unique_ptr<TestTransactionController>> transactions;
        transactions.resize(tranCount);
        for (std::size_t i = 0; i < transactions.size(); ++i)
            transactions[i] = startDbTransaction();
        return transactions;
    }

    std::unique_ptr<TestTransactionController> startDbTransaction()
    {
        const auto& accountToShareWith =
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size() - 1));

        auto tranController = std::make_unique<TestTransactionController>(
            transactionLog(),
            getSystem(0),
            accountToShareWith);
        tranController->startDbTransaction();
        return tranController;
    }

    void assertIfTransactionsWereNotSentInAscendingSequenceOrder()
    {
        int prevSequence = -1;
        for (const auto& transaction: m_outgoingTransactions)
        {
            ASSERT_GT(transaction->transactionHeader().persistentInfo.sequence, prevSequence);
            prevSequence = transaction->transactionHeader().persistentInfo.sequence;
        }
    }

private:
    QnMutex m_mutex;
    std::list<std::shared_ptr<const SerializableAbstractTransaction>> m_outgoingTransactions;

    void storeNewTransaction(
        const nx::String& /*systemId*/,
        std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer)
    {
        QnMutexLocker lk(&m_mutex);
        m_outgoingTransactions.push_back(std::move(transactionSerializer));
    }

    nx::db::DBResult shareSystemToRandomUser(nx::db::QueryContext* queryContext)
    {
        api::SystemSharingEx sharing;
        sharing.systemId = getSystem(0).id;
        const auto& accountToShareWith =
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size() - 1));
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
        auto dbResult = transactionLog()->generateTransactionAndSaveToLog<
            ::ec2::ApiCommand::saveUser>(
                queryContext,
                sharing.systemId.c_str(),
                std::move(userData));
        NX_GTEST_ASSERT_EQ(nx::db::DBResult::ok, dbResult);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(nx::utils::random::number<int>(0, 1000)));

        return nx::db::DBResult::ok;
    }
};

TEST_F(TransactionLogOverlappingTransactions, DISABLED_overlapping_transactions_sent_in_a_correct_order)
{
    givenRandomSystem();
    havingAddedOverlappingTransactions();
    assertIfTransactionsWereNotSentInAscendingSequenceOrder();
}

//TEST_F(TransactionLogOverlappingTransactions, overlapping_transactions_to_multiple_systems)

//-------------------------------------------------------------------------------------------------

class FtTransactionLogOverlappingTransactions:
    public TransactionLogOverlappingTransactions
{
};

TEST_F(FtTransactionLogOverlappingTransactions, DISABLED_multiple_simultaneous_transactions)
{
    givenRandomSystem();
    havingAddedBunchOfTransactionsConcurrently();
    assertIfTransactionsWereNotSentInAscendingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
