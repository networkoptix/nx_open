#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/fixed_size_message_pipeline.h>

#include "../abstract_transaction_transport.h"

namespace nx::data_sync_engine::transport {

class HttpTunnelTransportConnection:
    public AbstractTransactionTransport
{
    using base_type = AbstractTransactionTransport;

public:
    HttpTunnelTransportConnection(
        const std::string& connectionId,
        std::unique_ptr<network::AbstractStreamSocket> tunnelConnection,
        int protocolVersion);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual ConnectionClosedSubscription& connectionClosedSubscription() override;

    virtual void setOnGotTransaction(GotTransactionEventHandler handler) override;

    virtual QnUuid connectionGuid() const override;

    virtual const TransactionTransportHeader& commonTransportHeaderOfRemoteTransaction() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer) override;

    virtual void start() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const std::string m_connectionId;
    const network::SocketAddress m_remoteEndpoint;
    TransactionTransportHeader m_commonTransportHeader;
    nx::network::server::FixedSizeMessagePipeline m_messagePipeline;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    GotTransactionEventHandler m_gotTransactionEventHandler;
    ConnectionClosedSubscription m_connectionClosedSubscription;

    void processMessage(nx::network::server::FixedSizeMessage message);
};

} // namespace nx::data_sync_engine::transport
