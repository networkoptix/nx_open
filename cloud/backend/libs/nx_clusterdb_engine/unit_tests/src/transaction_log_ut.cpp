#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/counter.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/detail/query_execution_thread.h>
#include <nx/sql/test_support/test_with_db_helper.h>

#include <nx_ec/ec_proto_version.h>

#include <nx/clusterdb/engine/dao/memory/transaction_data_object_in_memory.h>
#include <nx/clusterdb/engine/outgoing_transaction_dispatcher.h>
#include <nx/clusterdb/engine/transaction_log.h>

#include <nx/cloud/db/controller.h>
#include <nx/cloud/db/data/account_data.h>
#include <nx/cloud/db/data/system_data.h>
#include <nx/cloud/db/ec2/data_conversion.h>
#include <nx/cloud/db/ec2/vms_command_descriptor.h>
#include <nx/cloud/db/test_support/business_data_generator.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>

#include "test_outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine::test {

class CommandLog:
    public nx::cloud::db::test::BasePersistentDataTest,
    public ::testing::Test
{
public:
    CommandLog(dao::DataObjectType dataObjectType):
        BasePersistentDataTest(DbInitializationType::delayed),
        m_protocolVersionRange(
            nx::cloud::db::kMinSupportedProtocolVersion,
            nx::cloud::db::kMaxSupportedProtocolVersion),
        m_peerId(QnUuid::createUuid())
    {
        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        m_factoryFuncBak =
             clusterdb::engine::dao::TransactionDataObjectFactory::instance()
                .setDataObjectType(dataObjectType);

        initializeTransactionLog();
    }

    ~CommandLog()
    {
        clusterdb::engine::dao::TransactionDataObjectFactory::instance()
            .setCustomFunc(std::move(m_factoryFuncBak));
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
        const std::string systemId = getSystem(0).id.c_str();
        const auto dbResult = executeUpdateQuerySync(
            std::bind(&clusterdb::engine::CommandLog::updateTimestampHiForSystem,
                m_commandLog.get(), _1, systemId, getSystem(0).systemSequence));
        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
    }

protected:
    const std::unique_ptr<clusterdb::engine::CommandLog>& commandLog()
    {
        return m_commandLog;
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
        m_commandLog.reset();
        initializeTransactionLog();
    }

    const ProtocolVersionRange& protocolVersionRange() const
    {
        return m_protocolVersionRange;
    }

private:
    TestOutgoingTransactionDispatcher m_outgoingTransactionDispatcher;
    ProtocolVersionRange m_protocolVersionRange;
    std::unique_ptr<clusterdb::engine::CommandLog> m_commandLog;
    const QnUuid m_peerId;
    clusterdb::engine::dao::TransactionDataObjectFactory::Function m_factoryFuncBak;

    void initializeTransactionLog()
    {
        m_commandLog = std::make_unique<clusterdb::engine::CommandLog>(
            m_peerId,
            m_protocolVersionRange,
            &persistentDbManager()->queryExecutor(),
            &m_outgoingTransactionDispatcher);
    }
};

