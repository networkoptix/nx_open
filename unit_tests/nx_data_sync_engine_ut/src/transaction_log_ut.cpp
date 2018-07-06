#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/counter.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/request_execution_thread.h>
#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx_ec/ec_proto_version.h>

#include <nx/data_sync_engine/dao/memory/transaction_data_object_in_memory.h>
#include <nx/data_sync_engine/outgoing_transaction_dispatcher.h>
#include <nx/data_sync_engine/transaction_log.h>

#include <nx/cloud/cdb/data/account_data.h>
#include <nx/cloud/cdb/data/system_data.h>
#include <nx/cloud/cdb/ec2/data_conversion.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>
#include <nx/cloud/cdb/test_support/base_persistent_data_test.h>

#include "test_outgoing_transaction_dispatcher.h"

namespace nx {
namespace data_sync_engine {
namespace test {

class TransactionLog:
    public cdb::test::BasePersistentDataTest,
    public ::testing::Test
{
public:
    TransactionLog(dao::DataObjectType dataObjectType):
        BasePersistentDataTest(DbInitializationType::delayed),
        m_peerId(QnUuid::createUuid())
    {
        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        data_sync_engine::dao::TransactionDataObjectFactory::setDataObjectType(dataObjectType);

        initializeTransactionLog();
    }

    ~TransactionLog()
    {
        data_sync_engine::dao::TransactionDataObjectFactory::resetToDefaultFactory();
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
            std::bind(&data_sync_engine::TransactionLog::updateTimestampHiForSystem,
                m_transactionLog.get(), _1, systemId, getSystem(0).systemSequence));
        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
    }

protected:
    const std::unique_ptr<data_sync_engine::TransactionLog>& transactionLog()
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

    void reinitialiseTransactionLog()
    {
        m_transactionLog.reset();
        initializeTransactionLog();
    }

private:
    TestOutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    std::unique_ptr<data_sync_engine::TransactionLog> m_transactionLog;
    const QnUuid m_peerId;

    void initializeTransactionLog()
    {
        m_transactionLog = std::make_unique<data_sync_engine::TransactionLog>(
            m_peerId,
            &persistentDbManager()->queryExecutor(),
            &m_outgoingTransactionDispatcher);
    }
};

class TransactionLogSameTransaction:
    public TransactionLog
{
public:
    TransactionLogSameTransaction():
        TransactionLog(dao::DataObjectType::rdbms),
        m_systemId(cdb::test::BusinessDataGenerator::generateRandomSystemId()),
        m_otherPeerId(QnUuid::createUuid()),
        m_otherPeerDbId(QnUuid::createUuid()),
        m_otherPeerSequence(1),
        m_dbConnectionHolder(dbConnectionOptions())
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
        ASSERT_EQ(nx::sql::DBResult::ok, m_activeQuery->transaction()->commit());
        m_activeQuery.reset();
    }

    void rollbackTran()
    {
        ASSERT_EQ(nx::sql::DBResult::ok, m_activeQuery->transaction()->rollback());
        m_activeQuery.reset();
    }

    void addTransactionsWithIncreasingTimestampSequence()
    {
        for (int i = 0; i < 3; ++i)
        {
            auto tran = prepareFromOtherPeerWithTimestampDiff(1);
            tran.persistentInfo.timestamp.sequence =
                std::max<std::uint64_t>(m_lastUsedSequence + 1, tran.persistentInfo.timestamp.sequence + 1);
            m_lastUsedSequence = tran.persistentInfo.timestamp.sequence;
            saveTransaction(tran);
        }
    }

    void whenGeneratedTransactionLocally()
    {
        auto queryContext = getQueryContext();
        auto transaction = transactionLog()->prepareLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
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

        boost::optional<Command<::ec2::ApiUserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        ASSERT_TRUE(static_cast<bool>(transaction));
    }

    void whenReceivedTransactionFromOtherPeerWithGreaterTimestamp()
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

    void whenReceivedTransactionFromOtherPeerWithLesserTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(-1);
    }

    void assertIfTransactionHasBeenReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(*m_initialTransaction, finalTransaction);
    }

    void whenAddedTransactionLocallyWithGreaterSequence()
    {
        whenGeneratedTransactionLocally();
    }

    void whenReceivedOwnOldTransactionWithLesserSequence()
    {
        auto queryContext = getQueryContext();
        auto transaction = transactionLog()->prepareLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
            m_transactionData);
        transaction.persistentInfo.sequence -= 2;
        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        const auto resultCode = transactionLog()->checkIfNeededAndSaveToLog(
            queryContext.get(),
            m_systemId.c_str(),
            data_sync_engine::SerializableTransaction<::ec2::ApiUserData>(std::move(transaction)));
        ASSERT_EQ(nx::sql::DBResult::cancelled, resultCode);
    }

    void assertMaxTimestampSequenceIsUsed()
    {
        const auto timestamp = transactionLog()->generateTransactionTimestamp(m_systemId.c_str());
        ASSERT_EQ(m_lastUsedSequence, timestamp.sequence);
    }

