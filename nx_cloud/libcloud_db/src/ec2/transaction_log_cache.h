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
    ::ec2::QnTranStateKey updatedBy;
    ::ec2::Timestamp timestamp;
};

/**
 * Supports transactioned updates.
 * Supported transaction isolation level - read committed.
 * @note Supports multiple concurrent transactions.
 * @note Calls with same tran id are expected to be serialized be caller. 
 * Otherwise, behavior is undefined.
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

    nx::String systemId;
    /** map<peer, transport sequence> */
    std::map<::ec2::QnTranStateKey, int> lastTransportSeq;
    VmsDataState committedData;
    std::unique_ptr<TransactionTimestampCalculator> timestampCalculator;

    VmsTransactionLogCache();

    bool isShouldBeIgnored(
        const nx::String& systemId,
        const ::ec2::QnAbstractTransaction& tran,
        const QByteArray& hash) const;

    void restoreTransaction(
        ::ec2::QnTranStateKey tranStateKey,
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
    const VmsDataState* getState(TranId tranId) const;

    ::ec2::Timestamp generateNewTransactionTimestamp(TranId tranId);

private:
    struct TranContext
    {
        VmsDataState data;
    };

    ::ec2::Timestamp m_maxTimestamp;
    std::map<TranId, TranContext> m_tranIdToContext;
    mutable QnMutex m_mutex;
    TranId m_tranIdSequence;

    std::uint64_t timestampSequence(TranId tranId) const;

    TranContext& getTranContext(TranId tranId);
    TranContext& getTranContext(const QnMutexLockerBase& lock, TranId tranId);
    const TranContext* getTranContext(TranId tranId) const;
    const TranContext* getTranContext(
        const QnMutexLockerBase& lock,
        TranId tranId) const;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
