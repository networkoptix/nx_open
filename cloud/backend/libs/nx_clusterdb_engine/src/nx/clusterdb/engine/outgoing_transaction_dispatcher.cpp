#include "outgoing_transaction_dispatcher.h"

namespace nx::clusterdb::engine {

OutgoingCommandDispatcher::OutgoingCommandDispatcher(
    const OutgoingCommandFilter& filter)
    :
    m_filter(filter)
{
}

void OutgoingCommandDispatcher::dispatchTransaction(
    const std::string& systemId,
    std::shared_ptr<const SerializableAbstractCommand> transactionSerializer)
{
    if (!m_filter.satisfies(transactionSerializer->header()))
    {
        NX_VERBOSE(this, "Ignoring command %1",
            engine::toString(transactionSerializer->header()));
        return;
    }

    m_onNewTransactionSubscription.notify(
        systemId,
        std::move(transactionSerializer));
}

OutgoingCommandDispatcher::OnNewTransactionSubscription&
    OutgoingCommandDispatcher::onNewTransactionSubscription()
{
    return m_onNewTransactionSubscription;
}

} // namespace nx::clusterdb::engine
