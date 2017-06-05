#include <deque>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

#include <ec2/transaction_log_cache.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace test {

class TransactionLogCache:
    public ::testing::Test
{
public:
    using TranId = VmsTransactionLogCache::TranId;

    TransactionLogCache():
        m_systemId(QnUuid::createUuid().toSimpleByteArray()),
        m_peerId(QnUuid::createUuid().toSimpleByteArray()),
        m_dbId(QnUuid(m_systemId))
    {
        m_initialData = readState();
        m_committedData = m_initialData;
    }

    ~TransactionLogCache()
    {
    }

protected:
    TranId beginTran()
    {
        const auto tranId = m_cache.beginTran();
        m_rawData[tranId] = m_committedData;
        return tranId;
    }

    void beginMultipleTransactions()
    {
        m_startedTransactions.resize(5);
        for (auto& tranId: m_startedTransactions)
            tranId = beginTran();
    }

    void modifyDataUnderTran(TranId tranId)
    {
        CacheState& rawData = m_rawData[tranId];
        rawData.timestampSequence =
            nx::utils::random::number<int>(m_committedData.timestampSequence + 1);

        m_cache.updateTimestampSequence(tranId, rawData.timestampSequence);

        generateTransaction(tranId);
    }

    void modifyDataUnderEachTransaction()
    {
        for (const auto& tranId: m_startedTransactions)
            modifyDataUnderTran(tranId);
    }

    void assertIfRawDataIsVisible()
    {
        assertIfNotEqual(m_committedData, m_cache);
    }

    void commit(TranId tranId)
    {
        m_cache.commit(tranId);
        m_committedData = m_rawData[tranId];
        m_rawData.erase(tranId);
    }

    void commitTransactionsInReverseOrder()
    {
        for (auto rIt = m_startedTransactions.rbegin(); rIt != m_startedTransactions.rend(); ++rIt)
            commit(*rIt);
    }

    void rollback(TranId tranId)
    {
        m_cache.rollback(tranId);
        m_rawData.erase(tranId);
    }

    void assertIfCommittedDataIsNotVisible()
    {
        assertIfNotEqual(m_committedData, m_cache);
    }

    void assertIfCacheStateIsNotValid()
    {
        const auto newSequence = m_cache.generateTransactionSequence(
            ::ec2::ApiPersistentIdData(QnUuid(m_peerId), m_dbId));
        if (!m_transactionSequenceGenerated.empty())
        {
            ASSERT_GT(newSequence, *m_transactionSequenceGenerated.begin());
        }
    }

    void assertIfStateHasBeenChanged()
    {
        assertIfNotEqual(m_committedData, m_cache);
    }

private:
    struct CacheState
    {
        std::uint64_t timestampSequence;

        CacheState():
            timestampSequence(0)
        {
        }
    };

    VmsTransactionLogCache m_cache;
    const nx::String m_systemId;
    const nx::String m_peerId;
    const QnUuid m_dbId;
    CacheState m_committedData;
    CacheState m_initialData;
    std::map<TranId, CacheState> m_rawData;
    std::deque<TranId> m_startedTransactions;
    std::set<int, std::greater<int>> m_transactionSequenceGenerated;

    CacheState readState()
    {
        CacheState result;
        result.timestampSequence = m_cache.committedTimestampSequence();
        return result;
    }

    void generateTransaction(TranId tranId)
    {
        auto transactionHeader = ::ec2::QnAbstractTransaction(QnUuid(m_peerId));
        transactionHeader.peerID = QnUuid(m_peerId);
        transactionHeader.command = ::ec2::ApiCommand::saveCamera;
        transactionHeader.transactionType = ::ec2::TransactionType::Cloud;
        transactionHeader.persistentInfo.sequence =
            m_cache.generateTransactionSequence(
                ::ec2::ApiPersistentIdData(QnUuid(m_peerId), m_dbId));
        transactionHeader.persistentInfo.dbID = m_dbId;
        transactionHeader.persistentInfo.timestamp = m_cache.generateTransactionTimestamp(tranId);
        m_cache.insertOrReplaceTransaction(tranId, transactionHeader, m_peerId + m_systemId);

        m_transactionSequenceGenerated.insert(transactionHeader.persistentInfo.sequence);
    }

    void assertIfNotEqual(const CacheState& cacheState, const VmsTransactionLogCache& cache)
    {
        ASSERT_EQ(cacheState.timestampSequence, cache.committedTimestampSequence());
    }
};

TEST_F(TransactionLogCache, commit_saves_data)
{
    auto tranId = beginTran();
    modifyDataUnderTran(tranId);
    assertIfRawDataIsVisible();
    commit(tranId);
    assertIfCommittedDataIsNotVisible();
}

TEST_F(TransactionLogCache, transaction_isolation)
{
    auto tran1Id = beginTran();
    auto tran2Id = beginTran();
    modifyDataUnderTran(tran1Id);
    modifyDataUnderTran(tran2Id);
    commit(tran1Id);
    commit(tran2Id);
    assertIfCommittedDataIsNotVisible();
}

TEST_F(TransactionLogCache, rollback)
{
    auto tranId = beginTran();
    modifyDataUnderTran(tranId);
    assertIfRawDataIsVisible();
    rollback(tranId);
    assertIfStateHasBeenChanged();
}

TEST_F(TransactionLogCache, transaction_pipelining)
{
    beginMultipleTransactions();

    modifyDataUnderEachTransaction();
    commitTransactionsInReverseOrder();

    assertIfCommittedDataIsNotVisible();
    assertIfCacheStateIsNotValid();
}

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
