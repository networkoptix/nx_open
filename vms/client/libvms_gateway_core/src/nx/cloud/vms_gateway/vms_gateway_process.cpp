// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_gateway_process.h"

#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <nx/branding.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/stree/stree_manager.h>

#include "access_control/authentication_manager.h"
#include "http/connect_handler.h"
#include "http/http_api_path.h"
#include "http/proxy_handler.h"
#include "http/url_rewriter.h"
#include "port_forwarding.h"
#include "stree/cdb_ns.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(vms_gateway_core);
    return 0;
}

namespace {

static const QString kApplicationDisplayName = nx::branding::company() + " VMS Gateway";

} // namespace

namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayProcess::VmsGatewayProcess(int argc, char **argv):
    base_type(argc, argv, "VmsGateway")
{
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cloud::gateway::libvms_gateway and fail...
    registerQtResources();
}

VmsGatewayProcess::~VmsGatewayProcess()
{
    if (m_endpointVerificatorFactoryBak)
    {
        nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            std::move(m_endpointVerificatorFactoryBak));
    }
}

const std::vector<network::SocketAddress>& VmsGatewayProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

void VmsGatewayProcess::enforceSslFor(const network::SocketAddress& targetAddress, bool enabled)
{
    m_runTimeOptions.enforceSsl(targetAddress, enabled);
}

void VmsGatewayProcess::enforceHeadersFor(
    const network::SocketAddress& targetAddress, network::http::HttpHeaders headers)
{
    m_runTimeOptions.enforceHeaders(targetAddress, std::move(headers));
}

std::unique_ptr<nx::utils::AbstractServiceSettings> VmsGatewayProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int VmsGatewayProcess::serviceMain(
    const nx::utils::AbstractServiceSettings& abstractSettings)
{
    using namespace std::placeholders;

    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    try
    {
        initializeCloudConnect(settings);

        std::unique_ptr<nx::network::PublicIPDiscovery> publicAddressFetcher =
            initializePublicIpDiscovery(settings);

        const auto& httpAddrToListenList = settings.general().endpointsToListen;
        if (httpAddrToListenList.empty())
        {
            NX_ERROR(this, "No HTTP address to listen");
            return 1;
        }

        nx::utils::stree::StreeManager streeManager(
            settings.auth().rulesXmlPath.toStdString());

        nx::network::http::server::rest::MessageDispatcher httpMessageDispatcher;

        nx::network::http::AuthMethodRestrictionList authRestrictionList;

        AuthenticationManager authenticationManager(
            &httpMessageDispatcher,
            authRestrictionList,
            streeManager);

        HttpConnectTunnelPool httpConnectTunnelPool;
        nx::network::http::server::proxy::MessageBodyConverterFactory::instance().setUrlConverter(
            std::make_unique<UrlRewriter>());
        registerApiHandlers(
            settings,
            m_runTimeOptions,
            &httpMessageDispatcher,
            &httpConnectTunnelPool);

        if (settings.http().sslSupport)
        {
            network::ssl::useOrCreateCertificate(
                settings.http().sslCertPath.toStdString(),
                {nx::branding::vmsName().toStdString(), "US", nx::branding::company().toStdString()});
        }

        using HttpServer = network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

        std::unique_ptr<HttpServer> httpServer;
        if (settings.http().sslSupport)
        {
            httpServer = std::make_unique<HttpServer>(
                &authenticationManager,
                nx::network::ssl::Context::instance());
        }
        else
        {
            httpServer = std::make_unique<HttpServer>(&authenticationManager);
        }

        if (!httpServer->bind(httpAddrToListenList))
            return 3;

        httpServer->forEachListener(
            [&settings](auto* listener)
            {
                listener->setConnectionInactivityTimeout(
                    settings.http().connectionInactivityTimeout);
            });

        // Process privilege reduction.
        nx::utils::CurrentProcess::changeUser(settings.general().changeUser);

        if (!httpServer->listen())
            return 5;

        m_httpEndpoints = httpServer->endpoints();
        m_portForwarding = std::make_unique<PortForwarding>(m_runTimeOptions);

        NX_INFO(this, "%1 has been started on %2",
            kApplicationDisplayName,
            containerString(m_httpEndpoints));

        const auto result = runMainLoop();
        NX_INFO(this, "%1 has been stopped", kApplicationDisplayName);
        return result;
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, "Failed to start application. %1", e.what());
        return 3;
    }
}

