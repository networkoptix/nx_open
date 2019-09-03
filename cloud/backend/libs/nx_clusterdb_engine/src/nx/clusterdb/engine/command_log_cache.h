#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <nx/network/buffer.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "command.h"
#include "command_timestamp_calculator.h"
#include "node_state.h"
#include "timestamp.h"

namespace nx::clusterdb::engine {

struct UpdateHistoryData
{
    NodeStateKey updatedBy;
    Timestamp timestamp;
};

/**
 * Transaction log cache of a single system.
 * Supports transactioned updates.
 * Supported transaction isolation level - read committed.
 * @note Supports multiple concurrent transactions.
 * @note Calls with same tran id are expected to be serialized by caller.
 *     Otherwise, behavior is undefined.
 */
class NX_DATA_SYNC_ENGINE_API CommandLogCache
{
public:
    using TranId = int;

    struct VmsDataState
    {
        /** map<transaction hash, peer which updated transaction> */
        std::map<nx::Buffer, UpdateHistoryData> transactionHashToUpdateAuthor;
        NodeState nodeState;
        std::optional<std::uint64_t> timestampSequence;
    };

    static const TranId InvalidTranId = -1;

    CommandLogCache();

    bool isShouldBeIgnored(
        const std::string& systemId,
        const CommandHeader& tran,
        const QByteArray& hash) const;

    void restoreTransaction(
        NodeStateKey tranStateKey,
        int sequence,
        const nx::Buffer& tranHash,
        const Timestamp& timestamp);

    TranId beginTran();
    void commit(TranId tranId);
    void rollback(TranId tranId);

    void updateTimestampSequence(TranId tranId, quint64 newValue);

    void insertOrReplaceTransaction(
        TranId tranId,
        const CommandHeader& transaction,
        const QByteArray& transactionHash);

    /**
     * @return nullptr if tranId is unknown.
     */
    const VmsDataState* state(TranId tranId) const;
    Timestamp generateTransactionTimestamp(TranId tranId);
    int generateTransactionSequence(const NodeStateKey& tranStateKey);
    int lastTransactionSequence(const NodeStateKey& tranStateKey);

    /**
     * Makes sure the sequence is not less than value.
     */
    void shiftTransactionSequenceTo(const NodeStateKey& tranStateKey, int value);

    void shiftTransactionSequence(const NodeStateKey& tranStateKey, int delta);

    NodeState committedTransactionState() const;
    std::uint64_t committedTimestampSequence() const;

    int activeTransactionCount() const;

private:
    struct TranContext
    {
        VmsDataState data;
    };

    struct RawState
    {
        NodeState nodeState;

        RawState(const VmsDataState& right):
            nodeState(right.nodeState)
        {
        }

        RawState& operator=(const VmsDataState& right)
        {
            nodeState = right.nodeState;
            return *this;
        }
    };

    Timestamp m_maxTimestamp;
    std::map<TranId, TranContext> m_tranIdToContext;
    mutable QnMutex m_mutex;
    TranId m_tranIdSequence;
    CommandTimestampCalculator m_timestampCalculator;
    VmsDataState m_committedData;
    std::optional<RawState> m_rawState;

    std::uint64_t timestampSequence(const QnMutexLockerBase& /*lock*/, TranId tranId) const;

    TranContext& findTranContext(TranId tranId);
    TranContext& findTranContext(const QnMutexLockerBase& lock, TranId tranId);
    const TranContext* findTranContext(TranId tranId) const;
    const TranContext* findTranContext(
        const QnMutexLockerBase& lock,
        TranId tranId) const;

    RawState& rawState();
};

} // namespace nx::clusterdb::engine
