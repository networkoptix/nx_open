// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <nx/network/aio/timer.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/cloud/mediator_client_connections.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/elapsed_timer.h>

#include "../abstract_tunnel_connector.h"

namespace nx::network::cloud::udp {

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
        std::string connectSessionId,
        std::unique_ptr<AbstractDatagramSocket> udpSocket);
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
    struct ConnectorContext
    {
        std::unique_ptr<RendezvousConnectorWithVerification> connector;
        TunnelConnectStatistics stats;
        nx::utils::ElapsedTimer responseTimer;
    };

    const AddressEntry m_targetHostAddress;
    const std::string m_connectSessionId;
    std::unique_ptr<AbstractDatagramSocket> m_udpSocket;
    ConnectCompletionHandler m_completionHandler;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    std::deque<ConnectorContext> m_rendezvousConnectors;
    SocketAddress m_localAddress;
    std::optional<ConnectorContext> m_chosenConnectorCtx;
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
        SystemError::ErrorCode sysErrorCode,
        std::optional<ConnectorContext> connectorCtx);
    ConnectorContext createRendezvousConnector(SocketAddress endpoint);
};

} // namespace nx::network::cloud::udp
