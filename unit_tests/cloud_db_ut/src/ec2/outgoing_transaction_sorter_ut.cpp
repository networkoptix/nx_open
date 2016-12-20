#include <gtest/gtest.h>

#include <deque>

#include <nx/utils/random.h>

#include <ec2/outgoing_transaction_sorter.h>

#include "test_outgoing_transaction_dispatcher.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

class TestTransaction:
    public SerializableAbstractTransaction
{
public:
    TestTransaction(const QnUuid& peerId, VmsTransactionLogCache::TranId tranId):
        m_header(peerId),
        m_tranId(tranId)
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat /*targetFormat*/,
        int /*transactionFormatVersion*/) const override
    {
        return nx::Buffer();
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat /*targetFormat*/,
        const TransactionTransportHeader& /*transportHeader*/,
        int /*transactionFormatVersion*/) const override
    {
        return nx::Buffer();
    }

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const override
    {
        return m_header;
    }

    ::ec2::QnAbstractTransaction& transactionHeader()
    {
        return m_header;
    }

    VmsTransactionLogCache::TranId tranId() const
    {
        return m_tranId;
    }

private:
    ::ec2::QnAbstractTransaction m_header;
    VmsTransactionLogCache::TranId m_tranId;
};

class TransactionSharedWrapper:
    public SerializableAbstractTransaction
{
public:
    TransactionSharedWrapper(std::shared_ptr<SerializableAbstractTransaction> sharedTransaction):
        m_sharedTransaction(std::move(sharedTransaction))
    {
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        int transactionFormatVersion) const override
    {
        return m_sharedTransaction->serialize(targetFormat, transactionFormatVersion);
    }

    virtual nx::Buffer serialize(
        Qn::SerializationFormat targetFormat,
        const TransactionTransportHeader& transportHeader,
        int transactionFormatVersion) const override
    {
        return m_sharedTransaction->serialize(
            targetFormat, transportHeader, transactionFormatVersion);
    }

    virtual const ::ec2::QnAbstractTransaction& transactionHeader() const override
    {
        return m_sharedTransaction->transactionHeader();
    }

private:
    const std::shared_ptr<SerializableAbstractTransaction> m_sharedTransaction;
};

class OutgoingTransactionSorter:
    public ::testing::Test
{
public:
    OutgoingTransactionSorter():
        m_transactionSorter(
            QnUuid::createUuid().toSimpleByteArray(),
            &m_transactionCache,
            &m_testOutgoingTransactionDispatcher),
        m_prevSequence(0)
    {
    }

protected:
    void givenMultipleStartedTransactions()
    {
        m_tranIds.resize(nx::utils::random::number<std::size_t>(5, 50));
        for (auto& tranId: m_tranIds)
        {
            tranId = m_transactionCache.beginTran();
            addTransaction(tranId);
        }
    }

    void whenCommittedEveryTransaction()
    {
        for (const auto& tranId: m_tranIds)
            commit(tranId);
        m_tranIds.clear();
    }

    void whenRolledBackAllTransactions()
    {
        for (const auto& tranId: m_tranIds)
            m_transactionSorter.rollback(tranId);
        m_tranIds.clear();
    }

    void whenCommittedTransactionsInReverseOrder()
    {
        for (auto rIt = m_tranIds.rbegin(); rIt != m_tranIds.rend(); ++rIt)
            commit(*rIt);
        m_tranIds.clear();
    }

    void whenRolledBackFirstTransaction()
    {
        m_transactionSorter.rollback(m_tranIds.front());
        m_tranIds.pop_front();
    }

    void whenRandomlyFinishedTransactions()
    {
        while (!m_tranIds.empty())
        {
            const auto tranIndex = nx::utils::random::number<std::size_t>(0, m_tranIds.size()-1);
            const bool commitOrRollback = nx::utils::random::number<std::size_t>(0, 1) > 0;
            if (commitOrRollback)
                commit(m_tranIds[tranIndex]);
            else
                m_transactionSorter.rollback(m_tranIds[tranIndex]);
            m_tranIds.erase(m_tranIds.begin()+tranIndex);
        }
    }

    void thenTransactionsShouldBeSentInIncreasingSequenceOrder()
    {
        m_testOutgoingTransactionDispatcher.assertIfTransactionsWereNotSentInAscendingSequenceOrder();
    }

    void thenAllTransactionsWereSent()
    {
        m_testOutgoingTransactionDispatcher.assertIfNotAllTransactionsAreFound(
            m_transactionsCommitted);

        thenSorterShouldHoldNoTransactions();
    }

    void thenSorterShouldHoldNoTransactions()
    {
        m_transactionsCommitted.clear();
        m_testOutgoingTransactionDispatcher.clear();
        for (const auto& transaction: m_transactionsGenerated)
        {
            ASSERT_TRUE(transaction.unique());
        }
    }

private:
    TestOutgoingTransactionDispatcher m_testOutgoingTransactionDispatcher;
    VmsTransactionLogCache m_transactionCache;
    ec2::OutgoingTransactionSorter m_transactionSorter;
    std::atomic<int> m_prevSequence;
    std::deque<VmsTransactionLogCache::TranId> m_tranIds;
    std::vector<std::shared_ptr<TestTransaction>> m_transactionsGenerated;
    std::vector<std::shared_ptr<TestTransaction>> m_transactionsCommitted;

    void addTransaction(VmsTransactionLogCache::TranId tranId)
    {
        auto transaction = std::make_shared<TestTransaction>(QnUuid::createUuid(), tranId);
        transaction->transactionHeader().persistentInfo.sequence = ++m_prevSequence;
        m_transactionSorter.addTransaction(
            tranId,
            std::make_unique<TransactionSharedWrapper>(transaction));

        m_transactionsGenerated.push_back(transaction);
    }

    void commit(VmsTransactionLogCache::TranId tranId)
    {
        m_transactionSorter.commit(tranId);
        for (const auto& transaction: m_transactionsGenerated)
        {
            if (transaction->tranId() == tranId)
                m_transactionsCommitted.push_back(transaction);
        }
    }
};

TEST_F(OutgoingTransactionSorter, transactions_are_sent)
{
    givenMultipleStartedTransactions();
    whenCommittedEveryTransaction();
    thenAllTransactionsWereSent();
}

TEST_F(OutgoingTransactionSorter, transactions_are_removed_in_rollback)
{
    givenMultipleStartedTransactions();
    whenRolledBackAllTransactions();
    thenSorterShouldHoldNoTransactions();
}

TEST_F(OutgoingTransactionSorter, correct_send_order)
{
    givenMultipleStartedTransactions();
    whenCommittedTransactionsInReverseOrder();
    thenTransactionsShouldBeSentInIncreasingSequenceOrder();
}

TEST_F(OutgoingTransactionSorter, correct_send_order_after_rollback)
{
    givenMultipleStartedTransactions();

    whenRolledBackFirstTransaction();
    whenCommittedTransactionsInReverseOrder();

    thenTransactionsShouldBeSentInIncreasingSequenceOrder();
    thenAllTransactionsWereSent();
}

TEST_F(OutgoingTransactionSorter, correct_send_order_after_random_commit_rollback_order)
{
    givenMultipleStartedTransactions();
    whenRandomlyFinishedTransactions();
    thenTransactionsShouldBeSentInIncreasingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
