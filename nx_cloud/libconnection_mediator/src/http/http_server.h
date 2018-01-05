#pragma once

#include <memory>

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/connection_server/multi_address_server.h>

#include "discovery/discovery_http_server.h"

namespace nx {
namespace hpm {

namespace conf {

class Settings;

} // namespace conf

class PeerRegistrator;

namespace http {

class Server
{
public:
    Server(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool);

    void listen();
    void stopAcceptingNewRequests();

    nx::network::http::server::rest::MessageDispatcher& messageDispatcher();
    std::vector<network::SocketAddress> endpoints() const;

private:
    const conf::Settings& m_settings;
    std::unique_ptr<nx::network::http::server::rest::MessageDispatcher> m_httpMessageDispatcher;
    std::unique_ptr<nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>>
        m_multiAddressHttpServer;
    std::unique_ptr<nx::cloud::discovery::HttpServer> m_discoveryHttpServer;
    nx::cloud::discovery::RegisteredPeerPool* m_registeredPeerPool;
    std::vector<network::SocketAddress> m_endpoints;

    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool);
};

} // namespace http
} // namespace hpm
} // namespace nx
