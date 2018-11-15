#include "outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine {

static const QnUuid kCdbGuid("{674bafd7-4eec-4bba-84aa-a1baea7fc6db}");

OutgoingTransactionDispatcher::OutgoingTransactionDispatcher(
    const OutgoingCommandFilter& filter)
    :
    m_filter(filter)
{
}

void OutgoingTransactionDispatcher::dispatchTransaction(
    const std::string& systemId,
    std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer)
{
    if (!m_filter.satisfies(transactionSerializer->header()))
        return;

    m_onNewTransactionSubscription.notify(
        systemId,
        std::move(transactionSerializer));
}

OutgoingTransactionDispatcher::OnNewTransactionSubscription&
    OutgoingTransactionDispatcher::onNewTransactionSubscription()
{
    return m_onNewTransactionSubscription;
}

} // namespace nx::clusterdb::engine
