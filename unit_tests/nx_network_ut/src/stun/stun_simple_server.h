#pragma once

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>

namespace nx {
namespace stun {
namespace test {

class SimpleServer:
    public stun::SocketServer
{
    using base_type = stun::SocketServer;

public:
    SimpleServer():
        base_type(&m_dispatcher, false)
    {
    }

    ~SimpleServer()
    {
        pleaseStopSync();
    }

    QUrl getServerUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::stun::kUrlSchemeName).setEndpoint(address());
    }

    nx::stun::MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    void sendIndicationThroughEveryConnection(nx::stun::Message message)
    {
        forEachConnection(
            nx::network::server::MessageSender<nx::stun::ServerConnection>(std::move(message)));
    }

private:
    nx::stun::MessageDispatcher m_dispatcher;
};

} // namespace test
} // namespace stun
} // namespace nx
