#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>

#include "command.h"
#include "transaction_timestamp_calculator.h"

namespace nx {
namespace data_sync_engine {

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
class NX_DATA_SYNC_ENGINE_API VmsTransactionLogCache
{
public:
    using TranId = int;

    struct VmsDataState
    {
        /** map<transaction hash, peer which updated transaction> */
        std::map<nx::Buffer, UpdateHistoryData> transactionHashToUpdateAuthor;
        vms::api::TranState transactionState;
        boost::optional<std::uint64_t> timestampSequence;
    };

    static const TranId InvalidTranId = -1;

    VmsTransactionLogCache();

    bool isShouldBeIgnored(
        const nx::String& systemId,
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
    void shiftTransactionSequence(const vms::api::PersistentIdData& tranStateKey, int delta);

    vms::api::TranState committedTransactionState() const;
    std::uint64_t committedTimestampSequence() const;

private:
    struct TranContext
    {
        VmsDataState data;
    };

    vms::api::Timestamp m_maxTimestamp;
    std::map<TranId, TranContext> m_tranIdToContext;
    mutable QnMutex m_mutex;
    TranId m_tranIdSequence;
    TransactionTimestampCalculator m_timestampCalculator;
    VmsDataState m_committedData;

    std::uint64_t timestampSequence(const QnMutexLockerBase& /*lock*/, TranId tranId) const;

    TranContext& findTranContext(TranId tranId);
    TranContext& findTranContext(const QnMutexLockerBase& lock, TranId tranId);
    const TranContext* findTranContext(TranId tranId) const;
    const TranContext* findTranContext(
        const QnMutexLockerBase& lock,
        TranId tranId) const;
};

} // namespace data_sync_engine
} // namespace nx
