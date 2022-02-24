// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <nx/network/socket_common.h>
#include <nx/utils/counter.h>

#include "abstract_server_connection.h"

namespace nx {
namespace network {
namespace stun {

class UdpServer;

/**
 * Provides ability to send response to a request message received via UDP.
 */
class UDPMessageResponseSender:
    public nx::network::stun::AbstractServerConnection
{
public:
    UDPMessageResponseSender(
        UdpServer* udpServer,
        std::shared_ptr<nx::utils::Counter> activeRequestCounter,
        SocketAddress sourceAddress);
    virtual ~UDPMessageResponseSender() = default;

    virtual void sendMessage(
        nx::network::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler) override;

    virtual nx::network::TransportProtocol transportProtocol() const override;
    virtual SocketAddress getSourceAddress() const override;

    virtual void addOnConnectionCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    virtual AbstractCommunicatingSocket* socket() override;
    virtual void close() override;
    virtual void setInactivityTimeout(std::optional<std::chrono::milliseconds> value) override;

private:
    UdpServer* m_udpServer;
    std::shared_ptr<nx::utils::Counter> m_activeRequestCounter;
    SocketAddress m_sourceAddress;
    nx::utils::Counter::ScopedIncrement m_scopedIncrement;
};

} // namespace stun
} // namespace network
} // namespace nx
