#ifndef MEDIATOR_CONNECTIONS_H
#define MEDIATOR_CONNECTIONS_H

#include <nx/network/stun/async_client_user.h>

namespace nx {
namespace cc {

// Forward
class MediatorConnector;

/** Provides client related STUN functionality */
class MediatorClientConnection
    : public stun::AsyncClientUser
{
    MediatorClientConnection(std::shared_ptr<stun::AsyncClient> client);
    friend class MediatorConnector;

public:
    void connect(String host, std::function<void(std::list<SocketAddress>)> handler);
};

/** Provides system related STUN functionality */
class MediatorSystemConnection
    : public stun::AsyncClientUser
{
    MediatorSystemConnection(std::shared_ptr<stun::AsyncClient> client,
                             MediatorConnector* connector);
    friend class MediatorConnector;

public:
    void ping(std::list<SocketAddress> addresses,
              std::function<void(bool, std::list<SocketAddress>)> handler);

    void bind(std::list<SocketAddress> addresses,
              std::function<void(bool)> handler);

private:
    void sendAuthRequest(stun::Message request,
                         stun::AsyncClient::RequestHandler handler);

    MediatorConnector* m_connector;
};

} // namespase cc
} // namespase nx

#endif // MEDIATOR_CONNECTIONS_H
