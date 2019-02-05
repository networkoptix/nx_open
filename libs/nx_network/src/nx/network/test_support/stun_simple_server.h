#pragma once

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/stream_socket_server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::stun::test {

class NX_NETWORK_API SimpleServer:
    public stun::SocketServer
{
    using base_type = stun::SocketServer;

public:
    SimpleServer();
    ~SimpleServer();

    nx::utils::Url url() const;

    nx::network::stun::MessageDispatcher& dispatcher();

    void sendIndicationThroughEveryConnection(nx::network::stun::Message message);

private:
    nx::network::stun::MessageDispatcher m_dispatcher;
};

} // namespace nx::network::stun::test
