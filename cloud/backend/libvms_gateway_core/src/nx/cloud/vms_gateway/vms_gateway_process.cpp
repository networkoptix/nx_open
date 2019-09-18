#include "vms_gateway_process.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_connect_settings.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/stree/stree_manager.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/platform/current_process.h>

#include <network/cloud/cloud_media_server_endpoint_verificator.h>

#include "access_control/authentication_manager.h"
#include "http/connect_handler.h"
#include "http/http_api_path.h"
#include "http/proxy_handler.h"
#include "http/url_rewriter.h"
#include "libvms_gateway_core_app_info.h"
#include "stree/cdb_ns.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(vms_gateway_core);
    return 0;
}

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
            NX_ALWAYS(this, "No HTTP address to listen");
            return 1;
        }

        CdbAttrNameSet attrNameSet;
        nx::utils::stree::StreeManager streeManager(
            attrNameSet,
            settings.auth().rulesXmlPath);

        nx::network::http::server::rest::MessageDispatcher httpMessageDispatcher;

        // TODO: #ak Move following to stree xml.
        nx::network::http::AuthMethodRestrictionList authRestrictionList;

        AuthenticationManager authenticationManager(
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
            network::ssl::Engine::useOrCreateCertificate(
                settings.http().sslCertPath,
                nx::utils::AppInfo::customizationVmsName().toUtf8(),
                "US",
                nx::utils::AppInfo::organizationName().toUtf8());
        }

        network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>
            multiAddressHttpServer(
                &authenticationManager,
                &httpMessageDispatcher,
                settings.http().sslSupport,
                nx::network::NatTraversalSupport::disabled);

        if (!multiAddressHttpServer.bind(httpAddrToListenList))
            return 3;

        multiAddressHttpServer.forEachListener(
            [&settings](auto* listener)
            {
                listener->setConnectionInactivityTimeout(
                    settings.http().connectionInactivityTimeout);
            });

        // Process privilege reduction.
        nx::utils::CurrentProcess::changeUser(settings.general().changeUser);

        if (!multiAddressHttpServer.listen())
            return 5;
        m_httpEndpoints = multiAddressHttpServer.endpoints();

        NX_ALWAYS(this, lm("%1 has been started on %2")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName())
            .container(m_httpEndpoints));

        const auto result = runMainLoop();

        NX_ALWAYS(this, lm("%1 has been stopped")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName()));

        return result;
    }
    catch (const std::exception& e)
    {
        NX_ALWAYS(this, lit("Failed to start application. %1").arg(e.what()));
        return 3;
    }
}

void VmsGatewayProcess::initializeCloudConnect(const conf::Settings& settings)
{
    if (!settings.general().mediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::cloud().mediatorConnector().mockupMediatorAddress({
            nx::network::url::Builder().setScheme("stun")
                .setEndpoint(network::SocketAddress(settings.general().mediatorEndpoint)).toUrl(),
            settings.general().mediatorEndpoint});
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
    NX_INFO(this, lm("Retrieved public address %1. This address will be used for cloud connect")
        .arg(publicAddress));

    nx::network::SocketGlobals::cloud().settings()
        .replaceOriginatingHostAddress(publicAddress);
}

void VmsGatewayProcess::registerApiHandlers(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions,
    nx::network::http::server::rest::MessageDispatcher* const msgDispatcher,
    HttpConnectTunnelPool* httpConnectTunnelPool)
{
    msgDispatcher->registerRequestProcessor<ProxyHandler>(
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
                        tunnel->start(
                            [httpConnectTunnelPool, tunnelPtr = tunnel.get()](SystemError::ErrorCode error)
                            {
                                NX_VERBOSE(tunnelPtr, "Closing CONNECT tunnel.");
                                httpConnectTunnelPool->closeConnection(error, tunnelPtr);
                            });
                        httpConnectTunnelPool->saveConnection(std::move(tunnel));
                    };
                return std::make_unique<ConnectHandler>(settings, std::move(tunnelCreatedHandler));
            };

        msgDispatcher->registerRequestProcessor<ConnectHandler>(
            nx::network::http::kAnyPath,
            std::move(factoryFunc),
            nx::network::http::StringType("CONNECT"));
    }

    msgDispatcher->addModRewriteRule(
        nx::network::url::normalizePath(lm("/%1/").arg(kApiPathPrefix)),
        lit("/"));
}

} // namespace cloud
} // namespace gateway
} // namespace nx
