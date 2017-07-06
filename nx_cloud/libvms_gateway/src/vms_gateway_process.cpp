#include "vms_gateway_process.h"

#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>

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
#include "libvms_gateway_app_info.h"
#include "stree/cdb_ns.h"

static int registerQtResources()
{
    Q_INIT_RESOURCE(libvms_gateway);
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

const std::vector<SocketAddress>& VmsGatewayProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

void VmsGatewayProcess::enforceSslFor(const SocketAddress& targetAddress, bool enabled)
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
            NX_LOG("No HTTP address to listen", cl_logALWAYS);
            return 1;
        }

        nx::utils::TimerManager timerManager;
        timerManager.start();

        CdbAttrNameSet attrNameSet;
        nx::utils::stree::StreeManager streeManager(
            attrNameSet,
            settings.auth().rulesXmlPath);

        nx_http::MessageDispatcher httpMessageDispatcher;

        // TODO: #ak Move following to stree xml.
        nx_http::AuthMethodRestrictionList authRestrictionList;

        AuthenticationManager authenticationManager(
            authRestrictionList,
            streeManager);

        registerApiHandlers(settings, m_runTimeOptions, &httpMessageDispatcher);

        if (settings.http().sslSupport)
        {
            network::ssl::Engine::useOrCreateCertificate(
                settings.http().sslCertPath, 
                nx::utils::AppInfo::productName().toUtf8(),
                "US", nx::utils::AppInfo::organizationName().toUtf8());
        }

        network::server::MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
            &authenticationManager,
            &httpMessageDispatcher,
            settings.http().sslSupport,
            nx::network::NatTraversalSupport::disabled);

        if (!multiAddressHttpServer.bind(httpAddrToListenList))
            return 3;

        // Process privilege reduction.
        nx::utils::CurrentProcess::changeUser(settings.general().changeUser);

        if (!multiAddressHttpServer.listen())
            return 5;
        m_httpEndpoints = multiAddressHttpServer.endpoints();

        NX_LOG(lm("%1 has been started on %2")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName())
            .container(m_httpEndpoints), cl_logALWAYS);

        const auto result = runMainLoop();

        NX_LOG(lm("%1 has been stopped")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName()), cl_logALWAYS);

        return result;
    }
    catch (const std::exception& e)
    {
        NX_LOG(lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS);
        return 3;
    }
}

void VmsGatewayProcess::initializeCloudConnect(const conf::Settings& settings)
{
    if (!settings.general().mediatorEndpoint.isEmpty())
    {
        nx::network::SocketGlobals::mediatorConnector().mockupAddress(
            nx::network::url::Builder().setScheme("stun")
                .setEndpoint(SocketAddress(settings.general().mediatorEndpoint)).toUrl());
    }
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    m_endpointVerificatorFactoryBak =
        nx::network::cloud::tcp::EndpointVerificatorFactory::instance().setCustomFunc(
            [](const nx::String& connectSessionId) 
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
        publicAddressFetched(settings, settings.cloudConnect().publicIpAddress);
        return nullptr;
    }

    auto publicAddressFetcher = std::make_unique<nx::network::PublicIPDiscovery>(
        QStringList() << settings.cloudConnect().fetchPublicIpUrl);

    QObject::connect(
        publicAddressFetcher.get(), &nx::network::PublicIPDiscovery::found,
        [this, &settings](const QHostAddress& publicAddress)
        {
            publicAddressFetched(settings, publicAddress.toString());
        });

    publicAddressFetcher->update();
    return publicAddressFetcher;
}

void VmsGatewayProcess::publicAddressFetched(
    const conf::Settings& settings,
    const QString& publicAddress)
{
    NX_LOGX(lm("Retrieved public address %1. This address will be used for cloud connect")
        .arg(publicAddress), cl_logINFO);

    nx::network::SocketGlobals::cloudConnectSettings()
        .replaceOriginatingHostAddress(publicAddress);

    const auto& rcSettings = settings.cloudConnect().tcpReverse;
    if (rcSettings.poolSize != 0)
    {
        auto& pool = nx::network::SocketGlobals::tcpReversePool();
        pool.setPoolSize(rcSettings.poolSize);
        pool.setKeepAliveOptions(rcSettings.keepAlive);
        if (pool.start(publicAddress, rcSettings.port))
        {
            NX_LOG(lm("TCP reverse pool has started with port=%1, poolSize=%2, keepAlive=%3")
                .args(pool.port(), rcSettings.poolSize, rcSettings.keepAlive), cl_logALWAYS);
        }
        else
        {
            NX_LOGX(lm("Could not start TCP reverse pool on port %1").args(rcSettings.port),
                cl_logERROR);
        }
    }
}

void VmsGatewayProcess::registerApiHandlers(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions,
    nx_http::MessageDispatcher* const msgDispatcher)
{
    msgDispatcher->registerRequestProcessor<ProxyHandler>(
        nx_http::kAnyPath,
        [&settings, &runTimeOptions]() -> std::unique_ptr<ProxyHandler>
        {
            return std::make_unique<ProxyHandler>(settings, runTimeOptions);
        });

    if (settings.http().connectSupport)
    {
        NX_CRITICAL(false, "Currently ConnectHandler has some issues:"
            "please see implementation TODOs");

        msgDispatcher->registerRequestProcessor<ConnectHandler>(
            nx_http::kAnyPath,
            [&settings, &runTimeOptions]() -> std::unique_ptr<ConnectHandler> {
                return std::make_unique<ConnectHandler>(settings, runTimeOptions);
            },
            nx_http::StringType("CONNECT"));
    }

    msgDispatcher->addModRewriteRule(lm("/%1/").arg(kApiPathPrefix), lit("/"));
}

} // namespace cloud
} // namespace gateway
} // namespace nx
