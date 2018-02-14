#include "http_server.h"

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "get_listening_peer_list_handler.h"
#include "../controller.h"

namespace nx {
namespace hpm {
namespace http {

Server::Server(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
    :
    m_settings(settings)
{
    NX_ASSERT(!m_settings.http().addrToListenList.empty());

    if (!launchHttpServerIfNeeded(m_settings, peerRegistrator, registeredPeerPool))
        throw std::runtime_error("Failed to initialize http server");
}

void Server::listen()
{
    if (!m_multiAddressHttpServer->listen())
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        throw std::runtime_error(
            lm("Error listening on HTTP addresses %1. %2")
                .arg(containerString(m_settings.http().addrToListenList))
                .arg(SystemError::toString(osErrorCode)).toStdString());
    }

    NX_ALWAYS(this, lm("HTTP server is listening on %1")
        .args(containerString(m_multiAddressHttpServer->endpoints())));
}

void Server::stopAcceptingNewRequests()
{
    m_multiAddressHttpServer->pleaseStopSync();
}

nx::network::http::server::rest::MessageDispatcher& Server::messageDispatcher()
{
    return *m_httpMessageDispatcher;
}

std::vector<network::SocketAddress> Server::endpoints() const
{
    return m_endpoints;
}

bool Server::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
{
    NX_LOGX("Bringing up HTTP server", cl_logINFO);

    m_httpMessageDispatcher = std::make_unique<nx::network::http::server::rest::MessageDispatcher>();

    // Registering HTTP handlers.
    m_httpMessageDispatcher->registerRequestProcessor<http::GetListeningPeerListHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, GetListeningPeerListHandler::kHandlerPath),
        [&]() { return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator); });

    m_multiAddressHttpServer =
        std::make_unique<nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>>(
            nullptr, //< TODO: #ak Add authentication.
            m_httpMessageDispatcher.get(),
            /*ssl required*/ false,
            nx::network::NatTraversalSupport::disabled);

    if (!m_multiAddressHttpServer->bind(settings.http().addrToListenList))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to bind HTTP server to address ... . %1")
            .arg(SystemError::toString(osErrorCode)), cl_logERROR);
        return false;
    }

    m_endpoints = m_multiAddressHttpServer->endpoints();
    m_multiAddressHttpServer->forEachListener(
        [&settings](nx::network::http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
        });

    m_discoveryHttpServer = std::make_unique<nx::cloud::discovery::HttpServer>(
        m_httpMessageDispatcher.get(),
        registeredPeerPool);

    return true;
}

} // namespace http
} // namespace hpm
} // namespace nx
