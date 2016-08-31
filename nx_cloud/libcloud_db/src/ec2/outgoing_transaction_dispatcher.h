#pragma once

#include <nx/network/buffer.h>

#include <transaction/transaction.h>

#include "transaction_serializer.h"
#include "transaction_transport_header.h"

namespace nx {
namespace cdb {
namespace ec2 {

class ConnectionManager;

/**
 * Dispatches transactions that has to be sent to other peers.
 */
class OutgoingTransactionDispatcher
{
public:
    OutgoingTransactionDispatcher(ConnectionManager* const connectionManager);

    void dispatchTransaction(
        const nx::String& systemId,
        std::unique_ptr<TransactionSerializer> transactionSerializer);

private:
    ConnectionManager* const m_connectionManager;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
