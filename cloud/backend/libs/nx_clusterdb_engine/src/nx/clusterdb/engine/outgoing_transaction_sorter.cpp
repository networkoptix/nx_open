#include "outgoing_transaction_sorter.h"

#include <set>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine {

OutgoingTransactionSorter::OutgoingTransactionSorter(
    const std::string& systemId,
    CommandLogCache* vmsTransactionLogCache,
    AbstractOutgoingCommandDispatcher* const outgoingTransactionDispatcher)
:
    m_systemId(systemId),
    m_vmsTransactionLogCache(vmsTransactionLogCache),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_transactionsCommitted(0),
    m_transactionsDelivered(0),
    m_asyncCall(std::make_unique<nx::network::aio::AsyncCall>())
{
}

OutgoingTransactionSorter::~OutgoingTransactionSorter()
{
    // Freeing m_asyncCall first of all to be sure call is not running in a separate thread.
    m_asyncCall.reset();
}

void OutgoingTransactionSorter::registerTransactionSequence(
    CommandLogCache::TranId cacheTranId,
    OutgoingTransactionSorter::TransactionSequence transactionSequence)
{
    QnMutexLocker lock(&m_mutex);
    registerTransactionSequence(lock, cacheTranId, transactionSequence);
}

void OutgoingTransactionSorter::addTransaction(
    CommandLogCache::TranId cacheTranId,
    std::unique_ptr<const SerializableAbstractCommand> transaction)
{
    QnMutexLocker lock(&m_mutex);
    const auto sequence = transaction->header().persistentInfo.sequence;
    m_transactions[cacheTranId].transactions.push_back(std::move(transaction));
    registerTransactionSequence(
        lock,
        cacheTranId,
        sequence);
}

void OutgoingTransactionSorter::rollback(CommandLogCache::TranId cacheTranId)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_transactions.erase(cacheTranId);

        for (auto it = m_transactionsBySequence.begin();
            it != m_transactionsBySequence.end();
            )
        {
            if (it->second == cacheTranId)
                it = m_transactionsBySequence.erase(it);
            else
                ++it;
        }
    }

    m_vmsTransactionLogCache->rollback(cacheTranId);

    dispatchTransactions();
}

void OutgoingTransactionSorter::commit(CommandLogCache::TranId cacheTranId)
{
    // TranContext tranContext;
    QnMutexLocker lock(&m_mutex);
    auto it = m_transactions.find(cacheTranId);
    if (it == m_transactions.end())
    {
        // No transactions were added.
        lock.unlock();
        m_vmsTransactionLogCache->commit(cacheTranId);
        return;
    }

    it->second.isCommitted = true;
    lock.unlock();

    dispatchTransactions();
}

std::size_t OutgoingTransactionSorter::transactionsCommitted() const
{
    return m_transactionsCommitted;
}

std::size_t OutgoingTransactionSorter::transactionsDelivered() const
{
    return m_transactionsDelivered;
}

void OutgoingTransactionSorter::dispatchTransactions()
{
    QnMutexLocker lock(&m_mutex);

    for (;;)
    {
        std::vector<CommandLogCache::TranId> tranToCommitIds = findTransactionsToSend(lock);
        if (tranToCommitIds.empty())
            return;

        for(const auto& tranId: tranToCommitIds)
            dispatchTransactions(lock, tranId);
    }
}

std::vector<CommandLogCache::TranId> OutgoingTransactionSorter::findTransactionsToSend(
    const QnMutexLockerBase& /*lock*/)
{
    std::vector<CommandLogCache::TranId> completedTranIds;
    std::set<CommandLogCache::TranId> tranIdsNotFullyChecked;

    for (auto it = m_transactionsBySequence.begin();
        it != m_transactionsBySequence.end();
        ++it)
    {
        const auto transactionSequence = it->first;
        const auto tranId = it->second;
        const TranContext& tranContext = m_transactions[tranId];

        if (!tranContext.isCommitted)
            return {};

        if (transactionSequence != tranContext.maxSequence)
        {
            NX_ASSERT(transactionSequence < tranContext.maxSequence);
            tranIdsNotFullyChecked.insert(tranId);
            continue;
        }

        tranIdsNotFullyChecked.erase(tranId);
        completedTranIds.push_back(tranId);

        if (tranIdsNotFullyChecked.empty())
            return completedTranIds;
    }

    return {};
}

void OutgoingTransactionSorter::dispatchTransactions(
    const QnMutexLockerBase& /*lock*/,
    CommandLogCache::TranId tranId)
{
    TranContext tranContext;
    auto it = m_transactions.find(tranId);
    NX_ASSERT(it != m_transactions.end());
    if (it == m_transactions.end())
        return;
    tranContext = std::move(it->second);
    m_transactions.erase(it);

    for (const auto& transaction: tranContext.transactions)
        m_transactionsBySequence.erase(transaction->header().persistentInfo.sequence);

    m_vmsTransactionLogCache->commit(tranId);

    deliverTransactions(std::move(tranContext.transactions));
}

void OutgoingTransactionSorter::deliverTransactions(
    std::vector<std::unique_ptr<const SerializableAbstractCommand>> transactions)
{
    // We MUST ensure transactions are delivered to m_outgoingTransactionDispatcher in a correct order.
    // If we just release mutex here, order will be undefined.
    // Delivering with mutex locked is not good either.

    m_transactionsCommitted += transactions.size();

    m_asyncCall->post(
        [this, transactions = std::move(transactions)]() mutable
        {
            for (auto& transaction: transactions)
            {
                m_outgoingTransactionDispatcher->dispatchTransaction(
                    m_systemId,
                    std::move(transaction));
            }

            m_transactionsDelivered += transactions.size();
        });
}

void OutgoingTransactionSorter::registerTransactionSequence(
    const QnMutexLockerBase& /*lock*/,
    CommandLogCache::TranId cacheTranId,
    TransactionSequence transactionSequence)
{
    TranContext& tranContext = m_transactions[cacheTranId];

    const auto insertionPair = m_transactionsBySequence.emplace(transactionSequence, cacheTranId);
    if (!insertionPair.second)
    {
        // Transaction sequence is already registered.
        NX_ASSERT(tranContext.maxSequence >= transactionSequence);
    }
    else
    {
        NX_ASSERT(tranContext.maxSequence < transactionSequence);
        if (tranContext.maxSequence < transactionSequence)
            tranContext.maxSequence = transactionSequence;
    }
}

} // namespace nx::clusterdb::engine
