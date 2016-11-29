#include <gtest/gtest.h>

#include <nx/utils/random.h>

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
        m_systemId("abcdefghijklmnoprstquvwxyz")
    {
        generateSomeData();
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

    void modifyDataUnderTran(TranId tranId)
    {
        CacheState& rawData = m_rawData[tranId];
        rawData.timestampSequence = 
            nx::utils::random::number<int>(m_committedData.timestampSequence + 1);

        m_cache.updateTimestampSequence(tranId, rawData.timestampSequence);
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

    void assertIfCommittedDataIsNotVisible()
    {
        assertIfNotEqual(m_committedData, m_cache);
    }

    void rollback(TranId tranId)
    {
        m_cache.rollback(tranId);
        m_rawData.erase(tranId);
    }

    void assertIfStateHasBeenChanged()
    {
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
    CacheState m_committedData;
    CacheState m_initialData;
    std::map<TranId, CacheState> m_rawData;

    void generateSomeData()
    {
        // TODO
    }

    CacheState readState()
    {
        CacheState result;
        result.timestampSequence = *m_cache.committedData.timestampSequence;
        return result;
    }

    void assertIfNotEqual(const CacheState& cacheState, const VmsTransactionLogCache& cache)
    {
        ASSERT_EQ(cacheState.timestampSequence, cache.committedData.timestampSequence);
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

} // namespace test
} // namespace ec2
} // namespace cdb
} // namespace nx