class CommandLogSameTransaction:
    public CommandLog
{
public:
    CommandLogSameTransaction():
        CommandLog(dao::DataObjectType::rdbms),
        m_systemId(nx::cloud::db::test::BusinessDataGenerator::generateRandomSystemId()),
        m_otherPeerId(QnUuid::createUuid()),
        m_otherPeerDbId(QnUuid::createUuid()),
        m_otherPeerSequence(1),
        m_dbConnectionHolder(dbConnectionOptions())
    {
        init();
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
            tran.persistentInfo.timestamp.sequence = std::max<std::uint64_t>(
                m_lastUsedSequence + 1,
                tran.persistentInfo.timestamp.sequence + 1);
            m_lastUsedSequence = tran.persistentInfo.timestamp.sequence;
            saveTransaction(tran);
        }
    }

    void whenGenerateTransactionLocally()
    {
        auto queryContext = getQueryContext();
        auto transaction = commandLog()->prepareLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
            m_transactionData);
        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        const auto transactionHash =
            nx::cloud::db::ec2::command::SaveUser::hash(transaction.params);
        auto transactionSerializer = std::make_unique<
            UbjsonSerializedTransaction<nx::cloud::db::ec2::command::SaveUser>>(
                std::move(transaction),
                protocolVersionRange().currentVersion());

        commandLog()->saveLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            transactionHash,
            std::move(transactionSerializer));
    }

    void assertTransactionIsPresent()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        ASSERT_EQ(1U, allTransactions.size());

        boost::optional<Command<vms::api::UserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        ASSERT_TRUE(static_cast<bool>(transaction));
    }

    void whenReceiveTransactionFromOtherPeerWithGreaterTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(1);
    }

    void assertTransactionIsReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_NE(*m_initialTransaction, finalTransaction);
    }

    void assertTransactionAuthorIsLocalPeer()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(peerId(), finalTransaction.peerID);
    }

    void assertTransactionAuthorIsOtherPeer()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(m_otherPeerId, finalTransaction.peerID);
    }

    void whenReceiveTransactionFromOtherPeerWithLesserTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(-1);
    }

    void assertTransactionIsNotReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(*m_initialTransaction, finalTransaction);
    }

    void whenAddTransactionLocallyWithGreaterSequence()
    {
        whenGenerateTransactionLocally();
    }

    void whenReceiveOwnOldTransactionWithLesserSequence()
    {
        auto queryContext = getQueryContext();
        auto transaction = commandLog()->prepareLocalTransaction(
            queryContext.get(),
            m_systemId.c_str(),
            ::ec2::ApiCommand::saveUser,
            m_transactionData);
        transaction.persistentInfo.sequence -= 2;
        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        const auto resultCode = commandLog()->checkIfNeededAndSaveToLog
            <nx::cloud::db::ec2::command::SaveUser>(
                queryContext.get(),
                m_systemId.c_str(),
                clusterdb::engine::SerializableCommand<nx::cloud::db::ec2::command::SaveUser>(
                    std::move(transaction)));
        ASSERT_EQ(nx::sql::DBResult::cancelled, resultCode);
    }

    void assertMaxTimestampSequenceIsUsed()
    {
        const auto timestamp = commandLog()->generateTransactionTimestamp(m_systemId.c_str());
        ASSERT_EQ(m_lastUsedSequence, timestamp.sequence);
    }

