#include "vms_gateway_process.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/stree/stree_manager.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

//#include <platform/process/current_process.h>
//#include <utils/common/app_info.h>
#include <nx/network/public_ip_discovery.h>

#include "access_control/authentication_manager.h"
#include "http/connect_handler.h"
#include "http/proxy_handler.h"
#include "libvms_gateway_app_info.h"
#include "stree/cdb_ns.h"


static int registerQtResources()
{
    Q_INIT_RESOURCE( libvms_gateway );
    return 0;
}

namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayProcess::VmsGatewayProcess( int argc, char **argv ):
    m_argc( argc ),
    m_argv( argv ),
    m_terminated( false ),
    m_timerID( -1 )
{
    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void VmsGatewayProcess::pleaseStop()
{
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
}

void VmsGatewayProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

const std::vector<SocketAddress>& VmsGatewayProcess::httpEndpoints() const
{
    return m_httpEndpoints;
}

void VmsGatewayProcess::enforceSslFor(const SocketAddress& targetAddress, bool enabled)
{
    m_runTimeOptions.enforceSsl(targetAddress, enabled);
}

int VmsGatewayProcess::exec()
{
    using namespace std::placeholders;

    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopeGuard(
        [this, &processStartResult]
        {
            if (m_startedEventHandler)
                m_startedEventHandler(processStartResult);
        });

    try
    {
        //reading settings
        conf::Settings settings;
        //parsing command line arguments
        settings.load(m_argc, (const char**) m_argv);
        if (settings.isShowHelpRequested())
        {
            settings.printCmdLineArgsHelp();
            return 0;
        }

        nx::utils::log::initialize(
            settings.logging(), settings.general().dataDir,
            QnLibVmsGatewayAppInfo::applicationDisplayName());

        //enabling nat traversal
        if (!settings.general().mediatorEndpoint.isEmpty())
            nx::network::SocketGlobals::mediatorConnector().mockupAddress(
                SocketAddress(settings.general().mediatorEndpoint));
        nx::network::SocketGlobals::mediatorConnector().enable(true);

        std::unique_ptr<nx::network::PublicIPDiscovery> publicAddressFetcher;
        if (settings.cloudConnect().replaceHostAddressWithPublicAddress)
        {
            if (!settings.cloudConnect().publicIpAddress.isEmpty())
            {
                publicAddressFetched(settings, settings.cloudConnect().publicIpAddress);
            }
            else
            {
                publicAddressFetcher = std::make_unique<nx::network::PublicIPDiscovery>(
                    QStringList() << settings.cloudConnect().fetchPublicIpUrl);

                QObject::connect(
                    publicAddressFetcher.get(), &nx::network::PublicIPDiscovery::found,
                    [this, &settings](const QHostAddress& publicAddress)
                    {
                        publicAddressFetched(settings, publicAddress.toString());
                    });

                publicAddressFetcher->update();
            }
        }

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

        //TODO #ak move following to stree xml
        nx_http::AuthMethodRestrictionList authRestrictionList;

        AuthenticationManager authenticationManager(
            authRestrictionList,
            streeManager);

        //registering HTTP handlers
        registerApiHandlers(settings, m_runTimeOptions, &httpMessageDispatcher);

        if (settings.http().sslSupport)
        {
            network::ssl::Engine::useOrCreateCertificate(
                settings.http().sslCertPath, 
                QnAppInfo::productName().toUtf8(),
                "US", nx::utils::AppInfo::organizationName().toUtf8());
        }

        network::server::MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
            &authenticationManager,
            &httpMessageDispatcher,
            settings.http().sslSupport,
            nx::network::NatTraversalSupport::disabled);

        if (!multiAddressHttpServer.bind(httpAddrToListenList))
            return 3;

        // process privilege reduction
        CurrentProcess::changeUser(settings.general().changeUser);

        if (!multiAddressHttpServer.listen())
            return 5;
        m_httpEndpoints = multiAddressHttpServer.endpoints();

        if (m_terminated)
            return 0;

        NX_LOG(lm("%1 has been started on %2")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName())
            .container(m_httpEndpoints), cl_logALWAYS);

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

        {
            QnMutexLocker lk(&m_mutex);
            while (!m_terminated)
                m_cond.wait(lk.mutex());
        }

        NX_LOG(lm("%1 has been stopped")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName()), cl_logALWAYS);

        return 0;
    }
    catch (const std::exception& e)
    {
        NX_LOG(lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS);
        return 3;
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

    msgDispatcher->addModRewriteRule(lit("/gateway/"), lit("/"));
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

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
