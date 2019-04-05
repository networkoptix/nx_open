#include "http_server.h"

#include <memory>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/server/handler/fusion_based_handlers.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/network/url/url_builder.h>

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
    ListeningPeerDb* listeningPeerDb,
    HolePunchingProcessor* holePunchingProcessor)
    :
    m_settings(settings),
    m_multiAddressHttpServer(&m_authenticationDispatcher, &m_httpMessageDispatcher),
    m_listeningPeerDb(listeningPeerDb),
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

    if (!m_settings.http().maintenanceHtdigestPath.empty())
    {
        NX_INFO(
            this,
            lm("htdigest authentication for connection mediator maintenance server enabled. File path: %1")
                .arg(m_settings.http().maintenanceHtdigestPath));

        m_maintenanceAuthenticator = std::make_unique<MaintenanceAuthenticator>(
            m_settings.http().maintenanceHtdigestPath);

        m_authenticationDispatcher.add(
            std::regex(network::url::joinPath(m_maintenanceServer.maintenancePath(), "/.*")),
            &m_maintenanceAuthenticator->manager);
    }

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
                m_holePunchingProcessor,
                m_listeningPeerDb);
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
    HolePunchingProcessor* holePunchingProcessor,
    ListeningPeerDb* listeningPeerDb)
    :
    m_holePunchingProcessor(holePunchingProcessor),
    m_listeningPeerDb(listeningPeerDb)
{
}

void InitiateConnectionRequestHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    api::ConnectRequest inputData)
{
    CachedRequestContext cachedRequestContext{
        requestContext.request.requestLine.url,
        requestContext.connection->isSsl(),
        requestContext.response,
    };

    m_holePunchingProcessor->connect(
        RequestSourceDescriptor{
            network::TransportProtocol::tcp,
            requestContext.connection->socket()->getForeignAddress()},
        inputData,
        [this, cachedRequestContext = std::move(cachedRequestContext),
            targetServer = inputData.destinationHostName](
                api::ResultCode resultCode,
                api::ConnectResponse response)
        {
            if (resultCode == api::ResultCode::notFound)
            {
                return redirectToRemoteMediator(
                    std::move(cachedRequestContext),
                    targetServer,
                    std::move(resultCode),
                    std::move(response));
            }

            reportResult(std::move(resultCode), std::move(response));
        });
}

void InitiateConnectionRequestHandler::reportResult(
    api::ResultCode resultCode,
    api::ConnectResponse response,
    const std::optional<nx::network::http::StatusCode::Value>& httpStatusCode)
{
    network::http::FusionRequestResult result;
    if (resultCode != api::ResultCode::ok)
    {
        result.errorClass = network::http::FusionRequestErrorClass::logicError;
        result.errorDetail = static_cast<int>(resultCode);
        result.errorText = api::toString(resultCode);
        if (httpStatusCode.has_value())
        {
            result.setHttpStatusCode(*httpStatusCode);
        }
        else
        {
            result.setHttpStatusCode(
                resultCode == api::ResultCode::notFound
                    ? network::http::StatusCode::notFound
                    : network::http::StatusCode::badRequest);
        }
    }

    this->requestCompleted(std::move(result), std::move(response));
}


void InitiateConnectionRequestHandler::redirectToRemoteMediator(
    CachedRequestContext requestContext,
    const nx::String& targetServer,
    api::ResultCode resultCode,
    api::ConnectResponse response)
{
    m_listeningPeerDb->findMediatorByPeerDomain(
        targetServer.toStdString(),
        [this, requestContext = std::move(requestContext),
            resultCode, response = std::move(response)](
                MediatorEndpoint endpoint)
        {
            if (!validateMediatorEndpoint(endpoint, requestContext.isSsl))
                return reportResult(api::ResultCode::notFound, std::move(response));

            auto location = buildMediatorUrl(endpoint, requestContext);

            network::http::insertOrReplaceHeader(
                &requestContext.response->headers,
                nx::network::http::HttpHeader("Location", location.toStdString().c_str()));

            reportResult(
                api::ResultCode::notFound,
                std::move(response),
                nx::network::http::StatusCode::temporaryRedirect);
        });
}

bool InitiateConnectionRequestHandler::validateMediatorEndpoint(
    const MediatorEndpoint& endpoint,
    bool useHttpsPort) const
{
    if (endpoint.domainName.empty())
    {
        NX_VERBOSE(this,
            "Redirect to remote mediator failed. attemted to redirect to remote mediator: %1",
            endpoint);
        return false;
    }

    if (endpoint == m_listeningPeerDb->thisMediatorEndpoint())
    {
        NX_VERBOSE(this, "Redirecting to this mediator but connection already failed");
        return false;
    }

    if (useHttpsPort)
    {
        if (endpoint.httpsPort == MediatorEndpoint::kPortUnused)
        {
            NX_VERBOSE(this, "Found remote mediator: %1, but no https port is accessible.",
                endpoint);
            return false;
        }
    }
    else
    {
        if (endpoint.httpPort == MediatorEndpoint::kPortUnused)
        {
            NX_VERBOSE(this, "Found remote mediator: %1, but no http port is accessible.",
                endpoint);
            return false;
        }
    }

    return true;
}

nx::utils::Url InitiateConnectionRequestHandler::buildMediatorUrl(
    const MediatorEndpoint& endpoint,
    const CachedRequestContext& requestContext)
{
    return nx::network::url::Builder()
        .setHost(endpoint.domainName.c_str())
        .setScheme(
            requestContext.isSsl
                ? nx::network::http::kSecureUrlSchemeName
                : nx::network::http::kUrlSchemeName)
        .setPort(requestContext.isSsl ? endpoint.httpsPort : endpoint.httpPort).
        setPath(requestContext.requestUrl.path()).toUrl();
}

} // namespace http
} // namespace hpm
} // namespace nx