private:
    const std::string m_systemId;
    const QnUuid m_otherPeerId;
    const QnUuid m_otherPeerDbId;
    int m_otherPeerSequence;
    ::ec2::ApiUserData m_transactionData;
    std::shared_ptr<nx::sql::QueryContext> m_activeQuery;
    nx::sql::DbConnectionHolder m_dbConnectionHolder;
    boost::optional<Command<::ec2::ApiUserData>> m_initialTransaction;
    std::uint64_t m_lastUsedSequence = 0;

    void init()
    {
        const auto sharing =
            cdb::test::BusinessDataGenerator::generateRandomSharing(
                cdb::test::BusinessDataGenerator::generateRandomAccount(),
                m_systemId);
        cdb::ec2::convert(sharing, &m_transactionData);
        ASSERT_TRUE(m_dbConnectionHolder.open());

        // Moving local peer sequence.
        transactionLog()->shiftLocalTransactionSequence(m_systemId.c_str(), 100);
    }

    std::shared_ptr<nx::sql::QueryContext> getQueryContext()
    {
        // TODO: method is not clear
        if (m_activeQuery)
            return m_activeQuery;
        return createNewTran();
    }

    std::shared_ptr<nx::sql::QueryContext> createNewTran()
    {
        auto deleter =
            [](nx::sql::QueryContext* queryContext)
            {
                if (queryContext->transaction()->isActive())
                {
                    ASSERT_EQ(nx::sql::DBResult::ok, queryContext->transaction()->commit());
                }
                delete queryContext->transaction();
                delete queryContext;
            };

        QSqlDatabase* dbConnection = m_dbConnectionHolder.dbConnection();
        nx::sql::Transaction* transaction = new nx::sql::Transaction(dbConnection);
        NX_GTEST_ASSERT_EQ(nx::sql::DBResult::ok, transaction->begin());
        return std::shared_ptr<nx::sql::QueryContext>(
            new nx::sql::QueryContext(dbConnection, transaction),
            deleter);
    }

    template<typename TransactionDataType>
    boost::optional<Command<TransactionDataType>> findTransaction(
        const std::vector<dao::TransactionLogRecord>& allTransactions,
        const TransactionDataType& transactionData)
    {
        for (const auto& logRecord : allTransactions)
        {
            const auto serializedTransactionFromLog =
                logRecord.serializer->serialize(Qn::UbjsonFormat, nx_ec::EC2_PROTO_VERSION);

            Command<::ec2::ApiUserData> transaction(peerId());
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
                ResultCode resultCode,
                std::vector<dao::TransactionLogRecord> serializedTransactions,
                ::ec2::QnTranState /*readedUpTo*/)
            {
                ASSERT_EQ(ResultCode::ok, resultCode);
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
        saveTransaction(
            prepareFromOtherPeerWithTimestampDiff(timestampDiff));
    }

    Command<::ec2::ApiUserData> prepareFromOtherPeerWithTimestampDiff(
        int timestampDiff)
    {
        Command<::ec2::ApiUserData> transaction(m_otherPeerId);
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

        return transaction;
    }

    void saveTransaction(Command<::ec2::ApiUserData> transaction)
    {
        auto queryContext = getQueryContext();
        const auto dbResult = transactionLog()->checkIfNeededAndSaveToLog(
            queryContext.get(),
            m_systemId.c_str(),
            data_sync_engine::UbjsonSerializedTransaction<::ec2::ApiUserData>(std::move(transaction)));
        ASSERT_TRUE(dbResult == nx::sql::DBResult::ok || dbResult == nx::sql::DBResult::cancelled)
            << "Got " << toString(dbResult);
    }

    Command<::ec2::ApiUserData> getTransactionFromLog()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        NX_GTEST_ASSERT_EQ(1U, allTransactions.size());

        boost::optional<Command<::ec2::ApiUserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        NX_GTEST_ASSERT_TRUE(static_cast<bool>(transaction));

        return *transaction;
    }

    //void assertThatTransactionPresentAndBelongsTo(const QnUuid& peerId)
    //{
    //    const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
    //    ASSERT_EQ(1U, allTransactions.size());

    //    boost::optional<Command<::ec2::ApiUserData>> transaction =
    //        findTransaction(allTransactions, m_transactionData);
    //    ASSERT_TRUE(static_cast<bool>(transaction));

    //    ASSERT_EQ(peerId, transaction->peerID);
    //}
};

