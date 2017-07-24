#pragma once

#include <memory>

#include <nx/network/websocket/websocket.h>

#include "abstract_transaction_transport.h"

namespace nx {
namespace cdb {
namespace ec2 {

class WebSocketTransactionTransport:
    public AbstractTransactionTransport
{
public:
    WebSocketTransactionTransport(
        std::unique_ptr<network::websocket::WebSocket> webSocket);

    virtual SocketAddress remoteSocketAddr() const override;
    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;
    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;
    virtual QnUuid connectionGuid() const override;
    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;
    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

private:
    TransactionTransportHeader m_commonTransactionHeader;
    std::unique_ptr<network::websocket::WebSocket> m_webSocket;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
