// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/message.h>

#include "rendezvous_connector.h"

namespace nx::network::cloud::udp {

/**
 * Extends RendezvousConnector functionality by adding connection id verification.
 */
class RendezvousConnectorWithVerification:
    public RendezvousConnector
{
public:
    RendezvousConnectorWithVerification(
        std::string connectSessionId,
        SocketAddress remotePeerAddress,
        std::unique_ptr<AbstractDatagramSocket> udpSocket);

    RendezvousConnectorWithVerification(
        std::string connectSessionId,
        SocketAddress remotePeerAddress,
        SocketAddress localAddressToBindTo);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * @param completionHandler Success is returned only if remote side is aware of connection id.
     */
    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler completionHandler) override;
    virtual std::unique_ptr<nx::network::UdtStreamSocket> takeConnection() override;

    void notifyAboutChoosingConnection(ConnectCompletionHandler completionHandler);

private:
    std::chrono::milliseconds m_timeout{0};
    ConnectCompletionHandler m_connectCompletionHandler;
    std::unique_ptr<stun::MessagePipeline> m_requestPipeline;
    std::unique_ptr<nx::network::UdtStreamSocket> m_udtConnection;

    void onConnectionClosed(SystemError::ErrorCode closeReason, bool /*connectionDestroyed*/);

    void onConnectCompleted(SystemError::ErrorCode errorCode);
    void onMessageReceived(nx::network::stun::Message message);
    void processUdpHolePunchingSynAck(nx::network::stun::Message message);
    void processTunnelConnectionChosen(nx::network::stun::Message message);
    void onTimeout(std::string requestName);
    void processError(SystemError::ErrorCode errorCode);
};

} // namespace nx::network::cloud::udp