TEST_F(TransactionLogSameTransaction, newly_generated_transaction_is_there)
{
    whenGeneratedTransactionLocally();
    assertIfTransactionIsNotPresent();
}

TEST_F(TransactionLogSameTransaction, transaction_from_remote_peer_has_been_added)
{
    whenReceivedTransactionFromOtherPeerWithGreaterTimestamp();
    assertIfTransactionIsNotPresent();
}

TEST_F(
    TransactionLogSameTransaction,
    transaction_from_other_peer_with_greater_timestamp_replaces_existing_one)
{
    whenGeneratedTransactionLocally();
    whenReceivedTransactionFromOtherPeerWithGreaterTimestamp();
    assertIfTransactionHasNotBeenReplaced();
    assertThatTransactionAuthorIsOtherPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_lesser_timestamp_is_ignored)
{
    whenGeneratedTransactionLocally();
    whenReceivedTransactionFromOtherPeerWithLesserTimestamp();
    assertIfTransactionHasBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_greater_sequence_replaces_existing)
{
    whenGeneratedTransactionLocally();
    whenAddedTransactionLocallyWithGreaterSequence();
    assertIfTransactionHasNotBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, transaction_with_lesser_sequence_is_ignored)
{
    whenGeneratedTransactionLocally();
    whenReceivedOwnOldTransactionWithLesserSequence();
    assertIfTransactionHasBeenReplaced();
    assertThatTransactionAuthorIsLocalPeer();
}

TEST_F(TransactionLogSameTransaction, tran_rollback_clears_raw_data)
{
    whenGeneratedTransactionLocally();
    beginTran();
    whenAddedTransactionLocallyWithGreaterSequence();
    rollbackTran();
    assertIfTransactionHasBeenReplaced();
}

