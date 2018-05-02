#include "outgoing_transaction_dispatcher.h"

namespace nx {
namespace cdb {
namespace ec2 {

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

OutgoingTransactionDispatcher::OnNewTransactionSubscription*
    OutgoingTransactionDispatcher::onNewTransactionSubscription()
{
    return &m_onNewTransactionSubscription;
}

} // namespace ec2
} // namespace cdb
} // namespace nx
