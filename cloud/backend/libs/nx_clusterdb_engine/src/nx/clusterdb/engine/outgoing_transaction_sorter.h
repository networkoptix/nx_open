#pragma once

#include <atomic>
#include <map>
#include <memory>

#include <nx/network/aio/async_call.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/thread/mutex.h>

#include "serialization/serializable_transaction.h"
#include "transaction_log_cache.h"

namespace nx::clusterdb::engine {

class AbstractOutgoingCommandDispatcher;

/**
 * Postpones outgoing transaction delivery to ensure transactions are sent with
 *     monotonically increasing sequence.
 */
class NX_DATA_SYNC_ENGINE_API OutgoingTransactionSorter
{
public:
    using TransactionSequence = decltype(CommandHeader::PersistentInfo::sequence);

    OutgoingTransactionSorter(
        const std::string& systemId,
        CommandLogCache* vmsTransactionLogCache,
        AbstractOutgoingCommandDispatcher* const outgoingTransactionDispatcher);

    ~OutgoingTransactionSorter();

    /**
     * This optional method can be called prior to adding transaction to ensure
     * that transactionSequence is used in sort order calculations even before
     * actual OutgoingTransactionSorter::addTransaction call.
     */
    void registerTransactionSequence(
        CommandLogCache::TranId cacheTranId,
        TransactionSequence transactionSequence);

    void addTransaction(
        CommandLogCache::TranId cacheTranId,
        std::unique_ptr<const SerializableAbstractCommand> transaction);

    void rollback(CommandLogCache::TranId cacheTranId);
    void commit(CommandLogCache::TranId cacheTranId);

protected:
    std::size_t transactionsCommitted() const;
    std::size_t transactionsDelivered() const;

private:
    struct TranContext
    {
        std::vector<std::unique_ptr<const SerializableAbstractCommand>> transactions;
        bool isCommitted = false;
        TransactionSequence maxSequence = 0;
    };

    const std::string m_systemId;
    CommandLogCache* m_vmsTransactionLogCache;
    AbstractOutgoingCommandDispatcher* const m_outgoingTransactionDispatcher;
    std::map<CommandLogCache::TranId, TranContext> m_transactions;
    std::map<TransactionSequence, CommandLogCache::TranId> m_transactionsBySequence;
    QnMutex m_mutex;
    std::size_t m_transactionsCommitted;
    std::size_t m_transactionsDelivered;
    std::unique_ptr<nx::network::aio::AsyncCall> m_asyncCall;

    void dispatchTransactions();

    std::vector<CommandLogCache::TranId> findTransactionsToSend(
        const QnMutexLockerBase& /*lock*/);

    void dispatchTransactions(
        const QnMutexLockerBase& /*lock*/,
        CommandLogCache::TranId tranId);
    void deliverTransactions(
        std::vector<std::unique_ptr<const SerializableAbstractCommand>> transactions);

    void registerTransactionSequence(
        const QnMutexLockerBase& /*lock*/,
        CommandLogCache::TranId cacheTranId,
        TransactionSequence transactionSequence);
};

} // namespace nx::clusterdb::engine
