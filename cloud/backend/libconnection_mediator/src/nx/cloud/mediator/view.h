#pragma once

#include "http/get_listening_peer_list_handler.h"
#include "http/http_server.h"
#include "stun_server.h"
#include "server/redirecting_hole_punching_processor.h"

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
    void stop();

    const http::Server& httpServer() const;
    http::Server& httpServer();

    const StunServer& stunServer() const;

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

    // Used to override HolePunchingProcessor::connect().
    // Move ownership of this object into Controller after registerStunApiHandlers above are
    // made non static.
    RedirectingHolePunchingProcessor m_redirectingHolePunchingProcessor;

    void registerStunApiHandlers(Controller* controller);

    /**
     * Overrides HolePunchingProcessor::connect() with
     * RedirectingHolePunchingProcessor::connect().
     */
    void registerRedirectingHolePunchingProcessor(
        RedirectingHolePunchingProcessor* redirectingHolePunchingProcessor,
        network::stun::MessageDispatcher* dispatcher);
};

} // namespace hpm
} // namespace nx
