/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "mediator_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <platform/process/current_process.h>
#include <utils/common/command_line_parser.h>
#include <nx/utils/log/log.h>
#include <utils/common/systemerror.h>
#include <nx/network/socket_global.h>

#include "listening_peer_pool.h"
#include "mediaserver_api.h"
#include "settings.h"
#include "version.h"


namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess( int argc, char **argv )
:
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void MediatorProcess::pleaseStop()
{
    application()->quit();
}

int MediatorProcess::executeApplication()
{
    conf::Settings settings;
    //parsing command line arguments
    settings.load(m_argc, m_argv);
    if (settings.showHelp())
    {
        settings.printCmdLineArgsHelp();
        return 0;
    }

    initializeLogging(settings);

    if (settings.stun().addrToListenList.empty())
    {
        NX_LOG( "No STUN address to listen", cl_logALWAYS );
        return 1;
    }
    if (settings.http().addrToListenList.empty())
    {
        NX_LOG( "No HTTP address to listen", cl_logERROR );
        return 1;
    }

    TimerManager timerManager;
    std::unique_ptr<AbstractCloudDataProvider> cloudDataProvider;
    if (settings.cloudDB().runWithCloud)
    {
        cloudDataProvider = AbstractCloudDataProviderFactory::create(
            settings.cloudDB().address.toStdString(),
            settings.cloudDB().user.toStdString(),
            settings.cloudDB().password.toStdString(),
            settings.cloudDB().updateInterval);
    }
    else
    {
        NX_LOG( lit( "STUN Server is running without cloud (debug mode)" ), cl_logALWAYS );
    }

    //STUN handlers
    stun::MessageDispatcher stunMessageDispatcher;
    MediaserverApi mediaserverApi( cloudDataProvider.get(), &stunMessageDispatcher );
    ListeningPeerPool listeningPeerPool( cloudDataProvider.get(), &stunMessageDispatcher );

    //accepting STUN requests by both tcp and udt
    m_multiAddressStunServer.reset(
        new MultiAddressServer<stun::SocketServer>( false, SocketFactory::NatTraversalType::nttDisabled ) );

    if (!m_multiAddressStunServer->bind(settings.stun().addrToListenList))
    {
        NX_LOG(lit("Can not bind to %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 2;
    }

    // process privilege reduction
    CurrentProcess::changeUser(settings.general().systemUserToRunUnder);

    if (!m_multiAddressStunServer->listen())
    {
        NX_LOG(lit("Can not listen on %1")
            .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
        return 3;
    }

    NX_LOG(lit("STUN Server is listening on %1")
        .arg(containerString(settings.stun().addrToListenList)), cl_logERROR);
    std::cout << QN_APPLICATION_NAME << " has been started" << std::endl;

    //TODO #ak remove qt event loop
    //application's main loop
    const int result = application()->exec();

    //stopping accepting incoming connections
    m_multiAddressStunServer.reset();

    return result;
}

void MediatorProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }
}

void MediatorProcess::stop()
{
    application()->quit();
}

void MediatorProcess::initializeLogging(const conf::Settings& settings)
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
        NX_LOG(lit("%1 started").arg(QN_APPLICATION_NAME), cl_logALWAYS);
        NX_LOG(lit("Software version: %1").arg(QN_APPLICATION_VERSION), cl_logALWAYS);
        NX_LOG(lit("Software revision: %1").arg(QN_APPLICATION_REVISION), cl_logALWAYS);
    }
}

} // namespace hpm
} // namespace nx
