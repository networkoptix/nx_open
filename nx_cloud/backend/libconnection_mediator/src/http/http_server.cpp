#include "http_server.h"

#include <memory>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/server/handler/fusion_based_handlers.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "get_listening_peer_list_handler.h"
#include "../controller.h"
#include "../statistics/statistics_provider.h"

namespace nx {
namespace hpm {
namespace http {

Server::Server(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
    :
    m_settings(settings),
    m_multiAddressHttpServer(nullptr, &m_httpMessageDispatcher)
{
    NX_ASSERT(!m_settings.http().addrToListenList.empty());

    loadSslCertificate();

    if (!launchHttpServerIfNeeded(m_settings, peerRegistrator, registeredPeerPool))
        throw std::runtime_error("Failed to initialize http server");
}

void Server::listen()
{
    if (!m_multiAddressHttpServer.listen())
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        throw std::runtime_error(
            lm("Error listening on HTTP addresses %1. %2")
                .arg(containerString(m_settings.http().addrToListenList))
                .arg(SystemError::toString(osErrorCode)).toStdString());
    }

    NX_ALWAYS(this, lm("HTTP server is listening on %1, ssl: %2")
        .container(m_multiAddressHttpServer.endpoints())
        .container(m_multiAddressHttpServer.sslEndpoints()));
}

void Server::stopAcceptingNewRequests()
{
    m_multiAddressHttpServer.pleaseStopSync();
}

nx::network::http::server::rest::MessageDispatcher& Server::messageDispatcher()
{
    return m_httpMessageDispatcher;
}

std::vector<network::SocketAddress> Server::endpoints() const
{
    return m_multiAddressHttpServer.endpoints();
}

std::vector<network::SocketAddress> Server::sslEndpoints() const
{
    return m_multiAddressHttpServer.sslEndpoints();
}

const nx::network::http::server::MultiEndpointAcceptor& Server::server() const
{
    return m_multiAddressHttpServer;
}

void Server::registerStatisticsApiHandlers(const stats::Provider& provider)
{
    using GetAllStatisticsHandler =
        network::http::server::handler::GetHandler<stats::Statistics>;

    registerApiHandler<GetAllStatisticsHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, api::kStatisticsMetricsPath).c_str(),
        nx::network::http::Method::get,
        std::bind(&stats::Provider::getAllStatistics, &provider));
}

void Server::loadSslCertificate()
{
    if (!m_settings.https().certificatePath.empty())
    {
        nx::network::ssl::Engine::loadCertificateFromFile(
            m_settings.https().certificatePath.c_str());
    }
    else
    {
        nx::network::ssl::Engine::useCertificateAndPkey(
            nx::network::ssl::Engine::makeCertificateAndKey("nxcloud/mediator", "US", "Nx"));
    }
}

bool Server::launchHttpServerIfNeeded(
    const conf::Settings& settings,
    const PeerRegistrator& peerRegistrator,
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool)
{
    NX_LOGX("Bringing up HTTP server", cl_logINFO);

    // Registering HTTP handlers.
    m_httpMessageDispatcher.registerRequestProcessor<http::GetListeningPeerListHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, GetListeningPeerListHandler::kHandlerPath).c_str(),
        [&]() { return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator); });

    if (!m_multiAddressHttpServer.bind(
            settings.http().addrToListenList,
            settings.https().endpoints))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to bind HTTP server to address ... . %1")
            .arg(SystemError::toString(osErrorCode)), cl_logERROR);
        return false;
    }

    m_multiAddressHttpServer.forEachListener(
        [&settings](nx::network::http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
        });

    m_discoveryHttpServer = std::make_unique<nx::cloud::discovery::HttpServer>(
        &m_httpMessageDispatcher,
        registeredPeerPool);

    return true;
}

template<typename Handler, typename Arg>
void Server::registerApiHandler(
    const char* path,
    const nx::network::http::StringType& method,
    Arg arg)
{
    m_httpMessageDispatcher.registerRequestProcessor<Handler>(
        path,
        [this, arg]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(arg);
        },
        method);
}

} // namespace http
} // namespace hpm
} // namespace nx