private:
    const std::string m_systemId;
    const QnUuid m_otherPeerId;
    const QnUuid m_otherPeerDbId;
    int m_otherPeerSequence;
    vms::api::UserData m_transactionData;
    std::shared_ptr<nx::sql::QueryContext> m_activeQuery;
    nx::sql::DbConnectionHolder m_dbConnectionHolder;
    boost::optional<Command<vms::api::UserData>> m_initialTransaction;
    std::uint64_t m_lastUsedSequence = 0;

    void init()
    {
        const auto sharing =
            nx::cloud::db::test::BusinessDataGenerator::generateRandomSharing(
                nx::cloud::db::test::BusinessDataGenerator::generateRandomAccount(),
                m_systemId);
        nx::cloud::db::ec2::convert(sharing, &m_transactionData);
        ASSERT_TRUE(m_dbConnectionHolder.open());

        // Moving local peer sequence.
        // TODO: #ak Do it by generating commands and get rid of shiftLocalTransactionSequence method.
        commandLog()->shiftLocalTransactionSequence(m_systemId.c_str(), 100);
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

        auto dbConnection = m_dbConnectionHolder.dbConnection();
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

            Command<vms::api::UserData> transaction(peerId());
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
                nx::vms::api::TranState /*readedUpTo*/)
            {
                ASSERT_EQ(ResultCode::ok, resultCode);
                transactionsReadPromise.set_value(std::move(serializedTransactions));
            };

        commandLog()->readTransactions(
            m_systemId.c_str(),
            ReadCommandsFilter::kEmptyFilter,
            completionHandler);

        return transactionsReadPromise.get_future().get();
    }

    void addTransactionFromOtherPeerWithTimestampDiff(int timestampDiff)
    {
        saveTransaction(
            prepareFromOtherPeerWithTimestampDiff(timestampDiff));
    }

    Command<vms::api::UserData> prepareFromOtherPeerWithTimestampDiff(
        int timestampDiff)
    {
        Command<vms::api::UserData> transaction(m_otherPeerId);
        transaction.command = ::ec2::ApiCommand::saveUser;
        transaction.persistentInfo.dbID = m_otherPeerDbId;
        transaction.transactionType = ::ec2::TransactionType::Cloud;
        transaction.persistentInfo.sequence = m_otherPeerSequence++;
        transaction.persistentInfo.timestamp =
            m_initialTransaction
            ? m_initialTransaction->persistentInfo.timestamp + timestampDiff
            : commandLog()->generateTransactionTimestamp(m_systemId.c_str()) + timestampDiff;
        transaction.params = m_transactionData;

        if (!m_initialTransaction)
            m_initialTransaction = transaction;

        return transaction;
    }

    void saveTransaction(Command<vms::api::UserData> transaction)
    {
        auto queryContext = getQueryContext();
        const auto dbResult = commandLog()->checkIfNeededAndSaveToLog
            <nx::cloud::db::ec2::command::SaveUser>(
                queryContext.get(),
                m_systemId.c_str(),
                clusterdb::engine::UbjsonSerializedTransaction<nx::cloud::db::ec2::command::SaveUser>(
                    std::move(transaction),
                    nx_ec::EC2_PROTO_VERSION));
        ASSERT_TRUE(dbResult == nx::sql::DBResult::ok || dbResult == nx::sql::DBResult::cancelled)
            << "Got " << toString(dbResult);
    }

    Command<vms::api::UserData> getTransactionFromLog()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        NX_GTEST_ASSERT_EQ(1U, allTransactions.size());

        boost::optional<Command<vms::api::UserData>> transaction =
            findTransaction(allTransactions, m_transactionData);
        NX_GTEST_ASSERT_TRUE(static_cast<bool>(transaction));

        return *transaction;
    }
};

TEST_F(CommandLogSameTransaction, newly_generated_transaction_is_there)
{
    whenGenerateTransactionLocally();
    assertTransactionIsPresent();
}

TEST_F(CommandLogSameTransaction, transaction_from_remote_peer_has_been_added)
{
    whenReceiveTransactionFromOtherPeerWithGreaterTimestamp();
    assertTransactionIsPresent();
}

TEST_F(
    CommandLogSameTransaction,
    transaction_from_other_peer_with_greater_timestamp_replaces_existing_one)
{
    whenGenerateTransactionLocally();
    whenReceiveTransactionFromOtherPeerWithGreaterTimestamp();

    assertTransactionIsReplaced();
    assertTransactionAuthorIsOtherPeer();
}

TEST_F(CommandLogSameTransaction, transaction_with_lesser_timestamp_is_ignored)
{
    whenGenerateTransactionLocally();
    whenReceiveTransactionFromOtherPeerWithLesserTimestamp();

    assertTransactionIsNotReplaced();
    assertTransactionAuthorIsLocalPeer();
}

TEST_F(CommandLogSameTransaction, transaction_with_greater_sequence_replaces_existing)
{
    whenGenerateTransactionLocally();
    whenAddTransactionLocallyWithGreaterSequence();

    assertTransactionIsReplaced();
    assertTransactionAuthorIsLocalPeer();
}

TEST_F(CommandLogSameTransaction, transaction_with_lesser_sequence_is_ignored)
{
    whenGenerateTransactionLocally();
    whenReceiveOwnOldTransactionWithLesserSequence();

    assertTransactionIsNotReplaced();
    assertTransactionAuthorIsLocalPeer();
}

