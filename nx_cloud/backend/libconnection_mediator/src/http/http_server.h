#pragma once

#include <memory>

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "discovery/discovery_http_server.h"
#include "server/hole_punching_processor.h"

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
        nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool,
        HolePunchingProcessor* holePunchingProcessor);

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
    HolePunchingProcessor* m_holePunchingProcessor = nullptr;

    void loadSslCertificate();

    bool launchHttpServerIfNeeded(
        const conf::Settings& settings,
        const PeerRegistrator& peerRegistrator,
        nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool);

    void registerApiHandlers(const PeerRegistrator& peerRegistrator);

    template<typename Handler, typename Arg>
    void registerApiHandler(
        const char* path,
        const nx::network::http::StringType& method,
        Arg arg);
};

//-------------------------------------------------------------------------------------------------

class InitiateConnectionRequestHandler:
    public nx::network::http::AbstractFusionRequestHandler<
        api::ConnectRequest, api::ConnectResponse>
{
public:
    InitiateConnectionRequestHandler(
        HolePunchingProcessor* holePunchingProcessor);

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        api::ConnectRequest inputData) override;

private:
    HolePunchingProcessor* m_holePunchingProcessor = nullptr;

    void reportResult(
        api::ResultCode resultCode,
        api::ConnectResponse response);
};

} // namespace http
} // namespace hpm
} // namespace nx
