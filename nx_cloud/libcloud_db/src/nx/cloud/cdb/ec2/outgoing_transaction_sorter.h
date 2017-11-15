#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <nx/network/aio/async_call.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>

#include "serialization/serializable_transaction.h"
#include "transaction_log_cache.h"

namespace nx {
namespace cdb {
namespace ec2 {

class AbstractOutgoingTransactionDispatcher;

/**
 * Postpones outgoing transaction delivery to ensure transactions are sent with
 *     monotonically increasing sequence.
 */
class OutgoingTransactionSorter
{
public:
    using TransactionSequence = decltype(::ec2::QnAbstractTransaction::PersistentInfo::sequence);

    OutgoingTransactionSorter(
        const nx::String& systemId,
        VmsTransactionLogCache* vmsTransactionLogCache,
        AbstractOutgoingTransactionDispatcher* const outgoingTransactionDispatcher);

    ~OutgoingTransactionSorter();

    /**
     * This optional method can be called prior to adding transaction to ensure
     * that transactionSequence is used in sort order calculations even before
     * actual OutgoingTransactionSorter::addTransaction call.
     */
    void registerTransactionSequence(
        VmsTransactionLogCache::TranId cacheTranId,
        TransactionSequence transactionSequence);

    void addTransaction(
        VmsTransactionLogCache::TranId cacheTranId,
        std::unique_ptr<const SerializableAbstractTransaction> transaction);

    void rollback(VmsTransactionLogCache::TranId cacheTranId);
    void commit(VmsTransactionLogCache::TranId cacheTranId);

protected:
    std::size_t transactionsCommitted() const;
    std::size_t transactionsDelivered() const;

private:
    struct TranContext
    {
        std::vector<std::unique_ptr<const SerializableAbstractTransaction>> transactions;
        bool isCommitted = false;
        TransactionSequence maxSequence = 0;
    };

    const nx::String m_systemId;
    VmsTransactionLogCache* m_vmsTransactionLogCache;
    AbstractOutgoingTransactionDispatcher* const m_outgoingTransactionDispatcher;
    std::map<VmsTransactionLogCache::TranId, TranContext> m_transactions;
    std::map<TransactionSequence, VmsTransactionLogCache::TranId> m_transactionsBySequence;
    QnMutex m_mutex;
    std::size_t m_transactionsCommitted;
    std::size_t m_transactionsDelivered;
    std::unique_ptr<nx::network::aio::AsyncCall> m_asyncCall;

    void dispatchTransactions();

    std::vector<VmsTransactionLogCache::TranId> findTransactionsToSend(
        const QnMutexLockerBase& /*lock*/);

    void dispatchTransactions(
        const QnMutexLockerBase& /*lock*/,
        VmsTransactionLogCache::TranId tranId);
    void deliverTransactions(
        std::vector<std::unique_ptr<const SerializableAbstractTransaction>> transactions);

    void registerTransactionSequence(
        const QnMutexLockerBase& /*lock*/,
        VmsTransactionLogCache::TranId cacheTranId,
        TransactionSequence transactionSequence);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
