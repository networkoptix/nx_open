#pragma once

#include <memory>
#include <string>

#include <nx/network/connection_server/fixed_size_message_pipeline.h>

#include <nx/vms/api/data/peer_data.h>

#include "../abstract_command_transport.h"
#include "../../compatible_ec2_protocol_version.h"

namespace nx::clusterdb::engine::transport::http_tunnel {

class CommandPipeline:
    public AbstractCommandPipeline
{
    using base_type = AbstractCommandPipeline;

public:
    CommandPipeline(
        const ProtocolVersionRange& protocolVersionRange,
        const ConnectionRequestAttributes& connectionRequestAttributes,
        const std::string& clusterId,
        std::unique_ptr<network::AbstractStreamSocket> tunnelConnection);

    virtual ~CommandPipeline();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual network::SocketAddress remotePeerEndpoint() const override;

    virtual void setOnConnectionClosed(ConnectionClosedEventHandler handler) override;

    virtual void setOnGotTransaction(CommandDataHandler handler) override;

    virtual std::string connectionGuid() const override;

    virtual void sendTransaction(
        CommandTransportHeader transportHeader,
        const std::shared_ptr<const CommandSerializer>& transactionSerializer) override;

    virtual void closeConnection() override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    const ProtocolVersionRange m_protocolVersionRange;
    const ConnectionRequestAttributes m_connectionRequestAttributes;
    const std::string m_clusterId;
    const network::SocketAddress m_remoteEndpoint;
    nx::network::server::FixedSizeMessagePipeline m_messagePipeline;
    ConnectionClosedEventHandler m_connectionClosedEventHandler;
    CommandDataHandler m_gotTransactionEventHandler;
    bool m_closed = false;

    void processMessage(nx::network::server::FixedSizeMessage message);
    int highestProtocolVersionCompatibleWithRemotePeer() const;
    void onConnectionClosed(SystemError::ErrorCode errCode);
};

} // namespace nx::clusterdb::engine::transport::http_tunnel
