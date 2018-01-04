#pragma once

#include "http/get_listening_peer_list_handler.h"
#include "http/http_server.h"
#include "stun_server.h"

namespace nx {

namespace network { namespace stun { class MessageDispatcher; } }

namespace hpm {

namespace conf { class Settings; }
class Controller;
class HolePunchingProcessor;
class MediaserverEndpointTesterBase;
class PeerRegistrator;

class View
{
public:
    View(
        const conf::Settings& settings,
        Controller* controller);

    void start();

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> stunEndpoints() const;

    // TODO: #ak Make these private non-static after refactoring stun_ut.cpp.
    static void registerStunApiHandlers(
        MediaserverEndpointTesterBase* mediaserverEndpointTester,
        network::stun::MessageDispatcher* dispatcher);

    static void registerStunApiHandlers(
        PeerRegistrator* peerRegistrator,
        network::stun::MessageDispatcher* dispatcher);

    static void registerStunApiHandlers(
        HolePunchingProcessor* holePunchingProcessor,
        network::stun::MessageDispatcher* dispatcher);

private:
    http::Server m_httpServer;
    StunServer m_stunServer;

    void registerStunApiHandlers(Controller* controller);
};

} // namespace hpm
} // namespace nx
