#pragma once

#include <memory>

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "discovery/discovery_http_server.h"

namespace nx {
namespace hpm {

namespace conf { class Settings; }
namespace stats { class Provider; }

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
    std::vector<network::SocketAddress> sslEndpoints() const;
    const nx::network::http::server::MultiEndpointAcceptor& server() const;

    void registerStatisticsApiHandlers(const stats::Provider&);

private:
    const conf::Settings& m_settings;
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::server::MultiEndpointAcceptor m_multiAddressHttpServer;
    std::unique_ptr<nx::cloud::discovery::HttpServer> m_discoveryHttpServer;
    nx::cloud::discovery::RegisteredPeerPool* m_registeredPeerPool = nullptr;

    void loadSslCertificate();

    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool);

    template<typename Handler, typename Arg>
    void registerApiHandler(
        const char* path,
        const nx::network::http::StringType& method,
        Arg arg);
};

} // namespace http
} // namespace hpm
} // namespace nx
