#pragma once

#include "../abstract_tunnel_connector.h"

#include <functional>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/aio/timer.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/udt/udt_socket.h>

#include "nx/network/cloud/data/connect_data.h"
#include "nx/network/cloud/mediator_client_connections.h"

namespace nx {
namespace network {
namespace cloud {
namespace udp {

class RendezvousConnectorWithVerification;

/**
 * Establishes cross-nat connection to the specified host using UDP hole punching technique.
 * NOTE: One instance can keep only one session
 * NOTE: Can be safely freed within connect handler.
 * Otherwise, TunnelConnector::pleaseStop is required
 */
class NX_NETWORK_API TunnelConnector:
    public AbstractTunnelConnector,
    public stun::UnreliableMessagePipelineEventHandler
{
public:
    /**
        @param mediatorAddress This param is for test only
    */
    TunnelConnector(
        AddressEntry targetHostAddress,
        nx::String connectSessionId,
        std::unique_ptr<nx::network::UDPSocket> udpSocket);
    virtual ~TunnelConnector();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual int getPriority() const override;
    /**
     * Only one connect can be running at a time.
     * @param timeout 0 - no timeout.
     */
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

    SocketAddress localAddress() const;

protected:
    virtual void messageReceived(
        SocketAddress sourceAddress,
        stun::Message msg) override;
    virtual void ioFailure(SystemError::ErrorCode errorCode) override;
    virtual void stopWhileInAioThread() override;

private:
    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionId;
    std::unique_ptr<nx::network::UDPSocket> m_udpSocket;
    ConnectCompletionHandler m_completionHandler;
    std::unique_ptr<UdtStreamSocket> m_udtConnection;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    std::deque<std::unique_ptr<RendezvousConnectorWithVerification>> m_rendezvousConnectors;
    SocketAddress m_localAddress;
    std::unique_ptr<RendezvousConnectorWithVerification> m_chosenRendezvousConnector;
    hpm::api::CloudConnectVersion m_remotePeerCloudConnectVersion;

    void onUdtConnectionEstablished(
        RendezvousConnectorWithVerification* rendezvousConnectorPtr,
        SystemError::ErrorCode errorCode);
    void onHandshakeComplete(SystemError::ErrorCode errorCode);
    /**
     * NOTE: always called within aio thread.
     */
    void holePunchingDone(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode);
    std::unique_ptr<RendezvousConnectorWithVerification>
        createRendezvousConnector(SocketAddress endpoint);
};

} // namespace udp
} // namespace cloud
} // namespace network
} // namespace nx
