#include "outgoing_transaction_dispatcher.h"

#include "connection_manager.h"

namespace nx {
namespace cdb {
namespace ec2 {

OutgoingTransactionDispatcher::OutgoingTransactionDispatcher(
    ConnectionManager* const connectionManager)
:
    m_connectionManager(connectionManager)
{
}

void OutgoingTransactionDispatcher::dispatchTransaction(
    const nx::String& systemId,
    std::unique_ptr<TransactionSerializer> transactionSerializer)
{
    m_connectionManager->dispatchTransaction(
        systemId,
        std::move(transactionSerializer));
}

} // namespace ec2
} // namespace cdb
} // namespace nx
