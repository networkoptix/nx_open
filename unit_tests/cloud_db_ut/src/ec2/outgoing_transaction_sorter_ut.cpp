#include <gtest/gtest.h>

#include <deque>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>

#include <nx/cloud/cdb/ec2/outgoing_transaction_sorter.h>

#include "test_outgoing_transaction_dispatcher.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

class TestOutgoingTransactionSorter:
    public ec2::OutgoingTransactionSorter
{
public:
    TestOutgoingTransactionSorter(
        const nx::String& systemId,
        VmsTransactionLogCache* vmsTransactionLogCache,
        AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher)
    :
        ec2::OutgoingTransactionSorter(
            systemId,
            vmsTransactionLogCache,
            outgoingTransactionDispatcher)
    {
    }

    void waitUntilEveryCommittedTransactionHasBeenDelivered()
    {
        while (transactionsCommitted() > transactionsDelivered())
            std::this_thread::yield();
    }
};

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
        m_prevSequence(0),
        m_transactionCountRange(5, 50)
    {
    }

    void setTransactionCountRange(std::size_t min, std::size_t max)
    {
        m_transactionCountRange = {min, max};
    }

protected:
    void givenMultipleStartedTransactions()
    {
        m_tranIds.resize(nx::utils::random::number<std::size_t>(
            m_transactionCountRange.first, m_transactionCountRange.second));
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

        m_transactionSorter.waitUntilEveryCommittedTransactionHasBeenDelivered();
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

        m_transactionSorter.waitUntilEveryCommittedTransactionHasBeenDelivered();
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

        m_transactionSorter.waitUntilEveryCommittedTransactionHasBeenDelivered();
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

    void commit(VmsTransactionLogCache::TranId tranId)
    {
        m_transactionSorter.commit(tranId);

        QnMutexLocker lock(&m_mutex);
        for (const auto& transaction: m_transactionsGenerated)
        {
            if (transaction->tranId() == tranId)
                m_transactionsCommitted.push_back(transaction);
        }
    }

protected:
    std::deque<VmsTransactionLogCache::TranId> m_tranIds;
    TestOutgoingTransactionDispatcher m_testOutgoingTransactionDispatcher;

private:
    VmsTransactionLogCache m_transactionCache;
    TestOutgoingTransactionSorter m_transactionSorter;
    std::atomic<int> m_prevSequence;
    std::vector<std::shared_ptr<TestTransaction>> m_transactionsGenerated;
    std::vector<std::shared_ptr<TestTransaction>> m_transactionsCommitted;
    std::pair<std::size_t, std::size_t> m_transactionCountRange;
    QnMutex m_mutex;

    void addTransaction(VmsTransactionLogCache::TranId tranId)
    {
        auto transaction = std::make_shared<TestTransaction>(QnUuid::createUuid(), tranId);
        transaction->transactionHeader().persistentInfo.sequence = ++m_prevSequence;
        m_transactionSorter.addTransaction(
            tranId,
            std::make_unique<TransactionSharedWrapper>(transaction));

        m_transactionsGenerated.push_back(transaction);
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

class FtOutgoingTransactionSorter:
    public OutgoingTransactionSorter
{
    constexpr static int threadCount = 10;
    constexpr static int transactionPerThreadCount = 100;
    constexpr static int totalTransactionCount = threadCount * transactionPerThreadCount;

public:
    FtOutgoingTransactionSorter()
    {
        setTransactionCountRange(totalTransactionCount, totalTransactionCount);

        m_testOutgoingTransactionDispatcher.setDelayBeforeSavingTransaction(
            std::chrono::milliseconds(0), std::chrono::milliseconds(10));
    }

protected:
    void whenCommittedTransactionsConcurrently()
    {
        m_threads.resize(threadCount);
        for (auto& thread: m_threads)
        {
            std::vector<VmsTransactionLogCache::TranId> transactionsForThread =
                selectRandomTransactions(transactionPerThreadCount);
            thread = nx::utils::thread(
                std::bind(&FtOutgoingTransactionSorter::testThreadMain, this,
                    std::move(transactionsForThread)));
        }

        for (auto& thread: m_threads)
            thread.join();
    }

private:
    std::vector<nx::utils::thread> m_threads;

    std::vector<VmsTransactionLogCache::TranId> selectRandomTransactions(int maxTranCount)
    {
        std::vector<VmsTransactionLogCache::TranId> result;
        result.reserve(maxTranCount);
        for (int i = 0; (i < maxTranCount) && (!m_tranIds.empty()); ++i)
        {
            const auto index = nx::utils::random::number<std::size_t>(0, m_tranIds.size()-1);
            result.push_back(m_tranIds[index]);
            m_tranIds.erase(m_tranIds.begin() + index);
        }

        return result;
    }

    void testThreadMain(std::vector<VmsTransactionLogCache::TranId> tranIds)
    {
        for (const auto& tranId: tranIds)
            commit(tranId);
    }
};

TEST_F(FtOutgoingTransactionSorter, concurrent_transactions)
{
    givenMultipleStartedTransactions();
    whenCommittedTransactionsConcurrently();
    thenTransactionsShouldBeSentInIncreasingSequenceOrder();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
