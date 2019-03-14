#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <nx/network/buffer.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/mutex.h>

#include "command.h"
#include "transaction_timestamp_calculator.h"

namespace nx::clusterdb::engine {

struct UpdateHistoryData
{
    vms::api::PersistentIdData updatedBy;
    vms::api::Timestamp timestamp;
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
        vms::api::TranState transactionState;
        std::optional<std::uint64_t> timestampSequence;
    };

    static const TranId InvalidTranId = -1;

    CommandLogCache();

    bool isShouldBeIgnored(
        const std::string& systemId,
        const CommandHeader& tran,
        const QByteArray& hash) const;

    void restoreTransaction(
        vms::api::PersistentIdData tranStateKey,
        int sequence,
        const nx::Buffer& tranHash,
        const vms::api::Timestamp& timestamp);

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
    vms::api::Timestamp generateTransactionTimestamp(TranId tranId);
    int generateTransactionSequence(const vms::api::PersistentIdData& tranStateKey);
    int lastTransactionSequence(const vms::api::PersistentIdData& tranStateKey);

    /**
     * Makes sure the sequence is not less than value.
     */
    void shiftTransactionSequenceTo(const vms::api::PersistentIdData& tranStateKey, int value);

    void shiftTransactionSequence(const vms::api::PersistentIdData& tranStateKey, int delta);

    vms::api::TranState committedTransactionState() const;
    std::uint64_t committedTimestampSequence() const;

    int activeTransactionCount() const;

private:
    struct TranContext
    {
        VmsDataState data;
    };

    vms::api::Timestamp m_maxTimestamp;
    std::map<TranId, TranContext> m_tranIdToContext;
    mutable QnMutex m_mutex;
    TranId m_tranIdSequence;
    CommandTimestampCalculator m_timestampCalculator;
    VmsDataState m_committedData;
    VmsDataState m_rawData;

    std::uint64_t timestampSequence(const QnMutexLockerBase& /*lock*/, TranId tranId) const;

    TranContext& findTranContext(TranId tranId);
    TranContext& findTranContext(const QnMutexLockerBase& lock, TranId tranId);
    const TranContext* findTranContext(TranId tranId) const;
    const TranContext* findTranContext(
        const QnMutexLockerBase& lock,
        TranId tranId) const;
};

} // namespace nx::clusterdb::engine
