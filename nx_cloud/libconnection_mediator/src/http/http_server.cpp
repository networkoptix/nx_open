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
    const PeerRegistrator& peerRegistrator)
    :
    m_settings(settings)
{
    NX_ASSERT(!m_settings.http().addrToListenList.empty());

    if (!launchHttpServerIfNeeded(m_settings, peerRegistrator))
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

    NX_LOGX(lit("HTTP server is listening on %1")
        .arg(containerString(m_settings.http().addrToListenList)), cl_logALWAYS);
}

nx_http::MessageDispatcher& Server::messageDispatcher()
{
    return *m_httpMessageDispatcher;
}

bool Server::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator)
{
    NX_LOGX("Bringing up HTTP server", cl_logINFO);

    m_httpMessageDispatcher = std::make_unique<nx_http::MessageDispatcher>();

    // Registering HTTP handlers.
    m_httpMessageDispatcher->registerRequestProcessor<http::GetListeningPeerListHandler>(
        nx::network::url::normalizePath(lm("/%1/%2")
            .args(api::kMediatorApiPrefix, http::GetListeningPeerListHandler::kHandlerPath)),
        [&]() { return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator); });

    m_multiAddressHttpServer =
        std::make_unique<nx::network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>>(
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

    m_httpEndpoints = m_multiAddressHttpServer->endpoints();
    m_multiAddressHttpServer->forEachListener(
        [&settings](nx_http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
        });

    return true;
}

} // namespace http
} // namespace hpm
} // namespace nx
