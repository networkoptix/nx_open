/**********************************************************
* 27 aug 2013
* a.kolesnikov
***********************************************************/

#include "hole_puncher_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <utils/common/command_line_parser.h>
#include <utils/common/log.h>
#include <utils/network/aio/aioservice.h>
#include <utils/common/systemerror.h>

#include "stun/custom_stun.h"
#include "http/http_message_dispatcher.h"
#include "http/register_http_handler.h"
#include "stun/stun_message_dispatcher.h"
#include "listening_peer_pool.h"
#include "version.h"


HolePuncherProcess::HolePuncherProcess( int argc, char **argv )
: 
    QtService<QtSingleCoreApplication>(argc, argv, QN_APPLICATION_NAME),
    m_argc( argc ),
    m_argv( argv )
{
    setServiceDescription(QN_APPLICATION_NAME);
}

void HolePuncherProcess::pleaseStop()
{
    application()->quit();
}

int HolePuncherProcess::executeApplication()
{
    static const QLatin1String DEFAULT_LOG_LEVEL( "ERROR" );
    static const QLatin1String DEFAULT_ADDRESS_TO_LISTEN( ":3345" );


    //reading settings
#ifdef _WIN32
    m_settings.reset( new QSettings(QSettings::SystemScope, QnVersion::organizationName(), QN_APPLICATION_NAME) );
#else
    const QString configFileName = QString("/opt/%1/%2/etc/%2.conf").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    m_settings.reset( new QSettings(configFileName, QSettings::IniFormat) );
#endif

    const QString& dataLocation = getDataDirectory();

    //parsing command line arguments
    bool showHelp = false;
    QString logLevel;

    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&showHelp, "--help", NULL, QString(), false);
    commandLineParser.addParameter(&logLevel, "--log-level", NULL, QString(), DEFAULT_LOG_LEVEL);
    commandLineParser.parse(m_argc, m_argv, stderr);

    if( showHelp )
        return printHelp();

    //logging
    if( logLevel != QString::fromLatin1("none") )
    {
        const QString& logDir = m_settings->value( "logDir", dataLocation + QLatin1String("/log/") ).toString();
        QDir().mkpath( logDir );
        const QString& logFileName = logDir + QLatin1String("/log_file");
        if( cl_log.create( logFileName, 1024 * 1024 * 10, 5, cl_logDEBUG1 ) )
            QnLog::initLog( logLevel );
        else
            std::wcerr<<L"Failed to create log file "<<logFileName.toStdWString()<<std::endl;
        NX_LOG( lit( "=================================================================================" ), cl_logALWAYS );
        NX_LOG( lit( "%1 started" ).arg( QN_APPLICATION_NAME ), cl_logALWAYS );
        NX_LOG( lit( "Software version: %1" ).arg( QN_APPLICATION_VERSION ), cl_logALWAYS );
        NX_LOG( lit( "Software revision: %1" ).arg( QN_APPLICATION_REVISION ), cl_logALWAYS );
    }

    const QStringList& addrToListenStrList = m_settings->value("addressToListen", DEFAULT_ADDRESS_TO_LISTEN).toString().split(',');
    std::list<SocketAddress> addrToListenList;
    std::transform(
        addrToListenStrList.begin(),
        addrToListenStrList.end(),
        std::back_inserter(addrToListenList),
        [] (const QString& str) { return SocketAddress(str); } );
    if( addrToListenList.empty() )
    {
        NX_LOG( "No address to listen", cl_logALWAYS );
        return 1;
    }

    //TODO opening database
    nx_hpm::RegisteredDomainsDataManager registeredDomainsDataManager;

    //HTTP handlers
    RegisterSystemHttpHandler registerHttpHandler;

    nx_http::MessageDispatcher httpMessageDispatcher;
    httpMessageDispatcher.registerRequestProcessor(
        RegisterSystemHttpHandler::HANDLER_PATH,
        [&registerHttpHandler](const std::weak_ptr<HttpServerConnection>& connection, nx_http::Message&& message) -> bool {
            return registerHttpHandler.processRequest( connection, std::move(message) );
        }
    );

    using namespace std::placeholders;

    //STUN handlers
    ListeningPeerPool listeningPeerPool;

    STUNMessageDispatcher stunMessageDispatcher;
    stunMessageDispatcher.registerRequestProcessor(
        nx_hpm::StunMethods::bind,
        [&listeningPeerPool](const std::weak_ptr<StunServerConnection>& connection, nx_stun::Message&& message) -> bool {
            return listeningPeerPool.processBindRequest( connection, std::move(message) );
        } );
    stunMessageDispatcher.registerRequestProcessor(
        nx_hpm::StunMethods::connect,
        [&listeningPeerPool](const std::weak_ptr<StunServerConnection>& connection, nx_stun::Message&& message) -> bool {
            return listeningPeerPool.processConnectRequest( connection, std::move(message) );
        } );

    m_multiAddressStunServer.reset( new MultiAddressServer<StunStreamSocketServer>( addrToListenList, true, SocketFactory::nttEnabled ) );
    m_multiAddressHttpServer.reset( new MultiAddressServer<nx_http::HttpStreamSocketServer>( addrToListenList, true, SocketFactory::nttDisabled ) );

    if( !m_multiAddressStunServer->bind() )
        return 2;
    if( !m_multiAddressHttpServer->bind() )
        return 3;

    //TODO: #ak process privilege reduction should be made here

    if( !m_multiAddressStunServer->listen() )
        return 4;
    if( !m_multiAddressHttpServer->listen() )
        return 5;

    //application's main loop
    const int result = application()->exec();

    //stopping accepting incoming connections
    m_multiAddressStunServer.reset();
    m_multiAddressHttpServer.reset();

    return result;
}

void HolePuncherProcess::start()
{
    QtSingleCoreApplication* application = this->application();

    if( application->isRunning() )
    {
        NX_LOG( "Server already started", cl_logERROR );
        application->quit();
        return;
    }
}

void HolePuncherProcess::stop()
{
    application()->quit();
}

QString HolePuncherProcess::getDataDirectory()
{
    const QString& dataDirFromSettings = m_settings->value( "dataDir" ).toString();
    if( !dataDirFromSettings.isEmpty() )
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var").arg(VER_LINUX_ORGANIZATION_NAME).arg(VER_PRODUCTNAME_STR);
    QString varDirName = qSettings.value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

int HolePuncherProcess::printHelp()
{
    //TODO #ak

    return 0;
}
