#include "view.h"

#include <stdexcept>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/http/server/proxy/message_body_converter.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/relaying/http_view/begin_listening_http_handler.h>

#include "http_handlers.h"
#include "options_request_handler.h"
#include "proxy_handler.h"
#include "../controller/connect_session_manager.h"
#include "../controller/controller.h"
#include "../model/model.h"
#include "../settings.h"
#include "../statistics_provider.h"

namespace nx {
namespace cloud {
namespace relay {

class UrlRewriter:
    public network::http::server::proxy::AbstractUrlRewriter
{
public:
    virtual nx::utils::Url originalResourceUrlToProxyUrl(
        const nx::utils::Url& originalResourceUrl,
        const utils::Url& proxyHostUrl,
        const nx::String& /*targetHost*/) const override
    {
        auto absoluteUrl = network::url::Builder(originalResourceUrl)
            .setEndpoint(network::url::getEndpoint(proxyHostUrl))
            .setScheme(proxyHostUrl.scheme()).toUrl();
        return absoluteUrl;
    }
};

//-------------------------------------------------------------------------------------------------

View::View(
    const conf::Settings& settings,
    Model* model,
    Controller* controller)
    :
    m_settings(settings),
    m_model(model),
    m_controller(controller),
    m_listeningPeerConnectionTunnelingServer(
        &controller->listeningPeerManager(),
        &model->listeningPeerPool()),
    m_clientConnectionTunnelingServer(&m_controller->connectSessionManager()),
    m_authenticationManager(m_authRestrictionList),
    m_multiAddressHttpServer(
        &m_authenticationDispatcher,
        &m_httpMessageDispatcher),
    m_maintenanceAuthenticator(m_settings.http().maintenanceHtdigestPath)
{
    registerApiHandlers();
    registerAuthenticators();
    initializeProxy();
    loadSslCertificate();
    startAcceptor();
}

View::~View()
{
    m_multiAddressHttpServer.pleaseStopSync();
}

void View::registerStatisticsApiHandlers(
    const AbstractStatisticsProvider& statisticsProvider)
{
    network::http::registerFusionRequestHandler(
        &m_httpMessageDispatcher,
        api::kRelayStatisticsMetricsPath,
        std::bind(&AbstractStatisticsProvider::getAllStatistics, &statisticsProvider),
        nx::network::http::Method::get);
}

void View::start()
{
    m_multiAddressHttpServer.forEachListener(
        &nx::network::http::HttpStreamSocketServer::setConnectionInactivityTimeout,
        m_settings.http().connectionInactivityTimeout);

    if (!m_multiAddressHttpServer.listen(m_settings.http().tcpBacklogSize))
    {
        throw std::system_error(
            SystemError::getLastOSErrorCode(),
            std::system_category(),
            lm("Cannot start listening: %1")
                .args(SystemError::getLastOSErrorText()).toStdString());
    }
}

std::vector<network::SocketAddress> View::httpEndpoints() const
{
    return m_multiAddressHttpServer.endpoints();
}

std::vector<network::SocketAddress> View::httpsEndpoints() const
{
    return m_multiAddressHttpServer.sslEndpoints();
}

const nx::network::http::server::MultiEndpointAcceptor& View::httpServer() const
{
    return m_multiAddressHttpServer;
}

void View::registerAuthenticators()
{
    if (!m_settings.http().maintenanceHtdigestPath.empty())
    {
        NX_INFO(
            this,
            lm("htdigest authentication for traffic relay maintenance server enabled. File path: %1")
               .arg(m_settings.http().maintenanceHtdigestPath));

        m_authenticationDispatcher.add(
            std::regex(m_maintenanceServer.maintenancePath() + "/.*"),
            &m_maintenanceAuthenticator.manager);
    }

    m_authenticationDispatcher.add(std::regex(".*"), &m_authenticationManager);
}

void View::registerApiHandlers()
{
    registerApiHandler<view::CreateClientSessionHandler>(
        nx::network::http::Method::post,
        &m_controller->connectSessionManager());

    // TODO: #ak Remove handler after end of 3.2 support. This is a compatiblity handler.
    registerApiHandler<view::ConnectToListeningPeerWithHttpUpgradeHandler>(
        nx::network::http::Method::post,
        &m_controller->connectSessionManager());

    m_listeningPeerConnectionTunnelingServer.registerHandlers(
        api::kServerTunnelBasePath,
        &m_httpMessageDispatcher);

    m_clientConnectionTunnelingServer.registerHandlers(
        api::kClientTunnelBasePath,
        &m_httpMessageDispatcher);

    // TODO: #ak Remove handler after end of 3.2 support. This is a compatiblity handler.
    registerApiHandler<relaying::BeginListeningUsingConnectMethodHandler>(
        nx::network::http::Method::connect,
        &m_controller->listeningPeerManager());

    if (m_settings.http().serveOptions)
    {
        registerApiHandler<view::OptionsRequestHandler>(
            nx::network::http::Method::options);
    }

    m_maintenanceServer.registerRequestHandlers(
        api::kApiPrefix,
        &m_httpMessageDispatcher);
}

template<typename Handler, typename ... Args>
void View::registerApiHandler(
    const nx::network::http::StringType& method,
    Args ... args)
{
    registerApiHandler<Handler, Args...>(
        Handler::kPath, method, args...);
}

template<typename Handler, typename ... Args>
void View::registerApiHandler(
    const char* path,
    const nx::network::http::StringType& method,
    Args ... args)
{
    m_httpMessageDispatcher.registerRequestProcessor<Handler>(
        path,
        [this, args...]() -> std::unique_ptr<Handler>
        {
            return std::make_unique<Handler>(args...);
        },
        method);
}

void View::initializeProxy()
{
    network::http::server::proxy::MessageBodyConverterFactory::instance()
        .setUrlConverter(std::make_unique<UrlRewriter>());

    m_httpMessageDispatcher.registerRequestProcessor<view::ProxyHandler>(
        network::http::kAnyPath,
        [this]() -> std::unique_ptr<view::ProxyHandler>
        {
            return std::make_unique<view::ProxyHandler>(
                m_settings,
                &m_model->listeningPeerPool(),
                &m_model->remoteRelayPeerPoolAioWrapper(),
                &m_model->aliasManager());
        });
}

void View::loadSslCertificate()
{
    if (!m_settings.https().certificatePath.empty())
    {
        nx::network::ssl::Engine::loadCertificateFromFile(
            m_settings.https().certificatePath.c_str());
    }
    else
    {
        nx::network::ssl::Engine::useCertificateAndPkey(
            nx::network::ssl::Engine::makeCertificateAndKey("nxcloud/traffic_relay", "US", "Nx"));
    }
}

void View::startAcceptor()
{
    const auto& httpEndpoints = m_settings.http().endpoints;
    const auto& httpsEndpoints = m_settings.https().endpoints;
    if (httpEndpoints.empty() && httpsEndpoints.empty())
    {
        NX_ALWAYS(this, "No HTTP address to listen");
        throw std::runtime_error("No HTTP address to listen");
    }

    if (!m_multiAddressHttpServer.bind(httpEndpoints, httpsEndpoints))
    {
        throw std::runtime_error(
            lm("Cannot bind to address(es) %1, %2. %3")
                .container(httpEndpoints).container(httpsEndpoints)
                    .args(SystemError::getLastOSErrorText()).toStdString());
    }
}

} // namespace relay
} // namespace cloud
} // namespace nx
