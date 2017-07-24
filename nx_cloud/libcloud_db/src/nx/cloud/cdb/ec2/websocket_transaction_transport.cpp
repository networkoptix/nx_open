#include "websocket_transaction_transport.h"

namespace nx {
namespace cdb {
namespace ec2 {

WebSocketTransactionTransport::WebSocketTransactionTransport(
    std::unique_ptr<network::websocket::WebSocket> webSocket)
    :
    m_webSocket(std::move(webSocket))
{
}

SocketAddress WebSocketTransactionTransport::remoteSocketAddr() const
{
    return SocketAddress();
}

void WebSocketTransactionTransport::setOnConnectionClosed(
    ConnectionClosedEventHandler /*handler*/)
{
}

void WebSocketTransactionTransport::setOnGotTransaction(
    GotTransactionEventHandler /*handler*/)
{
}

QnUuid WebSocketTransactionTransport::connectionGuid() const
{
    return QnUuid();
}

const TransactionTransportHeader& 
    WebSocketTransactionTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransactionHeader;
}

void WebSocketTransactionTransport::sendTransaction(
    TransactionTransportHeader /*transportHeader*/,
    const std::shared_ptr<const SerializableAbstractTransaction>& /*transactionSerializer*/)
{
}

} // namespace ec2
} // namespace cdb
} // namespace nx