TEST_F(TransactionLogSameTransaction, max_timestamp_sequence_is_restored_after_restart)
{
    addTransactionsWithIncreasingTimestampSequence();
    reinitialiseTransactionLog();
    assertMaxTimestampSequenceIsUsed();
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
        const std::unique_ptr<data_sync_engine::TransactionLog>& transactionLog,
        const cdb::api::SystemData& system,
        const cdb::api::AccountData& accountToShareWith)
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
                nx::sql::DBResult /*dbResult*/)
            {
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
    const std::unique_ptr<data_sync_engine::TransactionLog>& m_transactionLog;
    const cdb::api::SystemData m_system;
    const cdb::api::AccountData m_accountToShareWith;
    State m_completedState;
    State m_desiredState;
    cdb::api::SystemSharingEx m_sharing;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    nx::utils::Counter m_startedAsyncCallsCounter;

    nx::sql::DBResult doSomeDataModifications(nx::sql::QueryContext* queryContext)
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
                    return nx::sql::DBResult::ok;

                default:
                    NX_GTEST_ASSERT_TRUE(false);
                    break;
            }
        }

        return nx::sql::DBResult::ok;
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
        m_sharing.accessRole = cdb::api::SystemAccessRole::advancedViewer;
        m_sharing.vmsUserId =
            guidFromArbitraryData(m_sharing.accountEmail).toSimpleString().toStdString();
    }

    void modifyBusinessData()
    {
        // It does not matter for transaction log whether we save application data or not.
        // TODO #ak But, it is still better to mockup data access object here.
        //NX_GTEST_ASSERT_EQ(
        //    nx::sql::DBResult::ok,
        //    systemSharingController().insertOrReplaceSharing(queryContext, sharing));
    }

    void addTransactionToLog(nx::sql::QueryContext* queryContext)
    {
        ::ec2::ApiUserData userData;
        cdb::ec2::convert(m_sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(m_accountToShareWith.fullName);
        auto dbResult = m_transactionLog->generateTransactionAndSaveToLog(
            queryContext,
            m_sharing.systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
            std::move(userData));
        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
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

    TestTransactionController(const TestTransactionController&) = delete;
    TestTransactionController& operator=(const TestTransactionController&) = delete;
};

class TransactionLogOverlappingTransactions:
    public TransactionLog
{
public:
    TransactionLogOverlappingTransactions():
        TransactionLog(dao::DataObjectType::ram)
    {
        persistentDbManager()->queryExecutor().setConcurrentModificationQueryLimit(0 /*no limit*/);
    }

protected:
    void whenAddedOverlappingTransactions()
    {
        constexpr std::size_t transactionCount = 5;

        persistentDbManager()->queryExecutor().reserveConnections(transactionCount);

        auto dbTransactions = startDbTransactions(transactionCount);
        for (std::size_t i = 0; i < dbTransactions.size(); ++i)
            dbTransactions[i]->generateTransaction();

        for (std::size_t i = dbTransactions.size(); i > 0; --i)
            dbTransactions[i-1]->commit();
    }

    void whenAddedBunchOfTransactionsConcurrently()
    {
        using namespace std::placeholders;

        const int transactionToAddCount = dbConnectionOptions().maxConnectionCount;
        ASSERT_GT(transactionToAddCount, 1);
        m_transactionToExecuteThreshold = 7;

        m_transactionOrder.resize(transactionToAddCount);
        for (int i = 0; i < transactionToAddCount; ++i)
            m_transactionOrder[i] = i;

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_transactionOrder.begin(), m_transactionOrder.end(), g);

        QnWaitCondition cond;
        int transactionsToWait = 0;

        QnMutexLocker lk(&m_mutex);

        for (int i = 0; i < transactionToAddCount; ++i)
        {
            ++transactionsToWait;
            transactionLog()->startDbTransaction(
                getSystem(0).id.c_str(),
                std::bind(&TransactionLogOverlappingTransactions::shareSystemToRandomUser, this, _1, i),
                [this, &transactionsToWait, &cond](
                    nx::sql::DBResult dbResult)
                {
                    if (dbResult != nx::sql::DBResult::cancelled)
                    {
                        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
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
        outgoingTransactionDispatcher().assertIfTransactionsWereNotSentInAscendingSequenceOrder();
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::vector<int> m_transactionOrder;
    int m_transactionToExecuteThreshold = 0;

    nx::sql::DBResult shareSystemToRandomUser(
        nx::sql::QueryContext* queryContext,
        int originalTransactionIndex)
    {
        cdb::api::SystemSharingEx sharing;
        sharing.systemId = getSystem(0).id;
        const auto& accountToShareWith =
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size() - 1));
        sharing.accountId = accountToShareWith.id;
        sharing.accountEmail = accountToShareWith.email;
        sharing.accessRole = cdb::api::SystemAccessRole::advancedViewer;

        // It does not matter for transaction log whether we save application data or not.
        // TODO #ak But, it is still better to mockup data access object here.
        //NX_GTEST_ASSERT_EQ(
        //    nx::sql::DBResult::ok,
        //    systemSharingController().insertOrReplaceSharing(queryContext, sharing));

        ::ec2::ApiUserData userData;
        cdb::ec2::convert(sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(accountToShareWith.fullName);
        auto dbResult = transactionLog()->generateTransactionAndSaveToLog(
            queryContext,
            sharing.systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
            std::move(userData));
        NX_GTEST_ASSERT_EQ(nx::sql::DBResult::ok, dbResult);

        // Making sure transactions are completed in the order
        // different from the one they were added.
        yieldToOtherTransactions(originalTransactionIndex);

        return nx::sql::DBResult::ok;
    }

    void yieldToOtherTransactions(int originalTransactionIndex)
    {
        using namespace std::chrono;

        constexpr milliseconds kMaxDelay = milliseconds(50);

        QnMutexLocker lock(&m_mutex);

        const auto t0 = std::chrono::steady_clock::now();
        while (m_transactionOrder[originalTransactionIndex] > m_transactionToExecuteThreshold)
        {
            const auto timePassed = duration_cast<milliseconds>(steady_clock::now() - t0);
            if (timePassed >= kMaxDelay)
                break;
            m_cond.wait(lock.mutex(), (kMaxDelay - timePassed).count());
        }

        ++m_transactionToExecuteThreshold;
        m_cond.wakeAll();
    }
};

TEST_F(TransactionLogOverlappingTransactions, overlapping_transactions_sent_in_a_correct_order)
{
    givenRandomSystem();
    whenAddedOverlappingTransactions();
    assertIfTransactionsWereNotSentInAscendingSequenceOrder();
}

//TEST_F(TransactionLogOverlappingTransactions, overlapping_transactions_to_multiple_systems)

//-------------------------------------------------------------------------------------------------

class FtTransactionLogOverlappingTransactions:
    public TransactionLogOverlappingTransactions
{
};

TEST_F(FtTransactionLogOverlappingTransactions, multiple_simultaneous_transactions)
{
    givenRandomSystem();
    whenAddedBunchOfTransactionsConcurrently();
    assertIfTransactionsWereNotSentInAscendingSequenceOrder();
}

} // namespace test
} // namespace data_sync_engine
} // namespace nx
