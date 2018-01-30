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
} // namespace network
} // namespace nx
