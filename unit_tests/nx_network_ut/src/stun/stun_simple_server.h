#pragma once

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/url/url_builder.h>

namespace nx {
namespace network {
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

    nx::utils::Url getServerUrl() const
    {
        return url::Builder()
            .setScheme(stun::kUrlSchemeName).setEndpoint(address());
    }

    MessageDispatcher& dispatcher()
    {
        return m_dispatcher;
    }

    void sendIndicationThroughEveryConnection(Message message)
    {
        forEachConnection(
            nx::network::server::MessageSender<ServerConnection>(std::move(message)));
    }

private:
    MessageDispatcher m_dispatcher;
};

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