void VmsGatewayProcess::initializeCloudConnect(const conf::Settings& settings)
{
    if (!settings.general().mediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            nx::network::url::Builder().setScheme("stun")
                .setEndpoint(network::SocketAddress(settings.general().mediatorEndpoint.toStdString())).toUrl(),
            settings.general().mediatorEndpoint.toStdString()});
    }

    m_endpointVerificatorFactoryBak =
        nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            [](const std::string& connectSessionId)
                -> std::unique_ptr<nx::network::cloud::tcp::AbstractEndpointVerificator>
            {
                return std::make_unique<CloudMediaServerEndpointVerificator>(
                    connectSessionId);
            });
}

std::unique_ptr<nx::network::PublicIPDiscovery> VmsGatewayProcess::initializePublicIpDiscovery(
    const conf::Settings& settings)
{
    if (!settings.cloudConnect().replaceHostAddressWithPublicAddress)
        return nullptr;

    if (!settings.cloudConnect().publicIpAddress.isEmpty())
    {
        publicAddressFetched(settings.cloudConnect().publicIpAddress);
        return nullptr;
    }

    auto publicAddressFetcher = std::make_unique<nx::network::PublicIPDiscovery>(
        QStringList() << settings.cloudConnect().fetchPublicIpUrl);

    QObject::connect(
        publicAddressFetcher.get(), &nx::network::PublicIPDiscovery::found,
        [this](const QHostAddress& publicAddress)
        {
            publicAddressFetched(publicAddress.toString());
        });

    publicAddressFetcher->update();
    return publicAddressFetcher;
}

void VmsGatewayProcess::publicAddressFetched(const QString& publicAddress)
{
    NX_INFO(this, nx::format("Retrieved public address %1. This address will be used for cloud connect")
        .arg(publicAddress));

    nx::network::SocketGlobals::cloud().settings()
        .replaceOriginatingHostAddress(publicAddress.toStdString());
}

void VmsGatewayProcess::registerApiHandlers(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions,
    nx::network::http::server::rest::MessageDispatcher* const msgDispatcher,
    HttpConnectTunnelPool* httpConnectTunnelPool)
{
    msgDispatcher->registerRequestProcessor(
        nx::network::http::kAnyPath,
        [&settings, &runTimeOptions]() -> std::unique_ptr<ProxyHandler>
        {
            return std::make_unique<ProxyHandler>(
                settings,
                runTimeOptions);
        });

    if (settings.http().connectSupport)
    {
        auto factoryFunc =
            [&settings, httpConnectTunnelPool]() -> std::unique_ptr<ConnectHandler>
            {
                auto tunnelCreatedHandler =
                    [httpConnectTunnelPool](std::unique_ptr<network::aio::AsyncChannelBridge> tunnel)
                    {
                        NX_VERBOSE(tunnel.get(), "Starting CONNECT tunnel.");
                        auto connection = std::make_unique<BridgeToServerConnectionAdaptor>(
                            std::move(tunnel));
                        connection->start();
                        httpConnectTunnelPool->saveConnection(std::move(connection));
                    };
                return std::make_unique<ConnectHandler>(settings, std::move(tunnelCreatedHandler));
            };

        msgDispatcher->registerRequestProcessor(
            nx::network::http::kAnyPath,
            std::move(factoryFunc),
            nx::network::http::Method::connect);
    }

    msgDispatcher->addModRewriteRule(
        nx::network::url::normalizePath(nx::format("/%1/").arg(kApiPathPrefix).toStdString()),
        "/");
}

std::map<uint16_t, uint16_t> VmsGatewayProcess::forward(
    const nx::network::SocketAddress& target,
    const std::set<uint16_t>& targetPorts,
    const nx::network::ssl::AdapterFunc& certificateCheck)
{
    return m_portForwarding->forward(target, targetPorts, certificateCheck);
}

} // namespace cloud
} // namespace gateway
} // namespace nx
