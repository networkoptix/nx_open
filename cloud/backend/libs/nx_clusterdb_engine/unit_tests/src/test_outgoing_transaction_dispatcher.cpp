#include "test_outgoing_transaction_dispatcher.h"

#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/random.h>

namespace nx::clusterdb::engine {
namespace test {

void TestOutgoingTransactionDispatcher::dispatchTransaction(
    const std::string& systemId,
    std::shared_ptr<const SerializableAbstractCommand> transactionSerializer)
{
    const auto delayMillis = nx::utils::random::number<int>(
        m_delayBeforeSavingTransaction.first.count(),
        m_delayBeforeSavingTransaction.second.count());
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMillis));

    if (m_onNewTransactionHandler)
        m_onNewTransactionHandler(systemId, *transactionSerializer);

    QnMutexLocker lk(&m_mutex);
    m_outgoingTransactions.push_back(std::move(transactionSerializer));
}

void TestOutgoingTransactionDispatcher::setOnNewTransaction(
    OnNewTransactionHandler onNewTransactionHandler)
{
    m_onNewTransactionHandler = std::move(onNewTransactionHandler);
}

void TestOutgoingTransactionDispatcher::setDelayBeforeSavingTransaction(
    std::chrono::milliseconds min, std::chrono::milliseconds max)
{
    m_delayBeforeSavingTransaction = {min, max};
}

void TestOutgoingTransactionDispatcher::clear()
{
    m_outgoingTransactions.clear();
}

void TestOutgoingTransactionDispatcher::assertTransactionsAreSentInAscendingSequenceOrder()
{
    int prevSequence = -1;
    for (const auto& transaction: m_outgoingTransactions)
    {
        ASSERT_GT(transaction->header().persistentInfo.sequence, prevSequence);
        prevSequence = transaction->header().persistentInfo.sequence;
    }
}

void TestOutgoingTransactionDispatcher::assertTransactionWithHeaderIsFound(
    const CommandHeader& transactionHeader)
{
    const auto it = std::find_if(
        m_outgoingTransactions.cbegin(),
        m_outgoingTransactions.cend(),
        [&transactionHeader](
            const std::shared_ptr<const SerializableAbstractCommand>& transactionSent)
        {
            return transactionHeader == transactionSent->header();
        });
    ASSERT_TRUE(it != m_outgoingTransactions.cend());
}

} // namespace test
} // namespace nx::clusterdb::engine
