#include <deque>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

#include <nx/data_sync_engine/transaction_log_cache.h>

namespace nx {
namespace data_sync_engine {
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
        assertEqual(m_committedData, m_cache);
    }

    void commit(TranId tranId)
    {
        m_cache.commit(tranId);
        // Timestamp sequence MUST NOT decrease.
        m_committedData.timestampSequence =
            std::max(m_committedData.timestampSequence, m_rawData[tranId].timestampSequence);
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

    void saveTransactionWithGreaterTimestampSequence()
    {
        const auto tranId = beginTran();

        auto transaction = prepareTransaction(tranId);
        transaction.persistentInfo.timestamp.sequence =
            m_cache.committedTimestampSequence() + 1;
        saveTransactionToCache(tranId, transaction);

        commit(tranId);
    }

    void assertGreaterSequenceIsApplied()
    {
        ASSERT_EQ(
            *m_transactionSequenceGenerated.begin(),
            (int)m_cache.committedTimestampSequence());
    }

    void assertCommittedDataIsVisible()
    {
        assertEqual(m_committedData, m_cache);
    }

    void assertCacheStateIsValid()
    {
        const auto newSequence = m_cache.generateTransactionSequence(
            vms::api::PersistentIdData(QnUuid(m_peerId), m_dbId));
        if (!m_transactionSequenceGenerated.empty())
        {
            ASSERT_GT(newSequence, *m_transactionSequenceGenerated.begin());
        }
    }

    void assertStateHasNotBeenChanged()
    {
        assertEqual(m_committedData, m_cache);
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

    CommandHeader prepareTransaction(TranId tranId)
    {
        auto transactionHeader = CommandHeader(QnUuid(m_peerId));
        transactionHeader.peerID = QnUuid(m_peerId);
        transactionHeader.command = ::ec2::ApiCommand::saveCamera;
        transactionHeader.transactionType = ::ec2::TransactionType::Cloud;
        transactionHeader.persistentInfo.sequence =
            m_cache.generateTransactionSequence(
                vms::api::PersistentIdData(QnUuid(m_peerId), m_dbId));
        transactionHeader.persistentInfo.dbID = m_dbId;
        transactionHeader.persistentInfo.timestamp =
            m_cache.generateTransactionTimestamp(tranId);
        return transactionHeader;
    }

    void generateTransaction(TranId tranId)
    {
        auto transactionHeader = prepareTransaction(tranId);
        saveTransactionToCache(tranId, transactionHeader);
    }

    void saveTransactionToCache(
        TranId tranId,
        CommandHeader transactionHeader)
    {
        m_cache.insertOrReplaceTransaction(tranId, transactionHeader, m_peerId + m_systemId);
        m_transactionSequenceGenerated.insert(transactionHeader.persistentInfo.sequence);
    }

    void assertEqual(const CacheState& cacheState, const VmsTransactionLogCache& cache)
    {
        ASSERT_EQ(cacheState.timestampSequence, cache.committedTimestampSequence());
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(TransactionLogCache, commit_saves_data)
{
    auto tranId = beginTran();
    modifyDataUnderTran(tranId);
    assertIfRawDataIsVisible();
    commit(tranId);
    assertCommittedDataIsVisible();
}

TEST_F(TransactionLogCache, transaction_isolation)
{
    auto tran1Id = beginTran();
    auto tran2Id = beginTran();
    modifyDataUnderTran(tran1Id);
    modifyDataUnderTran(tran2Id);
    commit(tran1Id);
    commit(tran2Id);
    assertCommittedDataIsVisible();
}

TEST_F(TransactionLogCache, rollback)
{
    auto tranId = beginTran();
    modifyDataUnderTran(tranId);
    assertIfRawDataIsVisible();
    rollback(tranId);
    assertStateHasNotBeenChanged();
}

TEST_F(TransactionLogCache, transaction_pipelining)
{
    beginMultipleTransactions();

    modifyDataUnderEachTransaction();
    commitTransactionsInReverseOrder();

    assertCommittedDataIsVisible();
    assertCacheStateIsValid();
}

TEST_F(TransactionLogCache, timestamp_sequence_is_updated_by_external_transaction)
{
    saveTransactionWithGreaterTimestampSequence();
    assertGreaterSequenceIsApplied();
}

} // namespace test
} // namespace data_sync_engine
} // namespace nx
