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
#include <utils/common/systemerror.h>

#include "access_control/authentication_manager.h"
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
    QtService<QtSingleCoreApplication>(argc, argv, QnLibVmsGatewayAppInfo::applicationName()),
    m_argc( argc ),
    m_argv( argv ),
    m_terminated( false ),
    m_timerID( -1 )
{
    setServiceDescription(QnLibVmsGatewayAppInfo::applicationDisplayName());

    //if call Q_INIT_RESOURCE directly, linker will search for nx::cdb::libcloud_db and fail...
    registerQtResources();
}

void VmsGatewayProcess::pleaseStop()
{
    m_terminated = true;
    if (application())
        application()->quit();
}

void VmsGatewayProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

int VmsGatewayProcess::executeApplication()
{
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
        //authRestrictionList.allow(PingHandler::kHandlerPath, AuthMethod::noAuth);
        //authRestrictionList.allow(AddAccountHttpHandler::kHandlerPath, AuthMethod::noAuth);
        //authRestrictionList.allow(ActivateAccountHandler::kHandlerPath, AuthMethod::noAuth);
        //authRestrictionList.allow(ReactivateAccountHttpHandler::kHandlerPath, AuthMethod::noAuth);

        AuthenticationManager authenticationManager(
            authRestrictionList,
            streeManager);

        //registering HTTP handlers
        registerApiHandlers(&httpMessageDispatcher);

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

        application()->installEventFilter(this);
        if (m_terminated)
            return 0;

        NX_LOG(lit("%1 has been started")
            .arg(QnLibVmsGatewayAppInfo::applicationDisplayName()),
            cl_logALWAYS);
        std::cout << QnLibVmsGatewayAppInfo::applicationDisplayName().toStdString()
            << " has been started" << std::endl;

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

        //starting timer to check for m_terminated again after event loop start
        m_timerID = application()->startTimer(0);

        //TODO #ak remove qt event loop
        //application's main loop
        const int result = application()->exec();

        return result;
    }
    catch (const std::exception& e)
    {
        NX_LOG(lit("Failed to start application. %1").arg(e.what()), cl_logALWAYS);
        return 3;
    }
}

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
    nx_http::MessageDispatcher* const msgDispatcher)
{
    //msgDispatcher->registerRequestProcessor<PingHandler>(
    //    PingHandler::kHandlerPath,
    //    [&authorizationManager]() -> std::unique_ptr<PingHandler> {
    //        return std::make_unique<PingHandler>( authorizationManager );
    //    } );
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
