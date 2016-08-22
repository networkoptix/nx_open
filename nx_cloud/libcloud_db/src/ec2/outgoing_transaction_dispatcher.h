/**********************************************************
* Aug 19, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/buffer.h>

#include <transaction/transaction.h>

#include "connection_manager.h"
#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

class ConnectionManager;

/** Dispatches transactions that need to be sent to other peers */
class OutgoingTransactionDispatcher
{
public:
    OutgoingTransactionDispatcher(ConnectionManager* const connectionManager)
    :
        m_connectionManager(connectionManager)
    {
    }

    template<class T>
    void dispatchTransaction(
        const nx::String& systemId,
        ::ec2::QnTransaction<T> transaction,
        TransactionTransportHeader transportHeader)
    {
        //TODO with such implementation OutgoingTransactionDispatcher class is redundant
        m_connectionManager->sendTransaction(
            systemId,
            std::move(transaction),
            std::move(transportHeader));
    }

private:
    ConnectionManager* const m_connectionManager;
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
