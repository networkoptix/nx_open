// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/network/async_stoppable.h>
#include <nx/network/stun/message.h>
#include <nx/network/stun/message_parser.h>
#include <nx/network/stun/message_serializer.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/utils/thread/mutex.h>

#include "abstract_server_connection.h"

namespace nx::network::stun {

class AbstractMessageHandler;

class NX_NETWORK_API ServerConnection:
    public nx::network::server::BaseStreamProtocolConnection<
        stun::ServerConnection,
        Message,
        MessageParser,
        MessageSerializer>,
    public AbstractServerConnection,
    public std::enable_shared_from_this<ServerConnection>
{
public:
    using base_type = nx::network::server::BaseStreamProtocolConnection<
        ServerConnection,
        Message,
        MessageParser,
        MessageSerializer>;

    /**
     * @param attrs These will be passed along with every message.
     */
    ServerConnection(
        std::unique_ptr<AbstractStreamSocket> sock,
        AbstractMessageHandler* messageHandler,
        nx::utils::stree::StringAttrDict attrs = {});

    ~ServerConnection();

    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

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

    void setDestructHandler(std::function< void() > handler = nullptr);

protected:
    virtual void processMessage(Message message) override;

private:
    void processBindingRequest(Message message);

private:
    const SocketAddress m_peerAddress;

    nx::Mutex m_mutex;
    std::function< void() > m_destructHandler;
    AbstractMessageHandler* m_messageHandler = nullptr;
    const nx::utils::stree::StringAttrDict m_attrs;
};

} // namespace nx::network::stun
