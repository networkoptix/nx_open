/**********************************************************
* Apr 14, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "rendezvous_connector.h"

#include "nx/network/stun/async_client.h"
#include "nx/network/stun/message.h"


namespace nx {
namespace network {
namespace cloud {
namespace udp {

/** Extends \a RendezvousConnector functionality by adding connection id verification. */
class RendezvousConnectorWithVerification
:
    public RendezvousConnector,
    public StreamConnectionHolder<stun::MessagePipeline>
{
public:
    RendezvousConnectorWithVerification(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<nx::network::UDPSocket> udpSocket);
    RendezvousConnectorWithVerification(
        nx::String connectSessionId,
        SocketAddress remotePeerAddress,
        SocketAddress localAddressToBindTo);
    virtual ~RendezvousConnectorWithVerification();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
        @param completionHandler Success is returned only if remote side is aware of connection id
    */
    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler completionHandler) override;
    virtual std::unique_ptr<nx::network::UdtStreamSocket> takeConnection() override;

private:
    std::chrono::milliseconds m_timeout;
    ConnectCompletionHandler m_connectCompletionHandler;
    std::unique_ptr<stun::MessagePipeline> m_requestPipeline;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        stun::MessagePipeline* connection) override;

    void onConnectCompleted(SystemError::ErrorCode errorCode);
    void onMessageReceived(nx::stun::Message message);
    void onTimeout();
    void processError(SystemError::ErrorCode errorCode);
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
