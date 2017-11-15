#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>

#include <transaction/transaction.h>

#include "transaction_timestamp_calculator.h"

namespace nx {
namespace cdb {
namespace ec2 {

struct UpdateHistoryData
{
    ::ec2::ApiPersistentIdData updatedBy;
    ::ec2::Timestamp timestamp;
};

/**
 * Transaction log cache of a single system.
 * Supports transactioned updates.
 * Supported transaction isolation level - read committed.
 * @note Supports multiple concurrent transactions.
 * @note Calls with same tran id are expected to be serialized by caller.
 *     Otherwise, behavior is undefined.
 */
class VmsTransactionLogCache
{
public:
    using TranId = int;

    struct VmsDataState
    {
        /** map<transaction hash, peer which updated transaction> */
        std::map<nx::Buffer, UpdateHistoryData> transactionHashToUpdateAuthor;
        ::ec2::QnTranState transactionState;
        boost::optional<std::uint64_t> timestampSequence;
    };

    static const TranId InvalidTranId = -1;

    VmsTransactionLogCache();

    bool isShouldBeIgnored(
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& tran,
        const QByteArray& hash) const;

    void restoreTransaction(
        ::ec2::ApiPersistentIdData tranStateKey,
        int sequence,
        const nx::Buffer& tranHash,
        std::uint64_t settingsTimestampHi,
        const ::ec2::Timestamp& timestamp);

    TranId beginTran();
    void commit(TranId tranId);
    void rollback(TranId tranId);

    void updateTimestampSequence(TranId tranId, quint64 newValue);
    void insertOrReplaceTransaction(
        TranId tranId,
        const ::ec2::QnAbstractTransaction& transaction,
        const QByteArray& transactionHash);
    /**
     * @return nullptr if tranId is unknown.
     */
    const VmsDataState* state(TranId tranId) const;
    ::ec2::Timestamp generateTransactionTimestamp(TranId tranId);
    int generateTransactionSequence(const ::ec2::ApiPersistentIdData& tranStateKey);
    void shiftTransactionSequence(const ::ec2::ApiPersistentIdData& tranStateKey, int delta);

    ::ec2::QnTranState committedTransactionState() const;
    std::uint64_t committedTimestampSequence() const;

private:
    struct TranContext
    {
        VmsDataState data;
    };

    ::ec2::Timestamp m_maxTimestamp;
    std::map<TranId, TranContext> m_tranIdToContext;
    mutable QnMutex m_mutex;
    TranId m_tranIdSequence;
    TransactionTimestampCalculator m_timestampCalculator;
    VmsDataState m_committedData;
    /** map<peer, transport sequence> */
    //std::map<::ec2::QnTranStateKey, int> m_lastTransportSeq;

    std::uint64_t timestampSequence(const QnMutexLockerBase& /*lock*/, TranId tranId) const;

    TranContext& findTranContext(TranId tranId);
    TranContext& findTranContext(const QnMutexLockerBase& lock, TranId tranId);
    const TranContext* findTranContext(TranId tranId) const;
    const TranContext* findTranContext(
        const QnMutexLockerBase& lock,
        TranId tranId) const;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
