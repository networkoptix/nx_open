#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/fixed_size_message_pipeline.h>

#include "../abstract_transaction_transport.h"
#include "../compatible_ec2_protocol_version.h"

namespace nx::data_sync_engine::transport {

class HttpTunnelTransportConnection:
    public AbstractCommandPipeline
{
    using base_type = AbstractCommandPipeline;

public:
    HttpTunnelTransportConnection(
        const ProtocolVersionRange& protocolVersionRange,
        const ConnectionRequestAttributes& connectionRequestAttributes,
        std::unique_ptr<network::AbstractStreamSocket> tunnelConnection);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;

    virtual void setOnGotTransaction(CommandDataHandler handler) override;

    virtual QnUuid connectionGuid() const override;

    virtual void sendTransaction(
        TransactionTransportHeader transportHeader,
        const std::shared_ptr<const TransactionSerializer>& transactionSerializer) override;

    virtual void closeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    const ConnectionRequestAttributes m_connectionRequestAttributes;
    const network::SocketAddress m_remoteEndpoint;
    nx::network::server::FixedSizeMessagePipeline m_messagePipeline;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    CommandDataHandler m_gotTransactionEventHandler;

    void processMessage(nx::network::server::FixedSizeMessage message);

    int highestProtocolVersionCompatibleWithRemotePeer() const;
};

} // namespace nx::data_sync_engine::transport
