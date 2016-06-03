/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "vms_gateway_process.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <list>
#include <thread>

#include <QtCore/QDir>
#include <QtSql/QSqlQuery>

#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <platform/process/current_process.h>
#include <plugins/videodecoder/stree/stree_manager.h>
#include <utils/common/app_info.h>
#include <utils/common/cpp14.h>
#include <utils/common/guard.h>
#include <utils/common/public_ip_discovery.h>
#include <utils/common/systemerror.h>

#include "access_control/authentication_manager.h"
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

VmsGatewayProcess::VmsGatewayProcess( int argc, char **argv )
:
#ifdef USE_QAPPLICATION
    QtService<QtSingleCoreApplication>(argc, argv, QnLibVmsGatewayAppInfo::applicationName()),
#endif
    m_argc( argc ),
    m_argv( argv ),
    m_terminated( false ),
    m_timerID( -1 )
{
#ifdef USE_QAPPLICATION
    setServiceDescription(QnLibVmsGatewayAppInfo::applicationDisplayName());
#endif

    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void VmsGatewayProcess::pleaseStop()
{
#ifdef USE_QAPPLICATION
    m_terminated = true;
    if (application())
        application()->quit();
#else
    QnMutexLocker lk(&m_mutex);
    m_terminated = true;
    m_cond.wakeAll();
#endif
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

#ifdef USE_QAPPLICATION
int VmsGatewayProcess::executeApplication()
#else
int VmsGatewayProcess::exec()
#endif
{
    using namespace std::placeholders;

    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopedGuard(
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
        settings.load(m_argc, m_argv);
        if (settings.showHelp())
        {
            settings.printCmdLineArgsHelp();
            return 0;
        }

        initializeLogging(settings);

        //enabling nat traversal
        if (!settings.general().mediatorEndpoint.isEmpty())
            nx::network::SocketGlobals::mediatorConnector().mockupAddress(
                SocketAddress(settings.general().mediatorEndpoint));
        nx::network::SocketGlobals::mediatorConnector().enable(true);

        std::unique_ptr<QnPublicIPDiscovery> publicAddressFetcher;
        if (settings.cloudConnect().replaceHostAddressWithPublicAddress)
        {
            publicAddressFetcher = std::make_unique<QnPublicIPDiscovery>(
                QStringList() << settings.cloudConnect().fetchPublicIpUrl);
            QObject::connect(
                publicAddressFetcher.get(), &QnPublicIPDiscovery::found,
                publicAddressFetcher.get(), [this](const QHostAddress& publicAddress) {
                    publicAddressFetched(publicAddress);
                },
                Qt::DirectConnection);
            publicAddressFetcher->update();
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
        stree::StreeManager streeManager(
            attrNameSet,
            settings.auth().rulesXmlPath);

        nx_http::MessageDispatcher httpMessageDispatcher;

        //TODO #ak move following to stree xml
        QnAuthMethodRestrictionList authRestrictionList;

        AuthenticationManager authenticationManager(
            authRestrictionList,
            streeManager);

        //registering HTTP handlers
        registerApiHandlers(
            settings,
            &httpMessageDispatcher);

        MultiAddressServer<nx_http::HttpStreamSocketServer> multiAddressHttpServer(
            &authenticationManager,
            &httpMessageDispatcher,
            false,  //TODO #ak enable ssl when it works properly
            SocketFactory::NatTraversalType::nttDisabled);

        if (!multiAddressHttpServer.bind(httpAddrToListenList))
            return 3;

        // process privilege reduction
        CurrentProcess::changeUser(settings.general().changeUser);

        if (!multiAddressHttpServer.listen())
            return 5;
        m_httpEndpoints = multiAddressHttpServer.endpoints();

#ifdef USE_QAPPLICATION
        application()->installEventFilter(this);
#endif
        if (m_terminated)
            return 0;

        NX_LOG(lit("%1 has been started")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName()),
            cl_logALWAYS);

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

#ifdef USE_QAPPLICATION
        //starting timer to check for m_terminated again after event loop start
        m_timerID = application()->startTimer(0);

        //TODO #ak remove qt event loop
        //application's main loop
        const int result = application()->exec();
        return result
#else
        {
            QnMutexLocker lk(&m_mutex);
            while (!m_terminated)
                m_cond.wait(lk.mutex());
        }

        return 0;
#endif
    }
    catch (const std::exception& e)
    {
        NX_LOG(lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS);
        return 3;
    }
}

#ifdef USE_QAPPLICATION
void VmsGatewayProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if (application->isRunning())
    {
        NX_LOG("Server already started", cl_logERROR);
        application->quit();
        return;
    }
}

void VmsGatewayProcess::stop()
{
    pleaseStop();
    //TODO #ak wait for executeApplication to return?
}

bool VmsGatewayProcess::eventFilter(QObject* /*watched*/, QEvent* /*event*/)
{
    if (m_timerID != -1)
    {
        application()->killTimer(m_timerID);
        m_timerID = -1;
    }

    if (m_terminated)
        application()->quit();
    return false;
}
#endif

void VmsGatewayProcess::initializeLogging(const conf::Settings& settings)
{
    //logging
    if (settings.logging().logLevel != QString::fromLatin1("none"))
    {
        const QString& logDir = settings.logging().logDir;

        QDir().mkpath(logDir);
        const QString& logFileName = logDir + lit("/log_file");
        if (cl_log.create(logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1))
            QnLog::initLog(settings.logging().logLevel);
        else
            std::wcerr << L"Failed to create log file " << logFileName.toStdWString() << std::endl;
        NX_LOG(lit("================================================================================="), cl_logALWAYS);
        NX_LOG(lit("%1 started").arg(QnLibVmsGatewayAppInfo::applicationDisplayName()), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QnAppInfo::applicationVersion()), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QnAppInfo::applicationRevision()), cl_logALWAYS);
    }
}

void VmsGatewayProcess::registerApiHandlers(
    const conf::Settings& settings,
    nx_http::MessageDispatcher* const msgDispatcher)
{
    msgDispatcher->setDefaultProcessor<ProxyHandler>(
        [&settings]() -> std::unique_ptr<ProxyHandler> {
            return std::make_unique<ProxyHandler>(settings);
        });
}

void VmsGatewayProcess::publicAddressFetched(const QHostAddress& publicAddress)
{
    const QString publicAddressStr = publicAddress.toString();

    NX_LOGX(lm("Retrieved public address %1. This address will be used for cloud connect")
        .arg(publicAddressStr), cl_logINFO);

    nx::network::SocketGlobals::cloudConnectSettings()
        .replaceOriginatingHostAddress(publicAddressStr);
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
