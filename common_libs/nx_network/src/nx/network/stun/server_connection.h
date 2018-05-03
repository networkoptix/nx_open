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

namespace nx {
namespace network {
namespace stun {

class MessageDispatcher;

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

    ServerConnection(
        nx::network::server::StreamConnectionHolder<ServerConnection>* socketServer,
        std::unique_ptr<AbstractStreamSocket> sock,
        const MessageDispatcher& dispatcher);
    ~ServerConnection();

    virtual void sendMessage(
        nx::network::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler) override;
    virtual nx::network::TransportProtocol transportProtocol() const override;
    virtual SocketAddress getSourceAddress() const override;
    virtual void addOnConnectionCloseHandler(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual AbstractCommunicatingSocket* socket() override;
    virtual void close() override;
    virtual void setInactivityTimeout(boost::optional<std::chrono::milliseconds> value) override;

    void setDestructHandler(std::function< void() > handler = nullptr);

protected:
    virtual void processMessage(Message message) override;

private:
    void processBindingRequest(Message message);
    void processCustomRequest(Message message);

private:
    const SocketAddress m_peerAddress;

    QnMutex m_mutex;
    std::function< void() > m_destructHandler;
    const MessageDispatcher& m_dispatcher;
};

} // namespace stun
} // namespace network
} // namespace nx
