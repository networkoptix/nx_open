#include "outgoing_transaction_dispatcher.h"

namespace nx {
namespace data_sync_engine {

OutgoingTransactionDispatcher::OutgoingTransactionDispatcher()
{
}

void OutgoingTransactionDispatcher::dispatchTransaction(
    const nx::String& systemId,
    std::shared_ptr<const SerializableAbstractTransaction> transactionSerializer)
{
    m_onNewTransactionSubscription.notify(
        systemId,
        std::move(transactionSerializer));
}

OutgoingTransactionDispatcher::OnNewTransactionSubscription&
    OutgoingTransactionDispatcher::onNewTransactionSubscription()
{
    return m_onNewTransactionSubscription;
}

} // namespace data_sync_engine
} // namespace nx