TEST_F(CommandLogSameTransaction, tran_rollback_clears_raw_data)
{
    whenGenerateTransactionLocally();
    beginTran();
    whenAddTransactionLocallyWithGreaterSequence();
    rollbackTran();
    assertTransactionIsNotReplaced();
}

TEST_F(CommandLogSameTransaction, max_timestamp_sequence_is_restored_after_restart)
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
        const std::unique_ptr<clusterdb::engine::CommandLog>& commandLog,
        const nx::cloud::db::api::SystemData& system,
        const nx::cloud::db::api::AccountData& accountToShareWith)
        :
        m_commandLog(commandLog),
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
        m_completedState = State::startedTransaction;
        m_desiredState = State::startedTransaction;
        m_commandLog->startDbTransaction(
            m_system.id.c_str(),
            [this](auto&&... args) { return doSomeDataModifications(std::move(args)...); },
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
    const std::unique_ptr<clusterdb::engine::CommandLog>& m_commandLog;
    const nx::cloud::db::api::SystemData m_system;
    const nx::cloud::db::api::AccountData m_accountToShareWith;
    State m_completedState;
    State m_desiredState;
    nx::cloud::db::api::SystemSharingEx m_sharing;
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
        m_sharing.accessRole = nx::cloud::db::api::SystemAccessRole::advancedViewer;
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
        vms::api::UserData userData;
        nx::cloud::db::ec2::convert(m_sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(m_accountToShareWith.fullName);
        auto dbResult = m_commandLog->generateTransactionAndSaveToLog
            <nx::cloud::db::ec2::command::SaveUser>(
                queryContext,
                m_sharing.systemId.c_str(),
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

class CommandLogOverlappingTransactions:
    public CommandLog
{
public:
    CommandLogOverlappingTransactions():
        CommandLog(dao::DataObjectType::ram)
    {
        persistentDbManager()->queryExecutor().setConcurrentModificationQueryLimit(0 /*no limit*/);
    }

protected:
    struct ReadResult
    {
        ResultCode resultCode;
        std::vector<dao::TransactionLogRecord> serializedTransactions;
        vms::api::TranState readedUpTo;
    };

    void whenAddOverlappingTransactions()
    {
        constexpr std::size_t transactionCount = 5;

        persistentDbManager()->queryExecutor().reserveConnections(transactionCount);

        auto dbTransactions = startDbTransactions(transactionCount);
        for (std::size_t i = 0; i < dbTransactions.size(); ++i)
            dbTransactions[i]->generateTransaction();

        for (std::size_t i = dbTransactions.size(); i > 0; --i)
            dbTransactions[i-1]->commit();
    }

    void whenAddBunchOfTransactionsConcurrently()
    {
        whenScheduleMultipleConcurrentCommands();

        waitUntilAllScheduledCommandsHaveCompleted();
    }

    void whenScheduleMultipleConcurrentCommands()
    {
        const int transactionToAddCount = dbConnectionOptions().maxConnectionCount;
        ASSERT_GT(transactionToAddCount, 1);
        m_transactionToExecuteThreshold = 7;

        m_transactionOrder.resize(transactionToAddCount);
        for (int i = 0; i < transactionToAddCount; ++i)
            m_transactionOrder[i] = i;

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_transactionOrder.begin(), m_transactionOrder.end(), g);

        for (int i = 0; i < transactionToAddCount; ++i)
        {
            ++m_scheduledCommandCount;
            commandLog()->startDbTransaction(
                getSystem(0).id,
                [this, i](auto&&... args) { return shareSystemToRandomUser(std::move(args)..., i); },
                [this](
                    nx::sql::DBResult dbResult)
                {
                    if (dbResult != nx::sql::DBResult::cancelled)
                    {
                        ASSERT_EQ(nx::sql::DBResult::ok, dbResult);
                    }

                    m_commandResults.push(dbResult);
                });
        }
    }

    void waitUntilAllScheduledCommandsHaveCompleted()
    {
        for (int i = 0; i < m_scheduledCommandCount; ++i)
            m_commandResults.pop();
    }

    void whenReadCommandLog()
    {
        commandLog()->readTransactions(
            getSystem(0).id,
            ReadCommandsFilter(),
            [this](auto&&... args) { saveReadResult(std::move(args)...); });
    }

    void thenReadSucceeded()
    {
        m_prevReadResult = m_readResults.pop();

        ASSERT_EQ(ResultCode::ok, m_prevReadResult.resultCode);
    }

    void andReportedReadPositionCorrespondsToCommandsRead()
    {
        ASSERT_EQ(
            m_prevReadResult.readedUpTo.values.begin().value(),
            m_prevReadResult.serializedTransactions.size());
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
            commandLog(),
            getSystem(0),
            accountToShareWith);
        tranController->startDbTransaction();
        return tranController;
    }

    void assertTransactionsAreSentInAscendingSequenceOrder()
    {
        outgoingTransactionDispatcher().assertTransactionsAreSentInAscendingSequenceOrder();
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    std::vector<int> m_transactionOrder;
    int m_transactionToExecuteThreshold = 0;
    int m_scheduledCommandCount = 0;
    nx::utils::SyncQueue<nx::sql::DBResult> m_commandResults;
    nx::utils::SyncQueue<ReadResult> m_readResults;
    ReadResult m_prevReadResult;

    nx::sql::DBResult shareSystemToRandomUser(
        nx::sql::QueryContext* queryContext,
        int originalTransactionIndex)
    {
        nx::cloud::db::api::SystemSharingEx sharing;
        sharing.systemId = getSystem(0).id;
        const auto& accountToShareWith =
            accounts().at(nx::utils::random::number<std::size_t>(1, accounts().size() - 1));
        sharing.accountId = accountToShareWith.id;
        sharing.accountEmail = accountToShareWith.email;
        sharing.accessRole = nx::cloud::db::api::SystemAccessRole::advancedViewer;
        sharing.vmsUserId = QnUuid::createUuid().toSimpleString().toStdString();

        // It does not matter for transaction log whether we save application data or not.
        // TODO #ak But, it is still better to mockup data access object here.
        //NX_GTEST_ASSERT_EQ(
        //    nx::sql::DBResult::ok,
        //    systemSharingController().insertOrReplaceSharing(queryContext, sharing));

        vms::api::UserData userData;
        nx::cloud::db::ec2::convert(sharing, &userData);
        userData.isCloud = true;
        userData.fullName = QString::fromStdString(accountToShareWith.fullName);
        auto dbResult = commandLog()->generateTransactionAndSaveToLog
            <nx::cloud::db::ec2::command::SaveUser>(
                queryContext,
                sharing.systemId.c_str(),
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

    void saveReadResult(
        ResultCode resultCode,
        std::vector<dao::TransactionLogRecord> serializedTransactions,
        vms::api::TranState readedUpTo)
    {
        m_readResults.push({
            resultCode,
            std::move(serializedTransactions),
            std::move(readedUpTo)});
    }
};

TEST_F(CommandLogOverlappingTransactions, overlapping_transactions_sent_in_a_correct_order)
{
    givenRandomSystem();
    whenAddOverlappingTransactions();
    assertTransactionsAreSentInAscendingSequenceOrder();
}

TEST_F(CommandLogOverlappingTransactions, multiple_simultaneous_transactions)
{
    givenRandomSystem();
    whenAddBunchOfTransactionsConcurrently();
    assertTransactionsAreSentInAscendingSequenceOrder();
}

TEST_F(CommandLogOverlappingTransactions, command_read_position_matches_actual_commands_read)
{
    givenRandomSystem();
    whenScheduleMultipleConcurrentCommands();

    whenReadCommandLog();

    thenReadSucceeded();
    andReportedReadPositionCorrespondsToCommandsRead();

    waitUntilAllScheduledCommandsHaveCompleted();
}

} // namespace nx::clusterdb::engine::test
