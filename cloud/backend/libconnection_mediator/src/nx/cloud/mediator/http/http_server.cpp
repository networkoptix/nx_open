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
    nx::cloud::discovery::RegisteredPeerPool* registeredPeerPool,
    HolePunchingProcessor* holePunchingProcessor)
    :
    m_settings(settings),
    m_multiAddressHttpServer(nullptr, &m_httpMessageDispatcher),
    m_holePunchingProcessor(holePunchingProcessor)
{
    NX_ASSERT(!m_settings.http().addrToListenList.empty());

    loadSslCertificate();

    if (!launchHttpServerIfNeeded(
            m_settings,
            peerRegistrator,
            registeredPeerPool))
    {
        throw std::runtime_error("Failed to initialize http server");
    }
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
    NX_INFO(this, "Bringing up HTTP server");

    registerApiHandlers(peerRegistrator);

    if (!m_multiAddressHttpServer.bind(
            settings.http().addrToListenList,
            settings.https().endpoints))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_ERROR(this, lm("Failed to bind HTTP server to address(-es) %1, %2. %3")
            .container(settings.http().addrToListenList)
            .container(settings.https().endpoints)
            .args(SystemError::toString(osErrorCode)));
        return false;
    }

    m_multiAddressHttpServer.forEachListener(
        [&settings](nx::network::http::HttpStreamSocketServer* server)
        {
            server->setConnectionKeepAliveOptions(settings.http().keepAliveOptions);
            server->setConnectionInactivityTimeout(settings.http().connectionInactivityTimeout);
        });

    m_discoveryHttpServer = std::make_unique<nx::cloud::discovery::HttpServer>(
        &m_httpMessageDispatcher,
        registeredPeerPool);

    m_maintenanceServer.registerRequestHandlers(
        api::kMediatorApiPrefix,
        &m_httpMessageDispatcher);

    return true;
}

void Server::registerApiHandlers(const PeerRegistrator& peerRegistrator)
{
    m_httpMessageDispatcher.registerRequestProcessor<http::GetListeningPeerListHandler>(
        network::url::joinPath(
            api::kMediatorApiPrefix,
            GetListeningPeerListHandler::kHandlerPath).c_str(),
        [&peerRegistrator]()
        {
            return std::make_unique<http::GetListeningPeerListHandler>(peerRegistrator);
        });

    m_httpMessageDispatcher.registerRequestProcessor<InitiateConnectionRequestHandler>(
        network::url::joinPath(api::kMediatorApiPrefix, api::kServerSessionsPath).c_str(),
        [this]()
        {
            return std::make_unique<InitiateConnectionRequestHandler>(
                m_holePunchingProcessor);
        },
        network::http::Method::post);
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

//-------------------------------------------------------------------------------------------------

InitiateConnectionRequestHandler::InitiateConnectionRequestHandler(
    HolePunchingProcessor* holePunchingProcessor)
    :
    m_holePunchingProcessor(holePunchingProcessor)
{
}

void InitiateConnectionRequestHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    api::ConnectRequest inputData)
{
    m_holePunchingProcessor->connect(
        RequestSourceDescriptor{
            network::TransportProtocol::tcp,
            requestContext.connection->socket()->getForeignAddress()},
        inputData,
        [this](auto&&... args) { reportResult(std::move(args)...); });
}

void InitiateConnectionRequestHandler::reportResult(
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    network::http::FusionRequestResult result;
    if (resultCode != api::ResultCode::ok)
    {
        result.errorClass = network::http::FusionRequestErrorClass::logicError;
        result.errorDetail = static_cast<int>(resultCode);
        result.errorText = api::toString(resultCode);
        result.setHttpStatusCode(
            resultCode == api::ResultCode::notFound
            ? network::http::StatusCode::notFound
            : network::http::StatusCode::badRequest);
    }

    this->requestCompleted(std::move(result), std::move(response));
}

} // namespace http
} // namespace hpm
} // namespace nx
