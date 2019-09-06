#include <optional>

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/detail/query_execution_thread.h>
#include <nx/sql/db_instance_controller.h>
#include <nx/sql/test_support/test_with_db_helper.h>
#include <nx/utils/counter.h>
#include <nx/utils/random.h>
#include <nx/utils/random_qt_device.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/time.h>

#include <nx/clusterdb/engine/dao/memory/command_data_object_in_memory.h>
#include <nx/clusterdb/engine/dao/rdb/structure_updater.h>
#include <nx/clusterdb/engine/outgoing_command_dispatcher.h>
#include <nx/clusterdb/engine/command_log.h>

#include "customer_db/data.h"
#include "test_outgoing_command_dispatcher.h"

namespace nx::clusterdb::engine::test {

class BaseDbTest:
    public nx::sql::test::TestWithDbHelper
{
public:
    BaseDbTest():
        nx::sql::test::TestWithDbHelper(""),
        m_dbController(dbConnectionOptions())
    {
    }

    void initializeDatabase()
    {
        ASSERT_TRUE(m_dbController.initialize());
    }

    nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor()
    {
        return m_dbController.queryExecutor();
    }

private:
    nx::sql::InstanceController m_dbController;
};

//-------------------------------------------------------------------------------------------------

static constexpr int kCurrentProtoVersion = 1;

class CommandLog:
    public BaseDbTest,
    public ::testing::Test
{
public:
    CommandLog(dao::DataObjectType dataObjectType):
        m_protocolVersionRange(kCurrentProtoVersion, kCurrentProtoVersion),
        m_peerId(QnUuid::createUuid())
    {
        dbConnectionOptions().maxConnectionCount = 100;
        initializeDatabase();

        dao::rdb::StructureUpdater updater(&queryExecutor());

        m_factoryFuncBak =
             clusterdb::engine::dao::TransactionDataObjectFactory::instance()
                .setDataObjectType(dataObjectType);

        initializeTransactionLog();
    }

    ~CommandLog()
    {
        clusterdb::engine::dao::TransactionDataObjectFactory::instance()
            .setCustomFunc(std::exchange(m_factoryFuncBak, {}));
    }

protected:
    clusterdb::engine::CommandLog& commandLog()
    {
        return *m_commandLog;
    }

    const clusterdb::engine::CommandLog& commandLog() const
    {
        return *m_commandLog;
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

    Customer generateRandomCustomer()
    {
        Customer customer;
        customer.id = QnUuid::createUuid().toSimpleString().toStdString();
        customer.fullName = QnUuid::createUuid().toSimpleString().toStdString();
        customer.address = QnUuid::createUuid().toSimpleString().toStdString();
        return customer;
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
            SynchronizationSettings(),
            m_peerId,
            m_protocolVersionRange,
            &queryExecutor(),
            &m_outgoingTransactionDispatcher);
    }
};

class CommandLogSameTransaction:
    public CommandLog
{
public:
    CommandLogSameTransaction():
        CommandLog(dao::DataObjectType::rdbms),
        m_clusterId(QnUuid::createUuid().toSimpleString().toStdString()),
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
            auto tran = prepareFromOtherPeerWithTimestampDiff(
                std::chrono::milliseconds(1));
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
        auto command = commandLog().prepareLocalTransaction(
            queryContext.get(),
            m_clusterId.c_str(),
            command::SaveCustomer::code,
            m_commandData);
        if (!m_initialCommand)
            m_initialCommand = command;

        const auto commandHash = command::SaveCustomer::hash(command.params);
        auto commandSerializer = std::make_unique<
            UbjsonSerializedTransaction<command::SaveCustomer>>(
                std::move(command),
                protocolVersionRange().currentVersion());

        commandLog().saveLocalTransaction(
            queryContext.get(),
            m_clusterId.c_str(),
            commandHash,
            std::move(commandSerializer));
    }

    void assertTransactionIsPresent()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        ASSERT_EQ(1U, allTransactions.size());

        std::optional<Command<command::SaveCustomer::Data>> transaction =
            findTransaction(allTransactions, m_commandData);
        ASSERT_TRUE(static_cast<bool>(transaction));
    }

    void whenReceiveTransactionFromOtherPeerWithGreaterTimestamp()
    {
        addTransactionFromOtherPeerWithTimestampDiff(
            std::chrono::milliseconds(1));
    }

    void assertTransactionIsReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_NE(*m_initialCommand, finalTransaction);
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
        addTransactionFromOtherPeerWithTimestampDiff(
            std::chrono::milliseconds(-1));
    }

    void assertTransactionIsNotReplaced()
    {
        const auto finalTransaction = getTransactionFromLog();
        ASSERT_EQ(*m_initialCommand, finalTransaction);
    }

    void whenAddTransactionLocallyWithGreaterSequence()
    {
        whenGenerateTransactionLocally();
    }

    void whenReceiveOwnOldTransactionWithLesserSequence()
    {
        auto queryContext = getQueryContext();
        auto transaction = commandLog().prepareLocalTransaction(
            queryContext.get(),
            m_clusterId.c_str(),
            command::SaveCustomer::code,
            m_commandData);
        transaction.persistentInfo.sequence -= 2;
        if (!m_initialCommand)
            m_initialCommand = transaction;

        const auto resultCode = commandLog().checkIfNeededAndSaveToLog
            <command::SaveCustomer>(
                queryContext.get(),
                m_clusterId.c_str(),
                clusterdb::engine::SerializableCommand<command::SaveCustomer>(
                    std::move(transaction)));
        ASSERT_EQ(nx::sql::DBResult::cancelled, resultCode);
    }

    void assertMaxTimestampSequenceIsUsed()
    {
        const auto timestamp = commandLog().generateTransactionTimestamp(m_clusterId.c_str());
        ASSERT_EQ(m_lastUsedSequence, timestamp.sequence);
    }

private:
    const std::string m_clusterId;
    const QnUuid m_otherPeerId;
    const QnUuid m_otherPeerDbId;
    int m_otherPeerSequence;
    command::SaveCustomer::Data m_commandData;
    std::shared_ptr<nx::sql::QueryContext> m_activeQuery;
    nx::sql::DbConnectionHolder m_dbConnectionHolder;
    std::optional<Command<command::SaveCustomer::Data>> m_initialCommand;
    std::uint64_t m_lastUsedSequence = 0;

    void init()
    {
        m_commandData = generateRandomCustomer();

        ASSERT_TRUE(m_dbConnectionHolder.open());

        // Moving local peer sequence.
        // TODO: #ak Do it by generating commands and get rid of shiftLocalTransactionSequence method.
        commandLog().shiftLocalTransactionSequence(m_clusterId.c_str(), 100);
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
        auto dbConnection = m_dbConnectionHolder.dbConnection();
        auto transaction = std::make_shared<nx::sql::Transaction>(dbConnection);
        NX_GTEST_ASSERT_EQ(nx::sql::DBResult::ok, transaction->begin());

        return std::shared_ptr<nx::sql::QueryContext>(
            new nx::sql::QueryContext(dbConnection, transaction.get()),
            [transaction](nx::sql::QueryContext* queryContext)
            {
                if (queryContext->transaction()->isActive())
                {
                    ASSERT_EQ(nx::sql::DBResult::ok, queryContext->transaction()->commit());
                }
                delete queryContext;
            });
    }

    template<typename TransactionDataType>
    std::optional<Command<TransactionDataType>> findTransaction(
        const std::vector<dao::TransactionLogRecord>& allTransactions,
        const TransactionDataType& transactionData)
    {
        for (const auto& logRecord: allTransactions)
        {
            const auto serializedTransactionFromLog =
                logRecord.serializer->serialize(Qn::UbjsonFormat, kCurrentProtoVersion);

            Command<TransactionDataType> command;
            command.peerID = peerId();
            QnUbjsonReader<QByteArray> ubjsonStream(&serializedTransactionFromLog);
            const bool isDeserialized = QnUbjson::deserialize(&ubjsonStream, &command);
            NX_GTEST_ASSERT_TRUE(isDeserialized);

            if (command.params == transactionData)
                return command;
        }

        return std::nullopt;
    }

    std::vector<dao::TransactionLogRecord> readAllTransactions()
    {
        nx::utils::promise<std::vector<dao::TransactionLogRecord>> transactionsReadPromise;
        auto completionHandler =
            [&transactionsReadPromise](
                ResultCode resultCode,
                std::vector<dao::TransactionLogRecord> serializedTransactions,
                NodeState /*readedUpTo*/)
            {
                ASSERT_EQ(ResultCode::ok, resultCode);
                transactionsReadPromise.set_value(std::move(serializedTransactions));
            };

        commandLog().readTransactions(
            m_clusterId.c_str(),
            ReadCommandsFilter::kEmptyFilter,
            completionHandler);

        return transactionsReadPromise.get_future().get();
    }

    void addTransactionFromOtherPeerWithTimestampDiff(
        std::chrono::milliseconds timestampDiff)
    {
        saveTransaction(
            prepareFromOtherPeerWithTimestampDiff(timestampDiff));
    }

    Command<command::SaveCustomer::Data> prepareFromOtherPeerWithTimestampDiff(
        std::chrono::milliseconds timestampDiff)
    {
        Command<command::SaveCustomer::Data> transaction(
            command::SaveCustomer::code, m_otherPeerId);
        transaction.persistentInfo.dbID = m_otherPeerDbId;
        transaction.persistentInfo.sequence = m_otherPeerSequence++;
        transaction.persistentInfo.timestamp =
            m_initialCommand
            ? m_initialCommand->persistentInfo.timestamp + timestampDiff
            : commandLog().generateTransactionTimestamp(m_clusterId.c_str()) + timestampDiff;
        transaction.params = m_commandData;

        if (!m_initialCommand)
            m_initialCommand = transaction;

        return transaction;
    }

    void saveTransaction(Command<command::SaveCustomer::Data> command)
    {
        auto queryContext = getQueryContext();
        const auto dbResult = commandLog().checkIfNeededAndSaveToLog<command::SaveCustomer>(
            queryContext.get(),
            m_clusterId.c_str(),
            clusterdb::engine::UbjsonSerializedTransaction<command::SaveCustomer>(
                std::move(command),
                kCurrentProtoVersion));
        ASSERT_TRUE(dbResult == nx::sql::DBResult::ok || dbResult == nx::sql::DBResult::cancelled)
            << "Got " << toString(dbResult);
    }

    Command<command::SaveCustomer::Data> getTransactionFromLog()
    {
        const std::vector<dao::TransactionLogRecord> allTransactions = readAllTransactions();
        NX_GTEST_ASSERT_EQ(1U, allTransactions.size());

        std::optional<Command<command::SaveCustomer::Data>> command =
            findTransaction(allTransactions, m_commandData);
        NX_GTEST_ASSERT_TRUE(static_cast<bool>(command));

        return *command;
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
        clusterdb::engine::CommandLog* commandLog,
        const std::string& clusterId,
        const command::SaveCustomer::Data& customer)
        :
        m_commandLog(commandLog),
        m_clusterId(clusterId),
        m_customer(customer),
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
            m_clusterId,
            [this](auto&&... args) { return doSomeDataModifications(std::move(args)...); },
            [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
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
    clusterdb::engine::CommandLog* m_commandLog = nullptr;
    const std::string m_clusterId;
    const command::SaveCustomer::Data m_customer;
    State m_completedState;
    State m_desiredState;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    nx::utils::Counter m_startedAsyncCallsCounter;

    nx::sql::DBResult doSomeDataModifications(nx::sql::QueryContext* queryContext)
    {
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

    void addTransactionToLog(nx::sql::QueryContext* queryContext)
    {
        auto customer = m_customer;
        customer.fullName = QnUuid::createUuid().toSimpleString().toStdString();

        auto dbResult = m_commandLog->generateTransactionAndSaveToLog
            <command::SaveCustomer>(
                queryContext,
                m_clusterId,
                std::move(customer));
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
        CommandLog(dao::DataObjectType::ram),
        m_clusterId(QnUuid::createUuid().toSimpleString().toStdString())
    {
        queryExecutor().setConcurrentModificationQueryLimit(0 /*no limit*/);
    }

protected:
    struct ReadResult
    {
        ResultCode resultCode;
        std::vector<dao::TransactionLogRecord> serializedTransactions;
        NodeState readedUpTo;
    };

    void givenRandomSystem()
    {
        using namespace std::placeholders;

        constexpr int customerCount = 10;

        for (int i = 0; i < customerCount; ++i)
            m_customers.push_back(generateRandomCustomer());

        // Initializing transaction log.
        ASSERT_NO_THROW(queryExecutor().executeUpdateQuerySync(
            [this](auto queryContext)
            {
                return commandLog().updateTimestampHiForSystem(queryContext, m_clusterId, 0);
            }));
    }

    void whenAddOverlappingTransactions()
    {
        constexpr std::size_t transactionCount = 5;

        static_cast<nx::sql::AsyncSqlQueryExecutor&>(queryExecutor()).reserveConnections(transactionCount);

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

        nx::utils::random::QtDevice rd;
        std::mt19937 g(rd());
        std::shuffle(m_transactionOrder.begin(), m_transactionOrder.end(), g);

        for (int i = 0; i < transactionToAddCount; ++i)
        {
            ++m_scheduledCommandCount;
            commandLog().startDbTransaction(
                m_clusterId,
                [this, i](auto&&... args) { return insertRandomCustomer(std::move(args)..., i); },
                [this](nx::sql::DBResult dbResult)
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
        commandLog().readTransactions(
            m_clusterId,
            ReadCommandsFilter(),
            [this](auto&&... args) { saveReadResult(std::forward<decltype(args)>(args)...); });
    }

    void thenReadSucceeded()
    {
        m_prevReadResult = m_readResults.pop();

        ASSERT_EQ(ResultCode::ok, m_prevReadResult.resultCode);
    }

    void andReportedReadPositionCorrespondsToCommandsRead()
    {
        ASSERT_EQ(
            m_prevReadResult.readedUpTo.nodeSequence.begin()->second,
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
        auto customer = nx::utils::random::choice(m_customers);
        customer.fullName = QnUuid::createUuid().toSimpleString().toStdString();

        auto tranController = std::make_unique<TestTransactionController>(
            &commandLog(),
            m_clusterId,
            std::move(customer));
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
    std::string m_clusterId;
    std::vector<command::SaveCustomer::Data> m_customers;

    nx::sql::DBResult insertRandomCustomer(
        nx::sql::QueryContext* queryContext,
        int originalTransactionIndex)
    {
        auto customer = generateRandomCustomer();

        auto dbResult = commandLog().generateTransactionAndSaveToLog<command::SaveCustomer>(
            queryContext,
            m_clusterId,
            std::move(customer));
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
        NodeState readedUpTo)
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
